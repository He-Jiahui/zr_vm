/// <reference lib="webworker" />

import { ErrorCodes, ResponseError } from 'vscode-languageserver/browser';
import type {
    CodeAction, CodeLens, CompletionItem, Diagnostic, DocumentHighlight,
    DocumentLink, FoldingRange, Hover, InlayHint, Location, PrepareRenameResult,
    SelectionRange, SemanticTokens, SymbolInformation, TextEdit,
} from 'vscode-languageserver/browser';

/** C WASM 导出统一返回的 JSON 封装；调用方仍需用 responseData 判错并取出业务载荷。 */
export type WasmResponse<T> =
    | { success: true; data: T }
    | { success: false; code: number; error: string; data?: unknown };

/** 内容身份由后端生成，推送和拉取诊断均按它判定结果是否相同。 */
type DiagnosticReport = { resultId: string; items: Diagnostic[] };
/** 工作区诊断导出保留在桥上，但 Web worker 未注册对应的 LSP 请求。 */
type WorkspaceDiagnosticReport = DiagnosticReport & { uri: string; version: number | null };

/** 此接口只描述 ccall、解码和响应内存所有权；模块工厂负责 WASM 资产定位。 */
type EmscriptenModule = {
    ccall: (
        name: string,
        returnType: string | null,
        argTypes: string[],
        args: unknown[],
    ) => number;
    UTF8ToString: (pointer: number) => string;
    _free: (pointer: number) => void;
};

/** 工厂由同目录的 Emscripten 脚本通过 importScripts 安装到当前 worker。 */
declare const self: DedicatedWorkerGlobalScope & {
    createZrLanguageServerModule?: (options: {
        locateFile?: (path: string) => string;
    }) => Promise<EmscriptenModule>;
};

// WASM C API 的 URI、文本与标识符长度以 UTF-8 字节计算，LSP 位置仍使用 UTF-16。
const encoder = new TextEncoder();

/** 保持指针相邻的显式长度与 TextEncoder 向 WASM 提供的 UTF-8 内容一致。 */
function byteLength(text: string): number {
    return encoder.encode(text).byteLength;
}

/** 资产根目录必须以斜线结束，避免 URL 相对解析把目录名当作文件名。 */
function normalizeBaseUrl(baseUrl: string): string {
    return baseUrl.endsWith('/') ? baseUrl : `${baseUrl}/`;
}

/**
 * 为单个浏览器 LSP worker 管理一个 Emscripten 模块与 C 服务端上下文。
 * 文档队列和快照由 DocumentSyncStore 负责；此层只转译导出 ABI、响应及资源寿命。
 */
export class ZrWasmBridge {
    // 模块装载由 Promise 合并；上下文指针只在成功创建后供所有导出共享。
    private modulePromise: Promise<EmscriptenModule> | undefined;
    private module: EmscriptenModule | undefined;
    private contextPointer = 0;
    private baseUrl = '';

    /** LSP initialize 入口先加载脚本和 WASM，再创建用于后续所有请求的 C 上下文。 */
    async initialize(baseUrl: string): Promise<void> {
        if (this.modulePromise === undefined) {
            this.baseUrl = normalizeBaseUrl(baseUrl);
            this.modulePromise = this.loadModule(this.baseUrl);
        }

        this.module = await this.modulePromise;
        if (this.contextPointer === 0) {
            this.contextPointer = this.module.ccall(
                'wasm_ZrLspContextNew',
                'number',
                [],
                [],
            );
        }

        if (this.contextPointer === 0) {
            throw new Error('Failed to create Zr WASM LSP context.');
        }
    }

    /** LSP shutdown/exit 释放当前 C 上下文；模块本体随 DedicatedWorker 退出回收。 */
    dispose(): void {
        if (this.module !== undefined && this.contextPointer !== 0) {
            this.module.ccall(
                'wasm_ZrLspContextFree',
                null,
                ['number'],
                [this.contextPointer],
            );
            this.contextPointer = 0;
        }
    }

