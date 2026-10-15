import * as vscode from 'vscode';
import {
    dirnameFromPath,
    joinNormalizedPath,
    normalizeFilePath,
    parseProjectManifestText,
    pickBestProjectForFile,
    type ParsedProjectManifest,
} from './projectSupport';

/** 工作区发现传给 VS Code 的包含模式；不递归越过 workspace 搜索边界。 */
const ZR_PROJECT_GLOB = '**/*.zrp';
/** 排除构建输出、依赖、Git 元数据与 VS Code 测试夹具目录。 */
const ZR_PROJECT_EXCLUDE_GLOB = '**/{build,bin,node_modules,.git,.vscode-test}/**';
/** 持久化当前项目 URI 字符串的 workspaceState 键；键存在不代表项目文件仍有效。 */
const ZR_SELECTED_PROJECT_KEY = 'zr.selectedProjectUri';
/** 扩展模块级的变更广播源；订阅者释放各自事件租约，本模块只在状态更新后 fire。 */
const onDidChangeSelectedProjectEmitter = new vscode.EventEmitter<vscode.Uri | undefined>();

/**
 * ## Summary
 * 表示由工作区 `.zrp` 清单构造的项目视图；URI 和 VS Code 根目录服务于宿主交互，路径字段服务于源码归属与调试配置。
 *
 * ## Fields
 * - `id` 是清单 URI 的字符串形式，用于和 `workspaceState` 中的选择值对应。
 * - `uri` 是清单 URI；`workspaceFolder` 是 `getWorkspaceFolder(uri)` 返回的宿主根对象。
 * - `manifest` 是字段筛选后的清单值；`projectPath` 是清单路径文本，`projectDirectoryPath` 是其目录文本。
 * - `sourceRootPath` 是目录与 `manifest.source` 组合后的比较路径。
 * - `relativePath` 是相对工作区根的展示路径；`label` 采用清单名或文件名后备标签。
 *
 * ## Invariants and lifecycle
 * 当前构造器只在 URI 属于一个 workspace folder 且清单可解析时创建对象。路径字段是文本推导值，不证明磁盘目录存在；`uri` 和 `workspaceFolder` 引用 VS Code 对象，不由该模型释放。
 *
 * ## References
 * `createWorkspaceProject` 创建此模型；`discoverWorkspaceProjects` 返回排序后的列表；选择器、结构视图和调试配置按需消费不同字段。
 */
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

/**
 * ## Summary
 * 向桌面宿主的项目操作、结构视图和语言服务器同步层公开选择变化事件。
 *
 * ## Value model
 * 事件载荷是当前选择的 `vscode.Uri`，清除选择时为 `undefined`；它来自本模块唯一的 `EventEmitter`。
 *
 * ## Constraints and lifecycle
 * `setSelectedProjectUri` 只在 workspaceState 值变化且 `update` 成功后同步 fire。订阅者负责释放 `event` 返回的 disposable，通常由扩展上下文或控制器持有；本导出不拥有订阅者资源。
 *
 * ## References
 * `setSelectedProjectUri` 是仓内发射入口；`extension.ts`、`projectActions.ts` 与 `structure.ts` 注册消费者。
 */
export const onDidChangeSelectedProject = onDidChangeSelectedProjectEmitter.event;

/**
 * ## Summary
 * 扫描 VS Code 工作区中的 `.zrp`，构造有效项目并给选择器、项目操作和结构视图提供共享候选集。
 *
 * ## Returns
 * @returns 按 `relativePath` 比较结果排序的项目数组；无匹配或所有清单无效时为空数组。返回数组和项目模型由调用方持有。
 *
 * ## Constraints and lifecycle
 * 查询使用模块级 include/exclude glob；每个 URI 由 `createWorkspaceProject` 异步解析，未能得到模型的 `undefined` 项会过滤掉。读取/解码或创建期间任一 Promise 拒绝会使整体 `Promise.all` 拒绝，函数不会局部吞掉读取失败，也不提供取消或固定工作区快照。
 *
 * ## References
 * `resolveSelectedProjectUri`、`resolveSelectedWorkspaceProject`、`selectWorkspaceProject` 和 `hasWorkspaceProjects` 调用本扫描入口；`createWorkspaceProject` 实施单清单解析。
 */
