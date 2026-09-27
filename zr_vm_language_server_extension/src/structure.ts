import * as vscode from 'vscode';
import { onDidChangeLanguageClient, sendLanguageServerRequest } from './languageClientRequests';
import { listBuiltinModuleSnapshots } from './structure/builtinModules';
import {
    activeWorkspaceFolder,
    isZrpDocument,
    onDidChangeSelectedProject,
    resolveSelectedWorkspaceProject,
    type WorkspaceProject,
} from './workspaceProjects';

// 视图和命令 ID 同时供 package.json 的贡献点、扩展激活入口及 smoke 检查使用。
export const ZR_FILES_VIEW_ID = 'zrFiles';
export const ZR_IMPORTS_VIEW_ID = 'zrImports';
export const ZR_BUILTIN_MODULES_VIEW_ID = 'zrBuiltinModules';
export const ZR_STRUCTURE_REFRESH_COMMAND = 'zr.structure.refresh';
export const ZR_STRUCTURE_INSPECT_COMMAND = 'zr.__inspectStructureViews';
export const ZR_STRUCTURE_OPEN_TARGET_COMMAND = 'zr.structure.openTarget';

// 当前文件树在语言服务器尚未就绪时仍需显示名称和导入，因此直接扫描编辑器文本。
const MODULE_PATTERN = /^\s*module\s+(?:(['"])([^'"]+)\1|([A-Za-z_]\w*(?:[./][A-Za-z_]\w*)*))\s*;/m;
const IMPORT_PATTERN = /(?:\b(?:let|var)\s+([A-Za-z_]\w*)\s*=\s*)?(?<![\w.%])import\b\s*\(\s*(['"])([^'"]+)\2\s*\)/g;
const REFRESH_DEBOUNCE_MS = 150;

/** 供三个树视图和测试快照共享的节点分类；序列化结果不携带 VS Code 对象。 */
type NodeType = 'info' | 'group' | 'file' | 'import' | 'declaration' | 'project' | 'module' | 'action';
/** 导航命令区分已知源码范围与需要语言服务器解析的导入目标。 */
type OpenTargetKind = 'range' | 'definition';

/** 命令参数仅传可序列化坐标，执行时才还原成 VS Code 类型。 */
interface SerializedPosition {
    line: number;
    character: number;
}

/** 与 LSP 的零基行列约定一致；用于视图命令和项目摘要边界。 */
interface SerializedRange {
    start: SerializedPosition;
    end: SerializedPosition;
}

/** 树节点交给命令系统的导航契约；定义查询无结果时允许使用已知回退目标。 */
interface OpenTargetPayload {
    kind: OpenTargetKind;
    uri: string;
    range?: SerializedRange;
    position?: SerializedPosition;
    fallbackUri?: string;
    fallbackRange?: SerializedRange;
}

/** zr/projectModules 的扩展侧投影；sourceKind 数值必须与服务端来源枚举保持一致。 */
interface ProjectModuleSummaryPayload {
    sourceKind: number;
    isEntry: boolean;
    moduleName: string;
    displayName?: string;
    description?: string;
    navigationUri?: string;
    range?: SerializedRange;
}

/** 提供器持有的视图模型；children 和 command 在刷新时整体替换。 */
interface TreeNode {
    id: string;
    nodeType: NodeType;
    label: string;
    description?: string;
    tooltip?: string;
    uri?: vscode.Uri;
    icon: vscode.ThemeIcon;
    collapsibleState: vscode.TreeItemCollapsibleState;
    command?: vscode.Command;
    children: TreeNode[];
}

/** 检查命令供端到端测试读取树结构，不暴露 ThemeIcon 或 Uri 实例。 */
interface SerializedTreeNode {
    id: string;
    nodeType: NodeType;
    label: string;
    description?: string;
    uri?: string;
    commandId?: string;
    commandArguments?: unknown[];
    children: SerializedTreeNode[];
}

/** 当前文本的轻量导入索引，保留整段与模块字面量两个导航范围。 */
interface ImportEntry {
    alias?: string;
    moduleName: string;
    range: vscode.Range;
    moduleLiteralRange: vscode.Range;
}

/** 激活入口持有的视图控制器；重启客户端后可主动请求一次完整刷新。 */
export interface ZrStructureController extends vscode.Disposable {
    refresh(): Promise<void>;
}

/** 注册三个结构视图；Web 调用方关闭项目索引，但仍保留当前文件和内置库视图。 */
export function registerZrStructureViews(
    context: vscode.ExtensionContext,
    options: { projectIndexAvailable?: boolean } = {},
): ZrStructureController {
    return new ZrStructureService(context, options.projectIndexAvailable ?? true);
}

/** 将一次刷新得到的树根发布给 VS Code；服务层负责计算数据与释放提供器。 */
class StructureTreeProvider implements vscode.TreeDataProvider<TreeNode> {
    private readonly onDidChangeTreeDataEmitter = new vscode.EventEmitter<TreeNode | undefined>();
    private roots: TreeNode[] = [];

    readonly onDidChangeTreeData = this.onDidChangeTreeDataEmitter.event;

    /** 以完整快照替换树根，避免三个视图各自维护增量状态。 */
    setRoots(roots: TreeNode[]): void {
        this.roots = roots;
        this.onDidChangeTreeDataEmitter.fire(undefined);
    }

    /** 把内部节点的展示与导航信息交给 TreeView。 */
    getTreeItem(element: TreeNode): vscode.TreeItem {
        const item = new vscode.TreeItem(element.label, element.collapsibleState);
        item.id = element.id;
        item.description = element.description;
        item.tooltip = element.tooltip;
        item.iconPath = element.icon;
        item.resourceUri = element.uri;
        item.command = element.command;
        return item;
    }

    /** 根节点和子节点共用同一套快照；调用方不应修改返回数组。 */
    getChildren(element?: TreeNode): Thenable<TreeNode[]> {
        return Promise.resolve(element?.children ?? this.roots);
    }

    /** 解除视图变化事件，随服务注销。 */
    dispose(): void {
        this.onDidChangeTreeDataEmitter.dispose();
    }
}

/** 汇合编辑器、项目选择、语言客户端和文件事件，再发布三个结构树。 */
class ZrStructureService implements ZrStructureController {
    private readonly disposables: vscode.Disposable[] = [];
    private readonly filesProvider = new StructureTreeProvider();
    private readonly projectProvider = new StructureTreeProvider();
    private readonly builtinProvider = new StructureTreeProvider();
    private readonly filesView: vscode.TreeView<TreeNode>;
    private readonly projectView: vscode.TreeView<TreeNode>;
    private readonly builtinView: vscode.TreeView<TreeNode>;
    private refreshChain: Promise<void> = Promise.resolve();
    private refreshTimer: ReturnType<typeof setTimeout> | undefined;
    private filesRoots: TreeNode[] = [];
    private projectRoots: TreeNode[] = [];
    private builtinRoots: TreeNode[] = [];

    /** 视图和订阅与扩展上下文同寿命；projectIndexAvailable 是 Web 的能力边界。 */
    constructor(private readonly context: vscode.ExtensionContext, private readonly projectIndexAvailable: boolean) {
        this.filesView = vscode.window.createTreeView(ZR_FILES_VIEW_ID, {
            treeDataProvider: this.filesProvider,
            showCollapseAll: true,
        });
        this.projectView = vscode.window.createTreeView(ZR_IMPORTS_VIEW_ID, {
            treeDataProvider: this.projectProvider,
            showCollapseAll: true,
        });
        this.builtinView = vscode.window.createTreeView(ZR_BUILTIN_MODULES_VIEW_ID, {
            treeDataProvider: this.builtinProvider,
            showCollapseAll: true,
        });

        this.disposables.push(
            this.filesProvider,
            this.projectProvider,
            this.builtinProvider,
            this.filesView,
            this.projectView,
            this.builtinView,
            vscode.commands.registerCommand(ZR_STRUCTURE_REFRESH_COMMAND, async () => {
                await this.refresh();
            }),
            vscode.commands.registerCommand(ZR_STRUCTURE_INSPECT_COMMAND, async () => {
                await this.refresh();
                return {
                    files: this.filesRoots.map((node) => serializeNode(node)),
                    imports: this.projectRoots.map((node) => serializeNode(node)),
                    project: this.projectRoots.map((node) => serializeNode(node)),
                    builtin: this.builtinRoots.map((node) => serializeNode(node)),
                };
            }),
            vscode.commands.registerCommand(ZR_STRUCTURE_OPEN_TARGET_COMMAND, async (payload: OpenTargetPayload) => {
                await openTarget(payload);
            }),
            vscode.window.onDidChangeActiveTextEditor(() => {
                this.scheduleRefresh();
            }),
            vscode.workspace.onDidChangeTextDocument((event) => {
                if (event.document.languageId === 'zr') {
                    this.scheduleRefresh();
                }
            }),
            vscode.workspace.onDidOpenTextDocument((document) => {
                if (document.languageId === 'zr') {
                    this.scheduleRefresh();
                }
            }),
            vscode.workspace.onDidSaveTextDocument((document) => {
                if (document.languageId === 'zr' || isZrpDocument(document)) {
                    this.scheduleRefresh();
                }
            }),
            vscode.workspace.onDidCreateFiles(() => {
                this.scheduleRefresh();
            }),
            vscode.workspace.onDidDeleteFiles(() => {
                this.scheduleRefresh();
            }),
            vscode.workspace.onDidRenameFiles(() => {
                this.scheduleRefresh();
            }),
            vscode.workspace.onDidChangeWorkspaceFolders(() => {
                this.scheduleRefresh();
            }),
            onDidChangeLanguageClient(() => {
                this.scheduleRefresh();
            }),
            onDidChangeSelectedProject(() => {
                this.scheduleRefresh();
            }),
        );

        // TODO: 激活入口还把控制器本身加入 context.subscriptions；核实 VS Code 对视图、
        // 命令和事件订阅的重复 dispose 契约，避免关闭时重复释放这些子资源。
        context.subscriptions.push(...this.disposables);
        // BUG: 首次刷新若因符号提供器或项目请求拒绝而失败，这个 Promise 被丢弃，
        // 产生未处理的拒绝，初始结构树也不会发布；事件定时刷新有同类风险。
        void this.refresh();
    }

    /** 串行化各个入口发起的完整刷新，避免旧异步任务在较新任务之后覆盖树根。 */
    async refresh(): Promise<void> {
        this.refreshChain = this.refreshChain.then(
            async () => {
                await this.performRefresh();
            },
            async () => {
                await this.performRefresh();
            },
        );
        await this.refreshChain;
    }

    /** 停止后续定时刷新并释放注册资源；已开始的异步请求尚不能取消。 */
    dispose(): void {
        if (this.refreshTimer !== undefined) {
            clearTimeout(this.refreshTimer);
            this.refreshTimer = undefined;
        }

        for (const disposable of this.disposables) {
            disposable.dispose();
        }
        this.disposables.length = 0;
    }

    /** 合并编辑器和工作区事件的短时连发，最终仍走串行刷新链。 */
    private scheduleRefresh(): void {
        if (this.refreshTimer !== undefined) {
            clearTimeout(this.refreshTimer);
        }

        this.refreshTimer = setTimeout(() => {
            this.refreshTimer = undefined;
            // BUG: 删除清单与读取竞态或符号提供器拒绝时 refresh 会拒绝，定时器丢弃
            // Promise，产生未处理的拒绝，且本次结构树不会更新；事件入口无法报告失败。
            void this.refresh();
        }, REFRESH_DEBOUNCE_MS);
    }

    /** 在相同刷新任务中计算三个视图，Web 用明确的不可用节点代替项目扫描。 */
    private async performRefresh(): Promise<void> {
        this.filesRoots = await buildCurrentFileRoots();
        this.projectRoots = this.projectIndexAvailable
            ? await buildProjectRoots(this.context)
            : [createInfoNode('project:unavailable:web',
                'Project indexing is unavailable in VS Code Web. Open a Zr file to use language features.')];
        this.builtinRoots = buildBuiltinLibraryRoots();
        this.filesProvider.setRoots(this.filesRoots);
        this.projectProvider.setRoots(this.projectRoots);
        this.builtinProvider.setRoots(this.builtinRoots);
    }
}

/** 把静态内置库快照变为可浏览目录；Web 无需语言服务器即可展示相同列表。 */
function buildBuiltinLibraryRoots(): TreeNode[] {
    const snapshots = listBuiltinModuleSnapshots();
    return snapshots.map((snapshot) => {
        const linkNodes: TreeNode[] =
            snapshot.modules?.map((link) => ({
                id: `builtin:link:${link.moduleName}`,
                nodeType: 'import' as const,
                label: link.name,
                description: link.detail,
                tooltip: link.moduleName,
                icon: new vscode.ThemeIcon('package'),
                collapsibleState: vscode.TreeItemCollapsibleState.None,
                children: [],
            })) ?? [];
        const symbolNodes: TreeNode[] =
            snapshot.symbols?.map((symbol) => ({
                id: `builtin:sym:${snapshot.moduleName}:${symbol.name}`,
                nodeType: 'declaration' as const,
                label: symbol.name,
                description: symbol.kind,
                tooltip: `${snapshot.moduleName}.${symbol.name}`,
                icon: new vscode.ThemeIcon(
                    symbol.kind === 'type' ? 'symbol-interface' : symbol.kind === 'constant' ? 'symbol-variable' : 'symbol-method',
                ),
                collapsibleState: vscode.TreeItemCollapsibleState.None,
                children: [],
            })) ?? [];
        const groups: TreeNode[] = [];
        if (linkNodes.length > 0) {
            groups.push(createGroupNode(`builtin:${snapshot.moduleName}:submodules`, 'Submodules', linkNodes));
        }
        if (symbolNodes.length > 0) {
            groups.push(createGroupNode(`builtin:${snapshot.moduleName}:symbols`, 'Symbols', symbolNodes));
        }
        return {
            id: `builtin:root:${snapshot.moduleName}`,
            nodeType: 'module',
            label: snapshot.moduleName,
            description: snapshot.detail,
            tooltip: snapshot.detail ?? snapshot.moduleName,
            icon: new vscode.ThemeIcon('library'),
            collapsibleState: groups.length > 0
                ? vscode.TreeItemCollapsibleState.Collapsed
                : vscode.TreeItemCollapsibleState.None,
            children: groups,
        };
    });
}

/** 当前编辑器优先使用 VS Code 符号提供器，文本回退只承担模块名与导入列表。 */
async function buildCurrentFileRoots(): Promise<TreeNode[]> {
    const editor = vscode.window.activeTextEditor;
    const document = editor?.document;

    if (!document || document.languageId !== 'zr') {
        return [
            createInfoNode(
                'current-file:empty',
                'Open a .zr file to see its structure.',
            ),
        ];
    }

    const moduleName = parseModuleName(document);
    const imports = parseImports(document);
    const symbols = await loadDocumentSymbols(document.uri);
    const importsGroup = createGroupNode(
        `current-file:${document.uri.toString()}:imports`,
        'Imports',
        imports.map((entry) => createImportNode(document, entry)),
    );
    const declarationsGroup = createGroupNode(
        `current-file:${document.uri.toString()}:declarations`,
        'Declarations',
        symbols.map((symbol) => createDeclarationNode(document, symbol)),
    );
    const fileRange = firstNavigableRange(symbols);

    return [
        {
            id: `current-file:${document.uri.toString()}`,
            nodeType: 'file',
            label: moduleName,
            description: vscode.workspace.asRelativePath(document.uri, false),
            tooltip: document.uri.fsPath || document.uri.toString(),
            uri: document.uri,
            icon: new vscode.ThemeIcon('file'),
            collapsibleState: vscode.TreeItemCollapsibleState.Expanded,
            command: createRangeCommand(document.uri, fileRange),
            children: [importsGroup, declarationsGroup],
        },
    ];
}

/** 按当前已选清单展示来源分类；LSP 不可用时仍保留项目操作和入口回退。 */
async function buildProjectRoots(context: vscode.ExtensionContext): Promise<TreeNode[]> {
    const selectedProject = await resolveSelectedWorkspaceProject(context, activeWorkspaceFolder(), false);
    const actionNodes = [
        createActionNode('project-action:select', 'Select Project', 'zr.selectProject', 'list-selection'),
        createActionNode('project-action:run', 'Run Selected Project', 'zr.runSelectedProject', 'play'),
        createActionNode('project-action:debug', 'Debug Selected Project', 'zr.debugSelectedProject', 'debug-alt-small'),
    ];

    if (!selectedProject) {
        return [
            ...actionNodes,
            createInfoNode(
                'project:empty',
                'No selected ZR project. Use "Select Project".',
            ),
        ];
    }

    // 可选请求在客户端未启动或服务端不支持时返回空结果，清单入口仍须可见。
    const summaries = await sendLanguageServerRequest<ProjectModuleSummaryPayload[]>('zr/projectModules', {
        uri: selectedProject.uri.toString(),
    }) ?? [];
    const projectModuleSummaries = ensureManifestProjectEntrySummary(
        selectedProject,
        summaries.filter((summary) => isProjectSourceKind(summary.sourceKind)),
    );
    const projectModuleNodes = projectModuleSummaries
        .sort(compareProjectModuleSummary)
        .map((summary) => createProjectModuleNode(summary));
    const binaryModuleNodes = summaries
        .filter((summary) => summary.sourceKind === 3)
        .sort(compareProjectModuleSummary)
        .map((summary) => createProjectModuleNode(summary));
    const nativeModuleNodes = summaries
        .filter((summary) => summary.sourceKind === 4 || summary.sourceKind === 5)
        .sort(compareProjectModuleSummary)
        .map((summary) => createProjectModuleNode(summary));

    return [
        ...actionNodes,
        {
            id: `project:${selectedProject.id}`,
            nodeType: 'project',
            label: selectedProject.label,
            description: selectedProject.relativePath,
            tooltip: selectedProject.uri.fsPath || selectedProject.uri.toString(),
            uri: selectedProject.uri,
            icon: new vscode.ThemeIcon('folder-library'),
            collapsibleState: vscode.TreeItemCollapsibleState.Expanded,
            command: createRangeCommand(selectedProject.uri, new vscode.Range(0, 0, 0, 0)),
            children: [
                createGroupNode('project:modules:source', 'Project Modules', projectModuleNodes),
                createGroupNode('project:modules:native', 'Native Modules', nativeModuleNodes),
                createGroupNode('project:modules:binary', 'Binary Modules', binaryModuleNodes),
            ],
        },
    ];
}

/** 用无命令叶节点解释当前文件、项目或 Web 能力为空的原因。 */
function createInfoNode(id: string, label: string): TreeNode {
    return {
        id,
        nodeType: 'info',
        label,
        icon: new vscode.ThemeIcon('info'),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        children: [],
    };
}

/** 为视图分组提供稳定 ID，刷新时可沿用用户的展开状态。 */
function createGroupNode(id: string, label: string, children: TreeNode[]): TreeNode {
    return {
        id,
        nodeType: 'group',
        label,
        icon: new vscode.ThemeIcon('symbol-folder'),
        collapsibleState: vscode.TreeItemCollapsibleState.Expanded,
        children,
    };
}

/** 结构树复用已注册的项目命令，点击后由项目操作控制器处理。 */
function createActionNode(
    id: string,
    label: string,
    commandId: string,
    iconId: string,
): TreeNode {
    return {
        id,
        nodeType: 'action',
        label,
        icon: new vscode.ThemeIcon(iconId),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        command: {
            command: commandId,
            title: label,
        },
        children: [],
    };
}

/** 点击导入先询问定义提供器；无结果时尝试同目录源码或原导入文本。 */
function createImportNode(document: vscode.TextDocument, entry: ImportEntry): TreeNode {
    const fallbackUri = createWorkspaceImportFallbackUri(document.uri, entry.moduleName);
    return {
        id: `import:${document.uri.toString()}:${entry.moduleName}:${entry.range.start.line}:${entry.range.start.character}`,
        nodeType: 'import',
        label: entry.moduleName,
        description: entry.alias ? `as ${entry.alias}` : undefined,
        tooltip: entry.alias ? `let ${entry.alias} = import("${entry.moduleName}")` : `import("${entry.moduleName}")`,
        uri: document.uri,
        icon: new vscode.ThemeIcon('package'),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        command: createDefinitionCommand(document.uri, entry.moduleLiteralRange.start, fallbackUri, entry.range),
        children: [],
    };
}

/** 保留符号提供器的层级和选择范围，声明树的语义由服务端决定。 */
function createDeclarationNode(document: vscode.TextDocument, symbol: vscode.DocumentSymbol): TreeNode {
    const selectionRange = symbol.selectionRange ?? symbol.range;
    return {
        id: `declaration:${document.uri.toString()}:${symbol.name}:${selectionRange.start.line}:${selectionRange.start.character}`,
        nodeType: 'declaration',
        label: symbol.name,
        description: symbol.detail || undefined,
        tooltip: symbol.detail ? `${symbol.name}: ${symbol.detail}` : symbol.name,
        uri: document.uri,
        icon: symbolThemeIcon(symbol.kind),
        collapsibleState: symbol.children.length > 0
            ? vscode.TreeItemCollapsibleState.Collapsed
            : vscode.TreeItemCollapsibleState.None,
        command: createRangeCommand(document.uri, selectionRange),
        children: symbol.children.map((child) => createDeclarationNode(document, child)),
    };
}

/** 为服务端项目摘要建立导航节点；没有 URI 的项仅作只读目录条目。 */
function createProjectModuleNode(summary: ProjectModuleSummaryPayload): TreeNode {
    const navigationUri = summary.navigationUri ? vscode.Uri.parse(summary.navigationUri) : undefined;
    const range = summary.range ? deserializeRange(summary.range) : new vscode.Range(0, 0, 0, 0);
    const description = summary.isEntry
        ? summary.description
            ? `entry, ${summary.description}`
            : 'entry'
        : summary.description;

    return {
        id: `project-module:${summary.sourceKind}:${summary.moduleName}`,
        nodeType: 'module',
        label: summary.displayName || summary.moduleName,
        description,
        tooltip: navigationUri ? navigationUri.toString() : summary.moduleName,
        uri: navigationUri,
        icon: projectModuleIcon(summary.sourceKind),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        command: navigationUri ? createRangeCommand(navigationUri, range) : undefined,
        children: [],
    };
}

/** 服务端索引暂缺入口时，用清单信息补一个可导航条目以维持项目概览。 */
function ensureManifestProjectEntrySummary(
    project: WorkspaceProject,
    summaries: ProjectModuleSummaryPayload[],
): ProjectModuleSummaryPayload[] {
    const entryModuleName = project.manifest.entry;
    if (!entryModuleName || summaries.some((summary) => summary.moduleName === entryModuleName)) {
        return summaries;
    }

    // TODO: `app/main` 一类子目录入口已有测试；进一步核实 entry 使用路径别名、
    // 绝对路径或不存在文件时，此处补造 URI 是否与服务端源路径解析一致。
    const entryUri = vscode.Uri.joinPath(project.uri, '..', project.manifest.source, `${entryModuleName}.zr`);
    return [
        {
            sourceKind: 1,
            isEntry: true,
            moduleName: entryModuleName,
            displayName: entryModuleName,
            description: 'manifest entry',
            navigationUri: entryUri.toString(),
            range: serializeRange(new vscode.Range(0, 0, 0, 0)),
        },
        ...summaries,
    ];
}

/** 已知项目或声明范围直接携带可序列化坐标，延迟到点击时打开文件。 */
function createRangeCommand(uri: vscode.Uri, range: vscode.Range): vscode.Command {
    return {
        command: ZR_STRUCTURE_OPEN_TARGET_COMMAND,
        title: 'Open ZR Structure Target',
        arguments: [
            {
                kind: 'range',
                uri: uri.toString(),
                range: serializeRange(range),
            } satisfies OpenTargetPayload,
        ],
    };
}

/** 导入导航保存主查询和回退位置，避免视图刷新时提前触发定义查询。 */
function createDefinitionCommand(
    uri: vscode.Uri,
    position: vscode.Position,
    fallbackUri: vscode.Uri | undefined,
    fallbackRange: vscode.Range,
): vscode.Command {
    return {
        command: ZR_STRUCTURE_OPEN_TARGET_COMMAND,
        title: 'Open ZR Structure Target',
        arguments: [
            {
                kind: 'definition',
                uri: uri.toString(),
                position: serializePosition(position),
                fallbackUri: fallbackUri?.toString(),
                fallbackRange: serializeRange(fallbackUri ? new vscode.Range(0, 0, 0, 0) : fallbackRange),
            } satisfies OpenTargetPayload,
        ],
    };
}

/** 检查命令输出可断言的快照；仅用于测试与诊断，不参与树视图更新。 */
function serializeNode(node: TreeNode): SerializedTreeNode {
    return {
        id: node.id,
        nodeType: node.nodeType,
        label: node.label,
        description: node.description,
        uri: node.uri?.toString(),
        commandId: node.command?.command,
        commandArguments: node.command?.arguments,
        children: node.children.map((child) => serializeNode(child)),
    };
}

/** 当前文件标题优先取源码中的 module 声明，缺失时退回文件名。 */
function parseModuleName(document: vscode.TextDocument): string {
    // BUG: 文本正则没有排除块注释；注释内独占一行的 `module fake;` 会成为
    // 当前文件树标题，尽管符号提供器与编译器不会把它当成模块声明。
    const match = MODULE_PATTERN.exec(document.getText());
    const moduleName = match?.[2] ?? match?.[3];
    if (moduleName) {
        return moduleName;
    }

    return removeExtension(lastPathSegment(document.uri.path));
}

/** 独立于语言服务器提取导入导航项，供未连接时的当前文件树使用。 */
function parseImports(document: vscode.TextDocument): ImportEntry[] {
    const text = document.getText();
    const entries: ImportEntry[] = [];
    IMPORT_PATTERN.lastIndex = 0;

    // BUG: 直接扫描未分词文本，`// import("ghost")` 和字符串里的 import 调用
    // 也会生成可见的 Imports 节点；需改由语法服务或词法状态判定真实调用。
    while (true) {
        const match = IMPORT_PATTERN.exec(text);
        if (!match) {
            break;
        }

        const fullStart = document.positionAt(match.index);
        const fullEnd = document.positionAt(match.index + match[0].length);
        const moduleLiteralStartOffset = match.index + match[0].lastIndexOf(match[3]);
        const moduleLiteralEndOffset = moduleLiteralStartOffset + match[3].length;

        entries.push({
            alias: match[1] || undefined,
            moduleName: match[3],
            range: new vscode.Range(fullStart, fullEnd),
            moduleLiteralRange: new vscode.Range(
                document.positionAt(moduleLiteralStartOffset),
                document.positionAt(moduleLiteralEndOffset),
            ),
        });
    }

    return entries;
}

/** 仅为普通单段工作区模块猜测同目录目标；内置库与路径导入交给定义提供器。 */
function createWorkspaceImportFallbackUri(documentUri: vscode.Uri, moduleName: string): vscode.Uri | undefined {
    if (!moduleName || moduleName.startsWith('zr.') || moduleName.includes('/') || moduleName.includes('\\')) {
        return undefined;
    }

    return vscode.Uri.joinPath(documentUri, '..', `${moduleName}.zr`);
}

/** 兼容符号提供器的层级与扁平结果，维持统一的声明树输入。 */
async function loadDocumentSymbols(uri: vscode.Uri): Promise<vscode.DocumentSymbol[]> {
    const result = await vscode.commands.executeCommand<(vscode.DocumentSymbol | vscode.SymbolInformation)[] | undefined>(
        'vscode.executeDocumentSymbolProvider',
        uri,
    );
    if (!Array.isArray(result) || result.length === 0) {
        return [];
    }

    if (isDocumentSymbol(result[0])) {
        return result as vscode.DocumentSymbol[];
    }

    return (result as vscode.SymbolInformation[]).map((symbol) =>
        new vscode.DocumentSymbol(
            symbol.name,
            symbol.containerName || '',
            symbol.kind,
            symbol.location.range,
            symbol.location.range,
        ));
}

/** 只在符号提供器返回层级结果时保留其 children。 */
function isDocumentSymbol(value: vscode.DocumentSymbol | vscode.SymbolInformation): value is vscode.DocumentSymbol {
    return Array.isArray((value as vscode.DocumentSymbol).children) &&
        (value as vscode.DocumentSymbol).selectionRange !== undefined;
}

/** 文件根点击时指向首个声明；空文件仍可打开编辑器的起点。 */
function firstNavigableRange(symbols: vscode.DocumentSymbol[]): vscode.Range {
    if (symbols.length === 0) {
        return new vscode.Range(0, 0, 0, 0);
    }

    return symbols[0].selectionRange ?? symbols[0].range;
}

/** 视图节点的统一打开入口；定义查询无结果时按构造命令时保存的回退位置导航。 */
async function openTarget(payload: OpenTargetPayload): Promise<void> {
    const uri = vscode.Uri.parse(payload.uri);
    if (payload.kind === 'definition' && payload.position) {
        const location = await resolveDefinitionLocation(uri, deserializePosition(payload.position));
        if (location) {
            await revealLocation(location.uri, location.range);
            return;
        }

        if (payload.fallbackRange) {
            // BUG: 单段普通模块的同目录回退 URI 未检查文件是否存在；定义提供器
            // 无结果且目标文件不存在时 openTextDocument 拒绝，命令不会回到原导入位置。
            const fallbackUri = payload.fallbackUri ? vscode.Uri.parse(payload.fallbackUri) : uri;
            await revealLocation(fallbackUri, deserializeRange(payload.fallbackRange));
            return;
        }
    }

    if (payload.range) {
        await revealLocation(uri, deserializeRange(payload.range));
    }
}

/** 通过 VS Code 定义提供器跨模块定位，接受 Location 与 LocationLink 形状。 */
async function resolveDefinitionLocation(
    uri: vscode.Uri,
    position: vscode.Position,
): Promise<{ uri: vscode.Uri; range: vscode.Range } | undefined> {
    const definition = await vscode.commands.executeCommand<any[]>(
        'vscode.executeDefinitionProvider',
        uri,
        position,
    );
    if (!Array.isArray(definition) || definition.length === 0) {
        return undefined;
    }

    const first = definition[0];
    const targetUri = first?.uri ?? first?.targetUri ?? first?.location?.uri;
    const targetRange = first?.range ?? first?.targetSelectionRange ?? first?.targetRange ?? first?.location?.range;
    if (!targetUri || !targetRange) {
        return undefined;
    }

    return { uri: targetUri, range: targetRange };
}

/** 导航动作交由编辑器打开并聚焦目标，与刷新视图的只读计算隔离。 */
async function revealLocation(uri: vscode.Uri, range: vscode.Range): Promise<void> {
    const document = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(document, {
        preview: false,
        preserveFocus: false,
    });
    editor.selection = new vscode.Selection(range.start, range.start);
    editor.revealRange(range, vscode.TextEditorRevealType.InCenterIfOutsideViewport);
}

/** 声明图标沿用符号提供器的种类，未知种类降级为通用符号。 */
function symbolThemeIcon(kind: vscode.SymbolKind): vscode.ThemeIcon {
    switch (kind) {
        case vscode.SymbolKind.Class:
            return new vscode.ThemeIcon('symbol-class');
        case vscode.SymbolKind.Method:
            return new vscode.ThemeIcon('symbol-method');
        case vscode.SymbolKind.Property:
        case vscode.SymbolKind.Field:
            return new vscode.ThemeIcon('symbol-field');
        case vscode.SymbolKind.Function:
            return new vscode.ThemeIcon('symbol-function');
        case vscode.SymbolKind.Enum:
        case vscode.SymbolKind.EnumMember:
            return new vscode.ThemeIcon('symbol-enum');
        case vscode.SymbolKind.Interface:
            return new vscode.ThemeIcon('symbol-interface');
        case vscode.SymbolKind.Variable:
            return new vscode.ThemeIcon('symbol-variable');
        default:
            return new vscode.ThemeIcon('symbol-misc');
    }
}

/** 服务端来源枚举映射为项目树图标；枚举定义见 lsp_interface_internal.h。 */
function projectModuleIcon(sourceKind: number): vscode.ThemeIcon {
    switch (sourceKind) {
        case 1:
        case 2:
            return new vscode.ThemeIcon('symbol-file');
        case 3:
            return new vscode.ThemeIcon('package');
        case 4:
        case 5:
            return new vscode.ThemeIcon('library');
        default:
            return new vscode.ThemeIcon('symbol-misc');
    }
}

/** 仅把源码与 FFI 包装源码放进项目源码组，避免二进制和原生库混列。 */
function isProjectSourceKind(sourceKind: number): boolean {
    return sourceKind === 1 || sourceKind === 2;
}

/** 项目入口固定排在组首，其余模块按用户可见名称排序。 */
function compareProjectModuleSummary(left: ProjectModuleSummaryPayload, right: ProjectModuleSummaryPayload): number {
    if (left.isEntry && !right.isEntry) {
        return -1;
    }
    if (!left.isEntry && right.isEntry) {
        return 1;
    }

    return (left.displayName || left.moduleName).localeCompare(right.displayName || right.moduleName);
}

/** 将编辑器坐标转为命令参数和测试快照可传递的纯数据。 */
function serializePosition(position: vscode.Position): SerializedPosition {
    return {
        line: position.line,
        character: position.character,
    };
}

/** 与 serializePosition 共享零基坐标约定。 */
function serializeRange(range: vscode.Range): SerializedRange {
    return {
        start: serializePosition(range.start),
        end: serializePosition(range.end),
    };
}

/** 命令执行时恢复 VS Code 位置对象，不在树快照中保存宿主对象。 */
function deserializePosition(position: SerializedPosition): vscode.Position {
    return new vscode.Position(position.line, position.character);
}

/** 导航入口恢复范围；调用方应提供同一文档的合法坐标。 */
function deserializeRange(range: SerializedRange): vscode.Range {
    return new vscode.Range(
        deserializePosition(range.start),
        deserializePosition(range.end),
    );
}

/** 无 module 声明时，以文件名作为结构根的用户可见标题。 */
function removeExtension(value: string): string {
    const lastDot = value.lastIndexOf('.');
    return lastDot > 0 ? value.slice(0, lastDot) : value;
}

/** 对文件 URI 的路径统一处理两类分隔符，供文件名回退使用。 */
function lastPathSegment(pathValue: string): string {
    const normalized = pathValue.replace(/[\\/]+/g, '/');
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}
