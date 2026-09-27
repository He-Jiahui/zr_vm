const { spawn } = require('child_process');

// 协议用例默认快速失败；需要等待语义分析的调用点必须显式给出更长时限。
const DEFAULT_TIMEOUT_MS = 1000;

/** 让客户端内部契约失败进入调用方测试的异常路径。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** 把 JSON-RPC 对象编码成 LSP 字节帧，供正常请求与协议一致性用例复用。 */
function encodeFrame(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

/**
 * 每个测试实例独占一个 stdio 服务进程，并把响应、通知和其他消息分别排队。
 * 等待者须在进程关闭前消费；关闭事件会拒绝已登记的等待者并附上 stderr。
 */
class StdioProtocolClient {
    /** 创建后立即启动服务端；调用方可按协议收尾，也可在 finally 中调用 terminate 回收进程。 */
    constructor(serverPath) {
        this.buffer = Buffer.alloc(0);
        this.closed = false;
        this.exitCode = null;
        this.exitSignal = null;
        this.nextId = 1;
        this.messageBacklog = [];
        this.messageWaiters = [];
        this.pendingResponses = new Map();
        this.notificationBacklog = new Map();
        this.pendingNotifications = new Map();
        this.stderrChunks = [];
        // BUG: serverPath 不存在或不可执行时 spawn 发出未监听的 error 事件，Node 进程直接崩溃；
        // 用缺失可执行文件构造客户端可复现 ENOENT，而调用方无法在 await/terminate 中接住该失败。
        this.child = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });
        // TODO: stdin 写入错误目前被吞掉；需确认 close 是否总能及时把原因交给挂起的协议断言。
        this.child.stdin.on('error', () => {});
        this.child.stdout.on('data', (chunk) => this.onStdout(chunk));
        this.child.stderr.on('data', (chunk) => {
            this.stderrChunks.push(chunk.toString('utf8'));
        });
        this.child.on('exit', (code, signal) => {
            this.exitCode = code;
            this.exitSignal = signal;
        });
        this.child.on('close', (code, signal) => this.onClose(code, signal));
    }

    /** 让超时与子进程退出断言带上累积的服务端诊断。 */
    stderr() {
        return this.stderrChunks.join('');
    }

    /** 把进程最终状态传播给所有已挂起的协议断言，避免测试一直等待超时。 */
    onClose(code, signal) {
        this.closed = true;
        if (this.exitCode === null) {
            this.exitCode = code;
        }
        if (this.exitSignal === null) {
            this.exitSignal = signal;
        }
        this.failWaiters(new Error(
            `server closed: exitCode=${this.exitCode} signal=${this.exitSignal} stderr=${this.stderr()}`));
    }

    /** 只终止当前等待者；已缓存的消息仍保留，供失败后的诊断读取。 */
    failWaiters(error) {
        for (const pending of this.pendingResponses.values()) {
            clearTimeout(pending.timer);
            pending.reject(error);
        }
        this.pendingResponses.clear();
        for (const waiter of this.messageWaiters) {
            clearTimeout(waiter.timer);
            waiter.reject(error);
        }
        this.messageWaiters = [];
        for (const waiters of this.pendingNotifications.values()) {
            for (const waiter of waiters) {
                clearTimeout(waiter.timer);
                waiter.reject(error);
            }
        }
        this.pendingNotifications.clear();
    }

    /**
     * 按字节增量组装 LSP 帧；stdout 的 chunk 边界不能作为消息边界，
     * Content-Length 也必须在 UTF-8 解码前用于截取消息体。
     */
    onStdout(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        for (;;) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd < 0) {
                return;
            }
            const header = this.buffer.subarray(0, headerEnd).toString('ascii');
            const match = /^Content-Length:\s*(\d+)\s*$/im.exec(header);
            if (match === null) {
                this.failWaiters(new Error(`invalid LSP response header: ${header}`));
                return;
            }
            const contentLength = Number(match[1]);
            const messageStart = headerEnd + 4;
            const messageEnd = messageStart + contentLength;
            if (this.buffer.length < messageEnd) {
                return;
            }
            const payload = this.buffer.subarray(messageStart, messageEnd).toString('utf8');
            this.buffer = this.buffer.subarray(messageEnd);
            try {
                this.dispatch(JSON.parse(payload));
            } catch (error) {
                this.failWaiters(new Error(`invalid LSP response JSON: ${error.message}`));
                return;
            }
        }
    }

    /** 优先按请求 ID 交付响应，再按方法交付通知；其余消息留给信封测试读取。 */
    dispatch(message) {
        if (message && typeof message === 'object' &&
            Object.prototype.hasOwnProperty.call(message, 'id') &&
            (Object.prototype.hasOwnProperty.call(message, 'result') ||
             Object.prototype.hasOwnProperty.call(message, 'error'))) {
            const pending = this.pendingResponses.get(message.id);
            if (pending !== undefined) {
                clearTimeout(pending.timer);
                this.pendingResponses.delete(message.id);
                pending.resolve(message);
                return;
            }
        }
        if (message && typeof message === 'object' && typeof message.method === 'string') {
            const waiters = this.pendingNotifications.get(message.method);
            if (waiters && waiters.length > 0) {
                const waiter = waiters.shift();
                if (waiters.length === 0) {
                    this.pendingNotifications.delete(message.method);
                }
                clearTimeout(waiter.timer);
                waiter.resolve(message.params);
                return;
            }
            const backlog = this.notificationBacklog.get(message.method) || [];
            backlog.push(message.params);
            this.notificationBacklog.set(message.method, backlog);
            return;
        }
        this.enqueueMessage(message);
    }

    /** 未归入待处理请求或通知的消息按到达顺序交给 nextMessage。 */
    enqueueMessage(message) {
        const waiter = this.messageWaiters.shift();
        if (waiter !== undefined) {
            clearTimeout(waiter.timer);
            waiter.resolve(message);
            return;
        }
        this.messageBacklog.push(message);
    }

    /** 请求与通知共用的正常 JSON-RPC 发送入口；发送前要求进程尚未进入 close 终态。 */
    sendPayload(payload) {
        assert(!this.closed, 'cannot send a JSON-RPC message after server exit');
        this.child.stdin.write(encodeFrame(payload));
    }

    /** 保留畸形字节序列，供帧解析负例绕开 JSON.stringify 与 encodeFrame。 */
    sendRawFrame(frame) {
        assert(!this.closed, 'cannot send a raw frame after server exit');
        this.child.stdin.write(frame);
    }

    /** 先登记有类型的请求 ID 再发送，防止快速响应早于等待者注册。 */
    requestEnvelope(payload, timeoutMs = DEFAULT_TIMEOUT_MS) {
        assert(payload !== null && typeof payload === 'object' &&
               Object.prototype.hasOwnProperty.call(payload, 'id'),
               'request payload must have an id');
        assert(!this.pendingResponses.has(payload.id),
               `request id is already pending: ${String(payload.id)}`);
        // BUG: 发送时若因进程已关闭而同步抛错，已登记的 id 和定时器仍保留到超时，重试会误判重复 id。
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendingResponses.delete(payload.id);
                reject(new Error(
                    `timed out waiting for response id=${String(payload.id)} stderr=${this.stderr()}`));
            }, timeoutMs);
            this.pendingResponses.set(payload.id, { resolve, reject, timer });
            this.sendPayload(payload);
        });
    }

    /** 构造标准 JSON-RPC 请求；id 由用例提供，以便断言响应原样保留其类型。 */
    request(method, params, id, timeoutMs = DEFAULT_TIMEOUT_MS) {
        const payload = { jsonrpc: '2.0', id, method };
        if (params !== undefined) {
            payload.params = params;
        }
        return this.requestEnvelope(payload, timeoutMs);
    }

    /** 为普通语义用例分配 ID，并把 JSON-RPC error 转成拒绝的 Promise。 */
    requestWithId(method, params, timeoutMs = DEFAULT_TIMEOUT_MS) {
        if (this.closed) {
            return {
                id: null,
                promise: Promise.reject(new Error('server already exited')),
            };
        }
        const id = this.nextId++;
        const promise = this.request(method, params, id, timeoutMs).then((response) => {
            if (response.error) {
                throw new Error(JSON.stringify(response.error));
            }
            return response.result;
        });
        return { id, promise };
    }

    /** 控制和文档变更通知共用此入口；通知本身不得建立响应等待者。 */
    notify(method, params) {
        this.sendPayload({ jsonrpc: '2.0', method, params });
    }

    /**
     * 先消耗早到的通知，再登记同一方法的 FIFO 等待者。
     * TODO: 无缓存通知且 close 已发生时，新等待者只会等到定时器超时；需核对是否应立即失败。
     */
    waitForNotification(method, timeoutMs = DEFAULT_TIMEOUT_MS) {
        const backlog = this.notificationBacklog.get(method);
        if (backlog && backlog.length > 0) {
            const params = backlog.shift();
            if (backlog.length === 0) {
                this.notificationBacklog.delete(method);
            }
            return Promise.resolve(params);
        }
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                const waiters = this.pendingNotifications.get(method);
                if (waiters) {
                    const index = waiters.findIndex((waiter) => waiter.timer === timer);
                    if (index >= 0) {
                        waiters.splice(index, 1);
                    }
                    if (waiters.length === 0) {
                        this.pendingNotifications.delete(method);
                    }
                }
                reject(new Error(
                    `timed out waiting for notification ${method} stderr=${this.stderr()}`));
            }, timeoutMs);
            const waiters = this.pendingNotifications.get(method) || [];
            waiters.push({ resolve, reject, timer });
            this.pendingNotifications.set(method, waiters);
        });
    }

    /** 协议信封测试读取未被请求或通知通道认领的下一条服务端消息。 */
    nextMessage(timeoutMs = DEFAULT_TIMEOUT_MS) {
        if (this.messageBacklog.length > 0) {
            return Promise.resolve(this.messageBacklog.shift());
        }
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                const index = this.messageWaiters.findIndex((waiter) => waiter.timer === timer);
                if (index >= 0) {
                    this.messageWaiters.splice(index, 1);
                }
                reject(new Error(`timed out waiting for an LSP message stderr=${this.stderr()}`));
            }, timeoutMs);
            this.messageWaiters.push({ resolve, reject, timer });
        });
    }

    /** 负例用例确认指定窗口内服务端没有额外响应，超时是此断言的成功条件。 */
    async expectNoMessage(timeoutMs = DEFAULT_TIMEOUT_MS) {
        if (this.messageBacklog.length > 0) {
            throw new Error(`unexpected LSP message: ${JSON.stringify(this.messageBacklog.shift())}`);
        }
        try {
            const message = await this.nextMessage(timeoutMs);
            throw new Error(`unexpected LSP message: ${JSON.stringify(message)}`);
        } catch (error) {
            if (error.message.startsWith('timed out waiting for an LSP message')) {
                return;
            }
            throw error;
        }
    }

    /** 测试结束时关闭请求流，让服务端有机会按协议自行退出。 */
    endInput() {
        if (!this.closed) {
            this.child.stdin.end();
        }
    }

    /** 等待 close 而非仅等待 exit，以保证 stdout/stderr 已收尾。 */
    waitForExit(timeoutMs = DEFAULT_TIMEOUT_MS) {
        if (this.closed) {
            return Promise.resolve(this.exitCode);
        }
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                reject(new Error(`timed out waiting for server exit stderr=${this.stderr()}`));
            }, timeoutMs);
            this.child.once('close', (code) => {
                clearTimeout(timer);
                resolve(code);
            });
        });
    }

    /** 清理本实例的服务进程并等待 close；调用方可在 finally 中统一执行此回收。 */
    async terminate() {
        if (this.closed) {
            return this.exitCode;
        }
        this.child.kill();
        return this.waitForExit();
    }
}

/** 只导出共享协议客户端及正常帧编码器；畸形帧仍由测试直接调用 sendRawFrame。 */
module.exports = {
    StdioProtocolClient,
    encodeFrame,
};
