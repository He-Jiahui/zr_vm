//
// LSP 客户端 - 集成所有 LSP 功能
//

import * as vscode from 'vscode';
import { WasmLanguageServer, LspPosition, LspLocation, LspCompletionItem, LspHover, LspDiagnostic } from './server';

export class ZrLanguageClient {
    private server: WasmLanguageServer;
    private documentVersions: Map<string, number> = new Map();
    private diagnosticCollection: vscode.DiagnosticCollection;
    
    constructor(server: WasmLanguageServer) {
        this.server = server;
        this.diagnosticCollection = vscode.languages.createDiagnosticCollection('zr');
    }
    
    /**
     * 启动客户端
     */
    async start(): Promise<void> {
        this.server.initialize();
        
        // 注册文档事件
        vscode.workspace.onDidOpenTextDocument(this.onDidOpenTextDocument, this);
        vscode.workspace.onDidChangeTextDocument(this.onDidChangeTextDocument, this);
        vscode.workspace.onDidCloseTextDocument(this.onDidCloseTextDocument, this);
        
        // 注册代码操作提供者
        vscode.languages.registerCompletionItemProvider('zr', {
            provideCompletionItems: this.provideCompletionItems.bind(this)
        });
        
        vscode.languages.registerHoverProvider('zr', {
            provideHover: this.provideHover.bind(this)
        });
        
        vscode.languages.registerDefinitionProvider('zr', {
            provideDefinition: this.provideDefinition.bind(this)
        });
        
        vscode.languages.registerReferenceProvider('zr', {
            provideReferences: this.provideReferences.bind(this)
        });
        
        vscode.languages.registerRenameProvider('zr', {
            provideRenameEdits: this.provideRenameEdits.bind(this)
        });
        
        vscode.languages.registerDocumentSymbolProvider('zr', {
            provideDocumentSymbols: this.provideDocumentSymbols.bind(this)
        }, {
            label: 'Zr'
        });
        
        // 注册工作区符号提供者
        vscode.languages.registerWorkspaceSymbolProvider({
            provideWorkspaceSymbols: this.provideWorkspaceSymbols.bind(this)
        });
        
        // 初始化已打开的文档
        vscode.workspace.textDocuments.forEach(doc => {
            if (doc.languageId === 'zr') {
                this.onDidOpenTextDocument(doc);
            }
        });
    }
    
    /**
     * 停止客户端
     */
    dispose(): void {
        this.server.dispose();
        this.diagnosticCollection.dispose();
    }
    
    /**
     * 文档打开事件
     */
    private async onDidOpenTextDocument(document: vscode.TextDocument): Promise<void> {
        if (document.languageId !== 'zr') {
            return;
        }
        
        await this.updateDocument(document);
    }
    
    /**
     * 文档更改事件
     */
    private async onDidChangeTextDocument(event: vscode.TextDocumentChangeEvent): Promise<void> {
        if (event.document.languageId !== 'zr') {
            return;
        }
        
        await this.updateDocument(event.document);
    }
    
    /**
     * 文档关闭事件
     */
    private onDidCloseTextDocument(document: vscode.TextDocument): void {
        if (document.languageId !== 'zr') {
            return;
        }
        
        this.diagnosticCollection.delete(document.uri);
        this.documentVersions.delete(document.uri.toString());
    }
    
    /**
     * 更新文档
     */
    private async updateDocument(document: vscode.TextDocument): Promise<void> {
        const uri = document.uri.toString();
        const version = document.version;
        
        // 检查版本是否已更新
        const lastVersion = this.documentVersions.get(uri) || 0;
        if (version <= lastVersion) {
            return;
        }
        
        try {
            await this.server.updateDocument(uri, document.getText(), version);
            this.documentVersions.set(uri, version);
            
            // 更新诊断
            await this.updateDiagnostics(document);
        } catch (error) {
            console.error('Failed to update document:', error);
        }
    }
    
    /**
     * 更新诊断
     */
    private async updateDiagnostics(document: vscode.TextDocument): Promise<void> {
        try {
            const diagnostics = await this.server.getDiagnostics(document.uri.toString());
            const vscodeDiagnostics: vscode.Diagnostic[] = diagnostics.map(diag => {
                const range = new vscode.Range(
                    diag.range.start.line,
                    diag.range.start.character,
                    diag.range.end.line,
                    diag.range.end.character
                );
                
                const severity = this.convertSeverity(diag.severity);
                const diagnostic = new vscode.Diagnostic(range, diag.message, severity);
                
                if (diag.code) {
                    diagnostic.code = diag.code;
                }
                
                return diagnostic;
            });
            
            this.diagnosticCollection.set(document.uri, vscodeDiagnostics);
        } catch (error) {
            console.error('Failed to get diagnostics:', error);
        }
    }
    
