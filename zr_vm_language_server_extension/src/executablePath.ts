import * as fs from 'node:fs';
import * as path from 'node:path';

/** 项目运行与调试共同读取的新旧 CLI 设置；非空新设置优先。 */
export interface CliSettingInputs {
    executablePath?: string;
    legacyDebugCliPath?: string;
}

/** 将用户配置路径映射到当前工作区和扩展安装位置；existsSync 可供测试注入。 */
export interface ConfiguredPathResolutionInputs {
    configuredPath?: string;
    workspaceFolderPaths?: string[];
    extensionPath?: string;
    existsSync?: (candidate: string) => boolean;
}

// 设置读取方可能返回空白；统一清理后再判断是否覆盖兼容设置。
function trimConfiguredPath(value: string | undefined): string {
    return typeof value === 'string' ? value.trim() : '';
}

// 保留多根工作区的先后顺序；相同解析路径只尝试一次。
function dedupeCandidates(values: string[]): string[] {
    const seen = new Set<string>();
    const result: string[] = [];

    for (const value of values) {
        const normalized = path.resolve(value);
        if (seen.has(normalized)) {
            continue;
        }

        seen.add(normalized);
        result.push(normalized);
    }

    return result;
}

// 额外识别 Windows 盘符写法，避免把该配置逐个拼接到工作区目录。
// TODO: 非 Windows 宿主的 path.resolve 仍会按本机规则解释盘符路径；需明确跨平台设置同步契约。
function isAbsoluteConfiguredPath(value: string): boolean {
    return path.isAbsolute(value) || /^[A-Za-z]:[\\/]/.test(value);
}

/**
 * 生成显式配置的候选路径：绝对配置只生成一条宿主归一化路径，
 * 相对路径按工作区顺序再到扩展目录。
 * 返回归一化路径而不查询文件系统，供解析器与优先级测试共同使用。
 */
export function configuredPathCandidates(inputs: ConfiguredPathResolutionInputs): string[] {
    const configuredPath = trimConfiguredPath(inputs.configuredPath);
    if (configuredPath.length === 0) {
        return [];
    }

    if (isAbsoluteConfiguredPath(configuredPath)) {
        return [path.resolve(configuredPath)];
    }

    const candidates: string[] = [];

    // 用户工作区优先于扩展捆绑目录，避免同名 CLI 意外遮蔽工作区设置。
    for (const workspaceFolderPath of inputs.workspaceFolderPaths ?? []) {
        const trimmedWorkspacePath = trimConfiguredPath(workspaceFolderPath);
        if (trimmedWorkspacePath.length === 0) {
            continue;
        }

        candidates.push(path.resolve(trimmedWorkspacePath, configuredPath));
    }

    const extensionPath = trimConfiguredPath(inputs.extensionPath);
    if (extensionPath.length > 0) {
        candidates.push(path.resolve(extensionPath, configuredPath));
    }

    return dedupeCandidates(candidates);
}

/**
 * 按候选顺序选第一个存在的显式配置路径，供 nativeAssets 的宿主入口使用。
 * 只确认存在性，不负责 CLI 可执行权限或文件类型；调用方仍需验证用途。
 */
export function resolveConfiguredPath(inputs: ConfiguredPathResolutionInputs): string | undefined {
    const existsSync = inputs.existsSync ?? fs.existsSync;

    for (const candidate of configuredPathCandidates(inputs)) {
        // BUG: 设置指向现存目录时也会返回；nativeAssets 将其当进程路径，启动时才失败。
        // 需在配置解析边界验证普通文件与可执行性，并补充目录配置用例。
        if (existsSync(candidate)) {
            return candidate;
        }
    }

    return undefined;
}

/** 将新 `zr.executablePath` 与旧调试设置合并为一个 CLI 路径选择入口。 */
export function resolvePreferredCliSetting(inputs: CliSettingInputs): string {
    const explicitExecutablePath = trimConfiguredPath(inputs.executablePath);
    if (explicitExecutablePath.length > 0) {
        return explicitExecutablePath;
    }

    return trimConfiguredPath(inputs.legacyDebugCliPath);
}
