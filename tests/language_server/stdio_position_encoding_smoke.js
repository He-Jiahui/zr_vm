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
        this.buffer = Buffer.alloc(0);
        this.nextId = 1;
        this.pending = new Map();
        this.stderrChunks = [];
        this.closed = false;
        this.exitCode = null;
        this.child = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });

        this.child.stdout.on('data', (chunk) => {
            try {
                this.consume(chunk);
            } catch (error) {
                this.failPending(error);
            }
        });
        this.child.stderr.on('data', (chunk) => {
            this.stderrChunks.push(chunk.toString('utf8'));
        });
        this.child.on('close', (code) => {
            this.closed = true;
            this.exitCode = code;
            this.failPending(new Error(
                `language server closed with ${code}: ${this.stderr()}`));
        });
    }

    stderr() {
        return this.stderrChunks.join('');
    }

    failPending(error) {
        for (const { reject, timer } of this.pending.values()) {
            clearTimeout(timer);
            reject(error);
        }
        this.pending.clear();
    }

    consume(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);

        while (true) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd < 0) {
                return;
            }

            const header = this.buffer.subarray(0, headerEnd).toString('ascii');
            const match = /Content-Length:\s*(\d+)/i.exec(header);
            assert(match, `Missing Content-Length header: ${header}`);
            const bodyStart = headerEnd + 4;
            const bodyEnd = bodyStart + Number(match[1]);
            if (this.buffer.length < bodyEnd) {
                return;
            }

            const message = JSON.parse(this.buffer.subarray(bodyStart, bodyEnd).toString('utf8'));
            this.buffer = this.buffer.subarray(bodyEnd);
            if (!Object.prototype.hasOwnProperty.call(message, 'id')) {
                continue;
            }

            const pending = this.pending.get(message.id);
            if (!pending) {
                continue;
            }
            this.pending.delete(message.id);
            clearTimeout(pending.timer);
            if (message.error) {
                pending.reject(new Error(JSON.stringify(message.error)));
            } else {
                pending.resolve(message.result);
            }
        }
    }

    request(method, params, timeoutMs = 10000) {
        assert(!this.closed, 'language server closed before request');
        const id = this.nextId++;

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pending.delete(id);
                reject(new Error(`Timed out waiting for ${method}: ${this.stderr()}`));
            }, timeoutMs);
            this.pending.set(id, { resolve, reject, timer });
            this.child.stdin.write(createMessage({
                jsonrpc: '2.0',
                id,
                method,
                params,
            }));
        });
    }

    notify(method, params) {
        assert(!this.closed, 'language server closed before notification');
        this.child.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

    waitForExit(timeoutMs = 10000) {
        if (this.closed) {
            return Promise.resolve(this.exitCode);
        }

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                reject(new Error(`Timed out waiting for language server exit: ${this.stderr()}`));
            }, timeoutMs);
            this.child.once('close', (code) => {
                clearTimeout(timer);
                resolve(code);
            });
        });
    }
}

function utf8ColumnForIndex(text, index) {
    return Buffer.byteLength(text.slice(0, index), 'utf8');
}

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server executable path');

    const documentUri = 'file:///zr-position-encoding-smoke.zr';
    const documentText = '/* \u03bb */ var system = import("zr.system");\n';
    const importLiteralIndex = documentText.indexOf('"zr.system"');
    const hoverIndex = documentText.indexOf('zr.system') + 1;
    const expectedRangeStart = utf8ColumnForIndex(documentText, importLiteralIndex);
    const expectedRangeEnd = expectedRangeStart + Buffer.byteLength('"zr.system"', 'utf8');
    const client = new LspClient(serverPath);

    const initialize = await client.request('initialize', {
        capabilities: {
            general: {
                positionEncodings: ['utf-8', 'utf-16'],
            },
        },
    });
    assert(initialize && initialize.capabilities,
        'initialize response missing capabilities');
    assert(initialize.capabilities.positionEncoding === 'utf-8',
        `server must negotiate utf-8 positionEncoding, got ${JSON.stringify(
            initialize.capabilities.positionEncoding)}`);

    client.notify('initialized', {});
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: documentUri,
            languageId: 'zr',
            version: 1,
            text: documentText,
        },
    });
    const hover = await client.request('textDocument/hover', {
        textDocument: { uri: documentUri },
        position: {
            line: 0,
            character: utf8ColumnForIndex(documentText, hoverIndex),
        },
    });
    assert(hover && hover.range,
        `hover response missing range: ${JSON.stringify(hover)}`);
    assert(hover.range.start.line === 0 && hover.range.end.line === 0,
        `hover range should stay on line 0: ${JSON.stringify(hover.range)}`);
    assert(hover.range.start.character === expectedRangeStart,
        `hover range start must be UTF-8 byte column ${expectedRangeStart}, got ${
            hover.range.start.character}`);
    assert(hover.range.end.character === expectedRangeEnd,
        `hover range end must be UTF-8 byte column ${expectedRangeEnd}, got ${
            hover.range.end.character}`);

    const shutdown = await client.request('shutdown', {});
    assert(shutdown === null, 'shutdown must return null');
    client.notify('exit', {});
    const exitCode = await client.waitForExit();
    assert(exitCode === 0,
        `Expected stdio server to exit cleanly, got status=${exitCode} stderr=${client.stderr()}`);
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
