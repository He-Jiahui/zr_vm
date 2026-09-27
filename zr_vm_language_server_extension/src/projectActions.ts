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

/** 状态栏和运行命令复用的快照类型；可见性只表示存在可解析清单，不保证已找到 CLI。 */
interface ProjectActionState {
    isVisible: boolean;
    projectPath?: string;
    cliPath?: string;
}

/** 在桌面扩展激活时注册项目运行、选择与状态栏，并把控制器交给宿主管理。 */
export function registerDesktopProjectActions(
    context: vscode.ExtensionContext,
): vscode.Disposable[] {
    const controller = new DesktopProjectActionsController(context);
    return [controller];
}

/** 汇聚项目命令与 VS Code 事件；状态栏和运行入口复用 computeState 规则。 */
class DesktopProjectActionsController implements vscode.Disposable {
    private readonly disposables: vscode.Disposable[] = [];
    private readonly runStatusBar: vscode.StatusBarItem;
    private readonly debugStatusBar: vscode.StatusBarItem;
    private state: ProjectActionState = { isVisible: false };

    /** 创建状态栏与命令订阅；运行入口共享同一项目选择路径。 */
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

        /* TODO: .zrp 增删后状态栏依赖结构视图间接发出选择事件；核查外部磁盘变更及结构刷新失败时的可见性。 */
        this.disposables.push(
            this.runStatusBar,
            this.debugStatusBar,
            /* 当前和已选运行命令复用解析器；活动文件的优先级由 workspaceProjects 决定。 */
            vscode.commands.registerCommand(ZR_RUN_SELECTED_PROJECT_COMMAND, async () => {
                await this.runSelectedProject();
            }),
            vscode.commands.registerCommand(ZR_RUN_CURRENT_PROJECT_COMMAND, async () => {
                await this.runSelectedProject();
            }),
            vscode.commands.registerCommand(ZR_SELECT_PROJECT_COMMAND, async () => {
                await this.selectProject();
            }),
            /* 内部检查命令返回刷新后的快照，供扩展宿主 smoke 测试观察。 */
            vscode.commands.registerCommand(ZR_PROJECT_ACTIONS_INSPECT_COMMAND, async () => {
                await this.update();
                return this.state;
            }),
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

        // BUG: 项目发现的读取/解析异常会令 update 拒绝；此处和事件回调均丢弃
        // Promise，激活或后续刷新会产生未处理拒绝，而非受控地隐藏项目操作。
        void this.update();
        // TODO: 控制器作为订阅项返回给 activate，同时资源又单独加入 subscriptions；
        // 核查 VS Code 对这些资源重复 dispose 的契约，避免停用时重复释放。
        context.subscriptions.push(...this.disposables);
    }

    /** 停用时释放命令与事件订阅，防止旧控制器继续刷新状态栏。 */
    dispose(): void {
        for (const disposable of this.disposables) {
            disposable.dispose();
        }
        this.disposables.length = 0;
    }

    /** 将选择器结果写入工作区状态，再刷新状态栏；取消时不改变现有选择。 */
    private async selectProject(): Promise<void> {
        const workspaceFolder = activeWorkspaceFolder();
        const projectUri = await selectWorkspaceProject(this.context, workspaceFolder);
        if (!projectUri) {
            return;
        }
        await this.update();
    }

    /** 经统一选择状态解析项目与 CLI，并以 VS Code Task 启动桌面运行。 */
    private async runSelectedProject(): Promise<void> {
        // BUG: 用户手动选 B 而活动文件仍属 A 时，computeState 的解析器会优先选 A；
        // CLI 可用时，即使运行“已选项目”命令，Task 仍以 A 为目标。根因见 workspaceProjects。
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
                'Unable to locate zr_vm_cli. Set zr.executablePath (relative or absolute) or build/sync the native assets.',
            );
            return;
        }

        /* Task 保留项目文件的绝对路径，工作目录优先取所属工作区，供 CLI 解析启动资源。 */
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

    /** 刷新状态栏快照，供可见性与内部 inspect 命令复用。 */
    private async update(): Promise<void> {
        const state = await this.computeState();
        // TODO: 多个编辑器/配置事件可并发执行 update，较早扫描若较晚返回会覆盖
        // 新状态；需用不同完成顺序的宿主测试确认状态栏是否回退。
        this.state = state;

        if (state.isVisible) {
            this.runStatusBar.show();
            this.debugStatusBar.show();
            return;
        }

        this.runStatusBar.hide();
        this.debugStatusBar.hide();
    }

    /** 先判定是否存在项目，再解析当前选择和 CLI；缺少 CLI 仍显示项目操作入口。 */
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
