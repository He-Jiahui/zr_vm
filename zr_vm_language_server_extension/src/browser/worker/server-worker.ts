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

// 一个 DedicatedWorker 对应一个 LSP 连接与一个 WASM 上下文；浏览器扩展重启时重建整个 worker。
const connection = createConnection(
    new BrowserMessageReader(self),
    new BrowserMessageWriter(self),
);
const bridge = new ZrWasmBridge();
const documents = new DocumentSyncStore(bridge);
// 推送诊断缓存同时绑定编辑器版本与打开代际，不能仅凭后端 resultId 去重。
const publishedDiagnosticResultIds = new Map<string, { resultId: string; version: number; generation: number }>();
// Web 的 token 编号必须与 WASM 序列化器及原生服务端使用的词法类别顺序一致。
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

// LSP 关闭与进程退出分两步；退出通知最终关闭 worker。
// TODO: shutdownRequested 目前只写不读；核查协议库在 shutdown 与 exit 之间是否拦截
// 既有文档请求，否则 ZrWasmBridge.context() 可能重新创建已释放的 C 上下文。
let shutdownRequested = false;
let serverBaseUrl = '';

// 宿主保留 worker 顶层异常信息，供无法沿请求通道返回的失败排查。
self.addEventListener('error', (event) => {
    console.error('[zr-web-worker] Unhandled worker error:', event.message, event.error);
});

self.addEventListener('unhandledrejection', (event) => {
    console.error('[zr-web-worker] Unhandled promise rejection:', event.reason);
});

// 客户端先交付与扩展 out/web 对应的资源基址，再发布 WASM 实际可响应的能力。
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

// shutdown 释放 C 上下文；exit 额外终止当前 DedicatedWorker。
connection.onShutdown(() => {
    shutdownRequested = true;
    bridge.dispose();
});

connection.onNotification('exit', () => {
    bridge.dispose();
    self.close();
});

// 通知先进入按 URI 排队的快照层；只有后端接纳的版本才发布新诊断。
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
    // 立即撤销推送缓存与编辑器显示，后端 close 仍按先前更新排队。
    const closing = documents.close(textDocument.uri);
    publishedDiagnosticResultIds.delete(textDocument.uri);
    connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [] });
    await closing;
});

connection.onDidSaveTextDocument(async ({ textDocument }) => {
    // didSave never replaces the synchronized editor snapshot, even when it includes text.
    await publishDiagnostics(textDocument.uri);
});

// 文档功能共用 queryDocument 围栏：查询返回时必须仍是同一次已同步快照。
connection.onCompletion(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getCompletion(textDocument.uri, position.line, position.character));
    return responseData<CompletionItem[]>(response, []);
});

connection.onHover(async ({ textDocument, position }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getHover(textDocument.uri, position.line, position.character));
    return responseData<Hover | null>(response, null);
});

// 富悬浮与虚拟声明是扩展私有入口；只有真实文档请求走快照围栏。
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

// 后端返回引用位置，编辑器协议要求 worker 将它们组装为跨 URI 的 WorkspaceEdit。
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

// WASM 返回完整动作集合；LSP context.only 的层级筛选留在协议边界执行。
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

// 每个位置单独查询 WASM；外层围栏保证组合结果来自同一文档修订。
connection.onRequest('textDocument/selectionRange', async ({
    textDocument,
    positions,
}: {
    textDocument: { uri: string };
    positions: Position[];
}) => {
    if (isVirtualDocumentUri(textDocument.uri)) {
        // BUG: zr-decompiled 的 .zr 文档会匹配 Web 客户端的 selectionRangeProvider；
        // 返回的 null 元素被 vscode-languageclient 的 asSelectionRanges 当作 SelectionRange.range
        // 解引用，触发 TypeError。需改为协议和转换器均可接受的空结果语义。
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

// Web 宿主没有原生项目运行器，不能把相关 CodeLens 命令交给客户端。
connection.onRequest('textDocument/codeLens', async ({ textDocument }: { textDocument: { uri: string } }) => {
    const response = await queryDocument(textDocument.uri, () => bridge.getCodeLens(textDocument.uri));
    const lenses = responseData<{ command?: { command?: string } }[]>(response, []);
    return lenses.filter(lens => lens.command?.command !== 'zr.runCurrentProject' &&
        lens.command?.command !== 'zr.debugCurrentProject');
});

// 拉取诊断以 C 后端 resultId 作身份；推送缓存另行纳入编辑器版本和代际。
connection.onRequest('textDocument/diagnostic', async ({
    textDocument,
    previousResultId,
}: {
    textDocument: { uri: string };
    previousResultId?: string;
}) => getDocumentDiagnosticReport(textDocument.uri, previousResultId));

// 全部能力路由注册完毕后才接收客户端消息，避免首个请求遇到未安装的 handler。
connection.listen();

/** 仅让已同步的真实文档查询穿过 WASM；异步完成后复查快照身份。 */
async function queryDocument<T>(uri: string, query: () => Promise<T>): Promise<T> {
    // 反编译文档通常没有编辑器快照；通用查询当前给它们返回空封装。
    // BUG: C 后端专门支持 zr-decompiled 的 Definition 与 DocumentLink，
    // 但这条短路使 Web 客户端导航模块声明和打开模块链接时永远得到空结果。
    // 见 lsp_interface.c 虚拟声明定位与 lsp_document_links.c 虚拟模块链接分支。
    if (isVirtualDocumentUri(uri)) {
        return { success: true, data: null } as T;
    }
    const result = await documents.read(uri, query);
    if (!documents.isCurrent(result.token)) { throw contentModified(); }
    return result.value;
}

/** 通知路径的尽力发布：后端失败只记录日志，过期报告必须静默丢弃。 */
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

/** 后端未填 severity 时保持编辑器可见的错误级别，与原生报告默认值对齐。 */
function normalizeDiagnostic(diagnostic: Diagnostic): Diagnostic {
    if (diagnostic.severity === undefined) {
        return {
            ...diagnostic,
            severity: 1 as DiagnosticSeverity,
        };
    }

    return diagnostic;
}

/** 将后端内容身份映射为 LSP full/unchanged；虚拟文档固定为空报告。 */
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

/** 识别仅供编辑器展示的反编译文档，它们没有 WASM 索引快照。 */
function isVirtualDocumentUri(uri: string): boolean {
    return uri.startsWith('zr-decompiled:');
}

/** 将 WASM 的跨文件重命名位置转换为客户端可应用的按 URI 分组编辑。 */
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

/** 实现 LSP kind 层级筛选，并拒绝无法辨识 kind 的响应项。 */
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

/** `source` 等父 kind 可以请求其子 kind，精确 kind 也必须匹配。 */
function codeActionKindMatches(actionKind: string, requestedKind: string): boolean {
    return actionKind === requestedKind || actionKind.startsWith(`${requestedKind}.`);
}

/** 仅在读取不可信 WASM 动作载荷的 kind 前做结构筛选。 */
function isObject(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null;
}

/** 初始化选项缺失时尝试从 worker 脚本目录推断 WASM 资产目录。 */
function resolveDefaultServerBaseUrl(): string {
    // BUG: browser.ts 通过 Blob URL 启动 worker；该 URL 无法解析 './'，回退 '' 经
    // normalizeBaseUrl 变为 '/'，loadModule 的 new URL(script, '/') 会抛错。
    // 可用无 serverBaseUrl 的 initialize 请求触发；需由宿主提供可解析的资产 URI。
    try {
        return new URL('./', self.location.href).toString();
    } catch {
        return '';
    }
}
