import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    Trace,
} from 'vscode-languageclient/node';
import {
    createTransportAwareLanguageClientLifecycle,
    isBenignLanguageClientStopError,
    stopLanguageClientSafely,
    type TransportAwareLanguageClientLifecycle,
} from './languageClientLifecycle';
import { registerDesktopDebugSupport } from './debug/configProvider';
import { setLanguageClientRequestClient } from './languageClientRequests';
import { LANGUAGE_SERVER_CONFIG_SECTION, resolveNativeLanguageServerPath } from './nativeAssets';
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
let clientLifecycle: TransportAwareLanguageClientLifecycle<LanguageClient> | undefined;
let structureController: ZrStructureController | undefined;
let richHoverController: ZrRichHoverController | undefined;
let clientResources: vscode.Disposable[] = [];
let restartChain: Promise<void> = Promise.resolve();
let restartSequence = 0;

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
    await stopClient();
    structureController?.dispose();
    structureController = undefined;
    richHoverController?.dispose();
    richHoverController = undefined;
}

async function enqueueRestart(context: vscode.ExtensionContext, requestedByUser: boolean): Promise<void> {
    const sequence = restartSequence + 1;
    restartSequence = sequence;
    restartChain = restartChain.then(async () => {
        await restartLanguageServer(context, requestedByUser, sequence);
    });
    await restartChain;
}

async function restartLanguageServer(
    context: vscode.ExtensionContext,
    requestedByUser: boolean,
    sequence: number,
): Promise<void> {
    await stopClient();
    await startClient(context, requestedByUser, sequence);
}

async function startClient(
    context: vscode.ExtensionContext,
    requestedByUser: boolean,
    sequence: number,
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

    const fileEvents = vscode.workspace.createFileSystemWatcher('**/*.{zr,zrp,zro,dll,so,dylib}');
    clientResources = [
        fileEvents,
    ];

    const selectedProjectUri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);

    const clientOptions: LanguageClientOptions = {
        documentSelector: createDocumentSelector() as LanguageClientOptions['documentSelector'],
        outputChannelName: 'Zr Language Server',
        initializationOptions: {
            zrSelectedProjectUri: selectedProjectUri?.toString() ?? null,
        },
        synchronize: {
            configurationSection: CONFIG_SECTION,
            fileEvents,
        },
        middleware: richHoverController?.createMiddleware(),
    };
    const lifecycle = createTransportAwareLanguageClientLifecycle<LanguageClient>();
    clientOptions.errorHandler = lifecycle.errorHandler;

    const nextClient = new LanguageClient(
        'zr-language-server',
        'Zr Language Server',
        serverOptions,
        clientOptions,
    );
    lifecycle.attachClient(nextClient);
    client = nextClient;
    clientLifecycle = lifecycle;
    setLanguageClientRequestClient(nextClient);

    await nextClient.start();
    await nextClient.setTrace(resolveTrace(config.get<string>('trace.server', 'off')));
    await sendZrSelectedProjectToLanguageServer(context, nextClient);
    refreshStructureViewsAsync();

    if (requestedByUser) {
        void vscode.window.showInformationMessage('Zr language server restarted.');
    }
}

async function stopClient(): Promise<void> {
    for (const resource of clientResources) {
        resource.dispose();
    }
    clientResources = [];

    if (client !== undefined) {
        const currentClient = client;
        const currentLifecycle = clientLifecycle;
        client = undefined;
        clientLifecycle = undefined;
        setLanguageClientRequestClient(undefined);
        try {
            if (currentLifecycle !== undefined) {
                await stopLanguageClientSafely(currentClient, currentLifecycle);
            } else {
                await currentClient.stop();
            }
        } catch (error) {
            if (!isBenignLanguageClientStopError(error)) {
                throw error;
            }
        } finally {
            currentLifecycle?.dispose();
        }
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
