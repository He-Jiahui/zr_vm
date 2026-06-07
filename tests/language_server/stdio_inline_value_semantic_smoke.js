const { spawn } = require('child_process');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

class LspClient {
    constructor(serverPath) {
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

        this.child.stdout.on('data', (chunk) => this.handleData(chunk));
        this.child.stderr.on('data', (chunk) => {
            this.stderrChunks.push(Buffer.from(chunk));
        });
        this.child.on('exit', (code, signal) => {
            this.exitCode = code;
            this.exitSignal = signal;
        });
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

    stderr() {
        return Buffer.concat(this.stderrChunks).toString('utf8');
    }

    notify(method, params) {
        this.child.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

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

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server executable path');

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
    const text = [
        'func main(): void {',
        '    var seed = 2;',
        '    seed + 3;',
        '}',
        '',
    ].join('\n');
    const multilineText = [
        'func main(): int {',
        '    return 1 +',
        '        2;',
        '}',
        '',
    ].join('\n');
    const returnNextLineText = [
        'func main(): int {',
        '    return',
        '        1 + 2;',
        '}',
        '',
    ].join('\n');
    const multilineInitializerText = [
        'func main(): void {',
        '    var sum =',
        '        1 + 2;',
        '}',
        '',
    ].join('\n');
    const unaryExpressionText = [
        'func main(): void {',
        '    !true;',
        '    -42;',
        '}',
        '',
    ].join('\n');
    const callMemberExpressionText = [
        'func pick(value: int): int {',
        '    return value;',
        '}',
        'func main(): void {',
        '    var seed = {value: 2};',
        '    pick(42);',
        '    seed.value;',
        '}',
        '',
    ].join('\n');
    const computedMemberExpressionText = [
        'func main(): void {',
        '    var index = 0;',
        '    var seed = {value: 2};',
        '    seed[index];',
        '}',
        '',
    ].join('\n');
    const aggregateExpressionText = [
        'func main(): void {',
        '    [1 + 2];',
        '    [true || false];',
        '}',
        '',
    ].join('\n');
    const objectAggregateExpressionText = [
        'func main(): void {',
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
        'func main(): void {',
        '    1 +',
        '        2;',
        '}',
        '',
    ].join('\n');
    const blockCommentInlineValueText = [
        'func main(): void {',
        '    /*',
        '    var ghost = 1;',
        '    */',
        '    /* var inlineGhost = 2; */',
        '}',
        '/* var topGhost = 3; */',
        '',
    ].join('\n');
    const stringInlineValueText = [
        'func main(): void {',
        '    "var stringGhost = 4;";',
        "    'var singleGhost = 5;';",
        '    `var templateGhost = 6;`;',
        '}',
        '',
    ].join('\n');
    const client = new LspClient(serverPath);

    try {
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

        await client.request('shutdown', {});
        client.notify('exit', {});
        await client.waitForExit();
    } catch (error) {
        client.child.kill();
        throw error;
    }
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