export async function discoverWorkspaceProjects(): Promise<WorkspaceProject[]> {
    const uris = await vscode.workspace.findFiles(ZR_PROJECT_GLOB, ZR_PROJECT_EXCLUDE_GLOB);
    // 保留 Promise.all 的整体拒绝边界；单个 readFile 失败会传播到发现调用方，见下方 read-file-failure BUG。
    const projects = await Promise.all(uris.map(async (uri) => createWorkspaceProject(uri)));

    return projects
        .filter((project): project is WorkspaceProject => project !== undefined)
        .sort((left, right) => compareStrings(left.relativePath, right.relativePath));
}

/**
 * ## Summary
 * 为一个清单 URI 创建工作区项目模型；优先读取已打开文档中的未保存文本，否则经 workspace 文件系统读取 UTF-8 字节，避免为发现而打开编辑器文档。
 *
 * ## Parameters
 * @param uri 候选 `.zrp` URI；此函数只接受 `getWorkspaceFolder` 能映射到当前 workspace folder 的 URI。
 *
 * ## Returns
 * @returns workspace folder 不存在或必填清单字段无效时返回 `undefined`；有效清单时返回新 `WorkspaceProject`。返回值不拥有 VS Code URI/folder 对象。
 *
 * ## Constraints and lifecycle
 * 先用 URI 字符串精确查找打开文档，再回退到 `workspace.fs.readFile` 和 UTF-8 `TextDecoder`；不调用 `openTextDocument`。文件读取拒绝会向上抛出并可中断整个 `discoverWorkspaceProjects`；合法 JSON `null` 也可能由解析器抛出。路径字段只作文本组合，不检查源码目录是否存在。
 *
 * ## References
 * `discoverWorkspaceProjects` 为每个发现 URI 调用此函数；`workspaceProjects.test.js` 的三项用例验证未打开磁盘文本、打开文档的未保存文本以及无效未保存文本不回退磁盘。
 */
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

/**
 * ## Summary
 * 把一个 VS Code URI 转成路径文本，再按最深匹配源码根查找其工作区项目，供活动编辑器优先级和结构视图同步使用。
 *
 * ## Parameters
 * @param uri 待匹配 URI；优先采用 `fsPath`，为空时采用 URI 的 `path`。
 * @param projects 本轮发现的候选模型数组；匹配期间只读。
 *
 * ## Returns
 * @returns `pickBestProjectForFile` 返回的项目对象，或没有源码根匹配时的 `undefined`；对象仍由调用方持有。
 *
 * ## Constraints and lifecycle
 * 只匹配 URI 路径，不检查 URI scheme、磁盘状态或 workspace folder；项目根与文件路径须使用相同文件系统语义。该函数不写入选择状态。
 *
 * ## References
 * `resolveSelectedProjectUri` 对活动文件调用此函数；`pickBestProjectForFile` 实施候选目录边界和最长根选择。
 */
export function pickWorkspaceProjectForUri(
    uri: vscode.Uri,
    projects: WorkspaceProject[],
): WorkspaceProject | undefined {
    return pickBestProjectForFile(normalizeFilePath(uri.fsPath || uri.path), projects);
}

/**
 * ## Summary
 * 仅按 URI path 后缀识别 `.zrp` 清单，使选择和刷新逻辑不依赖 VS Code language ID。
 *
 * ## Parameters
 * @param uri 要检查的 VS Code URI；scheme 不参与本判断。
 *
 * ## Returns
 * @returns `uri.path` 转为小写后以 `.zrp` 结尾时返回 `true`，否则返回 `false`。
 *
 * ## Constraints and lifecycle
 * 不读取 URI 指向的内容、不验证文件存在，也不要求 URI 使用 `file` scheme。调用方需自行决定非文件 URI 是否适用。
 *
 * ## References
 * `isZrpDocument` 用文档 URI 复用此判断；`resolveSelectedProjectUri` 和 `findProjectFile` 用它区分活动清单与源码。
 */
