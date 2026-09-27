import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    Trace,
} from 'vscode-languageclient/browser';
import {
    createTransportAwareLanguageClientLifecycle,
    isLanguageClientNotRunningError,
} from './languageClientLifecycle';
import { LanguageServerController, LanguageServerSession, StartupCancelled } from './languageServerSession';
import { registerWebDebugSupportUnavailable } from './debug/webSupport';
import { sendLanguageServerRequest, setLanguageClientRequestClient } from './languageClientRequests';
import { registerOrganizeImportsCommand } from './organizeImports';
import { registerWebProjectActionsUnavailable } from './projectActionsWeb';
import { registerReferenceCodeLensCommand } from './referenceCodeLens';
import { registerRichHoverSupport, type ZrRichHoverController } from './richHover';
import { registerZrStructureViews, ZrStructureController } from './structure';
import { registerVirtualDocumentSupport } from './virtualDocuments';
import { createDocumentSelector, registerZrpJsonSupport } from './zrpSupport';

// 两个宿主监听同一语言服务配置树；Web 的不支持项由本入口作明确选择。
const CONFIG_SECTION = 'zr.languageServer';
const RESTART_COMMAND = 'zr.restartLanguageServer';

// 只发布已完成启动的 Worker 客户端；视图控制器跨单次服务会话保留。
// controller 串行化激活、命令和配置变化产生的启动请求。
let client: LanguageClient | undefined;
let structureController: ZrStructureController | undefined;
let richHoverController: ZrRichHoverController | undefined;
const serverLifecycle = new LanguageServerController();

/** Web SDK 客户端仍由单次 session 管理，初次启动和 SDK 自动恢复共用退休边界。 */
class ZrLanguageClient extends LanguageClient {
    constructor(
        private readonly session: LanguageServerSession,
        clientOptions: LanguageClientOptions,
        worker: ConstructorParameters<typeof LanguageClient>[3],
    ) {
        super('zr-language-server-web', 'Zr Language Server', clientOptions, worker);
    }

    /** SDK 自发重启也必须有期限，并在旧 session 退休后回收迟到结果。 */
    override start(): Promise<void> {
        return this.session.observeClientStart(this, () => super.start());
    }

    /** 忽略 SDK 对未运行客户端的已知 stop 错误，其他失败继续交给 session 报告。 */
    override async stop(timeout?: number): Promise<void> {
        try {
            await super.stop(timeout);
        } catch (error) {
            // SDK 8.1 invokes stop without awaiting it after a failed initialization.
            if (!isLanguageClientNotRunningError(error)) { throw error; }
        }
    }
}

/** 与扩展配置共用的宿主选择值；Web 入口只启动 Worker 传输。 */
type LanguageServerMode = 'auto' | 'native' | 'web';

/** 客户端发布后刷新结构视图，视图错误不应让已启动的服务回滚。 */
function refreshStructureViewsAsync(): void {
    void structureController?.refresh().catch((error) => {
        console.warn('ZR structure refresh failed.', error);
    });
}

/**
 * VS Code Web 入口。先注册无须 Worker 的能力和不可用功能提示，
 * 再启动语言服务；首次失败后重启命令仍能重新尝试。
 */
export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(registerZrpJsonSupport());
    context.subscriptions.push(...registerWebDebugSupportUnavailable());
    context.subscriptions.push(...registerWebProjectActionsUnavailable(context));
    context.subscriptions.push(registerReferenceCodeLensCommand());
    context.subscriptions.push(registerOrganizeImportsCommand());
    context.subscriptions.push(registerVirtualDocumentSupport());
    structureController = registerZrStructureViews(context, { projectIndexAvailable: false });
    context.subscriptions.push(structureController);
    richHoverController = registerRichHoverSupport(context);
    context.subscriptions.push(richHoverController);

    // 命令和配置事件共用 restart 队列，旧 Worker 的释放先于新尝试。
    context.subscriptions.push(
        vscode.commands.registerCommand(RESTART_COMMAND, async () => {
            await enqueueRestart(context, true);
        }),
    );
    // 冒烟测试与视图可以通过此命令向当前客户端请求，strict 由调用者选择。
    context.subscriptions.push(
        vscode.commands.registerCommand('zr.__sendLanguageServerRequest',
            async (method: string, params?: unknown, options?: { strict?: boolean }) =>
                sendLanguageServerRequest(method, params, options)),
    );

    // 配置树变化统一重建客户端，使模式、开关和跟踪选项从新配置生效。
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(async (event) => {
            if (event.affectsConfiguration(CONFIG_SECTION)) {
                await enqueueRestart(context, false);
            }
        }),
    );

    await enqueueRestart(context, false);
}

/** 先退休当前 Worker 会话，再释放跨重启视图，即使停止报错也执行视图清理。 */
export async function deactivate(): Promise<void> {
    try {
        await serverLifecycle.dispose();
    } finally {
        structureController?.dispose();
        structureController = undefined;
        richHoverController?.dispose();
        richHoverController = undefined;
    }
}

/** 激活、手动命令和配置变更的共享重启入口；取消的旧尝试不显示启动错误。 */
async function enqueueRestart(context: vscode.ExtensionContext, requestedByUser: boolean): Promise<void> {
    try {
        await serverLifecycle.restart((session) => startClient(context, requestedByUser, session));
    } catch (error) {
        if (!(error instanceof StartupCancelled)) {
            console.error('[zr-web] language server restart failed:', error);
            void vscode.window.showErrorMessage(`Unable to start the Zr language server: ${String(error)}`);
        }
    }
}

