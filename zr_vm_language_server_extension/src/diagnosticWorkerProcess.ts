import { spawn } from 'node:child_process';
import * as path from 'node:path';
import { createMessageConnection, StreamMessageReader, StreamMessageWriter } from 'vscode-jsonrpc/node';
import type * as vscode from 'vscode';

/** A worker owns its native VM and parser. No SDK providers are registered here. */
export interface DiagnosticWorker {
    request(method: string, params: unknown, token?: vscode.CancellationToken): Promise<any>;
    notify(method: string, params: unknown): void | Promise<void>;
    dispose(): Promise<void>;
}

export interface DiagnosticWorkerContext {
    selectedProjectUri?: string;
    workspaceFolders: { uri: string; name: string }[];
}

/** Hard termination also cancels native analysis that cannot yield to the dispatcher. */
export function startDiagnosticWorker(
    serverPath: string, output: vscode.OutputChannel, context: DiagnosticWorkerContext,
): DiagnosticWorker {
    const child = spawn(serverPath, [], {
        cwd: path.dirname(serverPath), windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'],
    });
    const connection = createMessageConnection(new StreamMessageReader(child.stdout), new StreamMessageWriter(child.stdin));
    let disposed = false;
    let exited = false;
    const exit = new Promise<void>(resolve => {
        child.once('close', () => { exited = true; resolve(); });
        // Failed spawn still emits close; observe error so Node never treats it as unhandled.
        child.once('error', error => { output.appendLine(`[diagnostics] spawn failed: ${error.message}`); connection.dispose(); });
    });
    child.stderr.on('data', data => output.appendLine(`[diagnostics ${child.pid}] ${String(data).trimEnd()}`));
    connection.onClose(() => connection.dispose());
    connection.listen();
    let initializationTimer: ReturnType<typeof setTimeout>;
    const initialized = Promise.race([
        connection.sendRequest('initialize', {
            processId: process.pid,
            rootUri: context.workspaceFolders[0]?.uri ?? null,
            workspaceFolders: context.workspaceFolders,
            capabilities: {},
            initializationOptions: {
                zrSelectedProjectUri: context.selectedProjectUri ?? null,
                zrDeferDiagnostics: true,
            },
        }),
        new Promise<never>((_, reject) => {
            initializationTimer = setTimeout(() => reject(new Error('Diagnostic worker initialization timed out')), 15000);
        }),
    ]).then(async () => {
        if (disposed) { throw new Error('Diagnostic worker retired'); }
        await connection.sendNotification('initialized', {});
    }).finally(() => clearTimeout(initializationTimer));
    // A lazy worker may be cancelled before its first request observes initialization.
    void initialized.catch(() => {});
    output.appendLine(`[diagnostics] started process ${child.pid}`);
    let disposal: Promise<void> | undefined;
    return {
        async request(method, params, token) {
            await initialized;
            if (disposed) { throw new Error('Diagnostic worker retired'); }
            return token ? connection.sendRequest(method, params, token) : connection.sendRequest(method, params);
        },
        async notify(method, params) {
            await initialized;
            if (!disposed) { await connection.sendNotification(method, params); }
        },
        dispose() {
            return disposal ??= (async () => {
                disposed = true;
                clearTimeout(initializationTimer);
                connection.dispose();
                child.stdin.destroy();
                if (!exited) { child.kill(); }
                let timer: ReturnType<typeof setTimeout> | undefined;
                try {
                    await Promise.race([exit, new Promise<never>((_, reject) => {
                        timer = setTimeout(() => reject(new Error(`Diagnostic process ${child.pid} did not exit`)), 5000);
                    })]);
                    output.appendLine(`[diagnostics] stopped process ${child.pid}`);
                } finally { if (timer) { clearTimeout(timer); } }
            })();
        },
    };
}
