import * as path from 'node:path';
import * as vscode from 'vscode';
import { ZrCliLauncher } from './cliLauncher';
import {
    ZR_DEBUG_MAIN_THREAD_ID,
    ZR_DEBUG_MAIN_THREAD_NAME,
    ZRDBG_PROTOCOL,
} from './constants';
import type {
    DapEvent,
    DapRequest,
    DapResponse,
    ZrAttachRequestArguments,
    ZrDbgBreakpoint,
    ZrDbgEventMessage,
    ZrDbgFrame,
    ZrDbgScope,
    ZrDbgVariable,
    ZrLaunchRequestArguments,
} from './types';
import { ZrDbgClient } from './zrdbgClient';

type ScopeHandle = {
    scopeId: number;
    stateId: number;
};

export class ZrDebugAdapter implements vscode.DebugAdapter {
    private readonly emitter = new vscode.EventEmitter<vscode.DebugProtocolMessage>();
    private readonly scopeHandles = new Map<number, ScopeHandle>();
    private readonly runtimeSourcePaths = new Map<string, string>();
    private readonly debugConsole = vscode.debug.activeDebugConsole;
    private readonly sessionOutputPrefix: string;
    private client: ZrDbgClient | undefined;
    private launcher: ZrCliLauncher | undefined;
    private seq = 1;
    private stopOnEntry = true;
    private configurationDone = false;
    private initialStopSeen = false;
    private pendingAutoContinue = false;
    private currentStateId = 0;
    private terminated = false;
    private launchMode = false;

    readonly onDidSendMessage = this.emitter.event;

    constructor(private readonly session: vscode.DebugSession) {
        this.sessionOutputPrefix = `[zr:${session.name}] `;
    }

    dispose(): void {
        void this.shutdown(false);
        this.emitter.dispose();
    }

    handleMessage(message: vscode.DebugProtocolMessage): void {
        const request = message as DapRequest;
        if (request.type !== 'request' || typeof request.command !== 'string') {
            return;
        }

        void this.dispatchRequest(request).catch((error) => {
            this.sendErrorResponse(request, error instanceof Error ? error.message : String(error));
        });
    }

    private async dispatchRequest(request: DapRequest): Promise<void> {
        switch (request.command) {
            case 'initialize':
                this.handleInitialize(request);
                return;
            case 'launch':
                await this.handleLaunch(request);
                return;
            case 'attach':
                await this.handleAttach(request);
                return;
            case 'setBreakpoints':
                await this.handleSetBreakpoints(request);
                return;
            case 'configurationDone':
                await this.handleConfigurationDone(request);
                return;
            case 'threads':
                this.sendResponse(request, {
                    threads: [{ id: ZR_DEBUG_MAIN_THREAD_ID, name: ZR_DEBUG_MAIN_THREAD_NAME }],
                });
                return;
            case 'stackTrace':
                await this.handleStackTrace(request);
                return;
            case 'scopes':
                await this.handleScopes(request);
                return;
            case 'variables':
                await this.handleVariables(request);
                return;
            case 'continue':
                await this.handleSimpleControl(request, 'continue', { allThreadsContinued: true });
                return;
            case 'pause':
                await this.handleSimpleControl(request, 'pause');
                return;
            case 'next':
                await this.handleSimpleControl(request, 'next');
                return;
            case 'stepIn':
                await this.handleSimpleControl(request, 'stepIn');
                return;
            case 'stepOut':
                await this.handleSimpleControl(request, 'stepOut');
                return;
            case 'disconnect':
            case 'terminate':
                await this.handleDisconnect(request);
                return;
            default:
                this.sendErrorResponse(request, `Unsupported ZR debug request: ${request.command}`);
        }
    }

    private handleInitialize(request: DapRequest): void {
        this.sendResponse(request, {
            supportsConfigurationDoneRequest: true,
            supportsPauseRequest: true,
        });
    }

    private async handleLaunch(request: DapRequest): Promise<void> {
        const args = request.arguments as unknown as ZrLaunchRequestArguments;
        const authToken = typeof args.authToken === 'string' ? args.authToken : undefined;
        const cliPath = typeof args.cliPath === 'string' ? args.cliPath : '';
        if (!cliPath) {
            throw new Error('ZR launch configuration requires cliPath.');
        }

        this.launchMode = true;
        this.stopOnEntry = args.stopOnEntry !== false;
        this.launcher = new ZrCliLauncher((channel, text) => {
            this.sendOutput(text, channel === 'stderr' ? 'stderr' : 'stdout');
        });
        this.launcher.onExit((code) => {
            this.sendExitedEvent(code ?? 0);
            if (!this.terminated) {
                this.sendTerminatedEvent();
            }
        });

        const launchResult = await this.launcher.launch({
            ...args,
            cliPath,
            executionMode: args.executionMode ?? 'interp',
        });
        await this.connectRuntime(launchResult.endpoint, authToken);
        this.sendResponse(request);
    }

