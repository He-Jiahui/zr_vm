import * as net from 'node:net';
import { EventEmitter } from 'node:events';
import { ZRDBG_PROTOCOL } from './constants';
import type { ZrDbgEventMessage, ZrDbgResponseMessage } from './types';

type PendingRequest = {
    resolve: (value: Record<string, unknown>) => void;
    reject: (error: Error) => void;
};

export type ParsedEndpoint = {
    host: string;
    port: number;
};

export function isLoopbackHost(host: string): boolean {
    const normalized = host.trim().toLowerCase();
    return normalized === '127.0.0.1' ||
        normalized === 'localhost' ||
        normalized === '::1' ||
        normalized === '[::1]';
}

export function parseEndpoint(text: string): ParsedEndpoint {
    const trimmed = text.trim();
    if (trimmed.length === 0) {
        throw new Error('Missing zrdbg endpoint.');
    }

    if (trimmed.startsWith('[')) {
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

function parseEndpointParts(host: string, portText: string): ParsedEndpoint {
    const port = Number.parseInt(portText, 10);
    if (!Number.isInteger(port) || port <= 0 || port > 65535) {
        throw new Error(`Invalid zrdbg port: ${portText}`);
    }
    if (!isLoopbackHost(host)) {
        throw new Error(`ZR debugger only supports loopback endpoints, got ${host}`);
    }

    return {
        host,
        port,
    };
}

export class ZrDbgClient {
    private readonly emitter = new EventEmitter();
    private readonly pendingRequests = new Map<number, PendingRequest>();
    private socket: net.Socket | undefined;
    private nextId = 1;
    private readBuffer = Buffer.alloc(0);
    private closed = false;

    onEvent(listener: (message: ZrDbgEventMessage) => void): void {
        this.emitter.on('event', listener);
    }

    onClose(listener: (error?: Error) => void): void {
        this.emitter.on('close', listener);
    }

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
                cleanup();
                this.attachSocket(socket);
                resolve();
            };
            const onError = (error: Error) => {
                cleanup();
                reject(error);
            };

            socket.once('connect', onConnect);
            socket.once('error', onError);
        });
    }

    async initialize(authToken?: string): Promise<Record<string, unknown>> {
        const result = await this.request('initialize', authToken ? { authToken } : {});
        const protocol = typeof result.protocol === 'string' ? result.protocol : '';
        if (protocol !== ZRDBG_PROTOCOL) {
            throw new Error(`Unexpected zrdbg protocol '${protocol || '<missing>'}'`);
        }

        return result;
    }

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

        return await new Promise<Record<string, unknown>>((resolve, reject) => {
            this.pendingRequests.set(id, { resolve, reject });
        });
    }

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

    private attachSocket(socket: net.Socket): void {
        this.socket = socket;
        socket.on('data', (chunk) => {
            this.readBuffer = Buffer.concat([this.readBuffer, chunk]);
            this.drainFrames();
        });
        socket.on('error', (error) => {
            this.handleClose(error);
        });
        socket.on('close', () => {
            this.handleClose();
        });
    }

    private drainFrames(): void {
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

    private handleFrame(frameText: string): void {
        let message: ZrDbgEventMessage | ZrDbgResponseMessage;

        try {
            message = JSON.parse(frameText);
        } catch (error) {
            this.handleClose(error instanceof Error ? error : new Error(String(error)));
            return;
        }

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

    private handleClose(error?: Error): void {
        if (this.closed) {
            return;
        }

        this.closed = true;
        this.socket = undefined;
        this.failPendingRequests(error ?? new Error('ZR debugger connection closed.'));
        this.emitter.emit('close', error);
    }

    private failPendingRequests(error: Error): void {
        for (const pending of this.pendingRequests.values()) {
            pending.reject(error);
        }
        this.pendingRequests.clear();
    }
}

