import * as vscode from 'vscode';
import {
    dirnameFromPath,
    joinNormalizedPath,
    normalizeFilePath,
    parseProjectManifestText,
    pickBestProjectForFile,
    type ParsedProjectManifest,
} from './projectSupport';

/** 项目发现只扫描工作区内的清单，避开生成目录与扩展自身依赖。 */
const ZR_PROJECT_GLOB = '**/*.zrp';
const ZR_PROJECT_EXCLUDE_GLOB = '**/{build,bin,node_modules,.git,.vscode-test}/**';
/** 选择保存在工作区状态中；桌面宿主订阅事件以同步视图、命令与语言服务器。 */
const ZR_SELECTED_PROJECT_KEY = 'zr.selectedProjectUri';
const onDidChangeSelectedProjectEmitter = new vscode.EventEmitter<vscode.Uri | undefined>();

/** 工作区清单的发现结果；路径字段供源码归属，URI 字段供 VS Code 与 LSP 使用。 */
export interface WorkspaceProject {
    id: string;
    uri: vscode.Uri;
    workspaceFolder: vscode.WorkspaceFolder;
    manifest: ParsedProjectManifest;
    projectPath: string;
    projectDirectoryPath: string;
    sourceRootPath: string;
    relativePath: string;
    label: string;
}

/** 选择变化信号；订阅者应由扩展上下文持有并在停用时释放。 */
export const onDidChangeSelectedProject = onDidChangeSelectedProjectEmitter.event;

/** 枚举可解析的工作区 .zrp，供选择器、项目操作和结构视图共享候选集。 */
export async function discoverWorkspaceProjects(): Promise<WorkspaceProject[]> {
    const uris = await vscode.workspace.findFiles(ZR_PROJECT_GLOB, ZR_PROJECT_EXCLUDE_GLOB);
    const projects = await Promise.all(uris.map(async (uri) => createWorkspaceProject(uri)));

    return projects
        .filter((project): project is WorkspaceProject => project !== undefined)
        .sort((left, right) => compareStrings(left.relativePath, right.relativePath));
}

/** 从编辑器未保存文本或工作区文件系统创建项目模型，不主动打开文档。 */
export async function createWorkspaceProject(uri: vscode.Uri): Promise<WorkspaceProject | undefined> {
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(uri);
    if (!workspaceFolder) {
        return undefined;
    }

    const document = vscode.workspace.textDocuments.find((candidate) => candidate.uri.toString() === uri.toString());
    // BUG: findFiles 返回后清单若被删除或拒绝读取，readFile 的拒绝会沿 Promise.all
    // 使 discoverWorkspaceProjects 整体失败；单个坏文件不应阻断其他项目的选择。
    const text = document?.getText() ?? new TextDecoder('utf-8').decode(await vscode.workspace.fs.readFile(uri));
    const manifest = parseProjectManifestText(text, uri.fsPath || uri.path);
    if (!manifest) {
        return undefined;
    }

    const projectDirectoryPath = dirnameFromPath(uri.fsPath || uri.path);
    const sourceRootPath = joinNormalizedPath(projectDirectoryPath, manifest.source);
    return {
        id: uri.toString(),
        uri,
        workspaceFolder,
        manifest,
        projectPath: normalizeFilePath(uri.fsPath || uri.path),
        projectDirectoryPath,
        sourceRootPath,
        relativePath: relativePathWithinFolder(workspaceFolder, uri),
        label: manifest.name && manifest.name.length > 0
            ? manifest.name
            : removeExtension(lastPathSegment(uri.path)),
    };
}

/** 将当前源码 URI 归属到最深的项目源码根，供活动编辑器选择项目。 */
export function pickWorkspaceProjectForUri(
    uri: vscode.Uri,
    projects: WorkspaceProject[],
): WorkspaceProject | undefined {
    return pickBestProjectForFile(normalizeFilePath(uri.fsPath || uri.path), projects);
}

/** 辨认项目清单 URI；文件扩展名判断与宿主的语言 ID 无关。 */
export function isZrpUri(uri: vscode.Uri): boolean {
    return uri.path.toLowerCase().endsWith('.zrp');
}

/** 让结构视图在保存清单时刷新，而不依赖清单当前的语言 ID。 */
export function isZrpDocument(document: vscode.TextDocument): boolean {
    return isZrpUri(document.uri);
}

