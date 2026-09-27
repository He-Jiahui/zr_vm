import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vscode from 'vscode';
import assetLayout from '../asset-layout.json';
import { resolveConfiguredPath, resolvePreferredCliSetting } from './executablePath';
import {
    pickFirstExistingDirectoryWithFiles,
    pickFirstExistingPath,
    pickLatestExistingDirectoryWithFiles,
    pickLatestExistingPath,
} from './nativePathSelection';

/** 桌面宿主读取服务端配置时使用的 VS Code 配置命名空间。 */
export const LANGUAGE_SERVER_CONFIG_SECTION = 'zr.languageServer';
/** 调试配置仍承载旧版 CLI 路径，供迁移期回退。 */
export const DEBUG_CONFIG_SECTION = 'zr.debug';
/** 项目运行与调试共享的首选 CLI 路径所属命名空间。 */
export const ROOT_CONFIG_SECTION = 'zr';

/** 资产清单中同时打包的两个原生入口；名称由平台清单统一决定。 */
type NativeBinaryKind = 'languageServer' | 'cli';

// TODO: 此回退清单与 asset-layout.json 的 win32 清单重复；核查配置缺失时是否仍需
// 静态回退，并让打包清单变更时能检测两者漂移。
const WINDOWS_REQUIRED_RUNTIME_FILES = [
    'zr_vm_language_server_stdio.exe',
    'zr_vm_cli.exe',
    'zr_vm_core.dll',
    'zr_vm_debug.dll',
    'zr_vm_language_server.dll',
    'zr_vm_library.dll',
    'zr_vm_parser.dll',
    'zr_vm_lib_container.dll',
    'zr_vm_lib_ffi.dll',
    'zr_vm_lib_math.dll',
    'zr_vm_lib_system.dll',
];

/** 与打包脚本共用 asset-layout.json，避免运行时猜测可执行文件后缀。 */
function executableName(kind: NativeBinaryKind): string {
    const executable = assetLayout.native.executables[kind] as Record<string, string>;
    return executable[process.platform] ?? executable.default;
}

/** 只解释资产清单中的相对路径模板，供捆绑目录和开发构建候选复用。 */
function renderPathTemplate(template: string, replacements: Record<string, string> = {}): string {
    const rendered = Object.entries(replacements).reduce(
        (value, [key, replacement]) => value.replace(new RegExp(`\\{${key}\\}`, 'g'), replacement),
        template,
    );
    const segments = rendered
        .split(/[\\/]+/)
        .filter((segment) => segment.length > 0 && segment !== '.');
    return segments.length > 0 ? path.join(...segments) : '.';
}

/** 定位当前宿主平台与架构的扩展内资产目录。 */
function resolveBundledNativeAssetDir(extensionRoot: string): string {
    return path.join(
        extensionRoot,
        renderPathTemplate(assetLayout.native.bundledRelativeDir, {
            platform: process.platform,
            arch: process.arch,
        }),
    );
}

/** 完整目录检查与同步脚本采用同一平台清单，防止混用不齐全的 DLL。 */
function requiredRuntimeFiles(): string[] {
    const filesByPlatform = assetLayout.native.requiredRuntimeFiles as Record<string, string[]>;
    const files = filesByPlatform[process.platform] ?? filesByPlatform.default;
    return Array.isArray(files) ? [...files] : WINDOWS_REQUIRED_RUNTIME_FILES;
}

/** 去重候选根，保持工作区优先级和构建目录扫描顺序。 */
function dedupePaths(paths: string[]): string[] {
    const seen = new Set<string>();
    const result: string[] = [];

    for (const value of paths) {
        const normalized = path.resolve(value);
        if (seen.has(normalized)) {
            continue;
        }

        seen.add(normalized);
        result.push(normalized);
    }

    return result;
}

/** 将布局中的相对构建子目录落实到同一个候选根。 */
function resolveTemplateDirs(baseDir: string, templates: string[]): string[] {
    return templates.map((template) => {
        const relativePath = renderPathTemplate(template);
        return relativePath === '.'
            ? path.resolve(baseDir)
            : path.resolve(baseDir, relativePath);
    });
}

/** 开发态扫描扩展相邻 build 下各配置产物；缺失不会阻止已打包扩展启动。
 * @note TODO: 构建脚本支持 ZR_NATIVE_BUILD_DIR 指向其他位置；核查未同步资产的开发态
 * 是否也需要识别该目录，入口见 scripts/artifact-layout.js。
 */
