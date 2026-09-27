import * as net from 'node:net';
import { EventEmitter } from 'node:events';
import { ZRDBG_PROTOCOL } from './constants';
import type { ZrDbgEventMessage, ZrDbgResponseMessage } from './types';

/** 关联 zrdbg/1 请求编号与等待中的 DAP 转译；断线时必须统一拒绝，避免调试界面永久等待。 */
type PendingRequest = {
    resolve: (value: Record<string, unknown>) => void;
    reject: (error: Error) => void;
};

/** 只允许本机 TCP 调试端点；配置层与连接层共用此解析结果。 */
export type ParsedEndpoint = {
    host: string;
    port: number;
};

/** 限制 attach/launch 到回环地址，防止扩展误连远端调试协议服务。 */
export function isLoopbackHost(host: string): boolean {
    const normalized = host.trim().toLowerCase();
    return normalized === '127.0.0.1' ||
        normalized === 'localhost' ||
        normalized === '::1' ||
        normalized === '[::1]';
}

/** 将配置或 CLI 输出的 endpoint 验证为可连接的本机 host/port，连接前调用。 */
export function parseEndpoint(text: string): ParsedEndpoint {
    const trimmed = text.trim();
    if (trimmed.length === 0) {
        throw new Error('Missing zrdbg endpoint.');
    }

    if (trimmed.startsWith('[')) {
        // BUG: 只查到某个 ] 和其后的最后一个 :，中间任意字符也通过；
        // parseEndpoint('[::1]junk:9000') 返回 ::1:9000，错误配置被静默改写。
        const bracketEnd = trimmed.indexOf(']');
        const portSeparator = trimmed.lastIndexOf(':');
        if (bracketEnd < 0 || portSeparator <= bracketEnd) {
            throw new Error(`Invalid zrdbg endpoint: ${text}`);
        }

        return parseEndpointParts(trimmed.slice(1, bracketEnd), trimmed.slice(portSeparator + 1));
    }

    const portSeparator = trimmed.lastIndexOf(':');
    if (portSeparator <= 0) {
        throw new Error(`Invalid zrdbg endpoint: ${text}`);
    }

    return parseEndpointParts(trimmed.slice(0, portSeparator), trimmed.slice(portSeparator + 1));
}

/** 两种 endpoint 表示最终都经此处执行端口范围与回环约束。 */
function parseEndpointParts(host: string, portText: string): ParsedEndpoint {
    // BUG: parseInt 接受“9000junk”尾随文本；方括号分支也接受“[::1]junk:9000”。
    // 两种错误配置都通过校验并连接 9000；已用当前 out 模块复现。
    const port = Number.parseInt(portText, 10);
    if (!Number.isInteger(port) || port <= 0 || port > 65535) {
        throw new Error(`Invalid zrdbg port: ${portText}`);
    }
    if (!isLoopbackHost(host)) {
        throw new Error(`ZR debugger only supports loopback endpoints, got ${host}`);
    }
    // BUG: isLoopbackHost 用 trim 后的值校验，却把原 host 返回给 net.createConnection；
    // 'localhost :9000' 校验通过，连接阶段以带空格的主机名解析而失败。

    return {
        host,
        port,
    };
}

/** DAP 适配器到 zrdbg/1 的单连接传输；按请求 id 匹配回复并将异步事件转发给会话。 */
export class ZrDbgClient {
    /** 对 DAP 桥广播 zrdbg 事件与连接关闭，不暴露原始 socket。 */
    private readonly emitter = new EventEmitter();
    /** 请求 id 对应等待中的 DAP 转译；调用方不得绕过 request 直接写 socket。 */
    private readonly pendingRequests = new Map<number, PendingRequest>();
    private socket: net.Socket | undefined;
    /** zrdbg/1 内部请求编号，只用于 response 配对，不等于 DAP 消息序号。 */
    private nextId = 1;
    /** TCP 可拆分或合并帧；保留尚未形成完整长度前缀帧的字节。 */
    private readBuffer = Buffer.alloc(0);
    private closed = false;

    /** connectRuntime 注册 runtime 事件桥接；事件不与 DAP 请求队列串行。 */
    onEvent(listener: (message: ZrDbgEventMessage) => void): void {
        this.emitter.on('event', listener);
    }

    /** connectRuntime 注册断线通知，用于向 VS Code 发 terminated 事件。 */
    onClose(listener: (error?: Error) => void): void {
        this.emitter.on('close', listener);
    }

    /** 启动 attach/launch 的本机 TCP 连接；超时只覆盖握手建连，不覆盖之后的 RPC。 */
    async connect(endpointText: string, timeoutMs = 5000): Promise<void> {
        const endpoint = parseEndpoint(endpointText);

        await new Promise<void>((resolve, reject) => {
            const socket = net.createConnection({
                host: endpoint.host,
                port: endpoint.port,
            });
            const timeoutHandle = setTimeout(() => {
                socket.destroy(new Error(`Timed out connecting to ${endpointText}`));
            }, timeoutMs);

            const cleanup = () => {
                clearTimeout(timeoutHandle);
                socket.removeListener('connect', onConnect);
                socket.removeListener('error', onError);
            };
            const onConnect = () => {
                // 建连阶段结束后由 attachSocket 接管运行期错误和分片消息。
                cleanup();
                this.attachSocket(socket);
                resolve();
            };
            const onError = (error: Error) => {
                // 尚未接管 socket 的错误只拒绝 connect，不发布运行期 close 事件。
                cleanup();
                reject(error);
            };

            socket.once('connect', onConnect);
            socket.once('error', onError);
        });
    }

