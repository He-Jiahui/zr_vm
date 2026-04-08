import * as vscode from 'vscode';

type RequestCapableClient = {
    sendRequest<TResult>(method: string, params?: unknown): Thenable<TResult> | Promise<TResult>;
};

const onDidChangeClientEmitter = new vscode.EventEmitter<void>();

let currentClient: RequestCapableClient | undefined;

export const onDidChangeLanguageClient = onDidChangeClientEmitter.event;

export function setLanguageClientRequestClient(client: RequestCapableClient | undefined): void {
    currentClient = client;
    onDidChangeClientEmitter.fire();
}

export async function sendLanguageServerRequest<TResult>(
    method: string,
    params?: unknown,
): Promise<TResult | undefined> {
    if (!currentClient) {
        return undefined;
    }

    return await currentClient.sendRequest<TResult>(method, params);
}
