import * as vscode from 'vscode';
import { onDidChangeLanguageClient, sendLanguageServerRequest } from './languageClientRequests';

const ZR_DECOMPILED_SCHEME = 'zr-decompiled';

class ZrVirtualDocumentProvider implements vscode.TextDocumentContentProvider {
    private readonly onDidChangeEmitter = new vscode.EventEmitter<vscode.Uri>();

    readonly onDidChange = this.onDidChangeEmitter.event;

    constructor() {
        onDidChangeLanguageClient(() => {
            for (const document of vscode.workspace.textDocuments) {
                if (document.uri.scheme === ZR_DECOMPILED_SCHEME) {
                    this.onDidChangeEmitter.fire(document.uri);
                }
            }
        });
    }

    async provideTextDocumentContent(uri: vscode.Uri): Promise<string> {
        const result = await sendLanguageServerRequest<string>('zr/nativeDeclarationDocument', {
            uri: uri.toString(),
        });
        return typeof result === 'string' ? result : '';
    }
}

export function registerVirtualDocumentSupport(): vscode.Disposable {
    return vscode.workspace.registerTextDocumentContentProvider(
        ZR_DECOMPILED_SCHEME,
        new ZrVirtualDocumentProvider(),
    );
}