    /** 首个 RPC 验证 zrdbg/1 协议并传递可选令牌；其成功是 DAP launch/attach 的前提。 */
    async initialize(authToken?: string): Promise<Record<string, unknown>> {
        const result = await this.request('initialize', authToken ? { authToken } : {});
        const protocol = typeof result.protocol === 'string' ? result.protocol : '';
        if (protocol !== ZRDBG_PROTOCOL) {
            throw new Error(`Unexpected zrdbg protocol '${protocol || '<missing>'}'`);
        }

        return result;
    }

    /** 供 DAP handler 与断点重放共用的 RPC；仅在已连接且未关闭时调用，当前无回复超时。 */
    async request(method: string, params: Record<string, unknown> = {}): Promise<Record<string, unknown>> {
        const socket = this.socket;
        if (!socket || this.closed) {
            throw new Error('ZR debugger socket is not connected.');
        }

        const id = this.nextId++;
        const payload = JSON.stringify({
            jsonrpc: '2.0',
            id,
            method,
            params,
        });
        // zrdbg/1 在 JSON 前放四字节大端长度；runtime 测试与 CLI 端共用该帧约定。
        const frame = Buffer.allocUnsafe(4 + Buffer.byteLength(payload));
        frame.writeUInt32BE(Buffer.byteLength(payload), 0);
        frame.write(payload, 4, 'utf8');

        await new Promise<void>((resolve, reject) => {
            socket.write(frame, (error) => {
                if (error) {
                    reject(error);
                    return;
                }
                resolve();
            });
        });

        // TODO: pending id 在 write 回调后才登记；可控 socket 先发 data 可复现回复丢失，
        // 需再核实真实 net.Socket 是否允许同样的事件顺序。
        return await new Promise<Record<string, unknown>>((resolve, reject) => {
            this.pendingRequests.set(id, { resolve, reject });
        });
    }

    /** 会话主动退出时关闭本地资源并结束未决 RPC；不再向已终止的 DAP 会话转发 close。 */
    close(): void {
        if (this.closed) {
            return;
        }

        this.closed = true;
        if (this.socket) {
            this.socket.destroy();
            this.socket = undefined;
        }
        this.failPendingRequests(new Error('ZR debugger connection closed.'));
    }

    /** TCP 建连成功后才接管 data/error/close，保持连接失败与运行期断线的语义分离。 */
    private attachSocket(socket: net.Socket): void {
        this.socket = socket;
        socket.on('data', (chunk) => {
            // 一个 TCP data 可含半帧或多帧；只有 drainFrames 决定何时向上游交付。
            this.readBuffer = Buffer.concat([this.readBuffer, chunk]);
            this.drainFrames();
        });
        socket.on('error', (error) => {
            // 运行期错误与正常 close 汇合到一次终止通知，拒绝所有未完成 RPC。
            this.handleClose(error);
        });
        socket.on('close', () => {
            this.handleClose();
        });
    }

    /** 消化任意分片/粘包的 zrdbg/1 帧，将完整 JSON 交给响应或事件分派。 */
    private drainFrames(): void {
        // TODO: 长度字段没有上限；需按 runtime 帧限制核对后加资源边界，避免异常本机对端持续占用内存。
        while (this.readBuffer.length >= 4) {
            const frameLength = this.readBuffer.readUInt32BE(0);
            if (this.readBuffer.length < frameLength + 4) {
                return;
            }

            const frameText = this.readBuffer.toString('utf8', 4, frameLength + 4);
            this.readBuffer = this.readBuffer.subarray(frameLength + 4);
            this.handleFrame(frameText);
        }
    }

    /** 响应按 id 唤醒等待者，未配对的 method 则交给 DAP 会话处理。 */
    private handleFrame(frameText: string): void {
        let message: ZrDbgEventMessage | ZrDbgResponseMessage;

        try {
            message = JSON.parse(frameText);
        } catch (error) {
            // BUG: 这里仅标记关闭却不 destroy 原 socket；同一 TCP chunk 后续帧仍可进入事件回调，
            // 调试会话收到 terminated 后仍可能处理残余消息，且持有连接资源。
            this.handleClose(error instanceof Error ? error : new Error(String(error)));
            return;
        }

        // BUG: JSON 字面值 null 是合法 JSON 帧，解析后访问 .id 抛 TypeError；
        // data 回调未捕获，连接到发送该帧的本机端点会造成扩展宿主未捕获异常。
        if (typeof (message as ZrDbgResponseMessage).id === 'number') {
            const response = message as ZrDbgResponseMessage;
            const pending = this.pendingRequests.get(response.id);
            if (!pending) {
                return;
            }

            this.pendingRequests.delete(response.id);
            if (response.error) {
                pending.reject(new Error(response.error.message));
                return;
            }

            pending.resolve(response.result ?? {});
            return;
        }

        if (typeof (message as ZrDbgEventMessage).method === 'string') {
            this.emitter.emit('event', message as ZrDbgEventMessage);
        }
    }

    /** 运行期断线统一拒绝 RPC 并只向 DAP 会话报告一次关闭事件。 */
    private handleClose(error?: Error): void {
        if (this.closed) {
            return;
        }

        this.closed = true;
        this.socket = undefined;
        this.failPendingRequests(error ?? new Error('ZR debugger connection closed.'));
        this.emitter.emit('close', error);
    }

    /** close/handleClose 共享的失败出口；调试控制命令不得在断线后悬挂。 */
    private failPendingRequests(error: Error): void {
        for (const pending of this.pendingRequests.values()) {
            pending.reject(error);
        }
        this.pendingRequests.clear();
    }
}
