import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import * as vscode from 'vscode';
import { parseProjectManifestText } from '../projectSupport';
import { DesiredSourceBreakpoint, PendingSourceBreakpointStore } from './breakpointReplay';
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

type VariablesHandle = {
    handleId: number;
    stateId: number;
};

type LaunchSourceContext = {
    projectPath: string;
    projectRoot: string;
    sourceRoot: string;
    binaryRoot: string;
    cwd: string;
};

export class ZrDebugAdapter implements vscode.DebugAdapter {
    private readonly emitter = new vscode.EventEmitter<vscode.DebugProtocolMessage>();
    private readonly variableHandles = new Map<number, VariablesHandle>();
    private readonly runtimeSourcePaths = new Map<string, string>();
    private readonly pendingSourceBreakpoints = new PendingSourceBreakpointStore();
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
    private launchSourceContext: LaunchSourceContext | undefined;
    /**
     * VS Code may dispatch DAP requests concurrently (async handlers without awaiting the previous one).
     * `setBreakpoints` must run only after `launch`/`attach` has connected `this.client`, otherwise
     * `requireClient()` fails and breakpoints never bind.
     */
    private requestChain: Promise<void> = Promise.resolve();

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

        this.requestChain = this.requestChain.then(() =>
            this.dispatchRequest(request).catch((error) => {
                this.sendErrorResponse(request, error instanceof Error ? error.message : String(error));
            }),
        );
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
            case 'setFunctionBreakpoints':
                await this.handleSetFunctionBreakpoints(request);
                return;
            case 'setExceptionBreakpoints':
                await this.handleSetExceptionBreakpoints(request);
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
            case 'evaluate':
                await this.handleEvaluate(request);
                return;
            case 'source':
                await this.handleSource(request);
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
            supportsFunctionBreakpoints: true,
            supportsConditionalBreakpoints: true,
            supportsHitConditionalBreakpoints: true,
            supportsLogPoints: true,
            supportsVariablePaging: true,
            supportsEvaluateForHovers: true,
            exceptionBreakpointFilters: [
                {
                    filter: 'caught',
                    label: 'Caught Exceptions',
                    default: false,
                },
                {
                    filter: 'uncaught',
                    label: 'Uncaught Exceptions',
                    default: true,
                },
            ],
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
        this.launchSourceContext = await this.createLaunchSourceContext(args.project, args.cwd);
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
        this.launchSourceContext = undefined;
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

