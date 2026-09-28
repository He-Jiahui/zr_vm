const { spawn } = require('child_process');

// CTest 以服务端可执行文件路径调用本脚本；断言失败须让整个进程以非零状态结束。
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// 发送端按 UTF-8 字节数写 LSP 帧，保证含非 ASCII 文本时与服务端的 Content-Length 契约一致。
function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

// 单次测试复用一个 stdio 会话：按请求 id 匹配响应，并按方法缓存可能先于等待者到达的诊断通知。
class LspClient {
    constructor(serverPath) {
        // 两组 pending 表分别跟踪请求和通知；关闭时必须让所有等待者收到服务端退出原因。
        this.nextId = 1;
        this.pendingResponses = new Map();
        this.pendingNotifications = new Map();
        this.notificationBacklog = new Map();
        this.buffer = Buffer.alloc(0);
        this.stderrChunks = [];
        this.closed = false;
        this.exitCode = null;
        this.exitSignal = null;

        this.child = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });

        // stdout 可能任意分片或合并多个帧；解析交给 handleData，stderr 只保留作失败诊断。
        this.child.stdout.on('data', (chunk) => this.handleData(chunk));
        this.child.stderr.on('data', (chunk) => {
            this.stderrChunks.push(Buffer.from(chunk));
        });
        this.child.on('exit', (code, signal) => {
            this.exitCode = code;
            this.exitSignal = signal;
        });
        // close 晚于流关闭，是解除响应与通知等待的统一终点；避免测试一直等超时。
        this.child.on('close', (code, signal) => {
            this.closed = true;
            if (this.exitCode === null) {
                this.exitCode = code;
            }
            if (this.exitSignal === null) {
                this.exitSignal = signal;
            }

            for (const { reject, timer, method } of this.pendingResponses.values()) {
                clearTimeout(timer);
                reject(new Error(
                    `Server closed before responding to ${method}. ` +
                    `exitCode=${this.exitCode} signal=${this.exitSignal} stderr=${this.stderr()}`));
            }
            this.pendingResponses.clear();

            for (const [method, waiters] of this.pendingNotifications.entries()) {
                for (const { reject, timer } of waiters) {
                    clearTimeout(timer);
                    reject(new Error(
                        `Server closed before notification ${method}. ` +
                        `exitCode=${this.exitCode} signal=${this.exitSignal} stderr=${this.stderr()}`));
                }
            }
            this.pendingNotifications.clear();
        });
    }

    // 请求失败时附带服务端 stderr，便于区分协议断言失败与服务端启动/运行失败。
    stderr() {
        return Buffer.concat(this.stderrChunks).toString('utf8');
    }

    // 仅用于 initialized、didOpen 和 exit 等无需响应的 LSP 通知；调用方负责时序。
    notify(method, params) {
        this.child.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

    // 以递增 id 关联响应；超时只撤销本次等待，不假定服务端已停止处理该请求。
    request(method, params, timeoutMs = 10000) {
        const id = this.nextId++;
        const payload = {
            jsonrpc: '2.0',
            id,
            method,
            params,
        };

        return new Promise((resolve, reject) => {
            if (this.closed) {
                reject(new Error(`Server already closed before ${method}. stderr=${this.stderr()}`));
                return;
            }

            const timer = setTimeout(() => {
                this.pendingResponses.delete(id);
                reject(new Error(`Timed out waiting for response to ${method}. stderr=${this.stderr()}`));
            }, timeoutMs);

            this.pendingResponses.set(id, { resolve, reject, timer, method });
            this.child.stdin.write(createMessage(payload));
        });
    }

    // didOpen 后诊断可能已进入积压队列，也可能稍后到达；两种次序都返回同一方法的下一条通知。
    waitForNotification(method, timeoutMs = 10000) {
        const backlog = this.notificationBacklog.get(method);
        if (backlog && backlog.length > 0) {
            return Promise.resolve(backlog.shift());
        }

        return new Promise((resolve, reject) => {
            if (this.closed) {
                reject(new Error(`Server already closed before notification ${method}. stderr=${this.stderr()}`));
                return;
            }

            const timer = setTimeout(() => {
                const waiters = this.pendingNotifications.get(method) || [];
                const index = waiters.findIndex((entry) => entry.reject === reject);
                if (index >= 0) {
                    waiters.splice(index, 1);
                }
                reject(new Error(`Timed out waiting for notification ${method}. stderr=${this.stderr()}`));
            }, timeoutMs);

            const waiters = this.pendingNotifications.get(method) || [];
            waiters.push({ resolve, reject, timer });
            this.pendingNotifications.set(method, waiters);
        });
    }

    // LSP 的长度按字节计算；保留未完成帧，直到 stdout 提供完整消息后才交给响应路由。
    handleData(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);

        while (true) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd < 0) {
                return;
            }

            const header = this.buffer.slice(0, headerEnd).toString('ascii');
            const match = /Content-Length: (\d+)/i.exec(header);
            if (!match) {
                throw new Error(`Malformed LSP header: ${header}`);
            }

            const length = Number(match[1]);
            const messageStart = headerEnd + 4;
            const messageEnd = messageStart + length;
            if (this.buffer.length < messageEnd) {
                return;
            }

            const message = JSON.parse(this.buffer.slice(messageStart, messageEnd).toString('utf8'));
            this.buffer = this.buffer.slice(messageEnd);
            this.handleMessage(message);
        }
    }

    // 响应按 id 唤醒对应请求；无 id 的服务端通知按方法交给等待者或积压队列。
    handleMessage(message) {
        if (Object.prototype.hasOwnProperty.call(message, 'id')) {
            const pending = this.pendingResponses.get(message.id);
            if (!pending) {
                return;
            }

            clearTimeout(pending.timer);
            this.pendingResponses.delete(message.id);
            if (message.error) {
                pending.reject(new Error(message.error.message || JSON.stringify(message.error)));
            } else {
                pending.resolve(message.result);
            }
            return;
        }

        if (!message.method) {
            return;
        }

        const waiters = this.pendingNotifications.get(message.method);
        if (waiters && waiters.length > 0) {
            const waiter = waiters.shift();
            clearTimeout(waiter.timer);
            waiter.resolve(message.params);
            return;
        }

        const backlog = this.notificationBacklog.get(message.method) || [];
        backlog.push(message.params);
        this.notificationBacklog.set(message.method, backlog);
    }

    // shutdown 的响应不代表进程已退出；exit 通知后还要等 stdio 真正关闭。
    waitForExit(timeoutMs = 10000) {
        return new Promise((resolve, reject) => {
            if (this.closed) {
                resolve(this.exitCode);
                return;
            }

            const timer = setTimeout(() => {
                reject(new Error(`Timed out waiting for server exit. stderr=${this.stderr()}`));
            }, timeoutMs);
            this.child.on('close', (code) => {
                clearTimeout(timer);
                resolve(code);
            });
        });
    }
}