function collectBuildCandidateDirs(buildRoot: string): string[] {
    const scannedDirs: string[] = [];

    try {
        for (const entry of fs.readdirSync(buildRoot, { withFileTypes: true })) {
            if (!entry.isDirectory()) {
                continue;
            }

            scannedDirs.push(
                ...resolveTemplateDirs(
                    path.join(buildRoot, entry.name),
                    assetLayout.native.scannedBuildSubdirs,
                ),
            );
        }
    } catch {
        // 缺失或不可枚举的构建根不会阻止已打包扩展启动。
    }

    return dedupePaths(scannedDirs);
}

/** 多根工作区的相对路径先按当前编辑器所属目录解释，再试其他根。 */
function orderedWorkspaceFolderPaths(): string[] {
    const activeWorkspaceFolder = vscode.window.activeTextEditor?.document
        ? vscode.workspace.getWorkspaceFolder(vscode.window.activeTextEditor.document.uri)
        : undefined;
    const allWorkspaceFolders = vscode.workspace.workspaceFolders ?? [];
    const ordered = [
        activeWorkspaceFolder?.uri.fsPath,
        ...allWorkspaceFolders.map((folder) => folder.uri.fsPath),
    ].filter((value): value is string => typeof value === 'string' && value.length > 0);

    return dedupePaths(ordered);
}

/** 优先选完整捆绑资产，其次选最近更新的完整开发构建，保持依赖同目录。 */
function resolveNativeAssetDirectory(context: vscode.ExtensionContext): string | undefined {
    const bundledDir = resolveBundledNativeAssetDir(context.extensionPath);
    const buildCandidateDirs = collectBuildCandidateDirs(path.join(context.extensionPath, '..', 'build'));

    return pickFirstExistingDirectoryWithFiles([bundledDir], requiredRuntimeFiles()) ??
        pickLatestExistingDirectoryWithFiles(buildCandidateDirs, requiredRuntimeFiles());
}

/** 显式设置优先；自动发现先保完整运行时，再允许单入口开发产物回退。 */
function resolveNativeExecutable(
    context: vscode.ExtensionContext,
    kind: NativeBinaryKind,
    configuredPath?: string,
): string | undefined {
    const extensionRoot = context.extensionPath;
    const fileName = executableName(kind);
    const explicitPath = resolveConfiguredPath({
        configuredPath,
        workspaceFolderPaths: orderedWorkspaceFolderPaths(),
        extensionPath: extensionRoot,
    });
    // BUG: resolveConfiguredPath 只检查存在性；用户把设置指向现存目录时这里仍返回目录，
    // 桌面 LSP 的 command 或 CLI 的 ProcessExecution/spawn 随后无法执行它。
    if (explicitPath) {
        return explicitPath;
    }

    const assetDirectory = resolveNativeAssetDirectory(context);
    if (assetDirectory) {
        return path.join(assetDirectory, fileName);
    }

    const bundledExecutable = path.join(resolveBundledNativeAssetDir(extensionRoot), fileName);
    const buildExecutables = collectBuildCandidateDirs(path.join(extensionRoot, '..', 'build'))
        .map((directory) => path.join(directory, fileName));

    return pickFirstExistingPath([bundledExecutable]) ??
        pickLatestExistingPath(buildExecutables);
}

/** 返回约定捆绑位置，供只需布局路径而不触发文件探测的调用方使用。
 * @note TODO: 仓内当前未发现调用；核查扩展外消费者后再决定是否保留公开入口。
 */
export function bundledNativeExecutablePath(context: vscode.ExtensionContext, kind: NativeBinaryKind): string {
    return path.join(
        resolveBundledNativeAssetDir(context.extensionPath),
        executableName(kind),
    );
}

/** 桌面语言客户端启动前选定 stdio 服务端路径；找不到时交由宿主提示用户。
 * @note 相对配置路径按当前编辑器工作区优先解析，返回值仅表示路径被发现。
 */
export function resolveNativeLanguageServerPath(
    context: vscode.ExtensionContext,
    config: vscode.WorkspaceConfiguration,
): string | undefined {
    return resolveNativeExecutable(context, 'languageServer', config.get<string>('native.path', ''));
}

/** 项目运行与本地调试共用 CLI 选择规则，首选 zr.executablePath，兼容旧设置。
 * @note 无配置且无可用资产时返回 undefined，由调用方决定提示或禁用操作。
 */
export function resolveNativeCliPath(
    context: vscode.ExtensionContext,
    config?: vscode.WorkspaceConfiguration,
): string | undefined {
    const rootConfig = vscode.workspace.getConfiguration(ROOT_CONFIG_SECTION);
    const debugConfig = config ?? vscode.workspace.getConfiguration(DEBUG_CONFIG_SECTION);
    const configuredPath = resolvePreferredCliSetting({
        executablePath: rootConfig.get<string>('executablePath', ''),
        legacyDebugCliPath: debugConfig.get<string>('cli.path', ''),
    });
    return resolveNativeExecutable(context, 'cli', configuredPath);
}
