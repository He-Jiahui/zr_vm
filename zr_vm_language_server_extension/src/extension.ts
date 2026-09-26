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

const CONFIG_SECTION = LANGUAGE_SERVER_CONFIG_SECTION;
const RESTART_COMMAND = 'zr.restartLanguageServer';

let client: LanguageClient | undefined;
let structureController: ZrStructureController | undefined;
let richHoverController: ZrRichHoverController | undefined;
const serverLifecycle = new LanguageServerController();

class ZrLanguageClient extends LanguageClient {
    constructor(
        private readonly session: LanguageServerSession,
        serverOptions: ServerOptions,
        clientOptions: LanguageClientOptions,
    ) {
        super('zr-language-server', 'Zr Language Server', serverOptions, clientOptions);
    }

    override start(): Promise<void> {
        return this.session.observeClientStart(this, () => super.start());
    }

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

    override async stop(timeout?: number): Promise<void> {
        try {
            await super.stop(timeout);
        } catch (error) {
            // SDK 8.1 invokes stop without awaiting it after a failed initialization.
            if (!isLanguageClientNotRunningError(error)) { throw error; }
        }
    }
}

type LanguageServerMode = 'auto' | 'native' | 'web';

function refreshStructureViewsAsync(): void {
    void structureController?.refresh().catch((error) => {
        console.warn('[zr-extension] structure.refresh:failed', error);
    });
}

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

    context.subscriptions.push(
        vscode.commands.registerCommand(RESTART_COMMAND, async () => {
            await enqueueRestart(context, true);
        }),
    );
    context.subscriptions.push(
        vscode.commands.registerCommand('zr.__sendLanguageServerRequest',
            async (method: string, params?: unknown, options?: { strict?: boolean }) =>
                sendLanguageServerRequest(method, params, options)),
    );

    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(async (event) => {
            if (event.affectsConfiguration(CONFIG_SECTION)) {
                await enqueueRestart(context, false);
            }
        }),
    );

    context.subscriptions.push(
        onDidChangeSelectedProject(() => {
            void sendZrSelectedProjectToLanguageServer(context, client);
        }),
    );

    await enqueueRestart(context, false);
}

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

    if (vscode.env.uiKind === vscode.UIKind.Web || mode === 'web') {
        if (requestedByUser || mode === 'web') {
            void vscode.window.showWarningMessage(
                'Zr web language server preview is not available in this build. Use desktop mode or set zr.languageServer.mode to native.',
            );
        }
        return;
    }

    const serverPath = resolveNativeLanguageServerPath(context, config);
    if (serverPath === undefined) {
        void vscode.window.showErrorMessage(
            'Unable to locate zr_vm_language_server_stdio. Set zr.languageServer.native.path (relative or absolute) or build the native server.',
        );
        return;
    }

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

    const fileEvents = session.own(vscode.workspace.createFileSystemWatcher('**/*.{zr,zrp,zro,dll,so,dylib}'));
    const outputChannel = session.own(vscode.window.createOutputChannel('Zr Language Server'));

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
    await sendZrSelectedProjectToLanguageServer(context, nextClient);
    session.assertActive();
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
