/// <reference lib="webworker" />

type WasmResponse<T> = {
    success: boolean;
    data?: T;
    error?: string;
};

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

declare const self: DedicatedWorkerGlobalScope & {
    createZrLanguageServerModule?: (options: {
        locateFile?: (path: string) => string;
    }) => Promise<EmscriptenModule>;
};

const encoder = new TextEncoder();

function byteLength(text: string): number {
    return encoder.encode(text).byteLength;
}

function normalizeBaseUrl(baseUrl: string): string {
    return baseUrl.endsWith('/') ? baseUrl : `${baseUrl}/`;
}

export class ZrWasmBridge {
    private modulePromise: Promise<EmscriptenModule> | undefined;
    private module: EmscriptenModule | undefined;
    private contextPointer = 0;
    private baseUrl = '';

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

    async updateDocument(uri: string, text: string, version: number): Promise<WasmResponse<Record<string, boolean>>> {
        return this.invoke<Record<string, boolean>>(
            'wasm_ZrLspUpdateDocument',
            ['number', 'string', 'number', 'string', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), text, byteLength(text), version],
        );
    }

    async closeDocument(uri: string): Promise<WasmResponse<Record<string, boolean>>> {
        return this.invoke<Record<string, boolean>>(
            'wasm_ZrLspCloseDocument',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    async getDiagnostics(uri: string): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetDiagnostics',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    async getCompletion(uri: string, line: number, character: number): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetCompletion',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    async getHover(uri: string, line: number, character: number): Promise<WasmResponse<unknown>> {
        return this.invoke<unknown>(
            'wasm_ZrLspGetHover',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    async getRichHover(uri: string, line: number, character: number): Promise<WasmResponse<unknown>> {
        return this.invoke<unknown>(
            'wasm_ZrLspGetRichHover',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    async getDefinition(uri: string, line: number, character: number): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetDefinition',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    async findReferences(
        uri: string,
        line: number,
        character: number,
        includeDeclaration: boolean,
    ): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspFindReferences',
            ['number', 'string', 'number', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character, includeDeclaration ? 1 : 0],
        );
    }

    async rename(
        uri: string,
        line: number,
        character: number,
        newName: string,
    ): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
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

    async getDocumentSymbols(uri: string): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetDocumentSymbols',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    async getInlayHints(
        uri: string,
        startLine: number,
        startCharacter: number,
        endLine: number,
        endCharacter: number,
    ): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetInlayHints',
            ['number', 'string', 'number', 'number', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), startLine, startCharacter, endLine, endCharacter],
        );
    }

    async getWorkspaceSymbols(query: string): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetWorkspaceSymbols',
            ['number', 'string', 'number'],
            [await this.context(), query, byteLength(query)],
        );
    }

    async getNativeDeclarationDocument(uri: string): Promise<WasmResponse<string>> {
        return this.invoke<string>(
            'wasm_ZrLspGetNativeDeclarationDocument',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    async getProjectModules(projectUri: string): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetProjectModules',
            ['number', 'string', 'number'],
            [await this.context(), projectUri, byteLength(projectUri)],
        );
    }

    async getDocumentHighlights(uri: string, line: number, character: number): Promise<WasmResponse<unknown[]>> {
        return this.invoke<unknown[]>(
            'wasm_ZrLspGetDocumentHighlights',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

    async getSemanticTokens(uri: string): Promise<WasmResponse<unknown>> {
        return this.invoke<unknown>(
            'wasm_ZrLspGetSemanticTokens',
            ['number', 'string', 'number'],
            [await this.context(), uri, byteLength(uri)],
        );
    }

    async prepareRename(uri: string, line: number, character: number): Promise<WasmResponse<unknown>> {
        return this.invoke<unknown>(
            'wasm_ZrLspPrepareRename',
            ['number', 'string', 'number', 'number', 'number'],
            [await this.context(), uri, byteLength(uri), line, character],
        );
    }

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

    private async context(): Promise<number> {
        if (this.contextPointer === 0) {
            await this.initialize(this.baseUrl || new URL('./', self.location.href).toString());
        }
        return this.contextPointer;
    }

    private async invoke<T>(
        name: string,
        argTypes: string[],
        args: unknown[],
    ): Promise<WasmResponse<T>> {
        await this.context();

        if (this.module === undefined) {
            throw new Error(`WASM module is not initialized for ${name}.`);
        }

        const pointer = this.module.ccall(name, 'number', argTypes, args);
        if (!pointer) {
            return {
                success: false,
                error: `${name} returned a null response pointer.`,
            };
        }

        const rawResponse = this.module.UTF8ToString(pointer);
        this.module._free(pointer);
        return JSON.parse(rawResponse) as WasmResponse<T>;
    }
}