    /**
     * 转换严重程度
     */
    private convertSeverity(severity: number): vscode.DiagnosticSeverity {
        switch (severity) {
            case 1: return vscode.DiagnosticSeverity.Error;
            case 2: return vscode.DiagnosticSeverity.Warning;
            case 3: return vscode.DiagnosticSeverity.Information;
            case 4: return vscode.DiagnosticSeverity.Hint;
            default: return vscode.DiagnosticSeverity.Error;
        }
    }
    
    /**
     * 提供补全项
     */
    private async provideCompletionItems(
        document: vscode.TextDocument,
        position: vscode.Position
    ): Promise<vscode.CompletionItem[] | undefined> {
        try {
            const lspPosition: LspPosition = {
                line: position.line,
                character: position.character
            };
            
            const completions = await this.server.getCompletion(document.uri.toString(), lspPosition);
            
            return completions.map(completion => {
                const item = new vscode.CompletionItem(completion.label, this.convertCompletionKind(completion.kind));
                
                if (completion.detail) {
                    item.detail = completion.detail;
                }
                
                if (completion.documentation) {
                    if (typeof completion.documentation === 'string') {
                        item.documentation = new vscode.MarkdownString(completion.documentation);
                    } else {
                        item.documentation = new vscode.MarkdownString(completion.documentation.value);
                    }
                }
                
                if (completion.insertText) {
                    item.insertText = completion.insertText;
                }
                
                if (completion.insertTextFormat === 'snippet') {
                    item.insertText = new vscode.SnippetString(completion.insertText || completion.label);
                }
                
                return item;
            });
        } catch (error) {
            console.error('Failed to get completion:', error);
            return undefined;
        }
    }
    
    /**
     * 转换补全类型
     */
    private convertCompletionKind(kind: number): vscode.CompletionItemKind {
        // LSP CompletionItemKind 映射到 VS Code CompletionItemKind
        const kindMap: { [key: number]: vscode.CompletionItemKind } = {
            1: vscode.CompletionItemKind.Text,
            2: vscode.CompletionItemKind.Method,
            3: vscode.CompletionItemKind.Function,
            4: vscode.CompletionItemKind.Constructor,
            5: vscode.CompletionItemKind.Field,
            6: vscode.CompletionItemKind.Variable,
            7: vscode.CompletionItemKind.Class,
            8: vscode.CompletionItemKind.Interface,
            9: vscode.CompletionItemKind.Module,
            10: vscode.CompletionItemKind.Property,
            13: vscode.CompletionItemKind.Enum,
            21: vscode.CompletionItemKind.Constant,
            22: vscode.CompletionItemKind.Struct,
        };
        
        return kindMap[kind] || vscode.CompletionItemKind.Text;
    }
    
    /**
     * 提供悬停信息
     */
    private async provideHover(
        document: vscode.TextDocument,
        position: vscode.Position
    ): Promise<vscode.Hover | undefined> {
        try {
            const lspPosition: LspPosition = {
                line: position.line,
                character: position.character
            };
            
            const hover = await this.server.getHover(document.uri.toString(), lspPosition);
            
            if (!hover) {
                return undefined;
            }
            
            const contents = hover.contents.map(content => {
                if (typeof content === 'string') {
                    return content;
                } else {
                    return new vscode.MarkdownString(content.value);
                }
            });
            
            let range: vscode.Range | undefined;
            if (hover.range) {
                range = new vscode.Range(
                    hover.range.start.line,
                    hover.range.start.character,
                    hover.range.end.line,
                    hover.range.end.character
                );
            }
            
            return new vscode.Hover(contents, range);
        } catch (error) {
            console.error('Failed to get hover:', error);
            return undefined;
        }
    }
    
    /**
     * 提供定义位置
     */
    private async provideDefinition(
        document: vscode.TextDocument,
        position: vscode.Position
    ): Promise<vscode.Definition | undefined> {
        try {
            const lspPosition: LspPosition = {
                line: position.line,
                character: position.character
            };
            
            const locations = await this.server.getDefinition(document.uri.toString(), lspPosition);
            
            return locations.map(loc => {
                const uri = vscode.Uri.parse(loc.uri);
                const range = new vscode.Range(
                    loc.range.start.line,
                    loc.range.start.character,
                    loc.range.end.line,
                    loc.range.end.character
                );
                return new vscode.Location(uri, range);
            });
        } catch (error) {
            console.error('Failed to get definition:', error);
            return undefined;
        }
    }
    
