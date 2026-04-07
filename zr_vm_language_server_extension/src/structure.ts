import * as vscode from 'vscode';
import {
    getBuiltinModuleSnapshot,
    type BuiltinModuleLinkSnapshot,
    type BuiltinModuleSnapshot,
    type BuiltinSymbolSnapshot,
} from './structure/builtinModules';
import {
    discoverWorkspaceProjects,
    isZrpDocument,
    pickWorkspaceProjectForUri,
    type WorkspaceProject,
} from './workspaceProjects';

export const ZR_FILES_VIEW_ID = 'zrFiles';
export const ZR_IMPORTS_VIEW_ID = 'zrImports';
export const ZR_STRUCTURE_REFRESH_COMMAND = 'zr.structure.refresh';
export const ZR_STRUCTURE_INSPECT_COMMAND = 'zr.__inspectStructureViews';
export const ZR_STRUCTURE_OPEN_TARGET_COMMAND = 'zr.structure.openTarget';

const ZR_FILE_GLOB = '**/*.zr';
const ZR_FILE_EXCLUDE_GLOB = '**/{build,bin,node_modules,.git,.vscode-test}/**';
const MODULE_PATTERN = /^\s*module\s+(['"])([^'"]+)\1\s*;/m;
const IMPORT_PATTERN = /(?:\bvar\s+([A-Za-z_]\w*)\s*=\s*)?%import\s*\(\s*(['"])([^'"]+)\2\s*\)/g;
const REFRESH_DEBOUNCE_MS = 150;

type StructureNodeType = 'workspace' | 'project' | 'file' | 'imports' | 'declarations' | 'import' | 'module' | 'declaration';
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

interface ImportEntry {
    alias?: string;
    moduleName: string;
    range: vscode.Range;
    targetUri?: vscode.Uri;
    builtinModule?: BuiltinModuleSnapshot;
    isExternal: boolean;
}

interface FileSnapshot {
    uri: vscode.Uri;
    workspaceFolder: vscode.WorkspaceFolder;
    project?: WorkspaceProject;
    moduleName: string;
    relativePath: string;
    projectRelativeModulePath?: string;
    lines: string[];
    imports: ImportEntry[];
    symbols: vscode.DocumentSymbol[];
    navigationRange: vscode.Range;
}

interface StructureNode {
    id: string;
    nodeType: StructureNodeType;
    label: string;
    description?: string;
    tooltip?: string;
    moduleName?: string;
    uri?: vscode.Uri;
    relativePath?: string;
    isExternal?: boolean;
    isRecursiveReference?: boolean;
    icon: vscode.ThemeIcon;
    collapsibleState: vscode.TreeItemCollapsibleState;
    command?: vscode.Command;
    children: StructureNode[];
}

interface SerializedStructureNode {
    id: string;
    nodeType: StructureNodeType;
    label: string;
    description?: string;
    moduleName?: string;
    uri?: string;
    relativePath?: string;
    isExternal?: boolean;
    isRecursiveReference?: boolean;
    commandId?: string;
    commandArguments?: unknown[];
    children: SerializedStructureNode[];
}

export interface ZrStructureController extends vscode.Disposable {
    refresh(): Promise<void>;
}

export function registerZrStructureViews(context: vscode.ExtensionContext): ZrStructureController {
    return new ZrStructureService(context);
}

class StructureTreeProvider implements vscode.TreeDataProvider<StructureNode> {
    private readonly onDidChangeTreeDataEmitter = new vscode.EventEmitter<StructureNode | undefined>();
    private roots: StructureNode[] = [];
    private readonly parentById = new Map<string, StructureNode>();

    public readonly onDidChangeTreeData = this.onDidChangeTreeDataEmitter.event;

    public setRoots(roots: StructureNode[]): void {
        this.roots = roots;
        this.parentById.clear();
        indexNodeParents(undefined, roots, this.parentById);
        this.onDidChangeTreeDataEmitter.fire(undefined);
    }

    public getTreeItem(element: StructureNode): vscode.TreeItem {
        const item = new vscode.TreeItem(element.label, element.collapsibleState);
        item.id = element.id;
        item.description = element.description;
        item.tooltip = element.tooltip;
        item.iconPath = element.icon;
        item.resourceUri = element.uri;
        item.command = element.command;
        return item;
    }

    public getChildren(element?: StructureNode): Thenable<StructureNode[]> {
        return Promise.resolve(element?.children ?? this.roots);
    }

    public getParent(element: StructureNode): vscode.ProviderResult<StructureNode> {
        return this.parentById.get(element.id);
    }

    public dispose(): void {
        this.onDidChangeTreeDataEmitter.dispose();
    }
}

class ZrStructureService implements ZrStructureController {
    private readonly disposables: vscode.Disposable[] = [];
    private readonly filesProvider = new StructureTreeProvider();
    private readonly importsProvider = new StructureTreeProvider();
    private readonly filesView: vscode.TreeView<StructureNode>;
    private readonly importsView: vscode.TreeView<StructureNode>;
    private readonly fileRootByUri = new Map<string, StructureNode>();
    private readonly importRootByUri = new Map<string, StructureNode>();
    private readonly snapshotByUri = new Map<string, FileSnapshot>();

    private filesRoots: StructureNode[] = [];
    private importsRoots: StructureNode[] = [];
    private refreshChain: Promise<void> = Promise.resolve();
    private refreshTimer: ReturnType<typeof setTimeout> | undefined;

    public constructor(context: vscode.ExtensionContext) {
        this.filesView = vscode.window.createTreeView(ZR_FILES_VIEW_ID, {
            treeDataProvider: this.filesProvider,
            showCollapseAll: true,
        });
        this.importsView = vscode.window.createTreeView(ZR_IMPORTS_VIEW_ID, {
            treeDataProvider: this.importsProvider,
            showCollapseAll: true,
        });

        this.disposables.push(
            this.filesProvider,
            this.importsProvider,
            this.filesView,
            this.importsView,
            vscode.commands.registerCommand(ZR_STRUCTURE_REFRESH_COMMAND, async () => {
                await this.refresh();
            }),
            vscode.commands.registerCommand(ZR_STRUCTURE_INSPECT_COMMAND, async () => {
                await this.refresh();
                return {
                    files: this.filesRoots.map((node) => serializeNode(node)),
                    imports: this.importsRoots.map((node) => serializeNode(node)),
                };
            }),
            vscode.commands.registerCommand(ZR_STRUCTURE_OPEN_TARGET_COMMAND, async (payload: OpenTargetPayload) => {
                await openTarget(payload);
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
                if (document.languageId === 'zr' || isProjectDocument(document)) {
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
        );

        const watcher = vscode.workspace.createFileSystemWatcher('**/*.{zr,zrp}');
        this.disposables.push(
            watcher,
            watcher.onDidChange(() => {
                this.scheduleRefresh();
            }),
            watcher.onDidCreate(() => {
                this.scheduleRefresh();
            }),
            watcher.onDidDelete(() => {
                this.scheduleRefresh();
            }),
        );

        context.subscriptions.push(...this.disposables);
        void this.refresh();
    }

    public async refresh(): Promise<void> {
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

    public dispose(): void {
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
        const projects = await discoverWorkspaceProjects();
        const snapshots = await this.collectSnapshots(projects);
        this.snapshotByUri.clear();
        for (const snapshot of snapshots) {
            this.snapshotByUri.set(snapshot.uri.toString(), snapshot);
        }

        const moduleIndex = buildModuleIndex(snapshots);
        resolveImportTargets(snapshots, moduleIndex);

        const singleWorkspace = (vscode.workspace.workspaceFolders?.length ?? 0) <= 1;
        const fileRoots = snapshots.map((snapshot) => this.buildFileNode(snapshot));
        const importRoots = snapshots.map((snapshot) => this.buildModuleRoot(snapshot));
        this.filesRoots = wrapWorkspaceRoots(wrapProjectRoots(snapshots, fileRoots), singleWorkspace);
        this.importsRoots = wrapWorkspaceRoots(wrapProjectRoots(snapshots, importRoots), singleWorkspace);
        this.filesProvider.setRoots(this.filesRoots);
        this.importsProvider.setRoots(this.importsRoots);

        this.fileRootByUri.clear();
        this.importRootByUri.clear();
        for (const node of fileRoots) {
            if (node.uri) {
                this.fileRootByUri.set(node.uri.toString(), node);
            }
        }
        for (const node of importRoots) {
            if (node.uri) {
                this.importRootByUri.set(node.uri.toString(), node);
            }
        }
    }

    private async collectSnapshots(projects: WorkspaceProject[]): Promise<FileSnapshot[]> {
        const uris = await vscode.workspace.findFiles(ZR_FILE_GLOB, ZR_FILE_EXCLUDE_GLOB);
        uris.sort((left, right) => compareStrings(left.toString(), right.toString()));

        const snapshots = await Promise.all(uris.map(async (uri) => this.createSnapshot(uri, projects)));
        return snapshots
            .filter((snapshot): snapshot is FileSnapshot => snapshot !== undefined)
            .sort((left, right) => {
                const folderCompare = compareStrings(left.workspaceFolder.name, right.workspaceFolder.name);
                if (folderCompare !== 0) {
                    return folderCompare;
                }

                const leftProjectKey = left.project?.relativePath ?? '';
                const rightProjectKey = right.project?.relativePath ?? '';
                const projectCompare = compareStrings(leftProjectKey, rightProjectKey);
                if (projectCompare !== 0) {
                    return projectCompare;
                }

                return compareStrings(left.relativePath, right.relativePath);
            });
    }

    private async createSnapshot(uri: vscode.Uri, projects: WorkspaceProject[]): Promise<FileSnapshot | undefined> {
        const workspaceFolder = vscode.workspace.getWorkspaceFolder(uri);
        if (!workspaceFolder) {
            return undefined;
        }

        const project = pickWorkspaceProjectForUri(uri, projects);
        const document = await vscode.workspace.openTextDocument(uri);
        const imports = parseImports(document);
        const symbols = filterImportAliasSymbols(await loadDocumentSymbols(uri), imports);
        const lines = document.getText().split(/\r?\n/);
        return {
            uri,
            workspaceFolder,
            project,
            moduleName: parseModuleName(document),
            relativePath: relativePathWithinFolder(workspaceFolder, uri),
            projectRelativeModulePath: projectRelativeModulePath(project, uri),
            lines,
            imports,
            symbols,
            navigationRange: firstNavigableRange(symbols, lines),
        };
    }

    private buildFileNode(snapshot: FileSnapshot): StructureNode {
        const importsGroup = createGroupNode(
            `file:${snapshot.uri.toString()}:imports`,
            'Imports',
            snapshot.uri,
            snapshot.relativePath,
            snapshot.imports
                .slice()
                .sort((left, right) => compareStrings(left.moduleName, right.moduleName))
                .map((entry) => createImportLeaf(snapshot, entry)),
        );
        const declarationsGroup = createGroupNode(
            `file:${snapshot.uri.toString()}:declarations`,
            'Declarations',
            snapshot.uri,
            snapshot.relativePath,
            snapshot.symbols.map((symbol) => createDeclarationNode(snapshot, symbol)),
        );
        return {
            id: `file:${snapshot.uri.toString()}`,
            nodeType: 'file',
            label: snapshot.moduleName,
            description: snapshot.relativePath,
            tooltip: snapshot.uri.fsPath || snapshot.uri.toString(),
            moduleName: snapshot.moduleName,
            uri: snapshot.uri,
            relativePath: snapshot.relativePath,
            icon: new vscode.ThemeIcon('file'),
            collapsibleState: vscode.TreeItemCollapsibleState.Collapsed,
            command: createRangeCommand(snapshot.uri, snapshot.navigationRange),
            children: [importsGroup, declarationsGroup],
        };
    }

    private buildModuleRoot(snapshot: FileSnapshot): StructureNode {
        return this.createModuleNode(snapshot, new Set<string>([snapshot.uri.toString()]), false);
    }

    private createModuleNode(
        snapshot: FileSnapshot,
        ancestry: Set<string>,
        isRecursiveReference: boolean,
    ): StructureNode {
        if (isRecursiveReference) {
            return {
                id: `module:${snapshot.uri.toString()}:recursive`,
                nodeType: 'module',
                label: snapshot.moduleName,
                description: `${snapshot.relativePath} (recursive)`,
                tooltip: snapshot.uri.fsPath || snapshot.uri.toString(),
                moduleName: snapshot.moduleName,
                uri: snapshot.uri,
                relativePath: snapshot.relativePath,
                isRecursiveReference: true,
                icon: new vscode.ThemeIcon('file-submodule'),
                collapsibleState: vscode.TreeItemCollapsibleState.None,
                command: createRangeCommand(snapshot.uri, snapshot.navigationRange),
                children: [],
            };
        }

        const declarationsGroup = createGroupNode(
            `module:${snapshot.uri.toString()}:declarations`,
            'Declarations',
            snapshot.uri,
            snapshot.relativePath,
            snapshot.symbols.map((symbol) => createDeclarationNode(snapshot, symbol)),
        );
        const importChildren = snapshot.imports
            .slice()
            .sort((left, right) => compareStrings(left.moduleName, right.moduleName))
            .map((entry) => this.createImportedModuleNode(snapshot, entry, ancestry));
        return {
            id: `module:${snapshot.uri.toString()}:${isRecursiveReference ? 'recursive' : 'node'}`,
            nodeType: 'module',
            label: snapshot.moduleName,
            description: snapshot.relativePath,
            tooltip: snapshot.uri.fsPath || snapshot.uri.toString(),
            moduleName: snapshot.moduleName,
            uri: snapshot.uri,
            relativePath: snapshot.relativePath,
            isRecursiveReference: false,
            icon: new vscode.ThemeIcon('file-submodule'),
            collapsibleState: vscode.TreeItemCollapsibleState.Collapsed,
            command: createRangeCommand(snapshot.uri, snapshot.navigationRange),
            children: [declarationsGroup, ...importChildren],
        };
    }

    private createImportedModuleNode(
        owner: FileSnapshot,
        entry: ImportEntry,
        ancestry: Set<string>,
    ): StructureNode {
        if (entry.builtinModule) {
            return createBuiltinModuleNode(
                owner,
                entry.builtinModule,
                entry.moduleName,
                entry.range,
                `module:${owner.uri.toString()}:builtin:${entry.moduleName}`,
                entry.moduleName,
                'builtin',
            );
        }

        if (!entry.targetUri) {
            return {
                id: `module:${owner.uri.toString()}:external:${entry.moduleName}:${entry.range.start.line}:${entry.range.start.character}`,
                nodeType: 'module',
                label: entry.moduleName,
                description: 'external',
                tooltip: `${entry.moduleName} (external module)`,
                moduleName: entry.moduleName,
                uri: owner.uri,
                relativePath: owner.relativePath,
                isExternal: true,
                icon: new vscode.ThemeIcon('package'),
                collapsibleState: vscode.TreeItemCollapsibleState.None,
                command: createDefinitionCommand(owner.uri, entry.range.start, entry.range),
                children: [],
            };
        }

        const target = this.snapshotByUri.get(entry.targetUri.toString());
        if (!target) {
            return {
                id: `module:${owner.uri.toString()}:missing:${entry.moduleName}`,
                nodeType: 'module',
                label: entry.moduleName,
                description: 'external',
                tooltip: `${entry.moduleName} (unresolved module)`,
                moduleName: entry.moduleName,
                uri: owner.uri,
                relativePath: owner.relativePath,
                isExternal: true,
                icon: new vscode.ThemeIcon('package'),
                collapsibleState: vscode.TreeItemCollapsibleState.None,
                command: createDefinitionCommand(owner.uri, entry.range.start, entry.range),
                children: [],
            };
        }

        const targetKey = target.uri.toString();
        if (ancestry.has(targetKey)) {
            return {
                ...this.createModuleNode(target, ancestry, true),
                id: `module:${owner.uri.toString()}:recursive:${targetKey}`,
                isRecursiveReference: true,
                description: `${target.relativePath} (recursive)`,
            };
        }

        const nextAncestry = new Set(ancestry);
        nextAncestry.add(targetKey);
        return this.createModuleNode(target, nextAncestry, false);
    }

    private async revealActiveEditor(): Promise<void> {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'zr') {
            return;
        }

        const key = editor.document.uri.toString();
        await revealNode(this.filesView, this.fileRootByUri.get(key));
        await revealNode(this.importsView, this.importRootByUri.get(key));
    }
}

function createGroupNode(
    id: string,
    label: string,
    uri: vscode.Uri,
    relativePath: string,
    children: StructureNode[],
): StructureNode {
    return {
        id,
        nodeType: label === 'Imports' ? 'imports' : 'declarations',
        label,
        uri,
        relativePath,
        icon: new vscode.ThemeIcon('folder'),
        collapsibleState: children.length > 0
            ? vscode.TreeItemCollapsibleState.Expanded
            : vscode.TreeItemCollapsibleState.None,
        children,
    };
}

function createImportLeaf(snapshot: FileSnapshot, entry: ImportEntry): StructureNode {
    const label = entry.alias ? `${entry.alias} -> ${entry.moduleName}` : entry.moduleName;
    const builtinChildren = entry.builtinModule
        ? createBuiltinChildren(
            snapshot,
            entry.builtinModule,
            entry.range,
            `import:${snapshot.uri.toString()}:${entry.moduleName}`,
        )
        : [];
    return {
        id: `import:${snapshot.uri.toString()}:${entry.moduleName}:${entry.range.start.line}:${entry.range.start.character}`,
        nodeType: 'import',
        label,
        description: entry.builtinModule
            ? 'builtin'
            : entry.isExternal
                ? 'external'
                : undefined,
        tooltip: `${entry.moduleName}${entry.alias ? ` (${entry.alias})` : ''}`,
        moduleName: entry.moduleName,
        uri: snapshot.uri,
        relativePath: snapshot.relativePath,
        isExternal: entry.isExternal,
        icon: entry.builtinModule
            ? new vscode.ThemeIcon('library')
            : entry.isExternal
                ? new vscode.ThemeIcon('package')
                : new vscode.ThemeIcon('file-submodule'),
        collapsibleState: builtinChildren.length > 0
            ? vscode.TreeItemCollapsibleState.Collapsed
            : vscode.TreeItemCollapsibleState.None,
        command: createRangeCommand(snapshot.uri, entry.range),
        children: builtinChildren,
    };
}

function createBuiltinChildren(
    snapshot: FileSnapshot,
    builtinModule: BuiltinModuleSnapshot,
    fallbackRange: vscode.Range,
    baseId: string,
): StructureNode[] {
    const children: StructureNode[] = [];
    const symbolNodes = (builtinModule.symbols ?? []).map((symbol) =>
        createBuiltinSymbolNode(
            snapshot,
            symbol,
            fallbackRange,
            `${baseId}:symbol:${symbol.name}`,
        ));
    if (symbolNodes.length > 0) {
        children.push(
            createGroupNode(
                `${baseId}:declarations`,
                'Declarations',
                snapshot.uri,
                snapshot.relativePath,
                symbolNodes,
            ),
        );
    }

    for (const moduleLink of builtinModule.modules ?? []) {
        children.push(
            createBuiltinModuleLinkNode(
                snapshot,
                moduleLink,
                fallbackRange,
                `${baseId}:module:${moduleLink.moduleName}`,
            ),
        );
    }

    return children;
}

function createBuiltinModuleNode(
    snapshot: FileSnapshot,
    builtinModule: BuiltinModuleSnapshot,
    moduleName: string,
    fallbackRange: vscode.Range,
    id: string,
    label: string,
    description?: string,
): StructureNode {
    const children = createBuiltinChildren(snapshot, builtinModule, fallbackRange, id);
    return {
        id,
        nodeType: 'module',
        label,
        description,
        tooltip: builtinModule.detail ?? moduleName,
        moduleName,
        uri: snapshot.uri,
        relativePath: snapshot.relativePath,
        icon: new vscode.ThemeIcon('library'),
        collapsibleState: children.length > 0
            ? vscode.TreeItemCollapsibleState.Collapsed
            : vscode.TreeItemCollapsibleState.None,
        command: createRangeCommand(snapshot.uri, fallbackRange),
        children,
    };
}

function createBuiltinModuleLinkNode(
    snapshot: FileSnapshot,
    moduleLink: BuiltinModuleLinkSnapshot,
    fallbackRange: vscode.Range,
    id: string,
): StructureNode {
    const builtinModule = getBuiltinModuleSnapshot(moduleLink.moduleName);
    if (builtinModule) {
        return createBuiltinModuleNode(
            snapshot,
            builtinModule,
            moduleLink.moduleName,
            fallbackRange,
            id,
            moduleLink.name,
            moduleLink.moduleName,
        );
    }

    return {
        id,
        nodeType: 'module',
        label: moduleLink.name,
        description: moduleLink.moduleName,
        tooltip: moduleLink.detail ?? moduleLink.moduleName,
        moduleName: moduleLink.moduleName,
        uri: snapshot.uri,
        relativePath: snapshot.relativePath,
        icon: new vscode.ThemeIcon('library'),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        command: createRangeCommand(snapshot.uri, fallbackRange),
        children: [],
    };
}

function createBuiltinSymbolNode(
    snapshot: FileSnapshot,
    symbol: BuiltinSymbolSnapshot,
    fallbackRange: vscode.Range,
    id: string,
): StructureNode {
    return {
        id,
        nodeType: 'declaration',
        label: symbol.name,
        description: symbol.detail ?? symbol.kind,
        tooltip: symbol.detail ?? symbol.name,
        moduleName: snapshot.moduleName,
        uri: snapshot.uri,
        relativePath: snapshot.relativePath,
        icon: builtinSymbolThemeIcon(symbol.kind),
        collapsibleState: vscode.TreeItemCollapsibleState.None,
        command: createRangeCommand(snapshot.uri, fallbackRange),
        children: [],
    };
}

function createDeclarationNode(snapshot: FileSnapshot, symbol: vscode.DocumentSymbol): StructureNode {
    const selectionRange = resolvedSymbolRange(snapshot.lines, symbol);
    return {
        id: `declaration:${snapshot.uri.toString()}:${symbol.name}:${symbol.selectionRange.start.line}:${symbol.selectionRange.start.character}`,
        nodeType: 'declaration',
        label: symbol.name,
        description: symbol.detail || undefined,
        tooltip: symbol.detail ? `${symbol.name}: ${symbol.detail}` : symbol.name,
        uri: snapshot.uri,
        relativePath: snapshot.relativePath,
        icon: symbolThemeIcon(symbol.kind),
        collapsibleState: symbol.children.length > 0
            ? vscode.TreeItemCollapsibleState.Collapsed
            : vscode.TreeItemCollapsibleState.None,
        command: createRangeCommand(snapshot.uri, selectionRange),
        children: symbol.children.map((child) => createDeclarationNode(snapshot, child)),
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

function serializeNode(node: StructureNode): SerializedStructureNode {
    return {
        id: node.id,
        nodeType: node.nodeType,
        label: node.label,
        description: node.description,
        moduleName: node.moduleName,
        uri: node.uri?.toString(),
        relativePath: node.relativePath,
        isExternal: node.isExternal,
        isRecursiveReference: node.isRecursiveReference,
        commandId: node.command?.command,
        commandArguments: node.command?.arguments,
        children: node.children.map((child) => serializeNode(child)),
    };
}

function buildModuleIndex(snapshots: FileSnapshot[]): Map<string, FileSnapshot[]> {
    const index = new Map<string, FileSnapshot[]>();
    for (const snapshot of snapshots) {
        for (const key of moduleLookupKeys(snapshot)) {
            const bucket = index.get(key);
            if (bucket) {
                bucket.push(snapshot);
            } else {
                index.set(key, [snapshot]);
            }
        }
    }

    return index;
}

function resolveImportTargets(snapshots: FileSnapshot[], moduleIndex: Map<string, FileSnapshot[]>): void {
    for (const snapshot of snapshots) {
        for (const entry of snapshot.imports) {
            const target = resolveModuleTarget(snapshot, entry.moduleName, moduleIndex);
            entry.targetUri = target?.uri;
            entry.builtinModule = getBuiltinModuleSnapshot(entry.moduleName);
            entry.isExternal = target === undefined && entry.builtinModule === undefined;
        }
    }
}

function resolveModuleTarget(
    owner: FileSnapshot,
    moduleName: string,
    moduleIndex: Map<string, FileSnapshot[]>,
): FileSnapshot | undefined {
    const candidates = uniqueSnapshots(moduleIndex.get(moduleName) ?? []);
    if (candidates.length === 0) {
        return undefined;
    }

    const sameFolder = candidates.filter((candidate) =>
        candidate.workspaceFolder.uri.toString() === owner.workspaceFolder.uri.toString());
    const scoped = sameFolder.length > 0 ? sameFolder : candidates;
    scoped.sort((left, right) => compareStrings(left.relativePath, right.relativePath));
    return scoped[0];
}

function uniqueSnapshots(snapshots: FileSnapshot[]): FileSnapshot[] {
    const seen = new Set<string>();
    const unique: FileSnapshot[] = [];
    for (const snapshot of snapshots) {
        const key = snapshot.uri.toString();
        if (!seen.has(key)) {
            seen.add(key);
            unique.push(snapshot);
        }
    }

    return unique;
}

function moduleLookupKeys(snapshot: FileSnapshot): string[] {
    const relativeStem = removeExtension(snapshot.relativePath);
    const fileStem = fileStemFromUri(snapshot.uri);
    const keys = new Set<string>([
        snapshot.moduleName,
        snapshot.projectRelativeModulePath ?? '',
        relativeStem,
        fileStem,
    ]);
    return Array.from(keys).filter((value) => value.length > 0);
}

function parseModuleName(document: vscode.TextDocument): string {
    const match = MODULE_PATTERN.exec(document.getText());
    if (match?.[2]) {
        return match[2];
    }

    return fileStemFromUri(document.uri);
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

        const start = document.positionAt(match.index);
        const end = document.positionAt(match.index + match[0].length);
        entries.push({
            alias: match[1] || undefined,
            moduleName: match[3],
            range: new vscode.Range(start, end),
            isExternal: true,
        });
    }

    return entries;
}

function filterImportAliasSymbols(symbols: vscode.DocumentSymbol[], imports: ImportEntry[]): vscode.DocumentSymbol[] {
    return symbols.filter((symbol) => !isImportAliasSymbol(symbol, imports));
}

function isImportAliasSymbol(symbol: vscode.DocumentSymbol, imports: ImportEntry[]): boolean {
    return imports.some((entry) =>
        entry.alias === symbol.name &&
        entry.range.start.line === symbol.range.start.line);
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

function wrapProjectRoots(snapshots: FileSnapshot[], nodes: StructureNode[]): StructureNode[] {
    const orderedProjects: Array<{ id: string; project?: WorkspaceProject; uri?: vscode.Uri; children: StructureNode[] }> = [];
    const byProject = new Map<string, { project?: WorkspaceProject; uri?: vscode.Uri; children: StructureNode[] }>();

    for (let index = 0; index < snapshots.length; index++) {
        const snapshot = snapshots[index];
        const node = nodes[index];
        const projectKey = snapshot.project?.id ?? `unassigned:${snapshot.workspaceFolder.uri.toString()}`;
        let bucket = byProject.get(projectKey);
        if (!bucket) {
            bucket = {
                project: snapshot.project,
                uri: snapshot.project?.uri ?? snapshot.workspaceFolder.uri,
                children: [],
            };
            byProject.set(projectKey, bucket);
            orderedProjects.push({ id: projectKey, ...bucket });
        }
        bucket.children.push(node);
    }

    return orderedProjects.map((entry) => {
        if (entry.project) {
            return {
                id: `project:${entry.project.id}`,
                nodeType: 'project',
                label: entry.project.label,
                description: entry.project.relativePath,
                tooltip: entry.project.uri.fsPath || entry.project.uri.toString(),
                uri: entry.project.uri,
                relativePath: entry.project.relativePath,
                icon: new vscode.ThemeIcon('folder'),
                collapsibleState: vscode.TreeItemCollapsibleState.Expanded,
                command: createRangeCommand(entry.project.uri, new vscode.Range(0, 0, 0, 0)),
                children: entry.children,
            };
        }

        return {
            id: `project:${entry.id}`,
            nodeType: 'project',
            label: 'Unassigned Files',
            description: 'No matching .zrp source root',
            tooltip: 'Files that do not belong to a discovered .zrp project',
            uri: entry.uri,
            icon: new vscode.ThemeIcon('warning'),
            collapsibleState: vscode.TreeItemCollapsibleState.Expanded,
            children: entry.children,
        };
    });
}

function wrapWorkspaceRoots(nodes: StructureNode[], singleWorkspace: boolean): StructureNode[] {
    if (singleWorkspace) {
        return nodes;
    }

    const byFolder = new Map<string, StructureNode[]>();
    for (const node of nodes) {
        const folder = node.uri ? vscode.workspace.getWorkspaceFolder(node.uri) : undefined;
        const key = folder?.uri.toString() ?? '__unknown__';
        const bucket = byFolder.get(key);
        if (bucket) {
            bucket.push(node);
        } else {
            byFolder.set(key, [node]);
        }
    }

    const wrapped: StructureNode[] = [];
    for (const [folderKey, children] of byFolder.entries()) {
        const folder = vscode.workspace.workspaceFolders?.find((item) => item.uri.toString() === folderKey);
        wrapped.push({
            id: `workspace:${folderKey}`,
            nodeType: 'workspace',
            label: folder?.name ?? folderKey,
            description: folder?.uri.fsPath || folderKey,
            tooltip: folder?.uri.fsPath || folderKey,
            uri: folder?.uri,
            icon: new vscode.ThemeIcon('folder'),
            collapsibleState: vscode.TreeItemCollapsibleState.Expanded,
            children,
        });
    }

    wrapped.sort((left, right) => compareStrings(left.label, right.label));
    return wrapped;
}

function indexNodeParents(
    parent: StructureNode | undefined,
    nodes: StructureNode[],
    parentById: Map<string, StructureNode>,
): void {
    for (const node of nodes) {
        if (parent) {
            parentById.set(node.id, parent);
        }
        if (node.children.length > 0) {
            indexNodeParents(node, node.children, parentById);
        }
    }
}

function firstNavigableRange(symbols: vscode.DocumentSymbol[], lines: string[]): vscode.Range {
    for (const symbol of symbols) {
        const range = resolvedSymbolRange(lines, symbol);
        if (range) {
            return range;
        }

        if (symbol.children.length > 0) {
            return firstNavigableRange(symbol.children, lines);
        }
    }

    return new vscode.Range(0, 0, 0, 0);
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
    const targetUri = locationUri(first);
    const targetRange = locationRange(first);
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

function locationUri(entry: any): vscode.Uri | undefined {
    return entry?.uri ?? entry?.targetUri ?? entry?.location?.uri;
}

function locationRange(entry: any): vscode.Range | undefined {
    return entry?.range ?? entry?.targetSelectionRange ?? entry?.targetRange ?? entry?.location?.range;
}

async function revealNode(view: vscode.TreeView<StructureNode>, node: StructureNode | undefined): Promise<void> {
    if (!node) {
        return;
    }

    try {
        await view.reveal(node, {
            select: false,
            focus: false,
            expand: true,
        });
    } catch {
        // Ignore reveal failures when the view has not been rendered yet.
    }
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
        case vscode.SymbolKind.Constructor:
            return new vscode.ThemeIcon('symbol-constructor');
        case vscode.SymbolKind.Variable:
            return new vscode.ThemeIcon('symbol-variable');
        case vscode.SymbolKind.Module:
            return new vscode.ThemeIcon('symbol-module');
        case vscode.SymbolKind.Interface:
            return new vscode.ThemeIcon('symbol-interface');
        default:
            return new vscode.ThemeIcon('symbol-misc');
    }
}

function builtinSymbolThemeIcon(kind: BuiltinSymbolSnapshot['kind']): vscode.ThemeIcon {
    switch (kind) {
        case 'constant':
            return new vscode.ThemeIcon('symbol-constant');
        case 'function':
            return new vscode.ThemeIcon('symbol-function');
        case 'type':
            return new vscode.ThemeIcon('symbol-class');
        default:
            return new vscode.ThemeIcon('symbol-misc');
    }
}

function resolvedSymbolRange(lines: string[], symbol: vscode.DocumentSymbol): vscode.Range {
    const fallback = symbol.selectionRange ?? symbol.range;
    const lineText = lines[symbol.range.start.line] ?? '';
    const lineStart = symbol.range.start.character;
    const rawLineEnd = symbol.range.start.line === symbol.range.end.line
        ? Math.min(symbol.range.end.character, lineText.length)
        : lineText.length;
    const lineEnd = rawLineEnd > lineStart ? rawLineEnd : lineText.length;
    const visibleSegment = lineText.slice(lineStart, lineEnd);
    const nameOffset = visibleSegment.indexOf(symbol.name);
    if (nameOffset >= 0) {
        const start = new vscode.Position(symbol.range.start.line, lineStart + nameOffset);
        const end = start.translate(0, symbol.name.length);
        return new vscode.Range(start, end);
    }

    return fallback;
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

function relativePathWithinFolder(workspaceFolder: vscode.WorkspaceFolder, uri: vscode.Uri): string {
    const folderPath = normalizePath(workspaceFolder.uri.path);
    const filePath = normalizePath(uri.path);
    if (filePath.startsWith(`${folderPath}/`)) {
        return filePath.slice(folderPath.length + 1);
    }

    return vscode.workspace.asRelativePath(uri, false);
}

function projectRelativeModulePath(project: WorkspaceProject | undefined, uri: vscode.Uri): string | undefined {
    if (!project) {
        return undefined;
    }

    const normalizedFilePath = normalizePath(uri.fsPath || uri.path);
    const normalizedSourceRoot = normalizePath(project.sourceRootPath);
    if (!normalizedFilePath.startsWith(`${normalizedSourceRoot}/`)) {
        return undefined;
    }

    const relativePath = normalizedFilePath.slice(normalizedSourceRoot.length + 1);
    return removeExtension(relativePath);
}

function fileStemFromUri(uri: vscode.Uri): string {
    return removeExtension(lastPathSegment(uri.path));
}

function removeExtension(value: string): string {
    const lastDot = value.lastIndexOf('.');
    return lastDot > 0 ? value.slice(0, lastDot) : value;
}

function lastPathSegment(pathValue: string): string {
    const normalized = normalizePath(pathValue);
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}

function normalizePath(pathValue: string): string {
    return pathValue.replace(/[\\/]+/g, '/');
}

function compareStrings(left: string, right: string): number {
    return left.localeCompare(right);
}

function isProjectDocument(document: vscode.TextDocument): boolean {
    return isZrpDocument(document);
}
