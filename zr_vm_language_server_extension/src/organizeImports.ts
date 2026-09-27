import * as vscode from 'vscode';
import { sendLanguageServerRequest } from './languageClientRequests';

// 命令 ID 与扩展清单的激活事件、命令和编辑器菜单对应；action kind 则与两种服务端公布的能力对应。
const ORGANIZE_IMPORTS_COMMAND = 'zr.organizeImports';
const REMOVE_UNUSED_IMPORTS_COMMAND = 'zr.removeUnusedImports';
const REMOVE_UNUSED_IMPORTS_KIND = 'source.removeUnused';

/** 统一处理 provider 动作中可能出现的 CodeActionKind 对象和 kind 字符串，以便按目标动作筛选。 */
function codeActionKindValue(kind: vscode.CodeActionKind | string | undefined): string | undefined {
    if (typeof kind === 'string') {
        return kind;
    }
    return kind?.value;
}

/** 只接受目标 kind 的 CodeAction；普通 Command 没有可由本模块直接应用的编辑契约。 */
function isSourceActionKind(action: vscode.Command | vscode.CodeAction, kind: string): action is vscode.CodeAction {
    return 'kind' in action && codeActionKindValue(action.kind) === kind;
}

// 原始 LSP 回退只使用位置、范围和文本替换；这些类型描述服务端返回值，而非 VS Code provider 的对象。
type ProtocolPosition = { line: number; character: number };
type ProtocolRange = { start: ProtocolPosition; end: ProtocolPosition };
type ProtocolTextEdit = { range: ProtocolRange; newText: string };
/** 原生服务端对打开文档用 documentChanges，Web 服务端和关闭文档用 changes；回退仅识别文本编辑。 */
type ProtocolWorkspaceEdit = {
    changes?: Record<string, ProtocolTextEdit[]>;
    documentChanges?: Array<{ textDocument?: { uri?: string }; edits?: ProtocolTextEdit[] }>;
};
/** 只读取执行源操作所需的 kind 和 edit；两端还有展示字段，原生服务端另带快照字段。 */
type ProtocolCodeAction = {
    title?: string;
    kind?: string;
    edit?: ProtocolWorkspaceEdit;
};
/** 两个用户命令共用相同选择及回退流程，仅目标 kind 和反馈文案不同。 */
type SourceActionCommand = {
    kind: string;
    openDocumentMessage: string;
    noChangesMessage: string;
    noEditMessage: string;
};

/** 在原始协议响应进入 VS Code 编辑 API 前转换坐标对象。 */
function toVsCodeRange(range: ProtocolRange): vscode.Range {
    return new vscode.Range(
        new vscode.Position(range.start.line, range.start.character),
        new vscode.Position(range.end.line, range.end.character),
    );
}

/** 将原始请求中的工作区编辑交给 VS Code 应用；服务端目前只发送同一 URI 的文本编辑。 */
function toVsCodeWorkspaceEdit(edit: ProtocolWorkspaceEdit | undefined): vscode.WorkspaceEdit | undefined {
    if (!edit) {
        return undefined;
    }

    const workspaceEdit = new vscode.WorkspaceEdit();
    const documentChanges = edit.documentChanges ?? [];
    if (documentChanges.length > 0) {
        for (const documentChange of documentChanges) {
            const uriText = documentChange.textDocument?.uri;
            if (!uriText) {
                continue;
            }
            const uri = vscode.Uri.parse(uriText);
            for (const textEdit of documentChange.edits ?? []) {
                // BUG: 原始回退收到打开文档的 versioned documentChanges 后，这里丢弃 version；
                // 若服务端生成编辑后、applyEdit 前文档再变化，旧 range 可写入新版本。可在回退请求期间插入编辑复现。
                workspaceEdit.replace(uri, toVsCodeRange(textEdit.range), textEdit.newText);
            }
        }
        return workspaceEdit;
    }

    for (const [uriText, edits] of Object.entries(edit.changes ?? {})) {
        const uri = vscode.Uri.parse(uriText);
        for (const textEdit of edits) {
            workspaceEdit.replace(uri, toVsCodeRange(textEdit.range), textEdit.newText);
        }
    }

    return workspaceEdit;
}