export function isZrpUri(uri: vscode.Uri): boolean {
    return uri.path.toLowerCase().endsWith('.zrp');
}

/**
 * ## Summary
 * 从文本文档的 URI 判断它是否是 `.zrp` 清单，供结构视图文档保存监听器决定是否刷新。
 *
 * ## Parameters
 * @param document VS Code 文本文档；只读取它的 `uri` 属性。
 *
 * ## Returns
 * @returns 与 `isZrpUri(document.uri)` 相同的布尔值。
 *
 * ## Constraints and lifecycle
 * 不读取文档文本、不依赖 languageId，也不持有文档；清单内容是否有效由后续项目解析决定。
 *
 * ## References
 * `ZrStructureController` 的文档变更监听调用此 helper；`isZrpUri` 定义扩展名规则。
 */
export function isZrpDocument(document: vscode.TextDocument): boolean {
    return isZrpUri(document.uri);
}

/**
 * ## Summary
 * 将调试配置给出的项目路径转为文件 URI；空值或非字符串值走活动 `.zrp` 与工作区搜索后备流程。
 *
 * ## Parameters
 * @param folder 可选的工作区根，用于解释显式相对路径及限制后备文件搜索。
 * @param projectPath 未信任的调试配置值；只在它是修剪后非空字符串时按显式路径处理。
 *
 * ## Returns
 * @returns 显式路径转换出的 `Uri.file`，或 `findProjectFile` 找到/用户选择的 URI；没有候选或用户取消时为 `undefined`。
 *
 * ## Constraints and lifecycle
 * 显式路径不会验证清单存在或内容有效。空路径后备仅由直接调用方可达；当前 `ZrDebugConfigurationProvider` 在传入该函数前已要求 project 为非空字符串。URI 构造、工作区查询和 Quick Pick 的拒绝会向调用方传播。
 *
 * ## References
 * `ZrDebugConfigurationProvider.resolveDebugConfiguration` 传入项目路径；`resolveRelativePath` 负责显式路径解释，`findProjectFile` 负责空值后备。
 */
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

/**
 * ## Summary
 * 为运行、调试与桌面 LSP 解析当前项目 URI，依次考虑活动文件归属、唯一候选、已存选择及允许时的 Quick Pick。
 *
 * ## Parameters
 * @param context 扩展上下文；只使用 `workspaceState` 持久化选择。
 * @param folder 可选的选择器搜索根；当前项目发现和前置优先级仍扫描全工作区，完整 folder 隔离仍待下方 TODO 确认。
 * @param allowPrompt 为 `false` 时禁止交互；为 `true` 时在无可用优先项且有多个候选时可显示 Quick Pick。
 *
 * ## Returns
 * @returns 选择的清单 URI；没有有效候选、无匹配选择且不允许提示或用户取消时返回 `undefined`。活动匹配、唯一候选及空工作区分支可能更新 workspaceState 并发出选择事件。
 *
 * ## Constraints and lifecycle
 * 每次调用都会完整运行 `discoverWorkspaceProjects`。仅 `file` scheme 活动 URI 参与自动归属；活动 `.zrp` 精确匹配清单 URI，其他活动文件按最长源码根匹配。活跃项目匹配优先于记忆选择，因此事件触发后的重入可能覆盖用户刚选的另一项目（附近 `BUG`）。发现、持久化和选择器错误向调用方传播；记忆值必须匹配本轮发现候选的 ID，匹配后不再另作文件存在性探测。
 *
 * ## References
 * 桌面 `extension.startClient`、`selectedProjectSync`、项目操作、调试提供器和 Web 不可用提示走此入口；`selectWorkspaceProject` 实施交互分支，`setSelectedProjectUri` 负责状态写入与广播。
 */
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

