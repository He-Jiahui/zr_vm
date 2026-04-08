import * as vscode from 'vscode';
import {
    ZR_RUN_SELECTED_PROJECT_COMMAND,
    ZR_SELECT_PROJECT_COMMAND,
    ZR_PROJECT_ACTIONS_INSPECT_COMMAND,
    ZR_RUN_CURRENT_PROJECT_COMMAND,
} from './projectActionConstants';
import { activeWorkspaceFolder, hasWorkspaceProjects, resolveSelectedProjectUri, selectWorkspaceProject } from './workspaceProjects';

export function registerWebProjectActionsUnavailable(
    context: vscode.ExtensionContext,
): vscode.Disposable[] {
    const unavailable = async () => {
        await vscode.window.showWarningMessage('ZR project run/debug actions are not available in VS Code Web. Use the desktop extension.');
    };

    return [
        vscode.commands.registerCommand(ZR_SELECT_PROJECT_COMMAND, async () => {
            await selectWorkspaceProject(context, activeWorkspaceFolder());
        }),
        vscode.commands.registerCommand(ZR_RUN_CURRENT_PROJECT_COMMAND, unavailable),
        vscode.commands.registerCommand(ZR_RUN_SELECTED_PROJECT_COMMAND, unavailable),
        vscode.commands.registerCommand(ZR_PROJECT_ACTIONS_INSPECT_COMMAND, async () => {
            const projectUri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);
            return {
                isVisible: await hasWorkspaceProjects(),
                projectPath: projectUri?.fsPath,
            };
        }),
    ];
}
