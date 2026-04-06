import * as vscode from 'vscode';
import {
    ZR_PROJECT_ACTIONS_INSPECT_COMMAND,
    ZR_RUN_CURRENT_PROJECT_COMMAND,
} from './projectActionConstants';

export function registerWebProjectActionsUnavailable(): vscode.Disposable[] {
    const unavailable = async () => {
        await vscode.window.showWarningMessage('ZR project run/debug actions are not available in VS Code Web. Use the desktop extension.');
    };

    return [
        vscode.commands.registerCommand(ZR_RUN_CURRENT_PROJECT_COMMAND, unavailable),
        vscode.commands.registerCommand(ZR_PROJECT_ACTIONS_INSPECT_COMMAND, () => ({
            isVisible: false,
        })),
    ];
}
