import * as vscode from 'vscode';

const ZRP_EXTENSION = '.zrp';
const ZRP_LANGUAGE_ID = 'json';
type DocumentSelectorEntry = {
    language?: string;
    scheme?: string;
    pattern?: string;
};

function isZrpDocument(document: vscode.TextDocument): boolean {
    if (document.uri.scheme !== 'file' && document.uri.scheme !== 'untitled') {
        return false;
    }

    return document.uri.path.toLowerCase().endsWith(ZRP_EXTENSION);
}

async function ensureZrpLanguage(document: vscode.TextDocument): Promise<void> {
    if (!isZrpDocument(document) || document.languageId === ZRP_LANGUAGE_ID) {
        return;
    }

    await vscode.languages.setTextDocumentLanguage(document, ZRP_LANGUAGE_ID);
}

export function registerZrpJsonSupport(): vscode.Disposable {
    for (const document of vscode.workspace.textDocuments) {
        void ensureZrpLanguage(document);
    }

    return vscode.workspace.onDidOpenTextDocument((document) => {
        void ensureZrpLanguage(document);
    });
}

export function createDocumentSelector(): DocumentSelectorEntry[] {
    return [
        { language: 'zr', scheme: 'file' },
        { language: 'zr', scheme: 'untitled' },
        { scheme: 'file', pattern: '**/*.zrp' },
    ];
}
