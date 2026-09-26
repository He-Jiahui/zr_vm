/// <reference lib="webworker" />

import {
    BrowserMessageReader,
    BrowserMessageWriter,
    createConnection,
    ResponseError,
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
    type PrepareRenameResult,
    type Range,
    type SemanticTokens,
    type SemanticTokensLegend,
    type SymbolInformation,
    type TextDocumentContentChangeEvent,
    type WorkspaceEdit,
} from 'vscode-languageserver/browser';
import { ZrWasmBridge } from './wasm-bridge';
import { DocumentSyncStore, contentModified } from './document-sync';
import { responseData } from './wasm-response';
import { LSPErrorCodes } from 'vscode-languageserver/browser';

declare const self: DedicatedWorkerGlobalScope;

const connection = createConnection(
    new BrowserMessageReader(self),
    new BrowserMessageWriter(self),
);
const bridge = new ZrWasmBridge();
const documents = new DocumentSyncStore(bridge);
const publishedDiagnosticResultIds = new Map<string, { resultId: string; version: number; generation: number }>();
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
        'decorator',
        'metaMethod',
    ],
    tokenModifiers: ['declaration'],
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
            documentHighlightProvider: true,
            documentFormattingProvider: true,
            documentRangeFormattingProvider: true,
            codeActionProvider: {
                codeActionKinds: ['quickfix', 'source.organizeImports', 'source.removeUnused'],
                resolveProvider: false,
            },
            foldingRangeProvider: true,
            selectionRangeProvider: true,
            documentLinkProvider: {
                resolveProvider: false,
            },
            codeLensProvider: {
                resolveProvider: false,
            },
            diagnosticProvider: {
                interFileDependencies: false,
                workspaceDiagnostics: false,
            },
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
    if (await documents.open(textDocument.uri, textDocument.text, textDocument.version)) {
        await publishDiagnostics(textDocument.uri);
    }
});

connection.onDidChangeTextDocument(async ({ textDocument, contentChanges }) => {
    if (await documents.change(textDocument.uri, textDocument.version, contentChanges)) {
        await publishDiagnostics(textDocument.uri);
    }
});

connection.onDidCloseTextDocument(async ({ textDocument }) => {
    const closing = documents.close(textDocument.uri);
    publishedDiagnosticResultIds.delete(textDocument.uri);
    connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [] });
    await closing;
});

connection.onDidSaveTextDocument(async ({ textDocument }) => {
    // didSave never replaces the synchronized editor snapshot, even when it includes text.
    await publishDiagnostics(textDocument.uri);
});

connection.onCompletion(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getCompletion(textDocument.uri, position.line, position.character));
    return responseData<CompletionItem[]>(response, []);
});

connection.onHover(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getHover(textDocument.uri, position.line, position.character));
    return responseData<Hover | null>(response, null);
});

connection.onRequest('zr/richHover', async ({ textDocument, position }: {
    textDocument: { uri: string }; position: Position;
}) => {
    const response = await queryDocument(textDocument.uri, () =>
        bridge.getRichHover(textDocument.uri, position.line, position.character));
    return responseData<unknown | null>(response, null);
});

connection.onDefinition(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getDefinition(textDocument.uri, position.line, position.character));
    return responseData<Location[]>(response, []);
});

connection.onReferences(async ({ textDocument, position, context }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.findReferences(
        textDocument.uri,
        position.line,
        position.character,
        context.includeDeclaration,
    ));
    return responseData<Location[]>(response, []);
});

connection.onDocumentSymbol(async ({ textDocument }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getDocumentSymbols(textDocument.uri));
    return responseData<SymbolInformation[]>(response, []);
});

connection.onRequest('textDocument/inlayHint', async ({ textDocument, range }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getInlayHints(
        textDocument.uri,
        range.start.line,
        range.start.character,
        range.end.line,
        range.end.character,
    ));
    return responseData<InlayHint[]>(response, []);
});

connection.onRequest('zr/nativeDeclarationDocument', async ({ uri }: { uri: string }) => {
    const response = await bridge.getNativeDeclarationDocument(uri);
    return responseData<string | null>(response, null);
});

connection.onDocumentHighlight(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getDocumentHighlights(textDocument.uri, position.line, position.character));
    return responseData<DocumentHighlight[]>(response, []);
});

connection.onRequest('textDocument/semanticTokens/full', async ({ textDocument }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getSemanticTokens(textDocument.uri));
    return responseData<SemanticTokens | null>(response, null);
});

connection.onPrepareRename(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.prepareRename(textDocument.uri, position.line, position.character));
    return responseData<PrepareRenameResult | null>(response, null);
});

connection.onRenameRequest(async ({ textDocument, position, newName }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.rename(textDocument.uri, position.line, position.character, newName));
    const locations = responseData<Location[] | null>(response, null);
    if (locations === null) {
        return null;
    }

    return buildWorkspaceEdit(locations, newName);
});

connection.onRequest('textDocument/formatting', async ({ textDocument }: { textDocument: { uri: string } }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getFormatting(textDocument.uri));
    return responseData<unknown[]>(response, []);
});

connection.onRequest('textDocument/rangeFormatting', async ({
    textDocument,
    range,
}: {
    textDocument: { uri: string };
    range: Range;
}) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getRangeFormatting(
        textDocument.uri,
        range.start.line,
        range.start.character,
        range.end.line,
        range.end.character,
    ));
    return responseData<unknown[]>(response, []);
});

