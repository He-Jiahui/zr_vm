import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    Trace,
} from 'vscode-languageclient/browser';
import { createDocumentSelector, registerZrpJsonSupport } from './zrpSupport';

const CONFIG_SECTION = 'zr.languageServer';
const RESTART_COMMAND = 'zr.restartLanguageServer';

let client: LanguageClient | undefined;
let workerHandle: { terminate: () => void } | undefined;
let workerScriptUrl: string | undefined;
let clientResources: vscode.Disposable[] = [];
let restartChain: Promise<void> = Promise.resolve();
const WEB_STARTUP_TIMEOUT_MS = 30000;

type LanguageServerMode = 'auto' | 'native' | 'web';

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(registerZrpJsonSupport());

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

    await enqueueRestart(context, false);
}

export async function deactivate(): Promise<void> {
    await stopClient();
}

async function enqueueRestart(context: vscode.ExtensionContext, requestedByUser: boolean): Promise<void> {
    restartChain = restartChain.then(async () => {
        await restartLanguageServer(context, requestedByUser);
    });
    await restartChain;
}

async function restartLanguageServer(context: vscode.ExtensionContext, requestedByUser: boolean): Promise<void> {
    await stopClient();
    await startClient(context, requestedByUser);
}

async function startClient(context: vscode.ExtensionContext, requestedByUser: boolean): Promise<void> {
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
    const fileEvents = vscode.workspace.createFileSystemWatcher('**/*.{zr,zrp,zro,zri,dll,so,dylib}');
    clientResources = [
        fileEvents,
    ];

    const worker = await createWorker(workerUri);
    worker.addEventListener('error', (event: Event) => {
        const errorEvent = event as Event & { message?: string; error?: unknown };
        console.error('[zr-web] Language server worker error:', errorEvent.message, errorEvent.error);
    });
    worker.addEventListener('messageerror', (event: MessageEvent) => {
        console.error('[zr-web] Language server worker message error:', event.data);
    });
    workerHandle = worker;

    const clientOptions: LanguageClientOptions = {
        documentSelector: createDocumentSelector() as LanguageClientOptions['documentSelector'],
        outputChannelName: 'Zr Language Server',
        initializationOptions: {
            serverBaseUrl: vscode.Uri.joinPath(context.extensionUri, 'out', 'web').toString(),
        },
        synchronize: {
            configurationSection: CONFIG_SECTION,
            fileEvents,
        },
    };

    client = new LanguageClient(
        'zr-language-server-web',
        'Zr Language Server',
        clientOptions,
        worker,
    );

    await withTimeout(
        client.start(),
        WEB_STARTUP_TIMEOUT_MS,
        'Timed out while starting the Zr web language server.',
    );
    await client.setTrace(resolveTrace(config.get<string>('trace.server', 'off')));

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
        client = undefined;
        await currentClient.stop();
    }

    if (workerHandle !== undefined) {
        workerHandle.terminate();
        workerHandle = undefined;
    }

    if (workerScriptUrl !== undefined) {
        URL.revokeObjectURL(workerScriptUrl);
        workerScriptUrl = undefined;
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

async function withTimeout<T>(promise: Promise<T>, timeoutMs: number, message: string): Promise<T> {
    let timeoutHandle: ReturnType<typeof setTimeout> | undefined;

    try {
        return await Promise.race([
            promise,
            new Promise<T>((_, reject) => {
                timeoutHandle = setTimeout(() => {
                    reject(new Error(message));
                }, timeoutMs);
            }),
        ]);
    } finally {
        if (timeoutHandle !== undefined) {
            clearTimeout(timeoutHandle);
        }
    }
}

async function createWorker(workerUri: vscode.Uri): Promise<{
    addEventListener: (type: string, listener: (event: any) => void) => void;
    terminate: () => void;
}> {
    const workerSource = await fetchWorkerSource(workerUri);
    const blob = new Blob(
        [
            workerSource,
            `\n//# sourceURL=${workerUri.toString()}`,
        ],
        { type: 'application/javascript' },
    );

    workerScriptUrl = URL.createObjectURL(blob);
    try {
        return new (globalThis as any).Worker(workerScriptUrl);
    } catch (error) {
        URL.revokeObjectURL(workerScriptUrl);
        workerScriptUrl = undefined;
        throw error;
    }
}

async function fetchWorkerSource(workerUri: vscode.Uri): Promise<string> {
    const response = await fetch(workerUri.toString());
    if (!response.ok) {
        throw new Error(`Failed to fetch the Zr language server worker from ${workerUri.toString()}: ${response.status} ${response.statusText}`);
    }

    return response.text();
}