// CTest 注入服务端路径；同一会话逐个打开独立文档，观察语义事实经 stdio 协议投影为 inlineValue。
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server executable path');

    // 使用带编码盘符与加号的 file URI，避免测试依赖本机真实工作区路径。
    const uri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-identifier-expression.zr';
    const multilineUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-multiline-return.zr';
    const returnNextLineUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-return-next-line.zr';
    const multilineInitializerUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-multiline-initializer.zr';
    const unaryExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-unary-expression.zr';
    const callMemberExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-call-member-expression.zr';
    const computedMemberExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-computed-member-expression.zr';
    const aggregateExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-aggregate-expression.zr';
    const objectAggregateExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-object-aggregate-expression.zr';
    const continuationExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-continuation-expression.zr';
    const blockCommentInlineValueUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-block-comment.zr';
    const stringInlineValueUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-string-literals.zr';
    // 正向样例分别覆盖标识符、跨行表达式、一元运算、调用/成员访问及聚合表达式的事实锚点。
    const text = [
        'fn main(): void {',
        '    var seed = 2;',
        '    seed + 3;',
        '}',
        '',
    ].join('\n');
    const multilineText = [
        'fn main(): int {',
        '    return 1 +',
        '        2;',
        '}',
        '',
    ].join('\n');
    const returnNextLineText = [
        'fn main(): int {',
        '    return',
        '        1 + 2;',
        '}',
        '',
    ].join('\n');
    const multilineInitializerText = [
        'fn main(): void {',
        '    var sum =',
        '        1 + 2;',
        '}',
        '',
    ].join('\n');
    const unaryExpressionText = [
        'fn main(): void {',
        '    !true;',
        '    -42;',
        '}',
        '',
    ].join('\n');
    const callMemberExpressionText = [
        'fn pick(value: int): int {',
        '    return value;',
        '}',
        'fn main(): void {',
        '    var seed = {value: 2};',
        '    pick(42);',
        '    seed.value;',
        '}',
        '',
    ].join('\n');
    const computedMemberExpressionText = [
        'fn main(): void {',
        '    var index = 0;',
        '    var seed = {value: 2};',
        '    seed[index];',
        '}',
        '',
    ].join('\n');
    const aggregateExpressionText = [
        'fn main(): void {',
        '    [1 + 2];',
        '    [true || false];',
        '}',
        '',
    ].join('\n');
    const objectAggregateExpressionText = [
        'fn main(): void {',
        '    var anchor = 0;',
        '    {[1 + 2]: 4};',
        '    {',
        '        a: 1 + 2',
        '    };',
        '    {a: 1 + 2};',
        '}',
        '',
    ].join('\n');
    const continuationExpressionText = [
        'fn main(): void {',
        '    1 +',
        '        2;',
        '}',
        '',
    ].join('\n');
    // 阴性样例中的“变量”只存在于注释或字符串，用来防止扫描器把词法文本误认作运行时变量。
    const blockCommentInlineValueText = [
        'fn main(): void {',
        '    /*',
        '    var ghost = 1;',
        '    */',
        '    /* var inlineGhost = 2; */',
        '}',
        '/* var topGhost = 3; */',
        '',
    ].join('\n');
    const stringInlineValueText = [
        'fn main(): void {',
        '    "var stringGhost = 4;";',
        "    'var singleGhost = 5;';",
        '    `var templateGhost = 6;`;',
        '}',
        '',
    ].join('\n');
    const client = new LspClient(serverPath);

    try {
        // initialize/initialized 建立标准 LSP 会话；每次 didOpen 后先等诊断，确认服务端已接收文档。
        await client.request('initialize', {
            processId: null,
            rootUri: null,
            capabilities: {},
        });
        client.notify('initialized', {});
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text,
            },
        });

        const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        assert(diagnostics.uri === uri, 'inline identifier expression diagnostics uri mismatch');
        assert(Array.isArray(diagnostics.diagnostics), 'diagnostics must be an array');

        // 当前函数声明本身没有可展示的求值事实；相邻的表达式语句才应产生数值事实。
        const declarationValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri },
            range: {
                start: { line: 0, character: 0 },
                end: { line: 0, character: 16 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 0, character: 0 },
                    end: { line: 0, character: 16 },
                },
            },
        });
        assert(Array.isArray(declarationValues) && declarationValues.length === 0,
            'textDocument/inlineValue must ignore current fn declarations');

        const values = await client.request('textDocument/inlineValue', {
            textDocument: { uri },
            range: {
                start: { line: 2, character: 0 },
                end: { line: 2, character: 13 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 2, character: 0 },
                    end: { line: 2, character: 13 },
                },
            },
        });

        assert(Array.isArray(values), 'inlineValue response must be an array');
        assert(values.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 5..5') &&
                value.range &&
                value.range.start.line === 2 &&
                value.range.start.character === 4 &&
                value.range.end.line === 2 &&
                value.range.end.character === 12),
        `textDocument/inlineValue must expose semantic numeric facts for identifier expression statements; values=${
            JSON.stringify(values)}`);

        // 跨行 return 的事实应覆盖完整表达式，而非只落在第一行的运算符附近。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: multilineUri,
                languageId: 'zr',
                version: 1,
                text: multilineText,
            },
        });

        const multilineDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        assert(multilineDiagnostics.uri === multilineUri, 'inline multiline return diagnostics uri mismatch');
        assert(Array.isArray(multilineDiagnostics.diagnostics), 'multiline diagnostics must be an array');

        const multilineValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: multilineUri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 2, character: 10 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 1, character: 0 },
                    end: { line: 2, character: 10 },
                },
            },
        });

        assert(Array.isArray(multilineValues), 'multiline inlineValue response must be an array');
        assert(multilineValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 11 &&
                value.range.end.line === 2 &&
                value.range.end.character === 9),
        `textDocument/inlineValue must expose semantic facts for multi-line return expressions; values=${
            JSON.stringify(multilineValues)}`);

        // return 与表达式分处两行时只返回一次事实，并锚在实际表达式范围。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: returnNextLineUri,
                languageId: 'zr',
                version: 1,
                text: returnNextLineText,
            },
        });

        const returnNextLineDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(returnNextLineDiagnostics.uri === returnNextLineUri,
            'inline return-next-line diagnostics uri mismatch');
        assert(Array.isArray(returnNextLineDiagnostics.diagnostics),
            'return-next-line diagnostics must be an array');

        const returnNextLineValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: returnNextLineUri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 2, character: 16 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 1, character: 0 },
                    end: { line: 2, character: 16 },
                },
            },
        });

        assert(Array.isArray(returnNextLineValues),
            'return-next-line inlineValue response must be an array');
        const returnNextLineFacts = returnNextLineValues.filter((value) =>
            value &&
            typeof value.text === 'string' &&
            value.text.includes('range 3..3'));
        assert(returnNextLineFacts.length === 1,
            `textDocument/inlineValue must emit one semantic fact for return-next-line expressions; values=${
                JSON.stringify(returnNextLineValues)}`);
        assert(returnNextLineFacts[0].range &&
                returnNextLineFacts[0].range.start.line === 2 &&
                returnNextLineFacts[0].range.start.character === 8 &&
                returnNextLineFacts[0].range.end.line === 2 &&
                returnNextLineFacts[0].range.end.character === 13,
        `textDocument/inlineValue must anchor return-next-line facts to the expression range; values=${
            JSON.stringify(returnNextLineValues)}`);

        // 跨行初始化需要同时保留变量运行时查找和语义事实；仅请求续行也应找回声明名锚点。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: multilineInitializerUri,
                languageId: 'zr',
                version: 1,
                text: multilineInitializerText,
            },
        });

        const multilineInitializerDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(multilineInitializerDiagnostics.uri === multilineInitializerUri,
            'inline multiline initializer diagnostics uri mismatch');
        assert(Array.isArray(multilineInitializerDiagnostics.diagnostics),
            'multiline initializer diagnostics must be an array');

        const multilineInitializerValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: multilineInitializerUri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 2, character: 16 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 1, character: 0 },
                    end: { line: 2, character: 16 },
                },
            },
        });

        assert(Array.isArray(multilineInitializerValues),
            'multiline initializer inlineValue response must be an array');
        assert(multilineInitializerValues.some((value) =>
                value &&
                value.variableName === 'sum' &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 8 &&
                value.range.end.line === 1 &&
                value.range.end.character === 11),
        `textDocument/inlineValue must still expose runtime lookup for multi-line initializers; values=${
            JSON.stringify(multilineInitializerValues)}`);
        assert(multilineInitializerValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 8 &&
                value.range.end.line === 1 &&
                value.range.end.character === 11),
        `textDocument/inlineValue must attach multi-line initializer semantic facts to the variable name; values=${
            JSON.stringify(multilineInitializerValues)}`);
        assert(!multilineInitializerValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 2 &&
                value.range.start.character === 8),
        `textDocument/inlineValue must not duplicate multi-line initializer facts on the continuation expression; values=${
            JSON.stringify(multilineInitializerValues)}`);

        // 仅请求续行仍应找到上一行的声明名；这是跨行扫描回溯的独立回归边界。
        const multilineInitializerContinuationOnlyValues =
            await client.request('textDocument/inlineValue', {
                textDocument: { uri: multilineInitializerUri },
                range: {
                    start: { line: 2, character: 0 },
                    end: { line: 2, character: 16 },
                },
                context: {
                    frameId: 1,
                    stoppedLocation: {
                        start: { line: 2, character: 0 },
                        end: { line: 2, character: 16 },
                    },
                },
            });

        assert(Array.isArray(multilineInitializerContinuationOnlyValues),
            'continuation-only initializer inlineValue response must be an array');
        assert(multilineInitializerContinuationOnlyValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 8 &&
                value.range.end.line === 1 &&
                value.range.end.character === 11),
        `textDocument/inlineValue must recover multi-line initializer facts when only the continuation line is requested; values=${
            JSON.stringify(multilineInitializerContinuationOnlyValues)}`);

        // 一元表达式语句分别锁住逻辑值与带符号数值事实，避免只支持二元表达式。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: unaryExpressionUri,
                languageId: 'zr',
                version: 1,
                text: unaryExpressionText,
            },
        });

        const unaryExpressionDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(unaryExpressionDiagnostics.uri === unaryExpressionUri,
            'inline unary expression diagnostics uri mismatch');
        assert(Array.isArray(unaryExpressionDiagnostics.diagnostics),
            'unary expression diagnostics must be an array');

        const unaryExpressionValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: unaryExpressionUri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 2, character: 9 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 1, character: 0 },
                    end: { line: 2, character: 9 },
                },
            },
        });

        assert(Array.isArray(unaryExpressionValues),
            'unary expression inlineValue response must be an array');
        assert(unaryExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('logical false') &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 4 &&
                value.range.end.line === 1 &&
                value.range.end.character === 9),
        `textDocument/inlineValue must expose logical facts for unary expression statements; values=${
            JSON.stringify(unaryExpressionValues)}`);
        assert(unaryExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range -42..-42') &&
                value.range &&
                value.range.start.line === 2 &&
                value.range.start.character === 4 &&
                value.range.end.line === 2 &&
                value.range.end.character === 7),
        `textDocument/inlineValue must expose numeric facts for unary expression statements; values=${
            JSON.stringify(unaryExpressionValues)}`);

        // 调用和成员访问从语义查询取得 payload；测试完整调用/访问范围与事实文本。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: callMemberExpressionUri,
                languageId: 'zr',
                version: 1,
                text: callMemberExpressionText,
            },
        });

        const callMemberExpressionDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(callMemberExpressionDiagnostics.uri === callMemberExpressionUri,
            'inline call/member expression diagnostics uri mismatch');
        assert(Array.isArray(callMemberExpressionDiagnostics.diagnostics),
            'call/member expression diagnostics must be an array');

        const callMemberExpressionValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: callMemberExpressionUri },
            range: {
                start: { line: 5, character: 0 },
                end: { line: 6, character: 16 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 5, character: 0 },
                    end: { line: 6, character: 16 },
                },
            },
        });

        assert(Array.isArray(callMemberExpressionValues),
            'call/member inlineValue response must be an array');
        assert(callMemberExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('call pick args=1') &&
                value.range &&
                value.range.start.line === 5 &&
                value.range.start.character === 4 &&
                value.range.end.line === 5 &&
                value.range.end.character === 12),
        `textDocument/inlineValue must expose call payload facts for call expression statements; values=${
            JSON.stringify(callMemberExpressionValues)}`);
        assert(callMemberExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('member value') &&
                value.range &&
                value.range.start.line === 6 &&
                value.range.start.character === 4 &&
                value.range.end.line === 6 &&
                value.range.end.character === 14),
        `textDocument/inlineValue must expose member payload facts for member expression statements; values=${
            JSON.stringify(callMemberExpressionValues)}`);

        // 下标成员访问还必须携带引用事实；此场景区别于点号成员访问。
        // TODO: 2026-08-11 的 L8 验收记录称旧验证二进制在此 payload 断言失败；
        // 需用当前源码构建的服务端重跑本 CTest，确认该历史缺口是否仍存在。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: computedMemberExpressionUri,
                languageId: 'zr',
                version: 1,
                text: computedMemberExpressionText,
            },
        });

        const computedMemberExpressionDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(computedMemberExpressionDiagnostics.uri === computedMemberExpressionUri,
            'inline computed-member expression diagnostics uri mismatch');
        assert(Array.isArray(computedMemberExpressionDiagnostics.diagnostics),
            'computed-member expression diagnostics must be an array');

        const computedMemberExpressionValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: computedMemberExpressionUri },
            range: {
                start: { line: 3, character: 0 },
                end: { line: 3, character: 16 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 3, character: 0 },
                    end: { line: 3, character: 16 },
                },
            },
        });

        assert(Array.isArray(computedMemberExpressionValues),
            'computed-member inlineValue response must be an array');
        assert(computedMemberExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('member index') &&
                value.text.includes('reference member access') &&
                value.range &&
                value.range.start.line === 3 &&
                value.range.start.character === 4 &&
                value.range.end.line === 3 &&
                value.range.end.character === 15),
        `textDocument/inlineValue must expose computed-member payload and reference facts for expression statements; values=${
            JSON.stringify(computedMemberExpressionValues)}`);

        // 数组表达式语句要把嵌套算术与短路逻辑事实投影到外层展示范围。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: aggregateExpressionUri,
                languageId: 'zr',
                version: 1,
                text: aggregateExpressionText,
            },
        });

        const aggregateExpressionDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(aggregateExpressionDiagnostics.uri === aggregateExpressionUri,
            'inline aggregate expression diagnostics uri mismatch');
        assert(Array.isArray(aggregateExpressionDiagnostics.diagnostics),
            'aggregate expression diagnostics must be an array');

        const aggregateExpressionValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: aggregateExpressionUri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 2, character: 21 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 1, character: 0 },
                    end: { line: 2, character: 21 },
                },
            },
        });

        assert(Array.isArray(aggregateExpressionValues),
            'aggregate expression inlineValue response must be an array');
        assert(aggregateExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 4 &&
                value.range.end.line === 1 &&
                value.range.end.character === 11),
        `textDocument/inlineValue must expose nested numeric facts for aggregate expression statements; values=${
            JSON.stringify(aggregateExpressionValues)}`);
        assert(aggregateExpressionValues.some((value) =>
                value &&
                value.text === 'logical true, short-circuits' &&
                value.range &&
                value.range.start.line === 2 &&
                value.range.start.character === 4 &&
                value.range.end.line === 2 &&
                value.range.end.character === 19),
        `textDocument/inlineValue must expose nested logical facts for aggregate expression statements; values=${
            JSON.stringify(aggregateExpressionValues)}`);

        // 对象聚合覆盖计算键、跨行普通键和同一行普通键，防止行首扫描或跨行终点截断事实。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: objectAggregateExpressionUri,
                languageId: 'zr',
                version: 1,
                text: objectAggregateExpressionText,
            },
        });

        const objectAggregateExpressionDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(objectAggregateExpressionDiagnostics.uri === objectAggregateExpressionUri,
            'inline object-aggregate expression diagnostics uri mismatch');
        assert(Array.isArray(objectAggregateExpressionDiagnostics.diagnostics),
            'object-aggregate expression diagnostics must be an array');

        const objectAggregateExpressionValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: objectAggregateExpressionUri },
            range: {
                start: { line: 2, character: 0 },
                end: { line: 6, character: 17 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 2, character: 0 },
                    end: { line: 6, character: 17 },
                },
            },
        });

        assert(Array.isArray(objectAggregateExpressionValues),
            'object-aggregate expression inlineValue response must be an array');
        assert(objectAggregateExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 2 &&
                value.range.start.character === 4 &&
                value.range.end.line === 2 &&
                value.range.end.character === 16),
        `textDocument/inlineValue must expose computed-key numeric facts for object expression statements; values=${
            JSON.stringify(objectAggregateExpressionValues)}`);
        assert(objectAggregateExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 3 &&
                value.range.start.character === 4 &&
                value.range.end.line === 5 &&
                value.range.end.character === 5),
        `textDocument/inlineValue must expose nested value facts for multi-line object expression statements; values=${
            JSON.stringify(objectAggregateExpressionValues)}`);
        assert(objectAggregateExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 6 &&
                value.range.start.character === 4 &&
                value.range.end.line === 6 &&
                value.range.end.character === 14),
        `textDocument/inlineValue must expose nested value facts for same-line object expression statements; values=${
            JSON.stringify(objectAggregateExpressionValues)}`);

        // 请求从表达式续行开始时，服务端仍应回溯到表达式起始处取得同一语义事实。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: continuationExpressionUri,
                languageId: 'zr',
                version: 1,
                text: continuationExpressionText,
            },
        });

        const continuationExpressionDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(continuationExpressionDiagnostics.uri === continuationExpressionUri,
            'inline continuation expression diagnostics uri mismatch');
        assert(Array.isArray(continuationExpressionDiagnostics.diagnostics),
            'continuation expression diagnostics must be an array');

        const continuationExpressionValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: continuationExpressionUri },
            range: {
                start: { line: 2, character: 0 },
                end: { line: 2, character: 10 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 2, character: 0 },
                    end: { line: 2, character: 10 },
                },
            },
        });

        assert(Array.isArray(continuationExpressionValues),
            'continuation expression inlineValue response must be an array');
        assert(continuationExpressionValues.some((value) =>
                value &&
                typeof value.text === 'string' &&
                value.text.includes('range 3..3') &&
                value.range &&
                value.range.start.line === 1 &&
                value.range.start.character === 4 &&
                value.range.end.line === 2 &&
                value.range.end.character === 9),
        `textDocument/inlineValue must expose semantic facts when the request starts on a continuation line; values=${
            JSON.stringify(continuationExpressionValues)}`);

        // 多行、行内与顶层块注释分别锁住状态延续、同一行过滤和零列起始过滤。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: blockCommentInlineValueUri,
                languageId: 'zr',
                version: 1,
                text: blockCommentInlineValueText,
            },
        });

        const blockCommentDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(blockCommentDiagnostics.uri === blockCommentInlineValueUri,
            'inline block-comment diagnostics uri mismatch');
        assert(Array.isArray(blockCommentDiagnostics.diagnostics),
            'block-comment diagnostics must be an array');

        const blockCommentValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: blockCommentInlineValueUri },
            range: {
                start: { line: 2, character: 0 },
                end: { line: 2, character: 18 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 2, character: 0 },
                    end: { line: 2, character: 18 },
                },
            },
        });

        assert(Array.isArray(blockCommentValues),
            'block-comment inlineValue response must be an array');
        assert(blockCommentValues.length === 0,
            `textDocument/inlineValue must ignore variable-looking text inside block comments; values=${
                JSON.stringify(blockCommentValues)}`);

        // 同一行起止的块注释不应因扫描状态未跨行而漏过过滤。
        const singleLineBlockCommentValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: blockCommentInlineValueUri },
            range: {
                start: { line: 4, character: 0 },
                end: { line: 4, character: 32 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 4, character: 0 },
                    end: { line: 4, character: 32 },
                },
            },
        });
        assert(Array.isArray(singleLineBlockCommentValues),
            'single-line block-comment inlineValue response must be an array');
        assert(singleLineBlockCommentValues.length === 0,
            `textDocument/inlineValue must ignore single-line block-comment variables; values=${
                JSON.stringify(singleLineBlockCommentValues)}`);

        // 顶层零列注释单独检查，避免行缩进假设掩盖伪声明。
        const topLevelBlockCommentValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: blockCommentInlineValueUri },
            range: {
                start: { line: 6, character: 0 },
                end: { line: 6, character: 23 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 6, character: 0 },
                    end: { line: 6, character: 23 },
                },
            },
        });
        assert(Array.isArray(topLevelBlockCommentValues),
            'top-level block-comment inlineValue response must be an array');
        assert(topLevelBlockCommentValues.length === 0,
            `textDocument/inlineValue must ignore zero-column block-comment variables; values=${
                JSON.stringify(topLevelBlockCommentValues)}`);

        // 三种字符串定界符都不能让形似变量声明的文本泄漏为 inlineValue。
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: stringInlineValueUri,
                languageId: 'zr',
                version: 1,
                text: stringInlineValueText,
            },
        });

        const stringDiagnostics =
            await client.waitForNotification('textDocument/publishDiagnostics');
        assert(stringDiagnostics.uri === stringInlineValueUri,
            'inline string-literal diagnostics uri mismatch');
        assert(Array.isArray(stringDiagnostics.diagnostics),
            'string-literal diagnostics must be an array');

        const stringLineValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: stringInlineValueUri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 1, character: 30 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 1, character: 0 },
                    end: { line: 1, character: 30 },
                },
            },
        });
        assert(Array.isArray(stringLineValues),
            'double-quoted string inlineValue response must be an array');
        assert(stringLineValues.length === 0,
            `textDocument/inlineValue must ignore variable-looking text inside double-quoted strings; values=${
                JSON.stringify(stringLineValues)}`);

        // 单引号字面量复用同一过滤契约，防止只识别双引号。
        const singleStringLineValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: stringInlineValueUri },
            range: {
                start: { line: 2, character: 0 },
                end: { line: 2, character: 30 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 2, character: 0 },
                    end: { line: 2, character: 30 },
                },
            },
        });
        assert(Array.isArray(singleStringLineValues),
            'single-quoted string inlineValue response must be an array');
        assert(singleStringLineValues.length === 0,
            `textDocument/inlineValue must ignore variable-looking text inside single-quoted strings; values=${
                JSON.stringify(singleStringLineValues)}`);

        // 模板字符串也应保持为非代码区，即使内容看似变量声明。
        const templateStringLineValues = await client.request('textDocument/inlineValue', {
            textDocument: { uri: stringInlineValueUri },
            range: {
                start: { line: 3, character: 0 },
                end: { line: 3, character: 34 },
            },
            context: {
                frameId: 1,
                stoppedLocation: {
                    start: { line: 3, character: 0 },
                    end: { line: 3, character: 34 },
                },
            },
        });
        assert(Array.isArray(templateStringLineValues),
            'template string inlineValue response must be an array');
        assert(templateStringLineValues.length === 0,
            `textDocument/inlineValue must ignore variable-looking text inside template strings; values=${
                JSON.stringify(templateStringLineValues)}`);

        // 按 LSP 顺序请求 shutdown、发送 exit，再观察子进程退出；异常时终止该会话。
        // BUG: waitForExit 对非零退出码仍兑现 Promise，此处丢弃返回值；
        // 若前面的 inlineValue 断言已通过，而服务端随后在 shutdown/exit 阶段失败，CTest 仍会报告通过。
        // 后续应断言零退出码，并明确 stderr 是否必须为空。
        await client.request('shutdown', {});
        client.notify('exit', {});
        await client.waitForExit();
    } catch (error) {
        client.child.kill();
        throw error;
    }
}

// 把异步会话中的断言或协议错误传递为 CTest 可观察的退出码。
main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