/** 优先执行 provider 已转换的 CodeAction；编辑失败时不执行可能依赖该编辑的附带命令。 */
async function applySourceAction(action: vscode.CodeAction): Promise<boolean> {
    if (action.edit !== undefined) {
        const applied = await vscode.workspace.applyEdit(action.edit);
        if (!applied) {
            return false;
        }
    }

    if (action.command !== undefined) {
        await vscode.commands.executeCommand(action.command.command, ...(action.command.arguments ?? []));
    }

    return action.edit !== undefined || action.command !== undefined;
}

/** provider 未给出可用动作时直接请求当前语言客户端；用 only 和精确 kind 限定目标源操作。 */
async function applyRawSourceAction(document: vscode.TextDocument, kind: string): Promise<boolean> {
    const actions = await sendLanguageServerRequest<ProtocolCodeAction[]>('textDocument/codeAction', {
        textDocument: { uri: document.uri.toString(true) },
        range: {
            start: { line: 0, character: 0 },
            end: { line: 0, character: 0 },
        },
        context: {
            diagnostics: [],
            only: [kind],
        },
    });
    const action = actions?.find((item) => item.kind === kind);
    const edit = toVsCodeWorkspaceEdit(action?.edit);
    return edit !== undefined && await vscode.workspace.applyEdit(edit);
}

/** 命令只作用于当前 Zr 编辑器；先走 VS Code provider，再尝试服务端原始编辑作为兼容回退。 */
async function applyActiveDocumentSourceAction(command: SourceActionCommand): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    if (editor === undefined || editor.document.languageId !== 'zr') {
        void vscode.window.showInformationMessage(command.openDocumentMessage);
        return;
    }

    const document = editor.document;
    const actions = await vscode.commands.executeCommand<(vscode.Command | vscode.CodeAction)[]>(
        'vscode.executeCodeActionProvider',
        document.uri,
        new vscode.Range(new vscode.Position(0, 0), new vscode.Position(0, 0)),
        command.kind,
    );
    const sourceAction = actions?.find((item): item is vscode.CodeAction => isSourceActionKind(item, command.kind));
    if (sourceAction === undefined) {
        if (!(await applyRawSourceAction(document, command.kind))) {
            // BUG: 禁用语言服务或启动失败时，可选请求返回 undefined；此处把未执行分析误报为“没有改动”。
            // 可关闭 zr.languageServer.enable、打开含待整理 import 的 .zr 文件后执行任一命令复现。
            void vscode.window.showInformationMessage(command.noChangesMessage);
        }
        return;
    }

    if (!(await applySourceAction(sourceAction)) &&
        !(await applyRawSourceAction(document, command.kind))) {
        void vscode.window.showWarningMessage(command.noEditMessage);
    }
}

/** 将用户的整理导入命令映射到 VS Code 标准 source.organizeImports 动作。 */
async function organizeActiveDocumentImports(): Promise<void> {
    await applyActiveDocumentSourceAction({
        kind: vscode.CodeActionKind.SourceOrganizeImports.value,
        openDocumentMessage: 'Open a Zr source file to organize imports.',
        noChangesMessage: 'No import changes available.',
        noEditMessage: 'Organize imports did not return an applicable edit.',
    });
}

/** 用扩展自定义的 source.removeUnused 动作触发服务端未使用导入清理。 */
async function removeUnusedImports(): Promise<void> {
    await applyActiveDocumentSourceAction({
        kind: REMOVE_UNUSED_IMPORTS_KIND,
        openDocumentMessage: 'Open a Zr source file to remove unused imports.',
        noChangesMessage: 'No unused imports found.',
        noEditMessage: 'Remove unused imports did not return an applicable edit.',
    });
}

/** 供桌面和 Web 激活入口注册两项清理命令；返回的句柄随扩展上下文一同释放。 */
export function registerOrganizeImportsCommand(): vscode.Disposable {
    return vscode.Disposable.from(
        vscode.commands.registerCommand(ORGANIZE_IMPORTS_COMMAND, organizeActiveDocumentImports),
        vscode.commands.registerCommand(REMOVE_UNUSED_IMPORTS_COMMAND, removeUnusedImports),
    );
}
