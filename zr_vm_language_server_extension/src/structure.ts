import * as vscode from 'vscode';
import { onDidChangeLanguageClient, sendLanguageServerRequest } from './languageClientRequests';
import {
    activeWorkspaceFolder,
    isZrpDocument,
    onDidChangeSelectedProject,
    resolveSelectedWorkspaceProject,
    type WorkspaceProject,
} from './workspaceProjects';

export const ZR_FILES_VIEW_ID = 'zrFiles';
export const ZR_IMPORTS_VIEW_ID = 'zrImports';
export const ZR_STRUCTURE_REFRESH_COMMAND = 'zr.structure.refresh';
export const ZR_STRUCTURE_INSPECT_COMMAND = 'zr.__inspectStructureViews';
export const ZR_STRUCTURE_OPEN_TARGET_COMMAND = 'zr.structure.openTarget';

const MODULE_PATTERN = /^\s*module\s+(['"])([^'"]+)\1\s*;/m;
const IMPORT_PATTERN = /(?:\bvar\s+([A-Za-z_]\w*)\s*=\s*)?%import\s*\(\s*(['"])([^'"]+)\2\s*\)/g;
const REFRESH_DEBOUNCE_MS = 150;

type NodeType = 'info' | 'group' | 'file' | 'import' | 'declaration' | 'project' | 'module' | 'action';
type OpenTargetKind = 'range' | 'definition';

interface SerializedPosition {
    line: number;
    character: number;
}

interface SerializedRange {
    start: SerializedPosition;
    end: SerializedPosition;
}

interface OpenTargetPayload {
    kind: OpenTargetKind;
    uri: string;
    range?: SerializedRange;
    position?: SerializedPosition;
    fallbackRange?: SerializedRange;
}

interface ProjectModuleSummaryPayload {
    sourceKind: number;
    isEntry: boolean;
    moduleName: string;
    displayName?: string;
    description?: string;
    navigationUri?: string;
    range?: SerializedRange;
}

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

interface ImportEntry {
    alias?: string;
    moduleName: string;
    range: vscode.Range;
    moduleLiteralRange: vscode.Range;
}

export interface ZrStructureController extends vscode.Disposable {
    refresh(): Promise<void>;
}

export function registerZrStructureViews(context: vscode.ExtensionContext): ZrStructureController {
    return new ZrStructureService(context);
}

class StructureTreeProvider implements vscode.TreeDataProvider<TreeNode> {
    private readonly onDidChangeTreeDataEmitter = new vscode.EventEmitter<TreeNode | undefined>();
    private roots: TreeNode[] = [];

    readonly onDidChangeTreeData = this.onDidChangeTreeDataEmitter.event;

    setRoots(roots: TreeNode[]): void {
        this.roots = roots;
        this.onDidChangeTreeDataEmitter.fire(undefined);
    }

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

    getChildren(element?: TreeNode): Thenable<TreeNode[]> {
        return Promise.resolve(element?.children ?? this.roots);
    }

    dispose(): void {
        this.onDidChangeTreeDataEmitter.dispose();
    }
}

class ZrStructureService implements ZrStructureController {
    private readonly disposables: vscode.Disposable[] = [];
    private readonly filesProvider = new StructureTreeProvider();
    private readonly projectProvider = new StructureTreeProvider();
    private readonly filesView: vscode.TreeView<TreeNode>;
    private readonly projectView: vscode.TreeView<TreeNode>;
    private refreshChain: Promise<void> = Promise.resolve();
    private refreshTimer: ReturnType<typeof setTimeout> | undefined;
    private filesRoots: TreeNode[] = [];
    private projectRoots: TreeNode[] = [];

    constructor(private readonly context: vscode.ExtensionContext) {
        this.filesView = vscode.window.createTreeView(ZR_FILES_VIEW_ID, {
            treeDataProvider: this.filesProvider,
            showCollapseAll: true,
        });
        this.projectView = vscode.window.createTreeView(ZR_IMPORTS_VIEW_ID, {
            treeDataProvider: this.projectProvider,
            showCollapseAll: true,
        });

        this.disposables.push(
            this.filesProvider,
            this.projectProvider,
            this.filesView,
            this.projectView,
            vscode.commands.registerCommand(ZR_STRUCTURE_REFRESH_COMMAND, async () => {
                await this.refresh();
            }),
            vscode.commands.registerCommand(ZR_STRUCTURE_INSPECT_COMMAND, async () => {
                await this.refresh();
                return {
                    files: this.filesRoots.map((node) => serializeNode(node)),
                    imports: this.projectRoots.map((node) => serializeNode(node)),
                    project: this.projectRoots.map((node) => serializeNode(node)),
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

        context.subscriptions.push(...this.disposables);
        void this.refresh();
    }

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

    private scheduleRefresh(): void {
        if (this.refreshTimer !== undefined) {
            clearTimeout(this.refreshTimer);
        }

        this.refreshTimer = setTimeout(() => {
            this.refreshTimer = undefined;
            void this.refresh();
        }, REFRESH_DEBOUNCE_MS);
    }

    private async performRefresh(): Promise<void> {
        this.filesRoots = await buildCurrentFileRoots();
        this.projectRoots = await buildProjectRoots(this.context);
        this.filesProvider.setRoots(this.filesRoots);
        this.projectProvider.setRoots(this.projectRoots);
    }
}

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

    const summaries = await sendLanguageServerRequest<ProjectModuleSummaryPayload[]>('zr/projectModules', {
        uri: selectedProject.uri.toString(),
    }) ?? [];
    const projectModuleNodes = summaries
        .filter((summary) => isProjectSourceKind(summary.sourceKind))
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

function createImportNode(document: vscode.TextDocument, entry: ImportEntry): TreeNode {
    return {
        id: `import:${document.uri.toString()}:${entry.moduleName}:${entry.range.start.line}:${entry.range.start.character}`,
        nodeType: 'import',
        label: entry.moduleName,
        description: entry.alias ? `as ${entry.alias}` : undefined,
        tooltip: entry.alias ? `${entry.alias} = %import("${entry.moduleName}")` : `%import("${entry.moduleName}")`,
        uri: document.uri,
        icon: new vscode.ThemeIcon('package'),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        command: createDefinitionCommand(document.uri, entry.moduleLiteralRange.start, entry.range),
        children: [],
    };
}

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

function createDefinitionCommand(
    uri: vscode.Uri,
    position: vscode.Position,
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
                fallbackRange: serializeRange(fallbackRange),
            } satisfies OpenTargetPayload,
        ],
    };
}

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

function parseModuleName(document: vscode.TextDocument): string {
    const match = MODULE_PATTERN.exec(document.getText());
    if (match?.[2]) {
        return match[2];
    }

    return removeExtension(lastPathSegment(document.uri.path));
}

function parseImports(document: vscode.TextDocument): ImportEntry[] {
    const text = document.getText();
    const entries: ImportEntry[] = [];
    IMPORT_PATTERN.lastIndex = 0;

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

function isDocumentSymbol(value: vscode.DocumentSymbol | vscode.SymbolInformation): value is vscode.DocumentSymbol {
    return Array.isArray((value as vscode.DocumentSymbol).children) &&
        (value as vscode.DocumentSymbol).selectionRange !== undefined;
}

function firstNavigableRange(symbols: vscode.DocumentSymbol[]): vscode.Range {
    if (symbols.length === 0) {
        return new vscode.Range(0, 0, 0, 0);
    }

    return symbols[0].selectionRange ?? symbols[0].range;
}

async function openTarget(payload: OpenTargetPayload): Promise<void> {
    const uri = vscode.Uri.parse(payload.uri);
    if (payload.kind === 'definition' && payload.position) {
        const location = await resolveDefinitionLocation(uri, deserializePosition(payload.position));
        if (location) {
            await revealLocation(location.uri, location.range);
            return;
        }

        if (payload.fallbackRange) {
            await revealLocation(uri, deserializeRange(payload.fallbackRange));
            return;
        }
    }

    if (payload.range) {
        await revealLocation(uri, deserializeRange(payload.range));
    }
}

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

async function revealLocation(uri: vscode.Uri, range: vscode.Range): Promise<void> {
    const document = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(document, {
        preview: false,
        preserveFocus: false,
    });
    editor.selection = new vscode.Selection(range.start, range.start);
    editor.revealRange(range, vscode.TextEditorRevealType.InCenterIfOutsideViewport);
}

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

function isProjectSourceKind(sourceKind: number): boolean {
    return sourceKind === 1 || sourceKind === 2;
}

function compareProjectModuleSummary(left: ProjectModuleSummaryPayload, right: ProjectModuleSummaryPayload): number {
    if (left.isEntry && !right.isEntry) {
        return -1;
    }
    if (!left.isEntry && right.isEntry) {
        return 1;
    }

    return (left.displayName || left.moduleName).localeCompare(right.displayName || right.moduleName);
}

function serializePosition(position: vscode.Position): SerializedPosition {
    return {
        line: position.line,
        character: position.character,
    };
}

function serializeRange(range: vscode.Range): SerializedRange {
    return {
        start: serializePosition(range.start),
        end: serializePosition(range.end),
    };
}

function deserializePosition(position: SerializedPosition): vscode.Position {
    return new vscode.Position(position.line, position.character);
}

function deserializeRange(range: SerializedRange): vscode.Range {
    return new vscode.Range(
        deserializePosition(range.start),
        deserializePosition(range.end),
    );
}

function removeExtension(value: string): string {
    const lastDot = value.lastIndexOf('.');
    return lastDot > 0 ? value.slice(0, lastDot) : value;
}

function lastPathSegment(pathValue: string): string {
    const normalized = pathValue.replace(/[\\/]+/g, '/');
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}
