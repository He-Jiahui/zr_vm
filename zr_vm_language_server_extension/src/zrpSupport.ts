import * as vscode from 'vscode';

// package.json 先以 zr-project 激活扩展；随后转成 JSON，交给 VS Code 的 schema 与补全服务。
const ZRP_EXTENSION = '.zrp';
const ZRP_LANGUAGE_ID = 'json';
/** 客户端只依赖语言或路径选择条件；JSON 化后的项目文件仍须按路径进入 LSP。 */
type DocumentSelectorEntry = {
    language?: string;
    scheme?: string;
    pattern?: string;
};

/** 仅按 URI 路径判定项目文件，以便虚拟文档与磁盘文档遵守同一语言切换条件。 */
function isZrpDocument(document: vscode.TextDocument): boolean {
    return document.uri.path.toLowerCase().endsWith(ZRP_EXTENSION);
}

/** 保留 zr-project 的激活入口，再将已打开的项目文档交给内置 JSON 编辑能力。 */
async function ensureZrpLanguage(document: vscode.TextDocument): Promise<void> {
    if (!isZrpDocument(document) || document.languageId === ZRP_LANGUAGE_ID) {
        return;
    }

    await vscode.languages.setTextDocumentLanguage(document, ZRP_LANGUAGE_ID);
}

/**
 * 桌面与浏览器入口在激活时注册：处理已有文档，并监听后续打开的 .zrp。
 * 返回的订阅随扩展上下文释放；语言切换异步完成，不随订阅撤销。
 */
export function registerZrpJsonSupport(): vscode.Disposable {
    // TODO: 两条调用链均丢弃 setTextDocumentLanguage 的拒绝；需要在文档关闭竞态与
    // VS Code API 拒绝场景核查是否会产生未处理 Promise 和未切换的项目文档。
    for (const document of vscode.workspace.textDocuments) {
        void ensureZrpLanguage(document);
    }

    return vscode.workspace.onDidOpenTextDocument((document) => {
        void ensureZrpLanguage(document);
    });
}

/**
 * 给两个宿主的语言客户端共用选择范围：.zr 按语言 ID，.zrp 在改为 json 后仍按路径发送。
 * 调用方将此结果交给 LanguageClientOptions，服务端再按文件类型决定解析或刷新项目索引。
 */
export function createDocumentSelector(): DocumentSelectorEntry[] {
    return [
        { language: 'zr' },
        // TODO: 后缀识别忽略大小写，但此 glob 仅写小写 .zrp；需在 VS Code 宿主
        // 核对 .ZRP 文件转成 JSON 后是否仍会同步给语言客户端。
        { pattern: '**/*.zrp' },
    ];
}