/**
 * ## Summary
 * 为结构视图把选中的项目 URI 映射回本次发现的 `WorkspaceProject` 模型，供视图读取清单、标签和源码根。
 *
 * ## Parameters
 * @param context 用于读取或更新 workspaceState 选择的扩展上下文。
 * @param folder 传给 URI 选择流程的可选工作区根；当前实现没有把两次发现约束为该根。
 * @param allowPrompt 控制选择流程是否可以显示 Quick Pick。
 *
 * ## Returns
 * @returns 已选择 URI 在首次候选数组中的对应模型；未选中或该 URI 不在首次快照中时返回 `undefined`。
 *
 * ## Constraints and lifecycle
 * 当前先发现一次，再调用会再次发现的 `resolveSelectedProjectUri`。工作区在两次扫描之间变化时，第二次选中的 URI 可能无法在第一次数组中找到；该快照一致性仍由本函数内 TODO 跟进。底层查询、持久化或提示失败会拒绝 Promise。
 *
 * ## References
 * `ZrStructureController` 调用此函数建立结构视图项目根；`discoverWorkspaceProjects` 和 `resolveSelectedProjectUri` 分别提供模型快照与选择结果。
 */
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

/**
 * ## Summary
 * 从预载或新发现的项目中选定 URI；单项目直接选中，多项目显示 Quick Pick，成功后持久化并广播变化。
 *
 * ## Parameters
 * @param context 提供 workspaceState 持久化的扩展上下文。
 * @param folder 可选调用方工作区根；当前参数未参与筛选，源码以 TODO 留待核对多根工作区期望。
 * @param preloadedProjects 可选候选快照；提供时不重新发现，数组只在此调用期间读取。
 *
 * ## Returns
 * @returns 成功选中的清单 URI；没有候选或用户取消/返回无法对应的条目时为 `undefined`。取消路径不改写旧选择。
 *
 * ## Constraints and lifecycle
 * 多候选 UI 显示 `label` 与 `relativePath`，并在条目上携带 URI 供回查；只有匹配到项目后才写状态。空候选会清空旧选择。发现、Quick Pick 或 workspaceState 更新的异步错误向调用方传播；当前无 folder 过滤和并发串行化。
 *
 * ## References
 * `resolveSelectedProjectUri` 在允许提示时调用此函数；桌面 `DesktopProjectActionsController.selectProject` 和 Web 不可用入口也会使用项目选择路径。
 */
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
            // 将项目模型投影成选择器需要的显示字段，同时保留 URI 供返回项回查。
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

/**
 * ## Summary
 * 为状态栏和 Web 项目操作入口判断工作区是否至少有一个可解析项目。
 *
 * ## Returns
 * @returns 完整发现结果中项目数组非空时为 `true`，否则为 `false`。
 *
 * ## Constraints and lifecycle
 * 这是完整扫描而非文件名快速探测：会逐清单读取并解析，任一未捕获的读取/解析拒绝会使 Promise 拒绝。没有缓存或取消机制。
 *
 * ## References
 * `DesktopProjectActionsController.computeState` 与 Web 不可用操作注册流程使用此探测；`discoverWorkspaceProjects` 提供具体候选。
 */
export async function hasWorkspaceProjects(): Promise<boolean> {
    return (await discoverWorkspaceProjects()).length > 0;
}

/**
 * ## Summary
 * 读取 workspaceState 中上次保存的项目 URI，供显式读取选择值的消费者使用。
 *
 * ## Parameters
 * @param context 提供 workspaceState 的扩展上下文。
 *
 * ## Returns
 * @returns 存储值为 truthy 字符串时通过 `vscode.Uri.parse` 返回 URI；键缺失或为空时返回 `undefined`。
 *
 * ## Constraints and lifecycle
 * 只解析序列化值，不重新发现项目、不验证目标清单是否仍存在，也不修复无效/过期状态。返回 URI 由调用方持有，本函数不更新状态或发事件。
 *
 * ## References
 * 使用 `ZR_SELECTED_PROJECT_KEY`；需要验证当前工作区候选时应调用 `resolveSelectedProjectUri`。
 */
export function getSelectedProjectUri(context: vscode.ExtensionContext): vscode.Uri | undefined {
    const storedId = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);
    return storedId ? vscode.Uri.parse(storedId) : undefined;
}

