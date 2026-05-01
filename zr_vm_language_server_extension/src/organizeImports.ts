import * as vscode from 'vscode';
import { sendLanguageServerRequest } from './languageClientRequests';

const ORGANIZE_IMPORTS_COMMAND = 'zr.organizeImports';
const REMOVE_UNUSED_IMPORTS_COMMAND = 'zr.removeUnusedImports';
const REMOVE_UNUSED_IMPORTS_KIND = 'source.removeUnused';

function codeActionKindValue(kind: vscode.CodeActionKind | string | undefined): string | undefined {
    if (typeof kind === 'string') {
        return kind;
    }
    return kind?.value;
}

function isSourceActionKind(action: vscode.Command | vscode.CodeAction, kind: string): action is vscode.CodeAction {
    return 'kind' in action && codeActionKindValue(action.kind) === kind;
}

type ProtocolPosition = { line: number; character: number };
type ProtocolRange = { start: ProtocolPosition; end: ProtocolPosition };
type ProtocolTextEdit = { range: ProtocolRange; newText: string };
type ProtocolWorkspaceEdit = {
    changes?: Record<string, ProtocolTextEdit[]>;
    documentChanges?: Array<{ textDocument?: { uri?: string }; edits?: ProtocolTextEdit[] }>;
};
type ProtocolCodeAction = {
    title?: string;
    kind?: string;
    edit?: ProtocolWorkspaceEdit;
};
type SourceActionCommand = {
    kind: string;
    openDocumentMessage: string;
    noChangesMessage: string;
    noEditMessage: string;
};

function toVsCodeRange(range: ProtocolRange): vscode.Range {
    return new vscode.Range(
        new vscode.Position(range.start.line, range.start.character),
        new vscode.Position(range.end.line, range.end.character),
    );
}

function toVsCodeWorkspaceEdit(edit: ProtocolWorkspaceEdit | undefined): vscode.WorkspaceEdit | undefined {
    if (!edit) {
        return undefined;
    }

    const workspaceEdit = new vscode.WorkspaceEdit();
    for (const [uriText, edits] of Object.entries(edit.changes ?? {})) {
        const uri = vscode.Uri.parse(uriText);
        for (const textEdit of edits) {
            workspaceEdit.replace(uri, toVsCodeRange(textEdit.range), textEdit.newText);
        }
    }
    for (const documentChange of edit.documentChanges ?? []) {
        const uriText = documentChange.textDocument?.uri;
        if (!uriText) {
            continue;
        }
        const uri = vscode.Uri.parse(uriText);
        for (const textEdit of documentChange.edits ?? []) {
            workspaceEdit.replace(uri, toVsCodeRange(textEdit.range), textEdit.newText);
        }
    }

    return workspaceEdit;
}

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
            void vscode.window.showInformationMessage(command.noChangesMessage);
        }
        return;
    }

    if (!(await applySourceAction(sourceAction)) &&
        !(await applyRawSourceAction(document, command.kind))) {
        void vscode.window.showWarningMessage(command.noEditMessage);
    }
}

async function organizeActiveDocumentImports(): Promise<void> {
    await applyActiveDocumentSourceAction({
        kind: vscode.CodeActionKind.SourceOrganizeImports.value,
        openDocumentMessage: 'Open a Zr source file to organize imports.',
        noChangesMessage: 'No import changes available.',
        noEditMessage: 'Organize imports did not return an applicable edit.',
    });
}

async function removeUnusedImports(): Promise<void> {
    await applyActiveDocumentSourceAction({
        kind: REMOVE_UNUSED_IMPORTS_KIND,
        openDocumentMessage: 'Open a Zr source file to remove unused imports.',
        noChangesMessage: 'No unused imports found.',
        noEditMessage: 'Remove unused imports did not return an applicable edit.',
    });
}

export function registerOrganizeImportsCommand(): vscode.Disposable {
    return vscode.Disposable.from(
        vscode.commands.registerCommand(ORGANIZE_IMPORTS_COMMAND, organizeActiveDocumentImports),
        vscode.commands.registerCommand(REMOVE_UNUSED_IMPORTS_COMMAND, removeUnusedImports),
    );
}