    /**
     * 提供引用
     */
    private async provideReferences(
        document: vscode.TextDocument,
        position: vscode.Position,
        context: vscode.ReferenceContext
    ): Promise<vscode.Location[] | undefined> {
        try {
            const lspPosition: LspPosition = {
                line: position.line,
                character: position.character
            };
            
            const locations = await this.server.findReferences(
                document.uri.toString(),
                lspPosition,
                context.includeDeclaration
            );
            
            return locations.map(loc => {
                const uri = vscode.Uri.parse(loc.uri);
                const range = new vscode.Range(
                    loc.range.start.line,
                    loc.range.start.character,
                    loc.range.end.line,
                    loc.range.end.character
                );
                return new vscode.Location(uri, range);
            });
        } catch (error) {
            console.error('Failed to find references:', error);
            return undefined;
        }
    }
    
    /**
     * 提供重命名编辑
     */
    private async provideRenameEdits(
        document: vscode.TextDocument,
        position: vscode.Position,
        newName: string
    ): Promise<vscode.WorkspaceEdit | undefined> {
        try {
            const lspPosition: LspPosition = {
                line: position.line,
                character: position.character
            };
            
            const locations = await this.server.rename(document.uri.toString(), lspPosition, newName);
            
            const edit = new vscode.WorkspaceEdit();
            locations.forEach(loc => {
                const uri = vscode.Uri.parse(loc.uri);
                const range = new vscode.Range(
                    loc.range.start.line,
                    loc.range.start.character,
                    loc.range.end.line,
                    loc.range.end.character
                );
                edit.replace(uri, range, newName);
            });
            
            return edit;
        } catch (error) {
            console.error('Failed to rename:', error);
            return undefined;
        }
    }
    
    /**
     * 提供文档符号
     */
    private async provideDocumentSymbols(
        document: vscode.TextDocument
    ): Promise<vscode.DocumentSymbol[] | undefined> {
        try {
            // 获取整个文档的范围
            const fullRange = new vscode.Range(
                new vscode.Position(0, 0),
                new vscode.Position(document.lineCount - 1, Number.MAX_VALUE)
            );
            
            // 使用诊断信息来推断符号（简化实现）
            // TODO: 在 WASM 服务器中添加专门的文档符号 API
            const diagnostics = await this.server.getDiagnostics(document.uri.toString());
            
            // 从诊断中提取符号信息（这是一个简化实现）
            // 实际应该从符号表获取
            const symbols: vscode.DocumentSymbol[] = [];
            
            // 解析文档内容查找符号（简化实现）
            const text = document.getText();
            const lines = text.split('\n');
            
            // 查找函数、类、结构体等定义
            const functionRegex = /^\s*(\w+)\s*\(/;
            const classRegex = /^\s*class\s+(\w+)/;
            const structRegex = /^\s*struct\s+(\w+)/;
            const varRegex = /^\s*var\s+(\w+)/;
            
            for (let i = 0; i < lines.length; i++) {
                const line = lines[i];
                let match: RegExpMatchArray | null = null;
                let kind = vscode.SymbolKind.Variable;
                
                if ((match = line.match(classRegex))) {
                    kind = vscode.SymbolKind.Class;
                } else if ((match = line.match(structRegex))) {
                    kind = vscode.SymbolKind.Struct;
                } else if ((match = line.match(functionRegex))) {
                    kind = vscode.SymbolKind.Function;
                } else if ((match = line.match(varRegex))) {
                    kind = vscode.SymbolKind.Variable;
                }
                
                if (match) {
                    const name = match[1];
                    const startPos = new vscode.Position(i, line.indexOf(name));
                    const endPos = new vscode.Position(i, line.length);
                    const range = new vscode.Range(startPos, endPos);
                    const selectionRange = new vscode.Range(startPos, new vscode.Position(i, startPos.character + name.length));
                    
                    symbols.push(new vscode.DocumentSymbol(
                        name,
                        '',
                        kind,
                        range,
                        selectionRange
                    ));
                }
            }
            
            return symbols.length > 0 ? symbols : undefined;
        } catch (error) {
            console.error('Failed to get document symbols:', error);
            return undefined;
        }
    }
    
    /**
     * 提供工作区符号
     */
    private async provideWorkspaceSymbols(
        query: string
    ): Promise<vscode.SymbolInformation[] | undefined> {
        try {
            const symbols: vscode.SymbolInformation[] = [];
            
            // 在所有打开的 zr 文档中搜索符号
            const documents = vscode.workspace.textDocuments.filter(doc => doc.languageId === 'zr');
            
            for (const doc of documents) {
                const docSymbols = await this.provideDocumentSymbols(doc);
                if (docSymbols) {
                    for (const symbol of docSymbols) {
                        // 过滤匹配查询的符号
                        if (!query || symbol.name.toLowerCase().includes(query.toLowerCase())) {
                            symbols.push(new vscode.SymbolInformation(
                                symbol.name,
                                symbol.kind,
                                '',
                                new vscode.Location(doc.uri, symbol.range)
                            ));
                        }
                    }
                }
            }
            
            return symbols.length > 0 ? symbols : undefined;
        } catch (error) {
            console.error('Failed to get workspace symbols:', error);
            return undefined;
        }
    }
}