        await this.replayDesiredSourceBreakpointsUsingKnownPaths();
    }

    private async handleSetBreakpoints(request: DapRequest): Promise<void> {
        const args = request.arguments ?? {};
        const source = args.source as { path?: string } | undefined;
        const pathText = typeof source?.path === 'string' ? path.normalize(source.path) : '';
        const breakpoints = this.normalizeDesiredSourceBreakpoints(
            Array.isArray(args.breakpoints) ? args.breakpoints : [],
        );
        const canonicalKey = canonicalSourcePath(pathText);
        let runtimeSourcePath = this.runtimeSourcePaths.get(canonicalKey) ?? pathText;
        if (
            this.launchSourceContext !== undefined &&
            pathText.length > 0 &&
            !path.isAbsolute(pathText)
        ) {
            const candidate = path.normalize(path.resolve(this.launchSourceContext.sourceRoot, pathText));
            const resolved = await existingFilePath(candidate);
            if (resolved !== undefined) {
                runtimeSourcePath = resolved;
                this.runtimeSourcePaths.set(canonicalKey, resolved);
            }
        }

        if (!pathText) {
            throw new Error('ZR setBreakpoints requires source.path.');
        }

        this.pendingSourceBreakpoints.rememberDesiredBreakpoints(pathText, breakpoints);
        const resolvedBreakpoints = this.client
            ? await this.bindDesiredSourceBreakpoints(pathText, runtimeSourcePath, breakpoints)
            : undefined;

        this.sendResponse(request, {
            breakpoints: this.toDapSourceBreakpoints(pathText, breakpoints, resolvedBreakpoints),
        });
    }

    private async handleSetFunctionBreakpoints(request: DapRequest): Promise<void> {
        const args = request.arguments ?? {};
        const breakpoints = Array.isArray(args.breakpoints) ? args.breakpoints : [];
        const result = await this.requireClient().request('setFunctionBreakpoints', {
            breakpoints: breakpoints.map((item) => ({
                name: typeof item.name === 'string' ? item.name : '',
                condition: typeof item.condition === 'string' ? item.condition : undefined,
                hitCondition: typeof item.hitCondition === 'string' ? item.hitCondition : undefined,
                logMessage: typeof item.logMessage === 'string' ? item.logMessage : undefined,
            })),
        });
        const resolvedBreakpoints = Array.isArray(result.breakpoints) ? result.breakpoints as ZrDbgBreakpoint[] : [];

        this.sendResponse(request, {
            breakpoints: resolvedBreakpoints.map((item, index) => ({
                id: index + 1,
                verified: item.verified !== false,
                line: typeof item.line === 'number' ? item.line : undefined,
            })),
        });
    }

    private normalizeDesiredSourceBreakpoints(rawBreakpoints: unknown[]): DesiredSourceBreakpoint[] {
        return rawBreakpoints
            .map((item) => {
                const breakpoint = item as {
                    line?: unknown;
                    condition?: unknown;
                    hitCondition?: unknown;
                    logMessage?: unknown;
                };

                return {
                    line: Number(breakpoint.line),
                    condition: typeof breakpoint.condition === 'string' ? breakpoint.condition : undefined,
                    hitCondition:
                        typeof breakpoint.hitCondition === 'string' ? breakpoint.hitCondition : undefined,
                    logMessage: typeof breakpoint.logMessage === 'string' ? breakpoint.logMessage : undefined,
                };
            })
            .filter((breakpoint) => Number.isInteger(breakpoint.line) && breakpoint.line > 0);
    }

    private async bindDesiredSourceBreakpoints(
        sourcePath: string,
        runtimeSourcePath: string,
        breakpoints: DesiredSourceBreakpoint[],
    ): Promise<ZrDbgBreakpoint[]> {
        const result = await this.requireClient().request('setBreakpoints', {
            sourceFile: runtimeSourcePath,
            breakpoints,
        });
        const resolvedBreakpoints = Array.isArray(result.breakpoints)
            ? result.breakpoints as ZrDbgBreakpoint[]
            : [];

        this.pendingSourceBreakpoints.markBindingApplied(sourcePath, runtimeSourcePath);
        return resolvedBreakpoints;
    }

    private toDapSourceBreakpoints(
        sourcePath: string,
        requestedBreakpoints: DesiredSourceBreakpoint[],
        resolvedBreakpoints?: ZrDbgBreakpoint[],
    ): Array<Record<string, unknown>> {
        return requestedBreakpoints.map((breakpoint, index) => {
            const resolved = resolvedBreakpoints?.[index];
            return {
                id: index + 1,
                verified: resolvedBreakpoints !== undefined ? Boolean(resolved && resolved.verified !== false) : false,
                line: typeof resolved?.line === 'number' ? resolved.line : breakpoint.line,
                source: { path: sourcePath, name: path.basename(sourcePath) },
            };
        });
    }

    private async replayPendingSourceBreakpointsForResolvedSource(
        runtimeSourcePath: string,
        resolvedSourcePath: string | undefined,
    ): Promise<void> {
        if (!this.client) {
            return;
        }

        const replays = this.pendingSourceBreakpoints.replayBindingsForResolvedSource(
            runtimeSourcePath,
            resolvedSourcePath,
        );
        for (const replay of replays) {
            await this.bindDesiredSourceBreakpoints(
                replay.sourcePath,
                replay.runtimeSourcePath,
                replay.breakpoints,
            );
        }
    }

    private async replayDesiredSourceBreakpointsUsingKnownPaths(): Promise<void> {
        if (!this.client) {
            return;
        }

        for (const desired of this.pendingSourceBreakpoints.getDesiredBreakpoints()) {
            const runtimeSourcePath =
                this.runtimeSourcePaths.get(canonicalSourcePath(desired.sourcePath)) ?? desired.sourcePath;
            await this.replayPendingSourceBreakpointsForResolvedSource(runtimeSourcePath, desired.sourcePath);
        }
    }

    private async handleSetExceptionBreakpoints(request: DapRequest): Promise<void> {
        const args = request.arguments ?? {};
        const filters = Array.isArray(args.filters) ? args.filters.filter((item): item is string => typeof item === 'string') : [];
        await this.requireClient().request('setExceptionBreakpoints', {
            filters,
            caught: filters.includes('caught'),
            uncaught: filters.includes('uncaught'),
        });
        this.sendResponse(request);
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
        const stackFrames = await Promise.all(frames.map(async (frame) => ({
            id: frame.frameId,
            name: this.formatFrameName(frame),
            line: frame.line || 1,
            column: 1,
            source: await this.toSource(frame.sourceFile, frame.moduleName),
            instructionPointerReference: String(frame.instructionIndex),
        })));

        this.sendResponse(request, {
            stackFrames,
            totalFrames: frames.length,
        });
    }

    private async handleScopes(request: DapRequest): Promise<void> {
        const frameId = Number((request.arguments ?? {}).frameId);
        const result = await this.requireClient().request('scopes', { frameId });
        const scopes = Array.isArray(result.scopes) ? result.scopes as ZrDbgScope[] : [];

        for (const scope of scopes) {
            this.variableHandles.set(scope.scopeId, {
                handleId: scope.scopeId,
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
        const args = request.arguments ?? {};
        const variablesReference = Number(args.variablesReference);
        const start = Number(args.start);
        const count = Number(args.count);
        const handle = this.variableHandles.get(variablesReference);
        if (!handle || handle.stateId !== this.currentStateId) {
            throw new Error(`Unknown or stale variablesReference ${variablesReference}.`);
        }

        const result = await this.requireClient().request('variables', {
            scopeId: handle.handleId,
            ...(Number.isInteger(start) && start >= 0 ? { start } : {}),
            ...(Number.isInteger(count) && count > 0 ? { count } : {}),
        });
        const variables = Array.isArray(result.variables) ? result.variables as ZrDbgVariable[] : [];

        for (const item of variables) {
            if (typeof item.variablesReference === 'number' && item.variablesReference > 0) {
                this.variableHandles.set(item.variablesReference, {
                    handleId: item.variablesReference,
                    stateId: this.currentStateId,
                });
            }
        }

        this.sendResponse(request, {
            variables: variables.map((item) => ({
                name: item.name,
                type: item.type,
                value: item.value,
                variablesReference: typeof item.variablesReference === 'number' ? item.variablesReference : 0,
            })),
        });
    }

    private async handleEvaluate(request: DapRequest): Promise<void> {
        const args = request.arguments ?? {};
        const expression = typeof args.expression === 'string' ? args.expression : '';
        const rawFrameId = args.frameId;
        const frameId = typeof rawFrameId === 'number' && Number.isInteger(rawFrameId) ? rawFrameId : 1;
        const result = await this.requireClient().request('evaluate', {
            expression,
            frameId,
        });
        const variablesReference = typeof result.variablesReference === 'number' ? result.variablesReference : 0;
        if (variablesReference > 0) {
            this.variableHandles.set(variablesReference, {
                handleId: variablesReference,
                stateId: this.currentStateId,
            });
        }

        this.sendResponse(request, {
            result: typeof result.value === 'string' ? result.value : '',
            type: typeof result.type === 'string' ? result.type : '',
            variablesReference,
        });
    }

    private async handleSource(request: DapRequest): Promise<void> {
        const source = (request.arguments ?? {}).source as { path?: unknown; name?: unknown } | undefined;
        const sourcePath = typeof source?.path === 'string' ? source.path : undefined;
        const sourceName = typeof source?.name === 'string' ? source.name : undefined;
        const resolvedPath = await this.resolveSourceReference(sourcePath, {
            moduleName: sourceName,
            allowModuleInference: this.launchMode,
        });

        if (!resolvedPath) {
            throw new Error(`cannot resolve source: ${this.describeSourceRequest(sourcePath, sourceName)}`);
        }

        const content = await fs.readFile(resolvedPath, 'utf8');
        this.sendResponse(request, {
            content,
            mimeType: 'text/plain',
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
        this.launchSourceContext = undefined;

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
                await this.replayDesiredSourceBreakpointsUsingKnownPaths();
                break;
            case 'breakpointResolved':
                await this.rememberRuntimeSourcePath(
                    String(message.params?.sourceFile ?? ''),
                    typeof message.params?.moduleName === 'string' ? message.params.moduleName : undefined,
                );
                this.sendEvent('breakpoint', {
                    reason: 'changed',
                    breakpoint: {
                        verified: Boolean(message.params?.resolved),
                        line: Number(message.params?.line ?? 0) || 1,
                        source: await this.toSource(message.params?.sourceFile, message.params?.moduleName),
                    },
                });
                break;
            case 'stopped':
                await this.rememberRuntimeSourcePath(
                    String(message.params?.sourceFile ?? ''),
                    typeof message.params?.moduleName === 'string' ? message.params.moduleName : undefined,
                );
                {
                    const nextStateId = Number(message.params?.stateId ?? 0);
                    if (nextStateId !== this.currentStateId) {
                        this.variableHandles.clear();
                    }
                    this.currentStateId = nextStateId;
                }
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
            case 'output':
                this.sendEvent('output', {
                    category: typeof message.params?.category === 'string' ? message.params.category : 'console',
                    output: typeof message.params?.output === 'string' ? message.params.output : '',
                });
                break;
            case 'continued':
                this.currentStateId = 0;
                this.variableHandles.clear();
                this.sendEvent('continued', {
                    threadId: ZR_DEBUG_MAIN_THREAD_ID,
                    allThreadsContinued: true,
                });
                break;
            case 'terminated':
                this.sendTerminatedEvent();
                break;
            case 'moduleLoaded':
                await this.rememberRuntimeSourcePath(
                    String(message.params?.sourceFile ?? ''),
                    typeof message.params?.moduleName === 'string' ? message.params.moduleName : undefined,
                );
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

    private formatFrameName(frame: ZrDbgFrame): string {
        const receiverName = typeof frame.receiver?.name === 'string' && frame.receiver.name.length > 0
            ? `${frame.receiver.name}.`
            : '';
        const argumentsPreview = Array.isArray(frame.arguments) && frame.arguments.length > 0
            ? `(${frame.arguments.map((item) => `${item.name}=${item.value}`).join(', ')})`
            : '';
        const callKind = typeof frame.callKind === 'string' && frame.callKind.length > 0
            ? `[${frame.callKind}] `
            : '';
        const moduleName = typeof frame.moduleName === 'string' && frame.moduleName.length > 0
            ? ` @${frame.moduleName}`
            : '';
        const frameDepth = typeof frame.frameDepth === 'number'
            ? ` depth=${frame.frameDepth}`
            : '';
        const returnSlot = typeof frame.returnSlot === 'number' && frame.returnSlot >= 0
            ? ` return=r${frame.returnSlot}`
            : '';
        const exceptionMarker = frame.isExceptionFrame ? ' exception' : '';

        return `${callKind}${receiverName}${frame.functionName}${argumentsPreview}${moduleName}${frameDepth}${returnSlot}${exceptionMarker}`;
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

    private async createLaunchSourceContext(projectPath: string, cwd?: string): Promise<LaunchSourceContext> {
        const resolvedProjectPath = path.resolve(projectPath);
        const projectRoot = path.dirname(resolvedProjectPath);
        const resolvedCwd = cwd && cwd.trim().length > 0
            ? path.resolve(cwd)
            : projectRoot;
        const fallbackSourceRoot = path.resolve(projectRoot, 'src');
        const fallbackBinaryRoot = path.resolve(projectRoot, 'bin');

        try {
            const manifestText = await fs.readFile(resolvedProjectPath, 'utf8');
            const manifest = parseProjectManifestText(manifestText, resolvedProjectPath);
            if (manifest) {
                return {
                    projectPath: resolvedProjectPath,
                    projectRoot,
                    sourceRoot: path.resolve(projectRoot, manifest.source),
                    binaryRoot: path.resolve(projectRoot, manifest.binary),
                    cwd: resolvedCwd,
                };
            }
        } catch {
            // Keep launch debugging working even if the adapter cannot parse the manifest locally.
        }

        return {
            projectPath: resolvedProjectPath,
            projectRoot,
            sourceRoot: fallbackSourceRoot,
            binaryRoot: fallbackBinaryRoot,
            cwd: resolvedCwd,
        };
    }

    private async toSource(sourceFile: unknown, moduleName?: unknown): Promise<{ path: string; name: string } | undefined> {
        if (typeof sourceFile !== 'string' || sourceFile.length === 0) {
            return undefined;
        }

        const resolvedPath = await this.rememberRuntimeSourcePath(
            sourceFile,
            typeof moduleName === 'string' ? moduleName : undefined,
        );
        const effectivePath = resolvedPath ?? sourceFile;

        return {
            path: effectivePath,
            name: path.basename(effectivePath),
        };
    }

    private async rememberRuntimeSourcePath(sourceFile: string, moduleName?: string): Promise<string | undefined> {
        if (!sourceFile) {
            return undefined;
        }

        this.runtimeSourcePaths.set(canonicalSourcePath(sourceFile), sourceFile);
        const resolvedPath = await this.resolveSourceReference(sourceFile, {
            moduleName,
            allowModuleInference: this.launchMode,
        });
        if (resolvedPath) {
            this.runtimeSourcePaths.set(canonicalSourcePath(resolvedPath), sourceFile);
            await this.replayPendingSourceBreakpointsForResolvedSource(sourceFile, resolvedPath);
        }

        return resolvedPath;
    }

    private async resolveSourceReference(
        sourceLike: string | undefined,
        options?: {
            moduleName?: string;
            allowModuleInference?: boolean;
        },
    ): Promise<string | undefined> {
        const candidates = collectSourceCandidates(sourceLike, options?.moduleName);

        for (const candidate of candidates) {
            const absolutePath = await this.resolveAbsoluteSourcePath(candidate);
            if (absolutePath) {
                return absolutePath;
            }
        }

        const context = this.launchSourceContext;
        if (context) {
            for (const candidate of candidates) {
                const relativePath = await this.resolveRelativeSourcePath(candidate, context);
                if (relativePath) {
                    return relativePath;
                }
            }
        }

        if (context && options?.allowModuleInference) {
            for (const candidate of candidates) {
                const modulePath = await this.resolveModuleSourcePath(candidate, context);
                if (modulePath) {
                    return modulePath;
                }
            }
        }

        return undefined;
    }

    private async resolveAbsoluteSourcePath(sourceLike: string): Promise<string | undefined> {
        if (!sourceLike || !path.isAbsolute(sourceLike)) {
            return undefined;
        }

        return await existingFilePath(sourceLike);
    }

    private async resolveRelativeSourcePath(
        sourceLike: string,
        context: LaunchSourceContext,
    ): Promise<string | undefined> {
        if (!sourceLike || path.isAbsolute(sourceLike)) {
            return undefined;
        }

        const bases = dedupeStrings([context.cwd, context.projectRoot]);
        for (const basePath of bases) {
            const resolvedPath = await existingFilePath(path.resolve(basePath, sourceLike));
            if (resolvedPath) {
                return resolvedPath;
            }
        }

        return undefined;
    }

    private async resolveModuleSourcePath(
        sourceLike: string,
        context: LaunchSourceContext,
    ): Promise<string | undefined> {
        const moduleName = normalizeModuleName(sourceLike);
        if (!moduleName) {
            return undefined;
        }

        return await existingFilePath(path.resolve(context.sourceRoot, `${moduleName}.zr`));
    }

    private describeSourceRequest(sourcePath: string | undefined, sourceName: string | undefined): string {
        const description = collectSourceCandidates(sourcePath, sourceName);
        return description.length > 0 ? description.join(', ') : 'unknown';
    }
}

function canonicalSourcePath(sourceFile: string): string {
    const normalized = sourceFile.replace(/[\\/]+/g, '/');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

function collectSourceCandidates(...values: Array<string | undefined>): string[] {
    return dedupeStrings(
        values
            .filter((value): value is string => typeof value === 'string')
            .map((value) => value.trim())
            .filter((value) => value.length > 0),
    );
}

function dedupeStrings(values: string[]): string[] {
    const seen = new Set<string>();
    const result: string[] = [];

    for (const value of values) {
        const key = canonicalSourcePath(value);
        if (seen.has(key)) {
            continue;
        }

        seen.add(key);
        result.push(value);
    }

    return result;
}

function normalizeModuleName(modulePath: string): string | undefined {
    let normalized = modulePath.trim();
    if (!normalized) {
        return undefined;
    }

    normalized = normalized.replace(/[\\/]+$/g, '');
    if (normalized.length === 0) {
        return undefined;
    }

    const lowerCasePath = normalized.toLowerCase();
    if (lowerCasePath.endsWith('.zro')) {
        normalized = normalized.slice(0, -4);
    } else if (lowerCasePath.endsWith('.zri')) {
        normalized = normalized.slice(0, -4);
    } else if (lowerCasePath.endsWith('.zr')) {
        normalized = normalized.slice(0, -3);
    }

    normalized = normalized.replace(/^[\\/]+/g, '').replace(/[\\]+/g, '/');
    return normalized.length > 0 ? normalized : undefined;
}

async function existingFilePath(filePath: string): Promise<string | undefined> {
    try {
        const stat = await fs.stat(filePath);
        return stat.isFile() ? path.normalize(filePath) : undefined;
    } catch {
        return undefined;
    }
}