    private async handleAttach(request: DapRequest): Promise<void> {
        const args = request.arguments as unknown as ZrAttachRequestArguments;
        const authToken = typeof args.authToken === 'string' ? args.authToken : undefined;

        this.launchMode = false;
        this.stopOnEntry = true;
        await this.connectRuntime(args.endpoint, authToken);
        this.sendResponse(request);
    }

    private async connectRuntime(endpoint: string, authToken?: string): Promise<void> {
        this.client = new ZrDbgClient();
        this.client.onEvent((message) => {
            void this.handleRuntimeEvent(message);
        });
        this.client.onClose((error) => {
            if (!this.terminated && error) {
                this.sendOutput(`${error.message}\n`, 'stderr');
            }
            if (!this.terminated) {
                this.sendTerminatedEvent();
            }
        });
        await this.client.connect(endpoint);
        const initializeResult = await this.client.initialize(authToken);
        if (initializeResult.protocol !== ZRDBG_PROTOCOL) {
            throw new Error(`Unexpected zrdbg protocol '${String(initializeResult.protocol ?? '')}'`);
        }
    }

    private async handleSetBreakpoints(request: DapRequest): Promise<void> {
        const client = this.requireClient();
        const args = request.arguments ?? {};
        const source = args.source as { path?: string } | undefined;
        const pathText = typeof source?.path === 'string' ? path.normalize(source.path) : '';
        const breakpoints = Array.isArray(args.breakpoints) ? args.breakpoints : [];
        const runtimeSourcePath = this.runtimeSourcePaths.get(canonicalSourcePath(pathText)) ?? pathText;

        if (!pathText) {
            throw new Error('ZR setBreakpoints requires source.path.');
        }

        const result = await client.request('setBreakpoints', {
            sourceFile: runtimeSourcePath,
            lines: breakpoints.map((item) => Number(item.line)).filter((line) => Number.isInteger(line) && line > 0),
        });
        const resolvedBreakpoints = Array.isArray(result.breakpoints) ? result.breakpoints as ZrDbgBreakpoint[] : [];

        this.sendResponse(request, {
            breakpoints: resolvedBreakpoints.map((item, index) => ({
                id: index + 1,
                verified: item.verified !== false,
                line: typeof item.line === 'number' ? item.line : breakpoints[index]?.line,
                source: { path: pathText, name: path.basename(pathText) },
            })),
        });
    }

    private async handleConfigurationDone(request: DapRequest): Promise<void> {
        this.configurationDone = true;
        this.sendResponse(request);

        if (!this.stopOnEntry) {
            if (this.initialStopSeen) {
                await this.continueAfterEntry();
            } else {
                this.pendingAutoContinue = true;
            }
        }
    }

    private async handleStackTrace(request: DapRequest): Promise<void> {
        const result = await this.requireClient().request('stackTrace');
        const frames = Array.isArray(result.frames) ? result.frames as ZrDbgFrame[] : [];
        for (const frame of frames) {
            this.rememberRuntimeSourcePath(frame.sourceFile);
        }

        this.sendResponse(request, {
            stackFrames: frames.map((frame) => ({
                id: frame.frameId,
                name: frame.functionName,
                line: frame.line || 1,
                column: 1,
                source: {
                    name: path.basename(frame.sourceFile),
                    path: frame.sourceFile,
                },
                instructionPointerReference: String(frame.instructionIndex),
            })),
            totalFrames: frames.length,
        });
    }

    private async handleScopes(request: DapRequest): Promise<void> {
        const frameId = Number((request.arguments ?? {}).frameId);
        const result = await this.requireClient().request('scopes', { frameId });
        const scopes = Array.isArray(result.scopes) ? result.scopes as ZrDbgScope[] : [];

        this.scopeHandles.clear();
        for (const scope of scopes) {
            this.scopeHandles.set(scope.scopeId, {
                scopeId: scope.scopeId,
                stateId: this.currentStateId,
            });
        }

        this.sendResponse(request, {
            scopes: scopes.map((scope) => ({
                name: scope.name,
                variablesReference: scope.scopeId,
                expensive: false,
            })),
        });
    }

    private async handleVariables(request: DapRequest): Promise<void> {
        const variablesReference = Number((request.arguments ?? {}).variablesReference);
        const scopeHandle = this.scopeHandles.get(variablesReference);
        if (!scopeHandle || scopeHandle.stateId !== this.currentStateId) {
            throw new Error(`Unknown or stale variablesReference ${variablesReference}.`);
        }

        const result = await this.requireClient().request('variables', { scopeId: scopeHandle.scopeId });
        const variables = Array.isArray(result.variables) ? result.variables as ZrDbgVariable[] : [];

        this.sendResponse(request, {
            variables: variables.map((item) => ({
                name: item.name,
                type: item.type,
                value: item.value,
                variablesReference: 0,
            })),
        });
    }

    private async handleSimpleControl(
        request: DapRequest,
        method: 'continue' | 'pause' | 'next' | 'stepIn' | 'stepOut',
        responseBody?: Record<string, unknown>,
    ): Promise<void> {
        await this.requireClient().request(method);
        this.sendResponse(request, responseBody);
    }

