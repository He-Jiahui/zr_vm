import * as vscode from 'vscode';
import { onDidChangeLanguageClient, sendLanguageServerRequest } from './languageClientRequests';

const ZR_DECOMPILED_SCHEME = 'zr-decompiled';

/** 为反编译 URI 从当前语言服务请求文本，并在客户端切换后刷新已打开的文档。 */
class ZrVirtualDocumentProvider implements vscode.TextDocumentContentProvider {
    private readonly onDidChangeEmitter = new vscode.EventEmitter<vscode.Uri>();

    readonly onDidChange = this.onDidChangeEmitter.event;

    constructor() {
        // BUG: 注册句柄注销时未释放此监听器；它仍持有 provider，重复注册会累积刷新回调。
        onDidChangeLanguageClient(() => {
            for (const document of vscode.workspace.textDocuments) {
                if (document.uri.scheme === ZR_DECOMPILED_SCHEME) {
                    this.onDidChangeEmitter.fire(document.uri);
                }
            }
        });
    }

    /** 从当前客户端拉取内容；服务不可用时保持虚拟文档可打开，但内容为空。 */
    async provideTextDocumentContent(uri: vscode.Uri): Promise<string> {
        const result = await sendLanguageServerRequest<string>('zr/nativeDeclarationDocument', {
            uri: uri.toString(),
        });
        return typeof result === 'string' ? result : '';
    }
}

/** 在桌面与浏览器入口注册同一 URI scheme，返回给扩展激活生命周期持有。 */
export function registerVirtualDocumentSupport(): vscode.Disposable {
    return vscode.workspace.registerTextDocumentContentProvider(
        ZR_DECOMPILED_SCHEME,
        new ZrVirtualDocumentProvider(),
    );
}
