import * as vscode from 'vscode';
import {
    ZR_DEBUG_ATTACH_COMMAND,
    ZR_DEBUG_CURRENT_PROJECT_COMMAND,
    ZR_DEBUG_SELECTED_PROJECT_COMMAND,
    ZR_DEBUG_TYPE,
} from './constants';

/** Web activate 的调试入口：命令仍可发现，但浏览器宿主不启动本地 CLI/TCP adapter。
 * 调用者需把所有注册项加入 subscriptions，使停用时一并释放。
 */
export function registerWebDebugSupportUnavailable(): vscode.Disposable[] {
    /** 统一项目菜单、命令面板和 launch.json 的不可用提示。 */
    const unavailable = async () => {
        await vscode.window.showWarningMessage('ZR debugger is not available in VS Code Web. Use the desktop extension.');
    };

    return [
        vscode.commands.registerCommand(ZR_DEBUG_CURRENT_PROJECT_COMMAND, unavailable),
        vscode.commands.registerCommand(ZR_DEBUG_SELECTED_PROJECT_COMMAND, unavailable),
        vscode.commands.registerCommand(ZR_DEBUG_ATTACH_COMMAND, unavailable),
        vscode.debug.registerDebugConfigurationProvider(ZR_DEBUG_TYPE, {
            // 返回 undefined 明确取消调试，避免 VS Code 继续寻找桌面适配器。
            resolveDebugConfiguration: async () => {
                await unavailable();
                return undefined;
            },
        }),
    ];
}
