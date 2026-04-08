import * as vscode from 'vscode';
import { resolveNativeCliPath } from './nativeAssets';
import {
    activeWorkspaceFolder,
    hasWorkspaceProjects,
    onDidChangeSelectedProject,
    resolveSelectedProjectUri,
    selectWorkspaceProject,
} from './workspaceProjects';
import {
    ZR_DEBUG_CURRENT_PROJECT_COMMAND,
    ZR_DEBUG_SELECTED_PROJECT_COMMAND,
} from './debug/constants';
import {
    ZR_PROJECT_ACTIONS_INSPECT_COMMAND,
    ZR_RUN_CURRENT_PROJECT_COMMAND,
    ZR_RUN_SELECTED_PROJECT_COMMAND,
    ZR_SELECT_PROJECT_COMMAND,
} from './projectActionConstants';

interface ProjectActionState {
    isVisible: boolean;
    projectPath?: string;
    cliPath?: string;
}

export function registerDesktopProjectActions(
    context: vscode.ExtensionContext,
): vscode.Disposable[] {
    const controller = new DesktopProjectActionsController(context);
    return [controller];
}

class DesktopProjectActionsController implements vscode.Disposable {
    private readonly disposables: vscode.Disposable[] = [];
    private readonly runStatusBar: vscode.StatusBarItem;
    private readonly debugStatusBar: vscode.StatusBarItem;
    private state: ProjectActionState = { isVisible: false };

    constructor(private readonly context: vscode.ExtensionContext) {
        this.runStatusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 98);
        this.runStatusBar.name = 'ZR Run Project';
        this.runStatusBar.text = '$(play) Run ZR Project';
        this.runStatusBar.tooltip = 'Run the selected ZR project with the configured ZR executable';
        this.runStatusBar.command = ZR_RUN_SELECTED_PROJECT_COMMAND;

        this.debugStatusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 97);
        this.debugStatusBar.name = 'ZR Debug Project';
        this.debugStatusBar.text = '$(debug-alt-small) Debug ZR Project';
        this.debugStatusBar.tooltip = 'Debug the selected ZR project';
        this.debugStatusBar.command = ZR_DEBUG_SELECTED_PROJECT_COMMAND;

        this.disposables.push(
            this.runStatusBar,
            this.debugStatusBar,
            vscode.commands.registerCommand(ZR_RUN_SELECTED_PROJECT_COMMAND, async () => {
                await this.runSelectedProject();
            }),
            vscode.commands.registerCommand(ZR_RUN_CURRENT_PROJECT_COMMAND, async () => {
                await this.runSelectedProject();
            }),
            vscode.commands.registerCommand(ZR_SELECT_PROJECT_COMMAND, async () => {
                await this.selectProject();
            }),
            vscode.commands.registerCommand(ZR_PROJECT_ACTIONS_INSPECT_COMMAND, () => this.state),
            vscode.window.onDidChangeActiveTextEditor(() => {
                void this.update();
            }),
            vscode.workspace.onDidChangeWorkspaceFolders(() => {
                void this.update();
            }),
            onDidChangeSelectedProject(() => {
                void this.update();
            }),
            vscode.workspace.onDidChangeConfiguration((event) => {
                if (event.affectsConfiguration('zr.executablePath') || event.affectsConfiguration('zr.debug.cli.path')) {
                    void this.update();
                }
            }),
        );

        void this.update();
        context.subscriptions.push(...this.disposables);
    }

    dispose(): void {
        for (const disposable of this.disposables) {
            disposable.dispose();
        }
        this.disposables.length = 0;
    }

    private async selectProject(): Promise<void> {
        const workspaceFolder = activeWorkspaceFolder();
        const projectUri = await selectWorkspaceProject(this.context, workspaceFolder);
        if (!projectUri) {
            return;
        }
        await this.update();
    }

    private async runSelectedProject(): Promise<void> {
        const state = await this.computeState();
        if (!state.projectPath) {
            await this.selectProject();
        }

        const nextState = state.projectPath ? state : await this.computeState();
        if (!nextState.projectPath) {
            await vscode.window.showErrorMessage('Unable to resolve a ZR project (.zrp) to run.');
            return;
        }
        if (!nextState.cliPath) {
            await vscode.window.showErrorMessage(
                'Unable to locate zr_vm_cli. Set zr.executablePath or build/sync the native assets.',
            );
            return;
        }

        const projectUri = vscode.Uri.file(nextState.projectPath);
        const workspaceFolder = vscode.workspace.getWorkspaceFolder(projectUri);
        const task = new vscode.Task(
            {
                type: 'zr',
                task: 'run',
                project: nextState.projectPath,
            },
            workspaceFolder ?? vscode.TaskScope.Workspace,
            `Run ${projectUri.path.split('/').pop() ?? 'ZR Project'}`,
            'ZR',
            new vscode.ProcessExecution(nextState.cliPath, [nextState.projectPath], {
                cwd: workspaceFolder?.uri.fsPath ?? projectUri.with({ path: projectUri.path.replace(/\/[^/]+$/, '') }).fsPath,
            }),
        );
        task.presentationOptions = {
            reveal: vscode.TaskRevealKind.Always,
            panel: vscode.TaskPanelKind.Dedicated,
            clear: true,
        };
        await vscode.tasks.executeTask(task);
    }

    private async update(): Promise<void> {
        const state = await this.computeState();
        this.state = state;

        if (state.isVisible) {
            this.runStatusBar.show();
            this.debugStatusBar.show();
            return;
        }

        this.runStatusBar.hide();
        this.debugStatusBar.hide();
    }

    private async computeState(): Promise<ProjectActionState> {
        if (!(await hasWorkspaceProjects())) {
            return { isVisible: false };
        }

        const workspaceFolder = activeWorkspaceFolder();
        const projectUri = await resolveSelectedProjectUri(this.context, workspaceFolder, false);
        return {
            isVisible: true,
            projectPath: projectUri?.fsPath,
            cliPath: resolveNativeCliPath(this.context),
        };
    }
}