/** 将显式调试项目路径解析为 URI；空路径调用保留活动清单或工作区搜索分支。 */
export async function resolveProjectUri(
    folder: vscode.WorkspaceFolder | undefined,
    projectPath: unknown,
): Promise<vscode.Uri | undefined> {
    if (typeof projectPath === 'string' && projectPath.trim().length > 0) {
        const resolved = resolveRelativePath(folder, projectPath.trim());
        return vscode.Uri.file(resolved);
    }

    return findProjectFile(folder);
}

/** 为运行、调试及 LSP 选项目：file URI 活动文件、单项目、记忆选择、交互选择依次生效。 */
export async function resolveSelectedProjectUri(
    context: vscode.ExtensionContext,
    folder: vscode.WorkspaceFolder | undefined,
    allowPrompt: boolean,
): Promise<vscode.Uri | undefined> {
    const projects = await discoverWorkspaceProjects();
    const storedId = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);
    const activeUri = vscode.window.activeTextEditor?.document.uri;

    if (projects.length === 0) {
        await setSelectedProjectUri(context, undefined);
        return undefined;
    }

    if (activeUri?.scheme === 'file') {
        // BUG: 桌面宿主中，用户在项目 A 的源码编辑器仍活动时手动选 B，选择事件触发
        // 的结构视图/LSP 再调用此函数会选回 A，导致显式选择无法保持。
        const activeProject = isZrpUri(activeUri)
            ? projects.find((project) => project.uri.toString() === activeUri.toString())
            : pickWorkspaceProjectForUri(activeUri, projects);
        if (activeProject) {
            await setSelectedProjectUri(context, activeProject.uri);
            return activeProject.uri;
        }
    }

    if (projects.length === 1) {
        await setSelectedProjectUri(context, projects[0].uri);
        return projects[0].uri;
    }

    if (storedId) {
        const storedProject = projects.find((project) => project.id === storedId);
        if (storedProject) {
            return storedProject.uri;
        }
    }

    if (!allowPrompt) {
        return undefined;
    }

    return selectWorkspaceProject(context, folder, projects);
}

/** 为结构视图取得已选项目模型；返回值可直接提供 URI 与源码根。 */
export async function resolveSelectedWorkspaceProject(
    context: vscode.ExtensionContext,
    folder: vscode.WorkspaceFolder | undefined,
    allowPrompt: boolean,
): Promise<WorkspaceProject | undefined> {
    // TODO: 两次独立 discover 之间候选集可能变化；核查工作区变动时结构视图
    // 是否会因选中 URI 不在第一次快照而短暂显示空项目。
    const projects = await discoverWorkspaceProjects();
    const selectedUri = await resolveSelectedProjectUri(context, folder, allowPrompt);
    if (!selectedUri) {
        return undefined;
    }

    return projects.find((project) => project.uri.toString() === selectedUri.toString());
}

/** 显示项目选择器并持久化选择；取消时保留原选择供后续操作使用。 */
export async function selectWorkspaceProject(
    context: vscode.ExtensionContext,
    folder: vscode.WorkspaceFolder | undefined,
    preloadedProjects?: WorkspaceProject[],
): Promise<vscode.Uri | undefined> {
    const projects = preloadedProjects ?? await discoverWorkspaceProjects();
    let selectedProject: WorkspaceProject | undefined;

    // TODO: 调试提供器传入指定 folder，但上游解析已扫描全工作区并按活动/记忆项目选取，
    // 此处也忽略 folder；需核查多根工作区的项目选择是否应限定调用者的根。
    void folder;

    if (projects.length === 0) {
        await setSelectedProjectUri(context, undefined);
        return undefined;
    }

    if (projects.length === 1) {
        selectedProject = projects[0];
    } else {
        const picked = await vscode.window.showQuickPick(
            projects.map((project) => ({
                label: project.label,
                description: project.relativePath,
                uri: project.uri,
            })),
            {
                title: 'Select ZR project',
            },
        );
        selectedProject = projects.find((project) => project.uri.toString() === picked?.uri.toString());
    }

    if (!selectedProject) {
        return undefined;
    }

    await setSelectedProjectUri(context, selectedProject.uri);
    return selectedProject.uri;
}

/** 供状态栏和 Web 宿主判断是否需要展示项目操作入口。 */
export async function hasWorkspaceProjects(): Promise<boolean> {
    return (await discoverWorkspaceProjects()).length > 0;
}

/** 读取上次持久化的项目 URI；调用者若需验证存在性应走选择解析流程。 */
export function getSelectedProjectUri(context: vscode.ExtensionContext): vscode.Uri | undefined {
    const storedId = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);
    return storedId ? vscode.Uri.parse(storedId) : undefined;
}

