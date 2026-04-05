import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    Trace,
} from 'vscode-languageclient/node';
import { registerDesktopDebugSupport } from './debug/configProvider';
import { LANGUAGE_SERVER_CONFIG_SECTION, resolveNativeLanguageServerPath } from './nativeAssets';
import { createDocumentSelector, registerZrpJsonSupport } from './zrpSupport';

const CONFIG_SECTION = LANGUAGE_SERVER_CONFIG_SECTION;
const RESTART_COMMAND = 'zr.restartLanguageServer';

let client: LanguageClient | undefined;
let clientResources: vscode.Disposable[] = [];
let restartChain: Promise<void> = Promise.resolve();
let restartSequence = 0;

type LanguageServerMode = 'auto' | 'native' | 'web';

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(registerZrpJsonSupport());
    context.subscriptions.push(...registerDesktopDebugSupport(context));

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
            'Unable to locate zr_vm_language_server_stdio. Set zr.languageServer.native.path or build the native server.',
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

    const fileEvents = vscode.workspace.createFileSystemWatcher('**/*.{zr,zrp,zro,zri,dll,so,dylib}');
    clientResources = [
        fileEvents,
    ];

    const clientOptions: LanguageClientOptions = {
        documentSelector: createDocumentSelector() as LanguageClientOptions['documentSelector'],
        outputChannelName: 'Zr Language Server',
        synchronize: {
            configurationSection: CONFIG_SECTION,
            fileEvents,
        },
    };

    client = new LanguageClient(
        'zr-language-server',
        'Zr Language Server',
        serverOptions,
        clientOptions,
    );
    await client.start();
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
