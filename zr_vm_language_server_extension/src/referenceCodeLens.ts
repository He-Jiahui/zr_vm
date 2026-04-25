import * as vscode from 'vscode';

export const ZR_SHOW_REFERENCES_COMMAND = 'zr.showReferences';

type SerializedPosition = {
    line?: number;
    character?: number;
};

export function registerReferenceCodeLensCommand(): vscode.Disposable {
    return vscode.commands.registerCommand(
        ZR_SHOW_REFERENCES_COMMAND,
        async (uriText?: string, position?: SerializedPosition) => {
            if (typeof uriText !== 'string' ||
                typeof position?.line !== 'number' ||
                typeof position?.character !== 'number') {
                return;
            }

            const uri = vscode.Uri.parse(uriText);
            const vscodePosition = new vscode.Position(position.line, position.character);
            const references = await vscode.commands.executeCommand<vscode.Location[]>(
                'vscode.executeReferenceProvider',
                uri,
                vscodePosition,
            );
            await vscode.commands.executeCommand(
                'editor.action.showReferences',
                uri,
                vscodePosition,
                references ?? [],
            );
        },
    );
}
