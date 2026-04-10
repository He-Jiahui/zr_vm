/// <reference lib="webworker" />

import {
    BrowserMessageReader,
    BrowserMessageWriter,
    createConnection,
    TextDocumentSyncKind,
    type CompletionItem,
    type Diagnostic,
    type DiagnosticSeverity,
    type DocumentHighlight,
    type Hover,
    type InlayHint,
    type InitializeParams,
    type InitializeResult,
    type Location,
    type Position,
    type Range,
    type SemanticTokens,
    type SemanticTokensLegend,
    type TextDocumentContentChangeEvent,
    type WorkspaceEdit,
} from 'vscode-languageserver/browser';
import { ZrWasmBridge } from './wasm-bridge';

declare const self: DedicatedWorkerGlobalScope;

type ManagedDocument = {
    text: string;
    version: number;
};

type WasmPayload<T> = {
    success: boolean;
    data?: T;
    error?: string;
};

const connection = createConnection(
    new BrowserMessageReader(self),
    new BrowserMessageWriter(self),
);
const bridge = new ZrWasmBridge();
const documents = new Map<string, ManagedDocument>();
const semanticTokenLegend: SemanticTokensLegend = {
    tokenTypes: [
        'namespace',
        'class',
        'struct',
        'interface',
        'enum',
        'function',
        'method',
        'property',
        'variable',
        'parameter',
        'keyword',
    ],
    tokenModifiers: [],
};

let shutdownRequested = false;
let serverBaseUrl = '';

self.addEventListener('error', (event) => {
    console.error('[zr-web-worker] Unhandled worker error:', event.message, event.error);
});

self.addEventListener('unhandledrejection', (event) => {
    console.error('[zr-web-worker] Unhandled promise rejection:', event.reason);
});

connection.onInitialize(async (params: InitializeParams): Promise<InitializeResult> => {
    if (typeof params.initializationOptions?.serverBaseUrl === 'string') {
        serverBaseUrl = params.initializationOptions.serverBaseUrl;
    } else {
        serverBaseUrl = resolveDefaultServerBaseUrl();
    }

    await bridge.initialize(serverBaseUrl);

    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            completionProvider: {
                resolveProvider: false,
                triggerCharacters: ['.'],
            },
            hoverProvider: true,
            definitionProvider: true,
            referencesProvider: true,
            renameProvider: {
                prepareProvider: true,
            },
            documentSymbolProvider: true,
            workspaceSymbolProvider: true,
            documentHighlightProvider: true,
            inlayHintProvider: true,
            semanticTokensProvider: {
                legend: semanticTokenLegend,
                full: true,
            },
        },
        serverInfo: {
            name: 'zr_vm_language_server_wasm',
            version: '0.0.1',
        },
    };
});

connection.onInitialized(() => {
    // Standard LSP lifecycle hook. No additional setup is required here.
});

connection.onShutdown(() => {
    shutdownRequested = true;
    bridge.dispose();
});

connection.onNotification('exit', () => {
    bridge.dispose();
    self.close();
});

connection.onDidOpenTextDocument(async ({ textDocument }) => {
    documents.set(textDocument.uri, {
        text: textDocument.text,
        version: textDocument.version,
    });

    const updateResponse = await bridge.updateDocument(textDocument.uri, textDocument.text, textDocument.version);
    if (!updateResponse.success) {
        console.error('[zr-web-worker] updateDocument failed on open:', textDocument.uri, updateResponse.error);
    }
    await publishDiagnostics(textDocument.uri, textDocument.version);
});

connection.onDidChangeTextDocument(async ({ textDocument, contentChanges }) => {
    const current = documents.get(textDocument.uri);
    const updatedText = applyContentChanges(current?.text ?? '', contentChanges);
    const version = textDocument.version ?? (current?.version ?? 0) + 1;

    documents.set(textDocument.uri, {
        text: updatedText,
        version,
    });

    const updateResponse = await bridge.updateDocument(textDocument.uri, updatedText, version);
    if (!updateResponse.success) {
        console.error('[zr-web-worker] updateDocument failed on change:', textDocument.uri, updateResponse.error);
    }
    await publishDiagnostics(textDocument.uri, version);
});

connection.onDidCloseTextDocument(async ({ textDocument }) => {
    documents.delete(textDocument.uri);
    await bridge.closeDocument(textDocument.uri);
    connection.sendDiagnostics({
        uri: textDocument.uri,
        diagnostics: [],
    });
});

connection.onDidSaveTextDocument(async ({ textDocument, text }) => {
    const current = documents.get(textDocument.uri);
    if (typeof text === 'string') {
        const version = current?.version ?? 0;
        documents.set(textDocument.uri, {
            text,
            version,
        });
        await bridge.updateDocument(textDocument.uri, text, version);
    }

    await publishDiagnostics(textDocument.uri, documents.get(textDocument.uri)?.version);
});

connection.onCompletion(async ({ textDocument, position }) => {
    const response = await bridge.getCompletion(textDocument.uri, position.line, position.character);
    return responseData<CompletionItem[]>(response, []);
});

