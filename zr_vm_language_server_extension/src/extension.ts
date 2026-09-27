import * as path from 'node:path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    MessageTransports,
    ServerOptions,
    Trace,
} from 'vscode-languageclient/node';
import {
    createTransportAwareLanguageClientLifecycle,
    isLanguageClientNotRunningError,
} from './languageClientLifecycle';
import { LanguageServerController, LanguageServerSession, StartupCancelled } from './languageServerSession';
import { registerDesktopDebugSupport } from './debug/configProvider';
import { sendLanguageServerRequest, setLanguageClientRequestClient } from './languageClientRequests';
import { LANGUAGE_SERVER_CONFIG_SECTION, resolveNativeLanguageServerPath } from './nativeAssets';
import { registerOrganizeImportsCommand } from './organizeImports';
import { registerDesktopProjectActions } from './projectActions';
import { registerReferenceCodeLensCommand } from './referenceCodeLens';
import { registerRichHoverSupport, type ZrRichHoverController } from './richHover';
import { sendZrSelectedProjectToLanguageServer } from './selectedProjectSync';
import { registerZrStructureViews, ZrStructureController } from './structure';
import { registerVirtualDocumentSupport } from './virtualDocuments';
import { createDocumentSelector, registerZrpJsonSupport } from './zrpSupport';
import { activeWorkspaceFolder, onDidChangeSelectedProject, resolveSelectedProjectUri } from './workspaceProjects';

// 桌面宿主与路径解析器共用配置命名空间，避免读取和重启监听落在不同设置树下。
const CONFIG_SECTION = LANGUAGE_SERVER_CONFIG_SECTION;
const RESTART_COMMAND = 'zr.restartLanguageServer';

// 仅发布已完成初始化的会话；视图/命令通过 languageClientRequests 间接读取。
// controller 管理一次次启动尝试，视图控制器则由扩展上下文跨重启持有。
let client: LanguageClient | undefined;
let structureController: ZrStructureController | undefined;
let richHoverController: ZrRichHoverController | undefined;
const serverLifecycle = new LanguageServerController();

/**
 * 将 Node SDK 的公开启动/停止接口接入单次 session 的期限与退休协议。
 * SDK 仍负责进程传输和自动恢复；session 负责旧尝试不能在异步完成后重新发布。
 */
class ZrLanguageClient extends LanguageClient {
    constructor(
        private readonly session: LanguageServerSession,
        serverOptions: ServerOptions,
        clientOptions: LanguageClientOptions,
    ) {
        super('zr-language-server', 'Zr Language Server', serverOptions, clientOptions);
    }

    /** 初次启动与 SDK 自动恢复均须经过同一个超时和迟到清理边界。 */
    override start(): Promise<void> {
        return this.session.observeClientStart(this, () => super.start());
    }

    /**
     * Node SDK 在异步工作目录检查后才可能取得进程传输；获取点复核 session，
     * 因为 controller 的首次 dispose 不可能等待一个尚未交付的 reader/writer。
     */
    protected override async createMessageTransports(encoding: string): Promise<MessageTransports> {
        this.session.assertActive();
        const transports = await super.createMessageTransports(encoding);
        if (this.session.retired) {
            // The SDK may spawn after its asynchronous cwd check, after our first dispose.
            // Retire at acquisition too: initialization may never settle for this process.
            const releases = await Promise.allSettled([
                Promise.resolve().then(() => transports.reader.dispose()),
                Promise.resolve().then(() => transports.writer.dispose()),
                Promise.resolve().then(() => this.dispose(1000)),
            ]);
            for (const release of releases) {
                if (release.status === 'rejected') {
                    console.warn('[zr-extension] late transport cleanup failed:', release.reason);
                }
            }
            throw new StartupCancelled();
        }
        return transports;
    }

    /** 兼容 SDK 在初始化失败后对未运行客户端执行的 stop，同时保留其他停止错误。 */
    override async stop(timeout?: number): Promise<void> {
        try {
            await super.stop(timeout);
        } catch (error) {
            // SDK 8.1 invokes stop without awaiting it after a failed initialization.
            if (!isLanguageClientNotRunningError(error)) { throw error; }
        }
    }
}

/** 与扩展配置的宿主选择值保持一致；桌面入口只启动 native 传输。 */
type LanguageServerMode = 'auto' | 'native' | 'web';

/** 客户端发布后主动刷新结构视图；刷新失败只影响视图，不回滚已启动的会话。 */
function refreshStructureViewsAsync(): void {
    void structureController?.refresh().catch((error) => {
        console.warn('[zr-extension] structure.refresh:failed', error);
    });
}

/**
 * VS Code 桌面入口。先注册与服务端存活无关的命令、视图和监听器，
 * 再排队首次启动，使失败后的手动重启仍有可调用入口。
 */
