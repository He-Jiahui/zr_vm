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

const CONFIG_SECTION = 'zr.languageServer';
const RESTART_COMMAND = 'zr.restartLanguageServer';

let client: LanguageClient | undefined;
let structureController: ZrStructureController | undefined;
let richHoverController: ZrRichHoverController | undefined;
const serverLifecycle = new LanguageServerController();

class ZrLanguageClient extends LanguageClient {
    constructor(
        private readonly session: LanguageServerSession,
        clientOptions: LanguageClientOptions,
        worker: ConstructorParameters<typeof LanguageClient>[3],
    ) {
        super('zr-language-server-web', 'Zr Language Server', clientOptions, worker);
    }

    override start(): Promise<void> {
        return this.session.observeClientStart(this, () => super.start());
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
        console.warn('ZR structure refresh failed.', error);
    });
}

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
            console.error('[zr-web] language server restart failed:', error);
            void vscode.window.showErrorMessage(`Unable to start the Zr language server: ${String(error)}`);
        }
    }
}

async function startClient(context: vscode.ExtensionContext, requestedByUser: boolean, session: LanguageServerSession): Promise<void> {
    const config = vscode.workspace.getConfiguration(CONFIG_SECTION);
    const enabled = config.get<boolean>('enable', true);
    const mode = config.get<LanguageServerMode>('mode', 'auto');

    if (!enabled) {
        return;
    }

    if (mode === 'native') {
        if (requestedByUser) {
            void vscode.window.showWarningMessage(
                'Zr native language server is not available in VS Code Web. Use zr.languageServer.mode=web or auto.',
            );
        }
        return;
    }

    const workerUri = vscode.Uri.joinPath(context.extensionUri, 'out', 'web', 'server-worker.js');
    const outputChannel = session.own(vscode.window.createOutputChannel('Zr Language Server'));

    const worker = await createWorker(workerUri, session);
    session.assertActive();
    worker.addEventListener('error', (event: Event) => {
        const errorEvent = event as Event & { message?: string; error?: unknown };
        console.error('[zr-web] Language server worker error:', errorEvent.message, errorEvent.error);
    });
    worker.addEventListener('messageerror', (event: MessageEvent) => {
        console.error('[zr-web] Language server worker message error:', event.data);
    });
    const clientOptions: LanguageClientOptions = {
        documentSelector: createDocumentSelector() as LanguageClientOptions['documentSelector'],
        outputChannel,
        initializationOptions: {
            serverBaseUrl: vscode.Uri.joinPath(context.extensionUri, 'out', 'web').toString(),
        },
        middleware: richHoverController?.createMiddleware(),
    };
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

async function createWorker(workerUri: vscode.Uri, session: LanguageServerSession): Promise<{
    addEventListener: (type: string, listener: (event: any) => void) => void;
    terminate: () => void;
}> {
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

async function fetchWorkerSource(workerUri: vscode.Uri, signal: AbortSignal): Promise<string> {
    const response = await fetch(workerUri.toString(), { signal });
    if (!response.ok) {
        throw new Error(`Failed to fetch the Zr language server worker from ${workerUri.toString()}: ${response.status} ${response.statusText}`);
    }

    return response.text();
}
