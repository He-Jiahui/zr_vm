import * as vscode from 'vscode';
import {
    ZR_RUN_SELECTED_PROJECT_COMMAND,
    ZR_SELECT_PROJECT_COMMAND,
    ZR_PROJECT_ACTIONS_INSPECT_COMMAND,
    ZR_RUN_CURRENT_PROJECT_COMMAND,
} from './projectActionConstants';
import { activeWorkspaceFolder, hasWorkspaceProjects, resolveSelectedProjectUri, selectWorkspaceProject } from './workspaceProjects';

/** Web 宿主保留项目选择和状态查询入口；本地运行命令以提示结束。 */
export function registerWebProjectActionsUnavailable(
    context: vscode.ExtensionContext,
): vscode.Disposable[] {
    /** 对调用运行命令的用户说明宿主能力边界，避免静默无响应。 */
    const unavailable = async () => {
        await vscode.window.showWarningMessage('ZR project run/debug actions are not available in VS Code Web. Use the desktop extension.');
    };

    /* 选择命令仍使用工作区清单；inspect 供宿主观察，运行命令不创建本地 Task。 */
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