/** 空路径调用时先取活动清单，再在工作区范围内搜索并请求选择。 */
export async function findProjectFile(folder: vscode.WorkspaceFolder | undefined): Promise<vscode.Uri | undefined> {
    const activeUri = vscode.window.activeTextEditor?.document.uri;
    // TODO: 指定 folder 时活动编辑器仍可指向另一工作区的 .zrp；若空路径
    // 分支未来供多根工作区调用，需确认 folder 与活动清单的优先级。
    if (activeUri && activeUri.scheme === 'file' && isZrpUri(activeUri)) {
        return activeUri;
    }

    const searchRoots = folder ? [folder] : (vscode.workspace.workspaceFolders ?? []);
    const candidates: vscode.Uri[] = [];

    for (const searchRoot of searchRoots) {
        const found = await vscode.workspace.findFiles(
            new vscode.RelativePattern(searchRoot, '**/*.zrp'),
            undefined,
            50,
        );
        candidates.push(...found);
    }

    if (candidates.length === 1) {
        return candidates[0];
    }
    if (candidates.length > 1) {
        const picked = await vscode.window.showQuickPick(
            candidates.map((uri) => ({
                label: lastPathSegment(uri.path),
                description: uri.fsPath,
                uri,
            })),
            {
                title: 'Select ZR project',
            },
        );
        return picked?.uri;
    }

    return undefined;
}

/** 以活动编辑器选工作区根；无活动编辑器时回退到首个工作区。 */
export function activeWorkspaceFolder(): vscode.WorkspaceFolder | undefined {
    const activeDocument = vscode.window.activeTextEditor?.document.uri;
    return activeDocument
        ? vscode.workspace.getWorkspaceFolder(activeDocument)
        : vscode.workspace.workspaceFolders?.[0];
}

/** 解析运行与调试配置中的本地路径，使用调用者工作区根解释相对值。 */
export function resolveRelativePath(folder: vscode.WorkspaceFolder | undefined, value: string): string {
    const trimmed = value.trim();
    if (trimmed.length === 0) {
        return trimmed;
    }

    try {
        if (vscode.Uri.file(trimmed).fsPath === trimmed && /^[A-Za-z]:[\\/]/.test(trimmed)) {
            return vscode.Uri.file(trimmed).fsPath;
        }
    } catch {
        // Fall back to workspace-relative resolution below.
    }

    if (/^(\/|[A-Za-z]:[\\/])/.test(trimmed)) {
        return vscode.Uri.file(trimmed).fsPath;
    }

    const base = folder?.uri ?? vscode.workspace.workspaceFolders?.[0]?.uri;
    if (!base) {
        return trimmed;
    }
    return vscode.Uri.joinPath(base, trimmed).fsPath;
}

/** 构建选择器中的稳定工作区相对展示路径，供同名项目辨认。 */
function relativePathWithinFolder(workspaceFolder: vscode.WorkspaceFolder, uri: vscode.Uri): string {
    const folderPath = normalizeFilePath(workspaceFolder.uri.path);
    const filePath = normalizeFilePath(uri.path);
    if (filePath.startsWith(`${folderPath}/`)) {
        return filePath.slice(folderPath.length + 1);
    }

    return vscode.workspace.asRelativePath(uri, false);
}

/** 清单未命名时从文件名推导列表标签。 */
function removeExtension(value: string): string {
    const lastDot = value.lastIndexOf('.');
    return lastDot > 0 ? value.slice(0, lastDot) : value;
}

/** 将 URI 路径压缩成选择器可读的文件名。 */
function lastPathSegment(pathValue: string): string {
    const normalized = normalizeFilePath(pathValue);
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}

/** 固定发现顺序，避免工作区扫描顺序变化使选择器跳动。 */
function compareStrings(left: string, right: string): number {
    return left.localeCompare(right);
}

/** 仅在值变化时写入并通知消费者，避免视图刷新和 LSP 通知循环。 */
async function setSelectedProjectUri(
    context: vscode.ExtensionContext,
    uri: vscode.Uri | undefined,
): Promise<void> {
    const nextValue = uri?.toString();
    const previousValue = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);

    if (previousValue === nextValue) {
        return;
    }

    await context.workspaceState.update(ZR_SELECTED_PROJECT_KEY, nextValue);
    onDidChangeSelectedProjectEmitter.fire(uri);
}
