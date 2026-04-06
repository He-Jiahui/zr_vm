import * as vscode from 'vscode';
import { resolveNativeCliPath } from './nativeAssets';
import {
    activeWorkspaceFolder,
    isZrpDocument,
    resolveProjectUri,
} from './workspaceProjects';
import { ZR_DEBUG_CURRENT_PROJECT_COMMAND } from './debug/constants';
import {
    ZR_PROJECT_ACTIONS_INSPECT_COMMAND,
    ZR_RUN_CURRENT_PROJECT_COMMAND,
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
        this.runStatusBar.tooltip = 'Run the active .zrp project with the configured ZR executable';
        this.runStatusBar.command = ZR_RUN_CURRENT_PROJECT_COMMAND;

        this.debugStatusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 97);
        this.debugStatusBar.name = 'ZR Debug Project';
        this.debugStatusBar.text = '$(debug-alt-small) Debug ZR Project';
        this.debugStatusBar.tooltip = 'Debug the active .zrp project';
        this.debugStatusBar.command = ZR_DEBUG_CURRENT_PROJECT_COMMAND;

        this.disposables.push(
            this.runStatusBar,
            this.debugStatusBar,
            vscode.commands.registerCommand(ZR_RUN_CURRENT_PROJECT_COMMAND, async () => {
                await this.runCurrentProject();
            }),
            vscode.commands.registerCommand(ZR_PROJECT_ACTIONS_INSPECT_COMMAND, () => this.state),
            vscode.window.onDidChangeActiveTextEditor(() => {
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

    private async runCurrentProject(): Promise<void> {
        const state = await this.computeState();
        if (!state.projectPath) {
            await vscode.window.showErrorMessage('Unable to resolve a ZR project (.zrp) to run.');
            return;
        }
        if (!state.cliPath) {
            await vscode.window.showErrorMessage(
                'Unable to locate zr_vm_cli. Set zr.executablePath or build/sync the native assets.',
            );
            return;
        }

        const projectUri = vscode.Uri.file(state.projectPath);
        const workspaceFolder = vscode.workspace.getWorkspaceFolder(projectUri);
        const task = new vscode.Task(
            {
                type: 'zr',
                task: 'run',
                project: state.projectPath,
            },
            workspaceFolder ?? vscode.TaskScope.Workspace,
            `Run ${projectUri.path.split('/').pop() ?? 'ZR Project'}`,
            'ZR',
            new vscode.ProcessExecution(state.cliPath, [state.projectPath], {
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
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isZrpDocument(editor.document)) {
            return { isVisible: false };
        }

        const workspaceFolder = activeWorkspaceFolder();
        const projectUri = await resolveProjectUri(workspaceFolder, editor.document.uri.fsPath);
        return {
            isVisible: true,
            projectPath: projectUri?.fsPath,
            cliPath: resolveNativeCliPath(this.context),
        };
    }
}
