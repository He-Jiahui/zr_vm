import * as vscode from 'vscode';

const ORGANIZE_IMPORTS_COMMAND = 'zr.organizeImports';

function codeActionKindValue(kind: vscode.CodeActionKind | string | undefined): string | undefined {
    if (typeof kind === 'string') {
        return kind;
    }
    return kind?.value;
}

function isOrganizeImportsAction(action: vscode.Command | vscode.CodeAction): action is vscode.CodeAction {
    return 'kind' in action && codeActionKindValue(action.kind) === vscode.CodeActionKind.SourceOrganizeImports.value;
}

async function applyOrganizeImportsAction(action: vscode.CodeAction): Promise<boolean> {
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

async function organizeActiveDocumentImports(): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    if (editor === undefined || editor.document.languageId !== 'zr') {
        void vscode.window.showInformationMessage('Open a Zr source file to organize imports.');
        return;
    }

    const document = editor.document;
    const actions = await vscode.commands.executeCommand<(vscode.Command | vscode.CodeAction)[]>(
        'vscode.executeCodeActionProvider',
        document.uri,
        new vscode.Range(new vscode.Position(0, 0), new vscode.Position(0, 0)),
        vscode.CodeActionKind.SourceOrganizeImports.value,
    );
    const organizeAction = actions?.find(isOrganizeImportsAction);
    if (organizeAction === undefined) {
        void vscode.window.showInformationMessage('No import changes available.');
        return;
    }

    if (!(await applyOrganizeImportsAction(organizeAction))) {
        void vscode.window.showWarningMessage('Organize imports did not return an applicable edit.');
    }
}

export function registerOrganizeImportsCommand(): vscode.Disposable {
    return vscode.commands.registerCommand(ORGANIZE_IMPORTS_COMMAND, organizeActiveDocumentImports);
}
