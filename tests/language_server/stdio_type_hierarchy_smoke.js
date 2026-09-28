const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');

/** 让层级语义断言失败传到 CTest 的进程退出路径。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** 以 UTF-8 字节长度封装发往 stdio 服务的 JSON-RPC 消息。 */
function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

/** 从测试夹具文本取得声明或调用点的位置，避免手工列号偏离源码。 */
function findPosition(text, substring, occurrence = 0, offset = 0) {
    let fromIndex = 0;
    let index = -1;

    for (let current = 0; current <= occurrence; current += 1) {
        index = text.indexOf(substring, fromIndex);
        if (index < 0) {
            throw new Error(`Unable to find substring "${substring}"`);
        }
        fromIndex = index + substring.length;
    }

    const target = index + offset;
    const lines = text.slice(0, target).split('\n');
    return {
        line: lines.length - 1,
        character: lines[lines.length - 1].length,
    };
}

/** 为本用例维护请求响应和诊断通知的独立队列。 */
class LspClient {
    /** 启动 CTest 提供的服务端，后续请求与通知共用同一 stdio 会话。 */
    constructor(serverPath) {
        this.nextId = 1;
        this.pending = new Map();
        this.notifications = [];
        this.waitingNotifications = [];
        this.buffer = Buffer.alloc(0);
        this.stderrChunks = [];
        this.process = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });

        /** stdout 按字节增量解析，不依赖 Node chunk 对应一条完整消息。 */
        this.process.stdout.on('data', (chunk) => this.handleData(chunk));
        /** stderr 只供失败和退出断言，不进入协议队列。 */
        this.process.stderr.on('data', (chunk) => this.stderrChunks.push(chunk));
    }

    /** 汇总服务端诊断以解释失败的退出码。 */
    stderr() {
        return Buffer.concat(this.stderrChunks).toString('utf8');
    }

    /** 用请求 id 关联层级查询与响应，超时使测试失败。 */
    request(method, params) {
        const id = this.nextId++;
        const payload = {
            jsonrpc: '2.0',
            id,
            method,
            params,
        };

        this.process.stdin.write(createMessage(payload));
        return new Promise((resolve, reject) => {
            this.pending.set(id, { resolve, reject });
            /** 响应等待存在 10 秒边界，防止服务端无响应时一直阻塞。
             * BUG: 正常响应只删除 pending id，未清除此定时器；每次成功请求的 10 秒计时
             * 仍保持 Node 事件循环，CTest 在服务端已退出后才结束。 */
            setTimeout(() => {
                if (this.pending.has(id)) {
                    this.pending.delete(id);
                    reject(new Error(`Timed out waiting for ${method}`));
                }
            }, 10000);
        });
    }

    /** 发送无响应的初始化和文档变更通知。 */
    notify(method, params) {
        this.process.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

    /**
     * 等待某类通知作为索引完成的同步点；早到的通知保存在队列中。
     * BUG: 服务端保持运行但不发布诊断时，本等待没有超时，只能依赖外部测试超时配置或进程终止；
     * 若服务端先退出，也没有 close 拒绝路径，主流程无法将缺失诊断报告为断言失败。
     */
    waitForNotification(method) {
        /** 先消费已经到达的目标通知，避免错过 didOpen 后的快速诊断。 */
        const index = this.notifications.findIndex((notification) => notification.method === method);
        if (index >= 0) {
            const [notification] = this.notifications.splice(index, 1);
            return Promise.resolve(notification.params);
        }

        return new Promise((resolve) => {
            this.waitingNotifications.push({ method, resolve });
        });
    }

    /** 把任意 stdout 分块重组为完整 LSP 帧后再分派。 */
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
                throw new Error(`Missing Content-Length header: ${header}`);
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

    /** 有 id 的消息交还请求者，其他消息按通知方法唤醒等待者。 */
    handleMessage(message) {
        if (Object.prototype.hasOwnProperty.call(message, 'id')) {
            const pending = this.pending.get(message.id);
            if (pending) {
                this.pending.delete(message.id);
                if (message.error) {
                    pending.reject(new Error(message.error.message || 'LSP request failed'));
                } else {
                    pending.resolve(message.result);
                }
            }
            return;
        }

        /** 同一方法的最早等待者接收本次通知。 */
        const waitingIndex = this.waitingNotifications.findIndex((entry) => entry.method === message.method);
        if (waitingIndex >= 0) {
            const [entry] = this.waitingNotifications.splice(waitingIndex, 1);
            entry.resolve(message.params);
            return;
        }
        this.notifications.push({ method: message.method, params: message.params });
    }

    /** shutdown/exit 后等待服务端退出，供调用者断言进程状态。 */
    waitForExit() {
        return new Promise((resolve) => {
            /** 退出事件是该用例唯一的进程完成同步点。 */
            this.process.on('exit', (code) => resolve(code));
        });
    }
}