    private async handleDisconnect(request: DapRequest): Promise<void> {
        this.sendResponse(request);
        await this.shutdown(this.launchMode);
    }

    private async shutdown(stopLauncher: boolean): Promise<void> {
        const client = this.client;
        const launcher = this.launcher;

        this.client = undefined;
        this.launcher = undefined;

        if (client) {
            try {
                await client.request('disconnect');
            } catch {
                // The runtime may already be gone. Best-effort shutdown is enough here.
            }
            client.close();
        }

        if (stopLauncher && launcher) {
            await launcher.stop();
        }
    }

    private async handleRuntimeEvent(message: ZrDbgEventMessage): Promise<void> {
        switch (message.method) {
            case 'initialized':
                this.sendEvent('initialized');
                break;
            case 'breakpointResolved':
                this.rememberRuntimeSourcePath(String(message.params?.sourceFile ?? ''));
                this.sendEvent('breakpoint', {
                    reason: 'changed',
                    breakpoint: {
                        verified: Boolean(message.params?.resolved),
                        line: Number(message.params?.line ?? 0) || 1,
                        source: this.toSource(message.params?.sourceFile),
                    },
                });
                break;
            case 'stopped':
                this.rememberRuntimeSourcePath(String(message.params?.sourceFile ?? ''));
                this.currentStateId = Number(message.params?.stateId ?? 0);
                this.initialStopSeen = true;
                if (!this.stopOnEntry && String(message.params?.reason ?? '') === 'entry') {
                    if (this.configurationDone) {
                        await this.continueAfterEntry();
                    } else {
                        this.pendingAutoContinue = true;
                    }
                    break;
                }
                this.sendEvent('stopped', {
                    reason: String(message.params?.reason ?? 'pause'),
                    threadId: ZR_DEBUG_MAIN_THREAD_ID,
                    allThreadsStopped: true,
                    text: typeof message.params?.functionName === 'string' ? message.params.functionName : undefined,
                });
                break;
            case 'continued':
                this.currentStateId = 0;
                this.scopeHandles.clear();
                this.sendEvent('continued', {
                    threadId: ZR_DEBUG_MAIN_THREAD_ID,
                    allThreadsContinued: true,
                });
                break;
            case 'terminated':
                this.sendTerminatedEvent();
                break;
            case 'moduleLoaded':
                this.rememberRuntimeSourcePath(String(message.params?.sourceFile ?? ''));
            default:
                break;
        }
    }

    private async continueAfterEntry(): Promise<void> {
        if (!this.pendingAutoContinue && !this.initialStopSeen) {
            return;
        }

        this.pendingAutoContinue = false;
        await this.requireClient().request('continue');
    }

    private requireClient(): ZrDbgClient {
        if (!this.client) {
            throw new Error('ZR debugger is not connected.');
        }

        return this.client;
    }

    private sendResponse(request: DapRequest, body?: Record<string, unknown>): void {
        const response: DapResponse = {
            seq: this.seq++,
            type: 'response',
            request_seq: request.seq,
            success: true,
            command: request.command,
        };
        if (body) {
            response.body = body;
        }
        this.emitter.fire(response);
    }

    private sendErrorResponse(request: DapRequest, message: string): void {
        const response: DapResponse = {
            seq: this.seq++,
            type: 'response',
            request_seq: request.seq,
            success: false,
            command: request.command,
            message,
        };
        this.emitter.fire(response);
    }

    private sendEvent(event: string, body?: Record<string, unknown>): void {
        const payload: DapEvent = {
            seq: this.seq++,
            type: 'event',
            event,
        };
        if (body) {
            payload.body = body;
        }
        this.emitter.fire(payload);
    }

    private sendOutput(text: string, category: 'stdout' | 'stderr'): void {
        const lines = text.length > 0 ? text : '\n';
        this.sendEvent('output', {
            category,
            output: `${this.sessionOutputPrefix}${lines}`,
        });
        this.debugConsole.append(`${this.sessionOutputPrefix}${lines}`);
    }

    private sendExitedEvent(exitCode: number): void {
        this.sendEvent('exited', { exitCode });
    }

    private sendTerminatedEvent(): void {
        if (this.terminated) {
            return;
        }

        this.terminated = true;
        this.sendEvent('terminated');
    }

    private toSource(sourceFile: unknown): { path: string; name: string } | undefined {
        if (typeof sourceFile !== 'string' || sourceFile.length === 0) {
            return undefined;
        }

        this.rememberRuntimeSourcePath(sourceFile);

        return {
            path: sourceFile,
            name: path.basename(sourceFile),
        };
    }

    private rememberRuntimeSourcePath(sourceFile: string): void {
        if (!sourceFile) {
            return;
        }

        this.runtimeSourcePaths.set(canonicalSourcePath(sourceFile), sourceFile);
    }
}

function canonicalSourcePath(sourceFile: string): string {
    const normalized = sourceFile.replace(/[\\/]+/g, '/');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}
