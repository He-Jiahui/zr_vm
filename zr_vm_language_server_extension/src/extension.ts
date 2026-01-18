//
// Extension 入口
//

import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { WasmLoader } from './wasm-loader';
import { WasmLanguageServer } from './server';
import { ZrLanguageClient } from './client';

let client: ZrLanguageClient | null = null;
let wasmLoader: WasmLoader | null = null;

export async function activate(context: vscode.ExtensionContext) {
    console.log('Zr Language Server extension is now active!');
    
    // 获取 WASM 文件路径
    const wasmPath = path.join(context.extensionPath, 'wasm', 'zr_vm_language_server.wasm');
    
    // 检查 WASM 文件是否存在
    if (!fs.existsSync(wasmPath)) {
        vscode.window.showErrorMessage(
            `WASM file not found: ${wasmPath}. Please build the WASM module first.`
        );
        return;
    }
    
    try {
        // 加载 WASM 模块
        wasmLoader = new WasmLoader();
        await wasmLoader.load(wasmPath);
        
        // 创建服务器
        const server = new WasmLanguageServer(wasmLoader);
        
        // 创建客户端
        client = new ZrLanguageClient(server);
        await client.start();
        
        // 注册命令
        const restartCommand = vscode.commands.registerCommand('zr.restartLanguageServer', async () => {
            await restartLanguageServer(context);
        });
        
        context.subscriptions.push(restartCommand);
        
        console.log('Zr Language Server started successfully');
    } catch (error) {
        console.error('Failed to activate Zr Language Server:', error);
        vscode.window.showErrorMessage(
            `Failed to activate Zr Language Server: ${error}`
        );
    }
}

export function deactivate() {
    if (client) {
        client.dispose();
        client = null;
    }
    
    if (wasmLoader) {
        wasmLoader = null;
    }
}

async function restartLanguageServer(context: vscode.ExtensionContext) {
    // 停用当前服务器
    deactivate();
    
    // 等待一段时间
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // 重新激活
    await activate(context);
}
