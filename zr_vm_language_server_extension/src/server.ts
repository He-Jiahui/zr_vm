//
// WASM 服务器包装 - 封装 LSP 接口到 WASM 调用
//

import { WasmLoader } from './wasm-loader';
import * as vscode from 'vscode';

export interface LspPosition {
    line: number;
    character: number;
}

export interface LspRange {
    start: LspPosition;
    end: LspPosition;
}

export interface LspLocation {
    uri: string;
    range: LspRange;
}

export interface LspDiagnostic {
    range: LspRange;
    severity: number;
    code?: string;
    message: string;
}

export interface LspCompletionItem {
    label: string;
    kind: number;
    detail?: string;
    documentation?: string | { kind: string; value: string };
    insertText?: string;
    insertTextFormat?: string;
}

export interface LspHover {
    contents: Array<{ kind: string; value: string }>;
    range?: LspRange;
}

export class WasmLanguageServer {
    private wasmLoader: WasmLoader;
    private context: number | null = null;
    
    constructor(wasmLoader: WasmLoader) {
        this.wasmLoader = wasmLoader;
    }
    
    /**
     * 初始化服务器
     */
    initialize(): void {
        const exports = this.wasmLoader.getExports();
        this.context = exports.ZrLspContextNew();
        if (this.context === 0) {
            throw new Error('Failed to create LSP context');
        }
    }
    
    /**
     * 清理资源
     */
    dispose(): void {
        if (this.context !== null) {
            const exports = this.wasmLoader.getExports();
            exports.ZrLspContextFree(this.context);
            this.context = null;
        }
    }
    
    /**
     * 更新文档
     */
    updateDocument(uri: string, content: string, version: number): Promise<void> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                
                const uriStr = this.wasmLoader.writeString(uri);
                const contentStr = this.wasmLoader.writeString(content);
                
                try {
                    const resultPtr = exports.ZrLspUpdateDocument(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len,
                        contentStr.ptr,
                        contentStr.len,
                        version
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    
                    // 清理临时字符串
                    this.wasmLoader.freeString(uriStr.ptr);
                    this.wasmLoader.freeString(contentStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        resolve();
                    } else {
                        reject(new Error(response.error || 'Failed to update document'));
                    }
                } catch (error) {
                    // 清理临时字符串
                    this.wasmLoader.freeString(uriStr.ptr);
                    this.wasmLoader.freeString(contentStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * 获取诊断
     */
    getDiagnostics(uri: string): Promise<LspDiagnostic[]> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                const uriStr = this.wasmLoader.writeString(uri);
                
                try {
                    const resultPtr = exports.ZrLspGetDiagnostics(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    this.wasmLoader.freeString(uriStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        const diagnostics = this.wasmLoader.getResponseData(response) || [];
                        resolve(diagnostics);
                    } else {
                        reject(new Error(response.error || 'Failed to get diagnostics'));
                    }
                } catch (error) {
                    this.wasmLoader.freeString(uriStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * 获取补全
     */
    getCompletion(uri: string, position: LspPosition): Promise<LspCompletionItem[]> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                const uriStr = this.wasmLoader.writeString(uri);
                
                try {
                    const resultPtr = exports.ZrLspGetCompletion(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len,
                        position.line,
                        position.character
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    this.wasmLoader.freeString(uriStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        const completions = this.wasmLoader.getResponseData(response) || [];
                        resolve(completions);
                    } else {
                        reject(new Error(response.error || 'Failed to get completion'));
                    }
                } catch (error) {
                    this.wasmLoader.freeString(uriStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * 获取悬停信息
     */
    getHover(uri: string, position: LspPosition): Promise<LspHover | null> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                const uriStr = this.wasmLoader.writeString(uri);
                
                try {
                    const resultPtr = exports.ZrLspGetHover(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len,
                        position.line,
                        position.character
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    this.wasmLoader.freeString(uriStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        const hover = this.wasmLoader.getResponseData(response);
                        resolve(hover || null);
                    } else {
                        resolve(null); // 没有悬停信息不算错误
                    }
                } catch (error) {
                    this.wasmLoader.freeString(uriStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * 获取定义位置
     */
    getDefinition(uri: string, position: LspPosition): Promise<LspLocation[]> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                const uriStr = this.wasmLoader.writeString(uri);
                
                try {
                    const resultPtr = exports.ZrLspGetDefinition(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len,
                        position.line,
                        position.character
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    this.wasmLoader.freeString(uriStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        const locations = this.wasmLoader.getResponseData(response) || [];
                        resolve(locations);
                    } else {
                        resolve([]); // 没有定义不算错误
                    }
                } catch (error) {
                    this.wasmLoader.freeString(uriStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * 查找引用
     */
    findReferences(uri: string, position: LspPosition, includeDeclaration: boolean): Promise<LspLocation[]> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                const uriStr = this.wasmLoader.writeString(uri);
                
                try {
                    const resultPtr = exports.ZrLspFindReferences(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len,
                        position.line,
                        position.character,
                        includeDeclaration ? 1 : 0
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    this.wasmLoader.freeString(uriStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        const locations = this.wasmLoader.getResponseData(response) || [];
                        resolve(locations);
                    } else {
                        resolve([]); // 没有引用不算错误
                    }
                } catch (error) {
                    this.wasmLoader.freeString(uriStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * 重命名符号
     */
    rename(uri: string, position: LspPosition, newName: string): Promise<LspLocation[]> {
        return new Promise((resolve, reject) => {
            try {
                const exports = this.wasmLoader.getExports();
                const uriStr = this.wasmLoader.writeString(uri);
                const newNameStr = this.wasmLoader.writeString(newName);
                
                try {
                    const resultPtr = exports.ZrLspRename(
                        this.context!,
                        uriStr.ptr,
                        uriStr.len,
                        position.line,
                        position.character,
                        newNameStr.ptr,
                        newNameStr.len
                    );
                    
                    const response = this.wasmLoader.readJsonResponse(resultPtr);
                    this.wasmLoader.freeString(uriStr.ptr);
                    this.wasmLoader.freeString(newNameStr.ptr);
                    
                    if (this.wasmLoader.isSuccessResponse(response)) {
                        const locations = this.wasmLoader.getResponseData(response) || [];
                        resolve(locations);
                    } else {
                        reject(new Error(response.error || 'Failed to rename'));
                    }
                } catch (error) {
                    this.wasmLoader.freeString(uriStr.ptr);
                    this.wasmLoader.freeString(newNameStr.ptr);
                    throw error;
                }
            } catch (error) {
                reject(error);
            }
        });
    }
}