connection.onHover(async ({ textDocument, position }) => {
    const response = await bridge.getHover(textDocument.uri, position.line, position.character);
    return responseData<Hover | null>(response, null);
});

connection.onDefinition(async ({ textDocument, position }) => {
    const response = await bridge.getDefinition(textDocument.uri, position.line, position.character);
    return responseData<Location[]>(response, []);
});

connection.onReferences(async ({ textDocument, position, context }) => {
    const response = await bridge.findReferences(
        textDocument.uri,
        position.line,
        position.character,
        context.includeDeclaration,
    );
    return responseData<Location[]>(response, []);
});

connection.onDocumentSymbol(async ({ textDocument }) => {
    const response = await bridge.getDocumentSymbols(textDocument.uri);
    return responseData<unknown[]>(response, []);
});

connection.onRequest('textDocument/inlayHint', async ({ textDocument, range }) => {
    const response = await bridge.getInlayHints(
        textDocument.uri,
        range.start.line,
        range.start.character,
        range.end.line,
        range.end.character,
    );
    return responseData<InlayHint[]>(response, []);
});

connection.onWorkspaceSymbol(async ({ query }) => {
    const response = await bridge.getWorkspaceSymbols(query);
    return responseData<unknown[]>(response, []);
});

connection.onRequest('zr/nativeDeclarationDocument', async ({ uri }: { uri: string }) => {
    const response = await bridge.getNativeDeclarationDocument(uri);
    return responseData<string | null>(response, null);
});

connection.onRequest('zr/projectModules', async ({ uri }: { uri: string }) => {
    const response = await bridge.getProjectModules(uri);
    return responseData<unknown[]>(response, []);
});

connection.onDocumentHighlight(async ({ textDocument, position }) => {
    const response = await bridge.getDocumentHighlights(textDocument.uri, position.line, position.character);
    return responseData<DocumentHighlight[]>(response, []);
});

connection.onRequest('textDocument/semanticTokens/full', async ({ textDocument }) => {
    const response = await bridge.getSemanticTokens(textDocument.uri);
    return responseData<SemanticTokens | null>(response, null);
});

connection.onPrepareRename(async ({ textDocument, position }) => {
    const response = await bridge.prepareRename(textDocument.uri, position.line, position.character);
    return responseData<unknown>(response, null);
});

connection.onRenameRequest(async ({ textDocument, position, newName }) => {
    const response = await bridge.rename(textDocument.uri, position.line, position.character, newName);
    const locations = responseData<Location[] | null>(response, null);
    if (locations === null) {
        return null;
    }

    return buildWorkspaceEdit(locations, newName);
});

connection.listen();

function responseData<T>(response: WasmPayload<T>, fallback: T): T {
    if (!response.success) {
        if (response.error) {
            console.error('[zr-web-worker] wasm request failed:', response.error);
            connection.console.warn(response.error);
        }
        return fallback;
    }

    return (response.data as T | undefined) ?? fallback;
}

async function publishDiagnostics(uri: string, version: number | undefined): Promise<void> {
    const response = await bridge.getDiagnostics(uri);
    const diagnostics = responseData<Diagnostic[]>(response, []);

    connection.sendDiagnostics({
        uri,
        version,
        diagnostics: diagnostics.map(normalizeDiagnostic),
    });
}

function normalizeDiagnostic(diagnostic: Diagnostic): Diagnostic {
    if (diagnostic.severity === undefined) {
        return {
            ...diagnostic,
            severity: 1 as DiagnosticSeverity,
        };
    }

    return diagnostic;
}

function buildWorkspaceEdit(locations: Location[], newName: string): WorkspaceEdit {
    const changes: Record<string, { range: Range; newText: string }[]> = {};

    for (const location of locations) {
        if (!changes[location.uri]) {
            changes[location.uri] = [];
        }

        changes[location.uri].push({
            range: location.range,
            newText: newName,
        });
    }

    return { changes };
}

function applyContentChanges(text: string, contentChanges: TextDocumentContentChangeEvent[]): string {
    let currentText = text;

    for (const change of contentChanges) {
        if (change.range === undefined) {
            currentText = change.text;
            continue;
        }

        const startOffset = positionToOffset(currentText, change.range.start);
        const endOffset = positionToOffset(currentText, change.range.end);
        currentText = currentText.slice(0, startOffset) + change.text + currentText.slice(endOffset);
    }

    return currentText;
}

function positionToOffset(text: string, position: Position): number {
    let offset = 0;
    let currentLine = 0;

    while (offset < text.length && currentLine < position.line) {
        const code = text.charCodeAt(offset);
        offset += 1;

        if (code === 13) {
            if (offset < text.length && text.charCodeAt(offset) === 10) {
                offset += 1;
            }
            currentLine += 1;
        } else if (code === 10) {
            currentLine += 1;
        }
    }

    return Math.min(offset + position.character, text.length);
}

function resolveDefaultServerBaseUrl(): string {
    try {
        return new URL('./', self.location.href).toString();
    } catch {
        return '';
    }
}