/**
 * ## Summary
 * 处理调试项目路径缺省时的后备查找：优先采用活动编辑器中的 `.zrp`，否则在指定根或所有工作区根搜索并允许用户挑选。
 *
 * ## Parameters
 * @param folder 可选的 workspace folder；存在时每根最多请求 50 个 `.zrp`，缺省时遍历当前所有根。
 *
 * ## Returns
 * @returns 活动清单、唯一搜索结果或用户选择的 URI；无结果或用户取消时为 `undefined`。
 *
 * ## Constraints and lifecycle
 * 当前活动 `.zrp` 优先级没有核对它是否属于传入 `folder`，该多根工作区契约由函数内 TODO 跟进。搜索不读取清单内容，也不保证结果可解析；`findFiles` 与 Quick Pick 错误传播。多根搜索按根串行执行。
 *
 * ## References
 * `resolveProjectUri` 的空路径分支调用此函数；当前 `ZrDebugConfigurationProvider` 在仓内要求非空项目路径，因此该后备分支暂无可达配置调用。
 */
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
            // Quick Pick 只展示文件名与 URI 路径，但保留原 URI 作为选中项载荷。
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

/**
 * ## Summary
 * 根据活动编辑器定位当前 workspace folder；没有活动编辑器时回退到首个已打开工作区根。
 *
 * ## Returns
 * @returns 活动文档所属根或首个 workspace folder；没有工作区或活动文档在所有根之外时返回 `undefined`。
 *
 * ## Constraints and lifecycle
 * 不访问文件系统、不创建工作区，也不持有活动编辑器。活动文档存在但不属于工作区时不会回退到首根。
 *
 * ## References
 * `extension.startClient`、项目操作、结构视图和 LSP 项目同步调用此 helper 取得默认根。
 */
export function activeWorkspaceFolder(): vscode.WorkspaceFolder | undefined {
    const activeDocument = vscode.window.activeTextEditor?.document.uri;
    return activeDocument
        ? vscode.workspace.getWorkspaceFolder(activeDocument)
        : vscode.workspace.workspaceFolders?.[0];
}

/**
 * ## Summary
 * 将运行或调试配置里的本地路径解释为宿主文件路径；相对路径以显式根或首个 workspace folder 为基准。
 *
 * ## Parameters
 * @param folder 可选基准根；缺省时尝试使用首个 workspace folder。
 * @param value 配置中的路径文本；函数会先修剪首尾空白。
 *
 * ## Returns
 * @returns 绝对路径经 `vscode.Uri.file` 转换后的 `fsPath`，相对路径按 URI 根拼接后的 `fsPath`；若无任何根则返回修剪后的原相对文本。空白输入返回空串。
 *
 * ## Constraints and lifecycle
 * 绝对路径通过宿主 URI 规则识别，不检查文件是否存在；尝试 Windows 盘符的精确往返失败时再走通用绝对路径判断。相对路径在无工作区时不解析，URI 构造异常可能向调用方传播。
 *
 * ## References
 * `resolveProjectUri` 解析显式项目路径；`ZrDebugConfigurationProvider.resolveDebugConfiguration` 用它解释其他运行/调试路径字段。
 */
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

/**
 * ## Summary
 * 为项目选择器生成相对工作区根的展示路径，帮助区分名称相同的清单。
 *
 * ## Parameters
 * @param workspaceFolder 包含清单 URI 的 VS Code 工作区根。
 * @param uri 清单 URI；函数优先比较两个 URI 的 `path` 文本。
 *
 * ## Returns
 * @returns URI 位于根路径下时返回归一化后的相对后缀；否则委托 `workspace.asRelativePath(uri, false)`。
 *
 * ## Constraints and lifecycle
 * 结果用于展示和排序，不作为真实文件系统相对路径证明。VS Code 回退格式由宿主 API 决定；输入 URI/folder 借用至同步调用结束。
 *
 * ## References
 * `createWorkspaceProject` 把结果写入 `WorkspaceProject.relativePath`；`discoverWorkspaceProjects` 按该字段排序。
 */