/** 供 main 的 finally 回收本测试创建的临时文档根目录。 */
function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (fs.existsSync(targetPath)) {
        fs.rmdirSync(targetPath, { recursive: true });
    }
}

/** 用磁盘文档快照验证类型和调用层级的规范身份、调用边及版本失效。 */
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Usage: node stdio_type_hierarchy_smoke.js <serverPath>');

    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-type-hierarchy-'));
    const sourcePath = path.join(rootPath, 'src');
    const documentPath = path.join(sourcePath, 'type_hierarchy.zr');
    const documentUri = pathToFileURL(documentPath).toString();
    const text = [
        'class Base {',
        '}',
        '',
        'class Derived : Base {',
        '}',
        '',
        'fn helper(value: int): int { return value; }',
        '',
        'fn run(value: int): int {',
        '    var first = helper(value);',
        '    return first + helper(value);',
        '}',
        '',
        'fn lambdaCallee(): int { return 1; }',
        '',
        'fn lambdaOuter(): int {',
        '    var callback = fn(): int => { return lambdaCallee(); };',
        '    return callback();',
        '}',
        '',
    ].join('\n');
    const client = new LspClient(serverPath);

    try {
        fs.mkdirSync(sourcePath, { recursive: true });
        fs.writeFileSync(documentPath, text);

        const initializeResult = await client.request('initialize', {
            processId: process.pid,
            rootUri: pathToFileURL(rootPath).toString(),
            capabilities: {},
        });
        assert(initializeResult.capabilities.typeHierarchyProvider === true,
            'typeHierarchyProvider must be enabled');
        assert(initializeResult.capabilities.callHierarchyProvider === true,
            'callHierarchyProvider must be enabled');

        client.notify('initialized', {});
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: documentUri,
                languageId: 'zr',
                version: 1,
                text,
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics');

        // 修改返回 item 的展示名称，验证后续查询依赖规范身份而非客户端文本。
        const derivedPosition = findPosition(text, 'Derived', 0, 1);
        const basePosition = findPosition(text, 'Base', 0, 1);
        const derivedItems = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri: documentUri },
            position: derivedPosition,
        });
        assert(Array.isArray(derivedItems) && derivedItems.length > 0,
            'prepareTypeHierarchy must return Derived');
        assert(derivedItems[0].data && Number.isInteger(derivedItems[0].data.symbolId) &&
            derivedItems[0].data.symbolId > 0 &&
            Number.isInteger(derivedItems[0].data.typeId) && derivedItems[0].data.typeId > 0 &&
            derivedItems[0].data.version === 1,
            'prepareTypeHierarchy must publish stable semantic identity and document version');

        const supertypes = await client.request('typeHierarchy/supertypes', {
            item: { ...derivedItems[0], name: 'Unrelated' },
        });
        assert(Array.isArray(supertypes) && supertypes.some((item) => item && item.name === 'Base'),
            'typeHierarchy/supertypes must use semantic identity instead of the display name');

        const baseItems = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri: documentUri },
            position: basePosition,
        });
        assert(Array.isArray(baseItems) && baseItems.length > 0,
            'prepareTypeHierarchy must return Base');
        assert(baseItems[0].data && Number.isInteger(baseItems[0].data.symbolId) &&
            baseItems[0].data.symbolId > 0 &&
            Number.isInteger(baseItems[0].data.typeId) && baseItems[0].data.typeId > 0 &&
            baseItems[0].data.version === 1,
            'base hierarchy item must preserve stable semantic identity and document version');

        const subtypes = await client.request('typeHierarchy/subtypes', {
            item: { ...baseItems[0], name: 'Unrelated' },
        });
        assert(Array.isArray(subtypes) && subtypes.some((item) => item && item.name === 'Derived'),
            'typeHierarchy/subtypes must use semantic identity instead of the display name');

        // 同一调用者的两处 helper 调用应合并为一条带两个范围的规范边。
        const runPosition = findPosition(text, 'fn run', 0, 3);
        const helperPosition = findPosition(text, 'fn helper', 0, 3);
        const runItems = await client.request('textDocument/prepareCallHierarchy', {
            textDocument: { uri: documentUri },
            position: runPosition,
        });
        const helperItems = await client.request('textDocument/prepareCallHierarchy', {
            textDocument: { uri: documentUri },
            position: helperPosition,
        });
        assert(Array.isArray(runItems) && runItems.length === 1,
            'prepareCallHierarchy must return run');
        assert(Array.isArray(helperItems) && helperItems.length === 1,
            'prepareCallHierarchy must return helper');
        assert(runItems[0].data && Number.isInteger(runItems[0].data.symbolId) &&
            runItems[0].data.symbolId > 0 &&
            Number.isInteger(runItems[0].data.typeId) && runItems[0].data.typeId > 0 &&
            runItems[0].data.version === 1,
            'run call hierarchy item must publish stable semantic identity and document version');
        assert(helperItems[0].data && Number.isInteger(helperItems[0].data.symbolId) &&
            helperItems[0].data.symbolId > 0 &&
            Number.isInteger(helperItems[0].data.typeId) && helperItems[0].data.typeId > 0 &&
            helperItems[0].data.version === 1,
            'helper call hierarchy item must publish stable semantic identity and document version');

        const outgoing = await client.request('callHierarchy/outgoingCalls', {
            item: { ...runItems[0], name: 'helper' },
        });
        assert(Array.isArray(outgoing) && outgoing.length === 1 &&
            outgoing[0].to && outgoing[0].to.name === 'helper' &&
            outgoing[0].to.data.symbolId === helperItems[0].data.symbolId &&
            Array.isArray(outgoing[0].fromRanges) && outgoing[0].fromRanges.length === 2,
            'outgoing calls must group canonical helper edges and ignore the item display name');

        const incoming = await client.request('callHierarchy/incomingCalls', {
            item: { ...helperItems[0], name: 'run' },
        });
        assert(Array.isArray(incoming) && incoming.length === 1 &&
            incoming[0].from && incoming[0].from.name === 'run' &&
            incoming[0].from.data.symbolId === runItems[0].data.symbolId &&
            Array.isArray(incoming[0].fromRanges) && incoming[0].fromRanges.length === 2,
            'incoming calls must group canonical run edges and ignore the item display name');

        // Lambda 调用者由编译器生成，返回的 item 仍须可用于反向查询。
        const lambdaCalleePosition = findPosition(text, 'fn lambdaCallee', 0, 3);
        const lambdaCalleeItems = await client.request('textDocument/prepareCallHierarchy', {
            textDocument: { uri: documentUri },
            position: lambdaCalleePosition,
        });
        assert(Array.isArray(lambdaCalleeItems) && lambdaCalleeItems.length === 1 &&
            lambdaCalleeItems[0].data && Number.isInteger(lambdaCalleeItems[0].data.symbolId) &&
            lambdaCalleeItems[0].data.symbolId > 0,
            'prepareCallHierarchy must return canonical lambda callee identity');
        const lambdaIncoming = await client.request('callHierarchy/incomingCalls', {
            item: { ...lambdaCalleeItems[0], name: 'not_lambda_callee' },
        });
        assert(Array.isArray(lambdaIncoming) && lambdaIncoming.length === 1 &&
            lambdaIncoming[0].from && lambdaIncoming[0].from.data &&
            Number.isInteger(lambdaIncoming[0].from.data.symbolId) &&
            lambdaIncoming[0].from.data.symbolId > 0 &&
            lambdaIncoming[0].from.data.symbolId !== lambdaCalleeItems[0].data.symbolId &&
            Array.isArray(lambdaIncoming[0].fromRanges) &&
            lambdaIncoming[0].fromRanges.length === 1,
            'incoming calls must serialize the canonical lambda caller identity');
        const lambdaOutgoing = await client.request('callHierarchy/outgoingCalls', {
            item: { ...lambdaIncoming[0].from, name: 'not_callback' },
        });
        assert(Array.isArray(lambdaOutgoing) && lambdaOutgoing.length === 1 &&
            lambdaOutgoing[0].to && lambdaOutgoing[0].to.data &&
            lambdaOutgoing[0].to.data.symbolId === lambdaCalleeItems[0].data.symbolId &&
            Array.isArray(lambdaOutgoing[0].fromRanges) &&
            lambdaOutgoing[0].fromRanges.length === 1,
            'returned lambda hierarchy item must re-resolve by canonical identity');

        // 内容即使未变，版本增加也必须阻止旧 item 跨快照查询。
        client.notify('textDocument/didChange', {
            textDocument: { uri: documentUri, version: 2 },
            contentChanges: [{ text }],
        });
        await client.waitForNotification('textDocument/publishDiagnostics');
        const staleOutgoing = await client.request('callHierarchy/outgoingCalls', {
            item: runItems[0],
        });
        assert(Array.isArray(staleOutgoing) && staleOutgoing.length === 0,
            'call hierarchy follow-up must fail closed for a stale document version');

        const shutdown = await client.request('shutdown', undefined);
        assert(shutdown === null, 'shutdown must return null');
        client.notify('exit', undefined);
        const exitCode = await client.waitForExit();
        assert(exitCode === 0, `server exited with ${exitCode}. stderr=${client.stderr()}`);
        assert(client.stderr().trim() === '', `language server stderr must stay empty. stderr=${client.stderr()}`);
    } finally {
        removePathSync(rootPath);
    }
}

/** 将协议或层级语义错误转换为 CTest 非零状态。 */
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
