//
// WASM 加载器 - 处理 WASM 模块的加载和内存管理
//

import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';

export interface WasmExports {
    // 内存管理
    malloc(size: number): number;
    free(ptr: number): void;
    
    // LSP 上下文
    ZrLspContextNew(): number;
    ZrLspContextFree(context: number): void;
    
    // LSP 函数
    ZrLspUpdateDocument(context: number, uri: number, uriLen: number, 
                       content: number, contentLen: number, version: number): number;
    ZrLspGetDiagnostics(context: number, uri: number, uriLen: number): number;
    ZrLspGetCompletion(context: number, uri: number, uriLen: number,
                      line: number, character: number): number;
    ZrLspGetHover(context: number, uri: number, uriLen: number,
                  line: number, character: number): number;
    ZrLspGetDefinition(context: number, uri: number, uriLen: number,
                       line: number, character: number): number;
    ZrLspFindReferences(context: number, uri: number, uriLen: number,
                       line: number, character: number, includeDeclaration: number): number;
    ZrLspRename(context: number, uri: number, uriLen: number,
                line: number, character: number, newName: number, newNameLen: number): number;
}

export class WasmLoader {
    private wasmModule: WebAssembly.Module | null = null;
    private wasmInstance: WebAssembly.Instance | null = null;
    private exports: WasmExports | null = null;
    private memory: WebAssembly.Memory | null = null;
    private memoryView: Uint8Array | null = null;
    
    /**
     * 加载 WASM 模块
     */
    async load(wasmPath: string): Promise<void> {
        try {
            const wasmBuffer = fs.readFileSync(wasmPath);
            this.wasmModule = await WebAssembly.compile(wasmBuffer);
            
            // 创建内存（初始 16MB，最大 2GB）
            this.memory = new WebAssembly.Memory({
                initial: 256,  // 16MB (256 * 64KB)
                maximum: 32768, // 2GB (32768 * 64KB)
            });
            
            // 创建导入对象
            const imports = {
                env: {
                    memory: this.memory,
                    // 如果需要其他导入函数，在这里添加
                }
            };
            
            this.wasmInstance = await WebAssembly.instantiate(this.wasmModule, imports);
            this.exports = this.wasmInstance.exports as unknown as WasmExports;
            
            // 更新内存视图
            this.updateMemoryView();
            
            console.log('WASM module loaded successfully');
        } catch (error) {
            console.error('Failed to load WASM module:', error);
            throw error;
        }
    }
    
    /**
     * 更新内存视图
     */
    private updateMemoryView(): void {
        if (this.memory) {
            this.memoryView = new Uint8Array(this.memory.buffer);
        }
    }
    
    /**
     * 获取 WASM 导出函数
     */
    getExports(): WasmExports {
        if (!this.exports) {
            throw new Error('WASM module not loaded');
        }
        return this.exports;
    }
    
    /**
     * 获取内存
     */
    getMemory(): WebAssembly.Memory {
        if (!this.memory) {
            throw new Error('WASM memory not initialized');
        }
        return this.memory;
    }
    
    /**
     * 将 JavaScript 字符串写入 WASM 内存
     * 返回指针和长度
     */
    writeString(str: string): { ptr: number; len: number } {
        if (!this.exports || !this.memoryView) {
            throw new Error('WASM not initialized');
        }
        
        const encoder = new TextEncoder();
        const utf8Bytes = encoder.encode(str);
        const len = utf8Bytes.length;
        
        // 分配内存（包括 null 终止符）
        const ptr = this.exports.malloc(len + 1);
        if (ptr === 0) {
            throw new Error('Failed to allocate memory');
        }
        
        // 写入内存
        this.memoryView.set(utf8Bytes, ptr);
        this.memoryView[ptr + len] = 0; // null 终止符
        
        return { ptr, len };
    }
    
    /**
     * 从 WASM 内存读取字符串
     */
    readString(ptr: number, len?: number): string {
        if (!this.memoryView) {
            throw new Error('WASM memory not initialized');
        }
        
        if (len === undefined) {
            // 查找 null 终止符
            let i = ptr;
            while (i < this.memoryView.length && this.memoryView[i] !== 0) {
                i++;
            }
            len = i - ptr;
        }
        
        const bytes = this.memoryView.slice(ptr, ptr + len);
        const decoder = new TextDecoder('utf-8');
        return decoder.decode(bytes);
    }
    
    /**
     * 释放字符串内存
     */
    freeString(ptr: number): void {
        if (!this.exports) {
            throw new Error('WASM not initialized');
        }
        this.exports.free(ptr);
    }
    
    /**
     * 读取 JSON 响应
     */
    readJsonResponse(ptr: number): any {
        const jsonStr = this.readString(ptr);
        try {
            const result = JSON.parse(jsonStr);
            // 释放 JSON 字符串内存
            this.freeString(ptr);
            return result;
        } catch (error) {
            this.freeString(ptr);
            throw new Error(`Failed to parse JSON: ${error}`);
        }
    }
    
    /**
     * 检查响应是否成功
     */
    isSuccessResponse(response: any): boolean {
        return response && response.success === true;
    }
    
    /**
     * 获取响应数据
     */
    getResponseData(response: any): any {
        if (this.isSuccessResponse(response)) {
            return response.data;
        }
        throw new Error(response.error || 'Unknown error');
    }
}