function relativePathWithinFolder(workspaceFolder: vscode.WorkspaceFolder, uri: vscode.Uri): string {
    const folderPath = normalizeFilePath(workspaceFolder.uri.path);
    const filePath = normalizeFilePath(uri.path);
    if (filePath.startsWith(`${folderPath}/`)) {
        return filePath.slice(folderPath.length + 1);
    }

    return vscode.workspace.asRelativePath(uri, false);
}

/**
 * ## Summary
 * 从清单文件名文本中移除最后一个扩展后缀，为未命名项目生成列表标签。
 *
 * ## Parameters
 * @param value URI path 中的末段文本；调用方已通过 `lastPathSegment` 提取。
 *
 * ## Returns
 * @returns 最后一个 `.` 位于索引大于 0 时返回其之前的前缀，否则返回原串；隐藏文件名如 `.project` 保持不变。
 *
 * ## Constraints and lifecycle
 * 仅处理字符串，不检查文件类型或存在性；一个字符串中的多个点只按最后一个处理。
 *
 * ## References
 * `createWorkspaceProject` 在 manifest.name 缺省时用此函数形成 `WorkspaceProject.label`。
 */
function removeExtension(value: string): string {
    const lastDot = value.lastIndexOf('.');
    return lastDot > 0 ? value.slice(0, lastDot) : value;
}

/**
 * ## Summary
 * 从 URI 路径文本取得最后一个非空路径段，用于清单名缺省和文件选择器标签。
 *
 * ## Parameters
 * @param pathValue URI path 文本；该 helper 只作分隔符归一化，不解析 URI 编码。
 *
 * ## Returns
 * @returns 归一化路径拆分后最后一个非空段；若无非空段，返回归一化后的原文本。
 *
 * ## Constraints and lifecycle
 * 尾部分隔符会先被 `normalizeFilePath` 去除；根路径可能因该 helper 的根斜线行为变为空串。过滤谓词只剔除空段，不改写其他字符。
 *
 * ## References
 * `createWorkspaceProject` 生成 fallback label；`findProjectFile` 为搜索结果生成 Quick Pick label。
 */
function lastPathSegment(pathValue: string): string {
    const normalized = normalizeFilePath(pathValue);
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}

/**
 * ## Summary
 * 用宿主字符串排序规则比较两个展示路径，供工作区发现结果排序，避免依赖文件查找返回顺序。
 *
 * ## Parameters
 * @param left 左侧相对路径文本。
 * @param right 右侧相对路径文本。
 *
 * ## Returns
 * @returns `String.localeCompare` 的排序结果，直接交给 `Array.sort`。
 *
 * ## Constraints and lifecycle
 * 排序受当前 JavaScript/宿主 locale 比较规则影响，不承诺跨 locale 的字节序；函数不持有参数。
 *
 * ## References
 * `discoverWorkspaceProjects` 按 `WorkspaceProject.relativePath` 调用此 comparator。
 */
function compareStrings(left: string, right: string): number {
    return left.localeCompare(right);
}

/**
 * ## Summary
 * 更新当前 workspaceState 项目 URI，并在值确实变化且持久化成功后广播变更，避免视图与 LSP 同步事件的自激循环。
 *
 * ## Parameters
 * @param context 持有本工作区状态的 VS Code 扩展上下文。
 * @param uri 新选择的清单 URI；`undefined` 表示清除选择，URI 在写入前序列化成字符串。
 *
 * ## Returns
 * @returns Promise 在无变化时立即完成；发生变化时在 `workspaceState.update` 成功且同步 fire 事件后完成。更新拒绝会向调用方传播，且不会 fire。
 *
 * ## Constraints and lifecycle
 * 比较只针对前后 URI 字符串；不验证目标文件、不发现候选或检查 folder。通知顺序是先 await 状态更新，再调用 emitter；本函数没有并发写入串行器，调用方仍需负责其业务时序。
 *
 * ## References
 * `resolveSelectedProjectUri`、`selectWorkspaceProject` 和空工作区分支调用此函数；`onDidChangeSelectedProject` 暴露其成功变更通知。
 */
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
