const { spawn } = require('child_process');

const DEFAULT_TIMEOUT_MS = 1000;

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function encodeFrame(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

class StdioProtocolClient {
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
        this.child = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });

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

    stderr() {
        return this.stderrChunks.join('');
    }

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

    enqueueMessage(message) {
        const waiter = this.messageWaiters.shift();
        if (waiter !== undefined) {
            clearTimeout(waiter.timer);
            waiter.resolve(message);
            return;
        }
        this.messageBacklog.push(message);
    }

    sendPayload(payload) {
        assert(!this.closed, 'cannot send a JSON-RPC message after server exit');
        this.child.stdin.write(encodeFrame(payload));
    }

    sendRawFrame(frame) {
        assert(!this.closed, 'cannot send a raw frame after server exit');
        this.child.stdin.write(frame);
    }

    requestEnvelope(payload, timeoutMs = DEFAULT_TIMEOUT_MS) {
        assert(payload !== null && typeof payload === 'object' &&
               Object.prototype.hasOwnProperty.call(payload, 'id'),
               'request payload must have an id');
        assert(!this.pendingResponses.has(payload.id),
               `request id is already pending: ${String(payload.id)}`);

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

    request(method, params, id, timeoutMs = DEFAULT_TIMEOUT_MS) {
        return this.requestEnvelope({ jsonrpc: '2.0', id, method, params }, timeoutMs);
    }

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

    notify(method, params) {
        this.sendPayload({ jsonrpc: '2.0', method, params });
    }

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

    endInput() {
        if (!this.closed) {
            this.child.stdin.end();
        }
    }

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

    async terminate() {
        if (this.closed) {
            return this.exitCode;
        }
        this.child.kill();
        return this.waitForExit();
    }
}

module.exports = {
    StdioProtocolClient,
    encodeFrame,
};
