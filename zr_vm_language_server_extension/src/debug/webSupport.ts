import * as vscode from 'vscode';
import {
    ZR_DEBUG_ATTACH_COMMAND,
    ZR_DEBUG_CURRENT_PROJECT_COMMAND,
    ZR_DEBUG_TYPE,
} from './constants';

export function registerWebDebugSupportUnavailable(): vscode.Disposable[] {
    const unavailable = async () => {
        await vscode.window.showWarningMessage('ZR debugger is not available in VS Code Web. Use the desktop extension.');
    };

    return [
        vscode.commands.registerCommand(ZR_DEBUG_CURRENT_PROJECT_COMMAND, unavailable),
        vscode.commands.registerCommand(ZR_DEBUG_ATTACH_COMMAND, unavailable),
        vscode.debug.registerDebugConfigurationProvider(ZR_DEBUG_TYPE, {
            resolveDebugConfiguration: async () => {
                await unavailable();
                return undefined;
            },
        }),
    ];
}