/**
 * 在当前 session 内获取 Worker 并启动浏览器客户端。只有初始化、跟踪设置和
 * 退休复核均完成后才将它提供给请求桥和视图。
 */
async function startClient(context: vscode.ExtensionContext, requestedByUser: boolean, session: LanguageServerSession): Promise<void> {
    const config = vscode.workspace.getConfiguration(CONFIG_SECTION);
    const enabled = config.get<boolean>('enable', true);
    const mode = config.get<LanguageServerMode>('mode', 'auto');

    if (!enabled) {
        return;
    }

    // TODO: Web 宿主的显式 native 模式仅在手动请求时提示；核对首次激活静默无服务是否符合产品预期。
    if (mode === 'native') {
        if (requestedByUser) {
            void vscode.window.showWarningMessage(
                'Zr native language server is not available in VS Code Web. Use zr.languageServer.mode=web or auto.',
            );
        }
        return;
    }

    // 发布包中的 Worker 路径与构建脚本输出约定一致；通道和 Worker 属于本次 session。
    const workerUri = vscode.Uri.joinPath(context.extensionUri, 'out', 'web', 'server-worker.js');
    const outputChannel = session.own(vscode.window.createOutputChannel('Zr Language Server'));

    const worker = await createWorker(workerUri, session);
    session.assertActive();
    // 原始 Worker 错误留在控制台；连接关闭与自动恢复仍交给 SDK 错误策略。
    worker.addEventListener('error', (event: Event) => {
        const errorEvent = event as Event & { message?: string; error?: unknown };
        console.error('[zr-web] Language server worker error:', errorEvent.message, errorEvent.error);
    });
    worker.addEventListener('messageerror', (event: MessageEvent) => {
        console.error('[zr-web] Language server worker message error:', event.data);
    });
    // 服务端的 WASM 资源以发布包 web 目录为基址；视图悬停复用当前客户端中间件。
    const clientOptions: LanguageClientOptions = {
        documentSelector: createDocumentSelector() as LanguageClientOptions['documentSelector'],
        outputChannel,
        initializationOptions: {
            serverBaseUrl: vscode.Uri.joinPath(context.extensionUri, 'out', 'web').toString(),
        },
        middleware: richHoverController?.createMiddleware(),
    };
    // 先向 SDK 注入错误策略，再绑定客户端；退休时禁止 SDK 复活旧 Worker。
    const lifecycle = session.own(createTransportAwareLanguageClientLifecycle<LanguageClient>(undefined, () => session.retired));
    clientOptions.errorHandler = lifecycle.errorHandler;

    const nextClient = new ZrLanguageClient(
        session,
        clientOptions,
        worker,
    );
    lifecycle.attachClient(nextClient);
    await session.startClient(nextClient);
    await nextClient.setTrace(resolveTrace(config.get<string>('trace.server', 'off')));
    session.assertActive();
    // 请求桥只看见完成启动的会话，旧 session 清理仅撤销自身发布的客户端。
    client = nextClient;
    session.addCleanup(() => {
        if (client === nextClient) {
            client = undefined;
            setLanguageClientRequestClient(undefined);
        }
    });
    setLanguageClientRequestClient(nextClient);
    refreshStructureViewsAsync();

    if (requestedByUser) {
        void vscode.window.showInformationMessage('Zr language server restarted.');
    }
}

/** 将扩展配置映射为 SDK 跟踪级别，未知值安全回退到关闭。 */
function resolveTrace(value: string): Trace {
    switch (value) {
        case 'messages':
            return Trace.Messages;
        case 'verbose':
            return Trace.Verbose;
        default:
            return Trace.Off;
    }
}

/**
 * 从扩展包读取 Worker 源码并以 Blob URL 启动，避免直接把扩展 URI 当作 Worker 脚本 URL。
 * URL 先登记清理，Worker 再登记，逆序退休会先终止 Worker 再撤销 URL。
 */
async function createWorker(workerUri: vscode.Uri, session: LanguageServerSession): Promise<{
    addEventListener: (type: string, listener: (event: any) => void) => void;
    terminate: () => void;
}> {
    // fetch 即使忽略取消信号，迟到结果也必须经过 session 复核才可创建 Worker。
    const workerSource = await fetchWorkerSource(workerUri, session.signal);
    session.assertActive();
    const blob = new Blob(
        [
            workerSource,
            `\n//# sourceURL=${workerUri.toString()}`,
        ],
        { type: 'application/javascript' },
    );

    const workerScriptUrl = URL.createObjectURL(blob);
    session.addCleanup(() => URL.revokeObjectURL(workerScriptUrl));
    const worker = new (globalThis as any).Worker(workerScriptUrl);
    session.addCleanup(() => worker.terminate());
    return worker;
}

/** 获取打包 Worker 源码；响应非成功状态保留 URI 和状态供诊断，取消交给 session。 */
async function fetchWorkerSource(workerUri: vscode.Uri, signal: AbortSignal): Promise<string> {
    // TODO: browserRunner 覆盖正常 Web 路径，但宿主单测只注入固定 fetch 文本；
    // 仍需核对其他 extensionUri scheme 的读取与失败响应在真实 Web 中的诊断。
    const response = await fetch(workerUri.toString(), { signal });
    if (!response.ok) {
        throw new Error(`Failed to fetch the Zr language server worker from ${workerUri.toString()}: ${response.status} ${response.statusText}`);
    }

    return response.text();
}
