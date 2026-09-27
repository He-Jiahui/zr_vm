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

/** 将 DAP variablesReference 限定在产生它的暂停快照中；继续运行后不可复用。 */
type VariablesHandle = {
    handleId: number;
    stateId: number;
};

/**
 * launch 会话拥有的本地源码查找上下文；attach 没有项目根，不进行模块路径推断。
 * TODO: binaryRoot 已从清单计算却未被任何调试路径使用；核对二进制源码映射是否仍需它。
 */
type LaunchSourceContext = {
    projectPath: string;
    projectRoot: string;
    sourceRoot: string;
    binaryRoot: string;
    cwd: string;
};

/**
 * 桌面扩展的内联 DAP 桥：VS Code 请求转为 zrdbg/1，runtime 事件再转为 DAP。
 * 每个 DebugSession 新建一个实例；launch 拥有 CLI 子进程，attach 只拥有客户端连接。
 */
export class ZrDebugAdapter implements vscode.DebugAdapter {
    /** DAP 响应和事件共享消息序列；宿主通过 onDidSendMessage 订阅。 */
    private readonly emitter = new vscode.EventEmitter<vscode.DebugProtocolMessage>();
    /** 仅登记当前停止状态下可展开的 scope/value；continued 会清空。 */
    private readonly variableHandles = new Map<number, VariablesHandle>();
    /** 本地编辑器路径映射回 runtime 的 sourceFile，避免绝对路径断点错过相对路径模块。 */
    private readonly runtimeSourcePaths = new Map<string, string>();
    /** 源断点意图独立于连接和模块加载状态，解析出 runtime 路径后可重放。 */
    private readonly pendingSourceBreakpoints = new PendingSourceBreakpointStore();
    /** CLI 输出同时进入 DAP output 与 VS Code 调试控制台；显示一致性待宿主核对。 */
    private readonly debugConsole = vscode.debug.activeDebugConsole;
    /** 每个会话的 CLI/连接诊断来源标记。 */
    private readonly sessionOutputPrefix: string;
    /** client 是当前 zrdbg/1 连接；launcher 仅 launch 创建并由该会话拥有。 */
    private client: ZrDbgClient | undefined;
    private launcher: ZrCliLauncher | undefined;
    /** 发往 VS Code 的 DAP 消息序号，与 zrdbg 内部请求 id 独立。 */
    private seq = 1;
    /** stopOnEntry=false 仍等待配置完成与 entry stop 两端到齐，再自动恢复运行。 */
    private stopOnEntry = true;
    private configurationDone = false;
    private initialStopSeen = false;
    private pendingAutoContinue = false;
    private currentStateId = 0;
    /** 只保留最近一次异常停止信息，继续执行后失效。 */
    private lastExceptionStack: string | undefined;
    /** runtime、socket 和 CLI exit 都可宣布结束；只向宿主发一次 terminated。 */
    private terminated = false;
    /** 记录资源归属和源码推断权限；attach 不拥有外部进程。 */
    private launchMode = false;
    private launchSourceContext: LaunchSourceContext | undefined;
    /**
     * VS Code may dispatch DAP requests concurrently (async handlers without awaiting the previous one).
     * `setBreakpoints` must run only after `launch`/`attach` has connected `this.client`, otherwise
     * `requireClient()` fails and breakpoints never bind.
     */
    private requestChain: Promise<void> = Promise.resolve();

    /** VS Code 内联适配器的唯一消息输出通道。 */
    readonly onDidSendMessage = this.emitter.event;

    /** 工厂按调试会话创建，输出前缀帮助区分同时存在的 launch/attach 会话。 */
    constructor(private readonly session: vscode.DebugSession) {
        this.sessionOutputPrefix = `[zr:${session.name}] `;
    }

    /** 宿主释放适配器的同步入口；协议 disconnect 负责异步善后。 */
    dispose(): void {
        // TODO: 此路径固定不停止 launcher；核查 launch 失败后仅触发 dispose 时子进程的归属与退出。
        void this.shutdown(false);
        this.emitter.dispose();
    }