connection.onRequest('textDocument/codeAction', async ({
    textDocument,
    range,
    context,
}: {
    textDocument: { uri: string };
    range: Range;
    context?: { only?: string[] };
}) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getCodeActions(
        textDocument.uri,
        range.start.line,
        range.start.character,
        range.end.line,
        range.end.character,
    ));
    return filterCodeActions(responseData<unknown[]>(response, []), context?.only);
});

connection.onRequest('textDocument/foldingRange', async ({ textDocument }: { textDocument: { uri: string } }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getFoldingRanges(textDocument.uri));
    return responseData<unknown[]>(response, []);
});

connection.onRequest('textDocument/selectionRange', async ({
    textDocument,
    positions,
}: {
    textDocument: { uri: string };
    positions: Position[];
}) => {
    if (isVirtualDocumentUri(textDocument.uri)) {
        return positions.map(() => null);
    }
    return queryDocument(textDocument.uri, async () => {
    const ranges: unknown[] = [];
    for (const position of positions) {
        const response = await queryDocument(textDocument.uri, () => bridge.getSelectionRange(textDocument.uri, position.line, position.character));
        const data = responseData<unknown[]>(response, []);
        ranges.push(data[0] ?? null);
    }
    return ranges;
    });
});

connection.onRequest('textDocument/documentLink', async ({ textDocument }: { textDocument: { uri: string } }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getDocumentLinks(textDocument.uri));
    return responseData<unknown[]>(response, []);
});

connection.onRequest('textDocument/codeLens', async ({ textDocument }: { textDocument: { uri: string } }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getCodeLens(textDocument.uri));
    const lenses = responseData<{ command?: { command?: string } }[]>(response, []);
    return lenses.filter(lens => lens.command?.command !== 'zr.runCurrentProject' &&
        lens.command?.command !== 'zr.debugCurrentProject');
});

connection.onRequest('textDocument/diagnostic', async ({
    textDocument,
    previousResultId,
}: {
    textDocument: { uri: string };
    previousResultId?: string;
}) => getDocumentDiagnosticReport(textDocument.uri, previousResultId));

connection.listen();

async function queryDocument<T>(uri: string, query: () => Promise<T>): Promise<T> {
    // Decompiled documents are editor-only projections. They have no source
    // snapshot in the WASM project index, so feature requests resolve through
    // the normal empty-result envelopes without touching the backend.
    if (isVirtualDocumentUri(uri)) {
        return { success: true, data: null } as T;
    }
    const result = await documents.read(uri, query);
    if (!documents.isCurrent(result.token)) { throw contentModified(); }
    return result.value;
}

async function publishDiagnostics(uri: string): Promise<void> {
    if (isVirtualDocumentUri(uri)) { return; }
    try {
        const { value: response, token } = await documents.read(uri, () => bridge.getDiagnosticReport(uri));
        const report = responseData<{ resultId: string; items: Diagnostic[] }>(response, { resultId: '', items: [] });
        if (!documents.isCurrent(token)) { return; }
        const published = publishedDiagnosticResultIds.get(uri);
        if (published?.resultId === report.resultId && published.version === token.version &&
            published.generation === token.generation) { return; }
        connection.sendDiagnostics({ uri, version: token.version, diagnostics: report.items.map(normalizeDiagnostic) });
        publishedDiagnosticResultIds.set(uri, { resultId: report.resultId, version: token.version, generation: token.generation });
    } catch (error) {
        if (!(error instanceof ResponseError && error.code === LSPErrorCodes.ContentModified)) {
            console.error('[zr-web-worker] diagnostics failed:', uri, error);
        }
    }
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

async function getDocumentDiagnosticReport(uri: string, previousResultId: string | undefined): Promise<unknown> {
    if (isVirtualDocumentUri(uri)) {
        return {
            kind: previousResultId === '' ? 'unchanged' : 'full',
            resultId: '',
            ...(previousResultId === '' ? {} : { items: [] }),
        };
    }
    const response = await queryDocument(uri, () => bridge.getDiagnosticReport(uri));
    const report = responseData<{ resultId: string; items: Diagnostic[] }>(response, {
        resultId: '',
        items: [],
    });
    const diagnostics = report.items.map(normalizeDiagnostic);
    const resultId = report.resultId;

    if (previousResultId === resultId) {
        return {
            kind: 'unchanged',
            resultId,
        };
    }

    return {
        kind: 'full',
        resultId,
        items: diagnostics,
    };
}

function isVirtualDocumentUri(uri: string): boolean {
    return uri.startsWith('zr-decompiled:');
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

function filterCodeActions(actions: unknown[], only: string[] | undefined): unknown[] {
    if (!only || only.length === 0) {
        return actions;
    }

    return actions.filter((action) => {
        if (!isObject(action)) {
            return false;
        }
        const kind = action.kind;
        return typeof kind === 'string' && only.some((requested) => codeActionKindMatches(kind, requested));
    });
}

function codeActionKindMatches(actionKind: string, requestedKind: string): boolean {
    return actionKind === requestedKind || actionKind.startsWith(`${requestedKind}.`);
}

function isObject(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null;
}

function resolveDefaultServerBaseUrl(): string {
    try {
        return new URL('./', self.location.href).toString();
    } catch {
        return '';
    }
}