export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(registerZrpJsonSupport());
    context.subscriptions.push(...registerDesktopDebugSupport(context));
    context.subscriptions.push(...registerDesktopProjectActions(context));
    context.subscriptions.push(registerReferenceCodeLensCommand());
    context.subscriptions.push(registerOrganizeImportsCommand());
    context.subscriptions.push(registerVirtualDocumentSupport());
    structureController = registerZrStructureViews(context);
    context.subscriptions.push(structureController);
    richHoverController = registerRichHoverSupport(context);
    context.subscriptions.push(richHoverController);

    // 命令和配置事件共用 restart 队列，避免相邻请求并发夺取当前客户端。
    context.subscriptions.push(
        vscode.commands.registerCommand(RESTART_COMMAND, async () => {
            await enqueueRestart(context, true);
        }),
    );
    // 冒烟测试及其他扩展功能经此命令访问当前会话，strict 由调用者选择。
    context.subscriptions.push(
        vscode.commands.registerCommand('zr.__sendLanguageServerRequest',
            async (method: string, params?: unknown, options?: { strict?: boolean }) =>
                sendLanguageServerRequest(method, params, options)),
    );

    // 整个服务端配置树变化会重建会话，以便路径、模式和跟踪选项同时生效。
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(async (event) => {
            if (event.affectsConfiguration(CONFIG_SECTION)) {
                await enqueueRestart(context, false);
            }
        }),
    );

    // BUG: 初次启动发布 client 前发生的选项目事件会传入 undefined 而丢失；
    // selectedProjectSync 不会在发布后自动重放该事件，服务端可能留在旧项目。
    // BUG: 已发布时该 void 异步通知若项目扫描或发送失败，会形成未处理拒绝。
    context.subscriptions.push(
        onDidChangeSelectedProject(() => {
            void sendZrSelectedProjectToLanguageServer(context, client);
        }),
    );

    await enqueueRestart(context, false);
}

/** 先退休语言服务会话，再释放跨重启视图；即使停止失败也不保留 UI 控制器。 */
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

/** 统一激活、命令和配置事件的重启入口；已退休尝试不向用户报告启动故障。 */
async function enqueueRestart(context: vscode.ExtensionContext, requestedByUser: boolean): Promise<void> {
    try {
        await serverLifecycle.restart((session) => startClient(context, requestedByUser, session));
    } catch (error) {
        if (!(error instanceof StartupCancelled)) {
            console.error('[zr-extension] language server restart failed:', error);
            void vscode.window.showErrorMessage(`Unable to start the Zr language server: ${String(error)}`);
        }
    }
}

/**
 * 为一个 session 启动桌面 stdio 客户端。资源在取得后立即交给 session，
 * 仅在启动、跟踪和首次项目通知完成且会话仍有效时发布请求客户端。
 */
async function startClient(
    context: vscode.ExtensionContext,
    requestedByUser: boolean,
    session: LanguageServerSession,
): Promise<void> {
    const config = vscode.workspace.getConfiguration(CONFIG_SECTION);
    const enabled = config.get<boolean>('enable', true);
    const mode = config.get<LanguageServerMode>('mode', 'auto');

    if (!enabled) {
        return;
    }

    // 此入口只承诺桌面 native 服务；显式 web 模式仅给出预览不可用提示。
    if (vscode.env.uiKind === vscode.UIKind.Web || mode === 'web') {
        if (requestedByUser || mode === 'web') {
            void vscode.window.showWarningMessage(
                'Zr web language server preview is not available in this build. Use desktop mode or set zr.languageServer.mode to native.',
            );
        }
        return;
    }

    // BUG: resolver 目前只验证路径存在，现有目录也可能被当作可执行文件交给 SDK。
    // 此调用点随后以该路径作为 command，导致实际启动失败；见 executablePath 的候选检查。
    const serverPath = resolveNativeLanguageServerPath(context, config);
    if (serverPath === undefined) {
        void vscode.window.showErrorMessage(
            'Unable to locate zr_vm_language_server_stdio. Set zr.languageServer.native.path (relative or absolute) or build the native server.',
        );
        return;
    }

    // run/debug 走同一已解析二进制，cwd 固定到二进制目录以免受活动编辑器切换影响。
    const serverOptions: ServerOptions = {
        run: {
            command: serverPath,
            options: {
                cwd: path.dirname(serverPath),
            },
        },
        debug: {
            command: serverPath,
            options: {
                cwd: path.dirname(serverPath),
            },
        },
    };

    // 监听器和输出通道属于本次启动；重启要释放旧订阅后才创建新的一组。
    const fileEvents = session.own(vscode.workspace.createFileSystemWatcher('**/*.{zr,zrp,zro,dll,so,dylib}'));
    const outputChannel = session.own(vscode.window.createOutputChannel('Zr Language Server'));

    // 初始化选项让服务端在第一批项目请求前获得无交互的项目选择。
    const selectedProjectUri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);
    session.assertActive();

    const clientOptions: LanguageClientOptions = {
        documentSelector: createDocumentSelector() as LanguageClientOptions['documentSelector'],
        outputChannel,
        initializationOptions: {
            zrSelectedProjectUri: selectedProjectUri?.toString() ?? null,
        },
        synchronize: {
            configurationSection: CONFIG_SECTION,
            fileEvents,
        },
        middleware: richHoverController?.createMiddleware(),
    };
    // 先放入 SDK 错误策略，再绑定实际客户端；崩溃恢复是否继续由 session 退休状态约束。
    const lifecycle = session.own(createTransportAwareLanguageClientLifecycle<LanguageClient>(undefined, () => session.retired));
    clientOptions.errorHandler = lifecycle.errorHandler;

    const nextClient = new ZrLanguageClient(
        session,
        serverOptions,
        clientOptions,
    );
    lifecycle.attachClient(nextClient);
    await session.startClient(nextClient);
    await nextClient.setTrace(resolveTrace(config.get<string>('trace.server', 'off')));
    session.assertActive();
    // 首次通知在发布前直达本次客户端，避免发送到上次会话；变化事件仍使用全局 client。
    await sendZrSelectedProjectToLanguageServer(context, nextClient);
    session.assertActive();
    // 只有完全启动的客户端可以供视图请求；旧 session 的清理不得清空后继实例。
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

/** 将扩展配置映射为 SDK 跟踪级别；未知值按关闭处理。 */
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