    /** 宿主入口：串行化 DAP 请求，令 launch/attach 先完成连接，再处理后续断点与查询。 */
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

    /** 将受支持的 DAP 命令交给会话操作；未支持命令必须返回明确失败响应。 */
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
                // TODO: runtime 已有 threads/threadId 协议；此桥仍固定主线程，需对照 test_debug_threads.c 扩展会话映射。
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
            case 'exceptionInfo':
                this.handleExceptionInfo(request);
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

    /** 告知 VS Code 可用交互；initialized 事件留待 runtime 握手后再发，避免过早配置断点。 */
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
            supportsExceptionInfoRequest: true,
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

    /** 启动由本会话拥有的 CLI，取得调试端点并握手后才确认 DAP launch。 */
    private async handleLaunch(request: DapRequest): Promise<void> {
        // BUG: CLI 成功启动后若 connect/initialize 失败，此 handler 仅把错误交给 DAP 队列；
        // launcher 未 stop，而 dispose() 调 shutdown(false)，--debug-wait 进程可继续留存。
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
            // CLI 原始输出用于启动诊断；独立于 zrdbg 的结构化 output 事件。
            this.sendOutput(text, channel === 'stderr' ? 'stderr' : 'stdout');
        });
        this.launcher.onExit((code) => {
            // CLI exit 给出进程退出码；socket close 与 runtime terminated 可能更早到达。
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

    /** 连接外部拥有的 runtime；不推测项目源码根，也不在断开时终止外部 CLI。 */
    private async handleAttach(request: DapRequest): Promise<void> {
        const args = request.arguments as unknown as ZrAttachRequestArguments;
        const authToken = typeof args.authToken === 'string' ? args.authToken : undefined;

        this.launchMode = false;
        this.launchSourceContext = undefined;
        this.stopOnEntry = true;
        await this.connectRuntime(args.endpoint, authToken);
        this.sendResponse(request);
    }

    /** launch/attach 共用握手与事件桥；连接完成后重放启动前记住的源断点。 */
    private async connectRuntime(endpoint: string, authToken?: string): Promise<void> {
        this.client = new ZrDbgClient();
        this.client.onEvent((message) => {
            // BUG: 断点重放的 RPC 在断线/错误响应时会拒绝；这里丢弃 async Promise，
            // 形成未处理 rejection，且后续 DAP 请求队列无法替该事件报告失败。
            void this.handleRuntimeEvent(message);
        });
        this.client.onClose((error) => {
            // runtime 可能先于 CLI 退出断开；只把尚未结束的会话报告给 DAP。
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

    /** 保存一份编辑器的完整源断点意图；未连接时先回未验证断点，路径解析后再绑定。 */
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
            // launch 的相对编辑器路径先以清单 source 根解释，使源码项目与构建产物的路径可会合。
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

    /** 将函数名断点及条件透传 runtime；与源断点不同，此入口要求连接已完成。 */
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
                // BUG: 函数断点也从 id=1 编号，与源断点及其他函数断点请求的 id 冲突；
                // breakpoint changed 事件要求 id 定位唯一原断点，当前映射无法满足该约束。
                id: index + 1,
                verified: item.verified !== false,
                line: typeof item.line === 'number' ? item.line : undefined,
            })),
        });
    }

    /** 提取可重放的源断点契约，过滤无效行号，保留条件、命中计数和日志表达式。 */
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

    /** 所有初次绑定与重放共用的提交入口；成功回复后记录编辑器/runtime 路径对应关系。 */
    private async bindDesiredSourceBreakpoints(
        sourcePath: string,
        runtimeSourcePath: string,
        breakpoints: DesiredSourceBreakpoint[],
    ): Promise<ZrDbgBreakpoint[]> {
        // BUG: DAP 每次只传一个 sourceFile；runtime 的 ZrDebug_SetBreakpoints 却替换所有 LINE 断点。
        // 在 A.zr、B.zr 依次设断点后，B 的请求删除 A 的断点；跨文件编辑器断点无法共存。
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

    /** 保持编辑器请求次序与 runtime 返回次序对应；连接前或未解析项显示为未验证。 */
    private toDapSourceBreakpoints(
        sourcePath: string,
        requestedBreakpoints: DesiredSourceBreakpoint[],
        resolvedBreakpoints?: ZrDbgBreakpoint[],
    ): Array<Record<string, unknown>> {
        // BUG: id 从每个文件的 1 重新开始；两个文件的首个断点拥有相同 id。
        // breakpoint changed 事件靠 id 查找目标，此处不能为异步验证提供唯一关联。
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

    /** 模块/栈/停止事件提供真实 sourceFile 后，把编辑器保存的断点重新提交给 runtime。 */
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

    /** 握手及 initialized 事件触发的补偿步骤，覆盖 launch 前已经收到 setBreakpoints 的情况。 */
    private async replayDesiredSourceBreakpointsUsingKnownPaths(): Promise<void> {
        // BUG: 遍历多个源文件的重放同样逐次覆盖 runtime 的全局 LINE 断点集合，
        // 因而启动后最终只保留最后一次绑定的文件；见 bindDesiredSourceBreakpoints。
        if (!this.client) {
            return;
        }

        for (const desired of this.pendingSourceBreakpoints.getDesiredBreakpoints()) {
            const runtimeSourcePath =
                this.runtimeSourcePaths.get(canonicalSourcePath(desired.sourcePath)) ?? desired.sourcePath;
            await this.replayPendingSourceBreakpointsForResolvedSource(runtimeSourcePath, desired.sourcePath);
        }
    }

    /** VS Code 异常筛选器转为 runtime 的 caught/uncaught 开关，空筛选器表示均不暂停。 */
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

    /** 配置完成后才允许自动跳过 entry stop，防止用户断点尚未安装就开始执行。 */
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

    /** 把 runtime 快照映射为可导航的 DAP 栈帧；源码解析也会补全待重放的断点路径。 */
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

    /** 登记当前帧作用域为本次暂停可用的 DAP 句柄，再交给变量树请求展开。 */
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

    /** 只展开当前停止状态登记过的句柄，并将 runtime 子句柄继续纳入相同生命周期。 */
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

    /** hover/watch/控制台求值交给 runtime；结果中的可展开引用仅在当前暂停快照有效。 */
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

    /** 异常停止的详情来自最近 stopped 事件保存的栈文本，继续运行后清除。 */
    private handleExceptionInfo(request: DapRequest): void {
        // BUG: runtime stopped 已携带 exceptionKind=caught/uncaught，但本适配器未保存该字段；
        // 启用 caught 筛选器并命中已捕获异常时，exceptionInfo 仍误报 breakMode=unhandled。
        const stackTrace = this.lastExceptionStack ?? '';
        const firstLine = stackTrace.split(/\r?\n/, 1)[0] ?? 'ZR exception';

        this.sendResponse(request, {
            exceptionId: 'zr.exception',
            breakMode: 'unhandled',
            description: stackTrace || 'ZR exception',
            details: stackTrace
                ? {
                    message: firstLine,
                    stackTrace,
                }
                : undefined,
        });
    }

    /** 为 DAP source 请求读取本地源文件；attach 仅接受可解析的本地绝对路径。 */
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

    /** 继续/暂停/单步共用控制出口；状态和句柄失效由随后到达的 runtime 事件驱动。 */
    private async handleSimpleControl(
        request: DapRequest,
        method: 'continue' | 'pause' | 'next' | 'stepIn' | 'stepOut',
        responseBody?: Record<string, unknown>,
    ): Promise<void> {
        await this.requireClient().request(method);
        this.sendResponse(request, responseBody);
    }

    /** 先应答宿主退出请求，再按 launch/attach 的资源所有权关闭连接与子进程。 */
    private async handleDisconnect(request: DapRequest): Promise<void> {
        this.sendResponse(request);
        await this.shutdown(this.launchMode);
    }

    /** 脱离会话可访问资源后尽力发协议 disconnect；只有拥有 launch 子进程的路径要求停止它。 */
    private async shutdown(stopLauncher: boolean): Promise<void> {
        const client = this.client;
        const launcher = this.launcher;

        this.client = undefined;
        this.launcher = undefined;
        this.launchSourceContext = undefined;
        this.lastExceptionStack = undefined;

        if (client) {
            // BUG: 对端保持连接但不回 disconnect 时 request 无超时，此 await 永不结束；
            // client.close 与 launch 子进程 stop 都被阻塞，DAP 已成功应答 disconnect 却不能清理。
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

    /** runtime 的异步生命周期入口：更新源码映射/暂停代次并向 VS Code 发布可观察状态。 */
    private async handleRuntimeEvent(message: ZrDbgEventMessage): Promise<void> {
        switch (message.method) {
            case 'initialized':
                this.sendEvent('initialized');
                await this.replayDesiredSourceBreakpointsUsingKnownPaths();
                break;
            case 'breakpointResolved':
                // BUG: changed 事件未携带原断点 id，且 setBreakpoints 按每个文件重用 1..N；
                // VS Code 无法把异步验证结果稳定对应到原断点，runtime 事件本身也不提供该 id。
                // TODO: runtime 可能先发 breakpointResolved 再回 setBreakpoints；需验证事件早于 DAP 响应的宿主行为。
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
                // 源码映射可能触发断点 RPC；完成后再公布暂停状态，便于编辑器导航和绑定。
                await this.rememberRuntimeSourcePath(
                    String(message.params?.sourceFile ?? ''),
                    typeof message.params?.moduleName === 'string' ? message.params.moduleName : undefined,
                );
                {
                    const nextStateId = Number(message.params?.stateId ?? 0);
                    // 同一快照可重复通知；只有代次变化才废弃变量句柄。
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
                {
                    // BUG: runtime 数据断点发 dataBreakpoint，DAP 的标准数据断点原因为 data breakpoint；
                    // attach 到已由原生 zrdbg 客户端设置数据断点的 runtime 时，这里原样转发导致宿主按未知原因显示。
                    const reason = String(message.params?.reason ?? 'pause');
                    const exceptionStack =
                        typeof message.params?.exceptionStack === 'string' && message.params.exceptionStack.length > 0
                            ? message.params.exceptionStack
                            : undefined;
                    this.lastExceptionStack = reason === 'exception' ? exceptionStack : undefined;
                    if (reason === 'exception' && exceptionStack) {
                        this.sendEvent('output', {
                            category: 'stderr',
                            output: exceptionStack.endsWith('\n') ? exceptionStack : `${exceptionStack}\n`,
                        });
                    }
                    this.sendEvent('stopped', {
                        reason,
                        threadId: ZR_DEBUG_MAIN_THREAD_ID,
                        allThreadsStopped: true,
                        text: exceptionStack ?? (typeof message.params?.functionName === 'string' ? message.params.functionName : undefined),
                        description: exceptionStack,
                    });
                }
                break;
            case 'output':
                this.sendEvent('output', {
                    category: typeof message.params?.category === 'string' ? message.params.category : 'console',
                    output: typeof message.params?.output === 'string' ? message.params.output : '',
                });
                break;
            case 'continued':
                // 继续执行即失效，不能等下一次 stopped 才拒绝旧变量展开。
                this.currentStateId = 0;
                this.lastExceptionStack = undefined;
                this.variableHandles.clear();
                this.sendEvent('continued', {
                    threadId: ZR_DEBUG_MAIN_THREAD_ID,
                    allThreadsContinued: true,
                });
                break;
            case 'terminated':
                this.lastExceptionStack = undefined;
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

    /** entry stop 与配置完成的汇合点；由两个到达顺序之一负责发自动 continue。 */
    private async continueAfterEntry(): Promise<void> {
        if (!this.pendingAutoContinue && !this.initialStopSeen) {
            return;
        }

        this.pendingAutoContinue = false;
        await this.requireClient().request('continue');
    }

    /** 已进入协议阶段的 handler 共用前置检查；连接对象存在不替代 socket 自身的状态检查。 */
    private requireClient(): ZrDbgClient {
        if (!this.client) {
            throw new Error('ZR debugger is not connected.');
        }

        return this.client;
    }

    /** 将 runtime 调用来源、receiver/参数和异常标记带入栈视图，帮助区分同名调用帧。 */
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

    /** 对应入站 request.seq 应答；事件和响应共用本会话递增序号。 */
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

    /** 将 handler 失败归属到原 DAP 请求，避免宿主把运行错误误当作缺少响应。 */
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

    /** 会话状态与 runtime 通知的统一 DAP 事件出口。 */
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

    /** 带会话前缀展示 CLI 及连接错误，使并行调试时仍能追溯输出来源。 */
    private sendOutput(text: string, category: 'stdout' | 'stderr'): void {
        // TODO: 同时发送 DAP output 和 append 到 DebugConsole；需在 VS Code 实测宿主是否重复展示同一文本。
        const lines = text.length > 0 ? text : '\n';
        this.sendEvent('output', {
            category,
            output: `${this.sessionOutputPrefix}${lines}`,
        });
        this.debugConsole.append(`${this.sessionOutputPrefix}${lines}`);
    }

    /** CLI 子进程退出才携带 exitCode；外部 attach 的断线不能推断进程退出码。 */
    private sendExitedEvent(exitCode: number): void {
        this.sendEvent('exited', { exitCode });
    }

    /** 汇合 runtime terminated、socket close 和 CLI exit，避免重复结束宿主会话。 */
    private sendTerminatedEvent(): void {
        if (this.terminated) {
            return;
        }

        this.terminated = true;
        this.sendEvent('terminated');
    }

    /** 读取 launch 项目清单的源码定位信息；本地解析失败时仍允许 CLI 自行解析项目并启动。 */
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

    /** 将 runtime 源身份转为编辑器可打开的位置，同时保留回传断点所需的原始身份。 */
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

    /** 建立本地路径到 runtime sourceFile 的映射，并借此激活先于模块加载创建的断点。 */
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

    /** 优先相信存在的显式路径，再用 launch 上下文解释相对路径，最后才推断模块源码。 */
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

    /** launch/attach 都可使用同机绝对路径；只返回实际文件，交给 source 请求读取。 */
    private async resolveAbsoluteSourcePath(sourceLike: string): Promise<string | undefined> {
        if (!sourceLike || !path.isAbsolute(sourceLike)) {
            return undefined;
        }

        return await existingFilePath(sourceLike);
    }

    /** launch 相对路径按 CLI cwd 优先、项目根次之解释，与运行时启动目录保持关联。 */
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

    /** 二进制模块只带模块名时，用清单 source 根寻找对应 .zr；仅 launch 启用此推断。 */
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

    /** source 查找失败时保留请求中的路径/模块线索，便于诊断 launch 与产物位置差异。 */
    private describeSourceRequest(sourcePath: string | undefined, sourceName: string | undefined): string {
        const description = collectSourceCandidates(sourcePath, sourceName);
        return description.length > 0 ? description.join(', ') : 'unknown';
    }
}

/** 路径映射与去重使用宿主平台的大小写语义；保持与 PendingSourceBreakpointStore 一致。 */
function canonicalSourcePath(sourceFile: string): string {
    const normalized = sourceFile.replace(/[\\/]+/g, '/');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

/** 将 source.path/source.name 等线索按调用方给定优先级归并，供解析与错误信息共用。 */
function collectSourceCandidates(...values: Array<string | undefined>): string[] {
    return dedupeStrings(
        values
            .filter((value): value is string => typeof value === 'string')
            .map((value) => value.trim())
            .filter((value) => value.length > 0),
    );
}

/** 按路径身份去重而保留首次出现次序，防止改变 cwd/项目根/模块推断的优先级。 */
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

/** 将 .zro/.zri/.zr 模块身份归一为 source 根下的相对模块名，供二进制调试定位源码。 */
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

/** 候选路径仅在 stat 确认为普通文件时参与导航；读取失败仍由后续 source 请求报告。 */
async function existingFilePath(filePath: string): Promise<string | undefined> {
    try {
        const stat = await fs.stat(filePath);
        return stat.isFile() ? path.normalize(filePath) : undefined;
    } catch {
        return undefined;
    }
}