    /** DocumentSyncStore 提交完整文本时调用；长度必须是 UTF-8 字节，版本是已验证的非负整数。 */
    async updateDocument(uri: string, text: string, version: number): Promise<WasmResponse<Record<string, boolean>>> {
        return this.invoke<Record<string, boolean>>(
            'wasm_ZrLspUpdateDocument',
            ['number', 'string', 'number', 'string', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), text, byteLength(text), version],
        );
    }

    /** 与同 URI 的更新排队后从 C 上下文移除文档；关闭通知立即由上层撤销快照。 */
    async closeDocument(uri: string): Promise<WasmResponse<Record<string, boolean>>> {
        return this.invoke<Record<string, boolean>>(
            'wasm_ZrLspCloseDocument',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 原始诊断导出保留给桥接调用，但 worker 当前使用带 resultId 的报告接口。 */
    async getDiagnostics(uri: string): Promise<WasmResponse<Diagnostic[]>> {
        return this.invoke<Diagnostic[]>(
            'wasm_ZrLspGetDiagnostics',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 补全查询由 worker 快照围栏保护；位置沿 LSP 的 UTF-16 行列原样传递。 */
    async getCompletion(uri: string, line: number, character: number): Promise<WasmResponse<CompletionItem[]>> {
        return this.invoke<CompletionItem[]>(
            'wasm_ZrLspGetCompletion',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 常规 hover 复用上下文索引；无内容时由 worker 保留 null 语义。 */
    async getHover(uri: string, line: number, character: number): Promise<WasmResponse<Hover | null>> {
        return this.invoke<Hover | null>(
            'wasm_ZrLspGetHover',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 同一后端报告供推送诊断和 LSP 拉取诊断共用其 resultId。 */
    async getDiagnosticReport(uri: string): Promise<WasmResponse<DiagnosticReport>> {
        return this.invoke<DiagnosticReport>(
            'wasm_ZrLspGetDiagnosticReport',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 导出存在但 Web 能力未宣告工作区诊断，当前 worker 不调用它。 */
    async getWorkspaceDiagnosticReports(): Promise<WasmResponse<WorkspaceDiagnosticReport[]>> {
        return this.invoke<WorkspaceDiagnosticReport[]>(
            'wasm_ZrLspGetWorkspaceDiagnosticReports',
            ['number'],
            [await this.context()],
        );
    }

    /** 扩展富悬浮私有请求的桥接入口；载荷由扩展消费而非 LSP 标准 Hover。 */
    async getRichHover(uri: string, line: number, character: number): Promise<WasmResponse<unknown>> {
        return this.invoke<unknown>(
            'wasm_ZrLspGetRichHover',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 定义导航只返回位置，worker 不为它创建客户端编辑。 */
    async getDefinition(uri: string, line: number, character: number): Promise<WasmResponse<Location[]>> {
        return this.invoke<Location[]>(
            'wasm_ZrLspGetDefinition',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 引用请求保留客户端的 includeDeclaration 选择并映射为 C 整数标志。 */
    async findReferences(
        uri: string,
        line: number,
        character: number,
        includeDeclaration: boolean,
    ): Promise<WasmResponse<Location[]>> {
        return this.invoke<Location[]>(
            'wasm_ZrLspFindReferences',
            ['number', 'string', 'number', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character, includeDeclaration ? 1 : 0],
        );
    }

    /** 后端返回重命名位置；worker 才把这些位置组装成 LSP WorkspaceEdit。 */
    async rename(
        uri: string,
        line: number,
        character: number,
        newName: string,
    ): Promise<WasmResponse<Location[]>> {
        return this.invoke<Location[]>(
            'wasm_ZrLspRename',
            ['number', 'string', 'number', 'number', 'number', 'string', 'number'],
            [
                await this.context(),
                uri,
                byteLength(uri),
                line,
                character,
                newName,
                byteLength(newName),
            ],
        );
    }

    /** 文档符号基于已同步文档索引，结果无需再经过 resolve。 */
    async getDocumentSymbols(uri: string): Promise<WasmResponse<SymbolInformation[]>> {
        return this.invoke<SymbolInformation[]>(
            'wasm_ZrLspGetDocumentSymbols',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 区间原样传给 C 导出；上层只宣告可直接显示的 hint，不宣告 resolve。 */
    async getInlayHints(
        uri: string,
        startLine: number,
        startCharacter: number,
        endLine: number,
        endCharacter: number,
    ): Promise<WasmResponse<InlayHint[]>> {
        return this.invoke<InlayHint[]>(
            'wasm_ZrLspGetInlayHints',
            ['number', 'string', 'number', 'number', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), startLine, startCharacter, endLine, endCharacter],
        );
    }

    /** C 导出保留，Web worker 当前未声明 workspace/symbol，因此无客户端调用。 */
    async getWorkspaceSymbols(query: string): Promise<WasmResponse<SymbolInformation[]>> {
        return this.invoke<SymbolInformation[]>(
            'wasm_ZrLspGetWorkspaceSymbols',
            ['number', 'string', 'number'],
            [await this.context(), query, byteLength(query)],
        );
    }

    /** 扩展虚拟文档提供者直接请求声明文本，无需已有编辑器文档快照。 */
    async getNativeDeclarationDocument(uri: string): Promise<WasmResponse<string>> {
        return this.invoke<string>(
            'wasm_ZrLspGetNativeDeclarationDocument',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** C 导出保留，但 Web worker 当前未注册项目模块私有请求。 */
    async getProjectModules(projectUri: string): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetProjectModules',
            ['number', 'string', 'number'],
            [await this.context(), projectUri, byteLength(projectUri)],
        );
    }

    /** 当前文档内引用高亮受 worker 快照围栏约束。 */
    async getDocumentHighlights(uri: string, line: number, character: number): Promise<WasmResponse<DocumentHighlight[]>> {
        return this.invoke<DocumentHighlight[]>(
            'wasm_ZrLspGetDocumentHighlights',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 全量 token 编码与 worker 宣告的固定 legend 成对使用。 */
    async getSemanticTokens(uri: string): Promise<WasmResponse<SemanticTokens | null>> {
        return this.invoke<SemanticTokens | null>(
            'wasm_ZrLspGetSemanticTokens',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 编辑前由客户端探测可重命名范围；无目标时保留 null。 */
    async prepareRename(uri: string, line: number, character: number): Promise<WasmResponse<PrepareRenameResult | null>> {
        return this.invoke<PrepareRenameResult | null>(
            'wasm_ZrLspPrepareRename',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 整文档格式化返回文本编辑；应用与版本协调由 LSP 客户端完成。 */
    async getFormatting(uri: string): Promise<WasmResponse<TextEdit[]>> {
        return this.invoke<TextEdit[]>(
            'wasm_ZrLspGetFormatting',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 区间格式化将 LSP 区间交给 C 导出，调用前须有可读快照。 */
    async getRangeFormatting(
        uri: string,
        startLine: number,
        startCharacter: number,
        endLine: number,
        endCharacter: number,
    ): Promise<WasmResponse<TextEdit[]>> {
        return this.invoke<TextEdit[]>(
            'wasm_ZrLspGetRangeFormatting',
            ['number', 'string', 'number', 'number', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), startLine, startCharacter, endLine, endCharacter],
        );
    }

    /** C 导出生成候选动作；worker 再按 context.only 筛选其层级 kind。 */
    async getCodeActions(
        uri: string,
        startLine: number,
        startCharacter: number,
        endLine: number,
        endCharacter: number,
    ): Promise<WasmResponse<CodeAction[]>> {
        return this.invoke<CodeAction[]>(
            'wasm_ZrLspGetCodeActions',
            ['number', 'string', 'number', 'number', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), startLine, startCharacter, endLine, endCharacter],
        );
    }

    /** 折叠区间取自已同步文档，无法读取的虚拟文档由 worker 返回空值。 */
    async getFoldingRanges(uri: string): Promise<WasmResponse<FoldingRange[]>> {
        return this.invoke<FoldingRange[]>(
            'wasm_ZrLspGetFoldingRanges',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 单个位置生成选择层级，多个位置由 worker 保持输入顺序聚合。 */
    async getSelectionRange(uri: string, line: number, character: number): Promise<WasmResponse<SelectionRange[]>> {
        return this.invoke<SelectionRange[]>(
            'wasm_ZrLspGetSelectionRange',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    /** 链接目标应已在基础响应中给齐；Web worker 不声明后续 resolve。 */
    async getDocumentLinks(uri: string): Promise<WasmResponse<DocumentLink[]>> {
        return this.invoke<DocumentLink[]>(
            'wasm_ZrLspGetDocumentLinks',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** C 导出返回含项目命令的 lens；worker 移除 Web 无法执行的命令。 */
    async getCodeLens(uri: string): Promise<WasmResponse<CodeLens[]>> {
        return this.invoke<CodeLens[]>(
            'wasm_ZrLspGetCodeLens',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    /** 将扩展 out/web 的 JS 与 WASM 资产绑定为同一个可解析的 URL 根目录。 */
    private async loadModule(baseUrl: string): Promise<EmscriptenModule> {
        const scriptUrl = new URL('zr_vm_language_server.js', baseUrl).toString();

        if (typeof self.createZrLanguageServerModule !== 'function') {
            try {
                self.importScripts(scriptUrl);
            } catch (error) {
                throw new Error(`Failed to load WASM language server script from ${scriptUrl}: ${String(error)}`);
            }
        }

        if (typeof self.createZrLanguageServerModule !== 'function') {
            throw new Error(`Emscripten module factory was not loaded from ${scriptUrl}.`);
        }

        try {
            return await self.createZrLanguageServerModule({
                locateFile: (assetPath: string) => new URL(assetPath, baseUrl).toString(),
            });
        } catch (error) {
            throw new Error(`Failed to initialize WASM language server from ${baseUrl}: ${String(error)}`);
        }
    }

    /** 导出调用的统一前提：上下文必须已创建或可由初始化路径创建。 */
    private async context(): Promise<number> {
        if (this.contextPointer === 0) {
            await this.initialize(this.baseUrl || new URL('./', self.location.href).toString());
        }
        return this.contextPointer;
    }

    /** 调用 C JSON 导出并在解码后释放响应指针；业务成功与失败由上层 responseData 判断。 */
    private async invoke<T>(
        name: string,
        argTypes: string[],
        args: unknown[],
    ): Promise<WasmResponse<T>> {
        await this.context();

        if (this.module === undefined) {
            throw new Error(`WASM module is not initialized for ${name}.`);
        }

        let pointer = 0;
        try {
            pointer = this.module.ccall(name, 'number', argTypes, args);
            if (!pointer) {
                throw new ResponseError(ErrorCodes.InternalError, `${name} returned a null response pointer.`);
            }
            return JSON.parse(this.module.UTF8ToString(pointer)) as WasmResponse<T>;
        } catch (error) {
            if (error instanceof ResponseError) {
                throw error;
            }
            throw new ResponseError(ErrorCodes.InternalError, `${name} response decoding failed: ${String(error)}`);
        } finally {
            if (pointer) {
                this.module._free(pointer);
            }
        }
    }
}
