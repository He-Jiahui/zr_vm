const { spawn } = require('child_process');

/** 让协议断言失败以异常形式抵达 CTest 的退出路径。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** 按 UTF-8 字节数封装 JSON-RPC，避免非 ASCII 测试内容扭曲 Content-Length。 */
function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

/** 独占 stdio 服务进程，只把带 id 的响应交给本测试的待处理请求。 */
class LspClient {
    /** 创建时启动服务端；stdout 可分段到达，stderr 留给失败诊断。 */
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

        /** 把帧解码失败交给所有挂起请求，免得悬停测试只报告超时。 */
        this.child.stdout.on('data', (chunk) => {
            try {
                this.consume(chunk);
            } catch (error) {
                this.failPending(error);
            }
        });
        /** 收集服务端诊断供超时和退出断言使用。 */
        this.child.stderr.on('data', (chunk) => {
            this.stderrChunks.push(chunk.toString('utf8'));
        });
        /** 服务端提前结束时撤销所有请求的等待者。 */
        this.child.on('close', (code) => {
            this.closed = true;
            this.exitCode = code;
            this.failPending(new Error(
                `language server closed with ${code}: ${this.stderr()}`));
        });
    }

    /** 汇总服务端 stderr，不参与 LSP stdout 帧解析。 */
    stderr() {
        return this.stderrChunks.join('');
    }

    /** 帧解析失败和进程关闭共用此出口，定时器必须随等待者一起撤销。 */
    failPending(error) {
        for (const { reject, timer } of this.pending.values()) {
            clearTimeout(timer);
            reject(error);
        }
        this.pending.clear();
    }

    /** 以字节缓存重组响应帧；通知不用于此用例的断言。 */
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

    /** 绑定请求 id 与超时，供 initialize、hover、shutdown 串行使用。 */
    request(method, params, timeoutMs = 10000) {
        assert(!this.closed, 'language server closed before request');
        const id = this.nextId++;

        return new Promise((resolve, reject) => {
            /** 无响应时使 CTest 明确失败，并包含服务端诊断。 */
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

    /** 发送无需响应的生命周期或文档通知。 */
    notify(method, params) {
        assert(!this.closed, 'language server closed before notification');
        this.child.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

    /** shutdown 后等待服务端 close，以检查协议收尾和退出码。 */
    waitForExit(timeoutMs = 10000) {
        if (this.closed) {
            return Promise.resolve(this.exitCode);
        }

        return new Promise((resolve, reject) => {
            /** 服务端不退出时防止测试无限等待。 */
            const timer = setTimeout(() => {
                reject(new Error(`Timed out waiting for language server exit: ${this.stderr()}`));
            }, timeoutMs);
            /** 只有 close 后 stdout/stderr 的诊断才已完整。 */
            this.child.once('close', (code) => {
                clearTimeout(timer);
                resolve(code);
            });
        });
    }
}

/** 把 JS 字符索引换成协商后 LSP 使用的 UTF-8 列，供请求和期望范围复用。 */
function utf8ColumnForIndex(text, index) {
    return Buffer.byteLength(text.slice(0, index), 'utf8');
}

/** 从 CTest 指定的 stdio 可执行文件端到端检查编码协商和 hover 范围。 */
async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server executable path');

    const documentUri = 'file:///zr-position-encoding-smoke.zr';
    // 非 ASCII 前缀保证 UTF-8 字节列与 UTF-16 字符列不同。
    const documentText = '/* \u03bb */ var system = import("zr.system");\n';
    const importLiteralIndex = documentText.indexOf('"zr.system"');
    const hoverIndex = documentText.indexOf('zr.system') + 1;
    const expectedRangeStart = utf8ColumnForIndex(documentText, importLiteralIndex);
    const expectedRangeEnd = expectedRangeStart + Buffer.byteLength('"zr.system"', 'utf8');
    const client = new LspClient(serverPath);

    /** 先协商 UTF-8，再用同一文档位置验证请求和返回范围均按该编码解释。 */
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

/** 将任一协议或范围失败反馈给 CTest。 */
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
