/** 扩展从 .zrp 提取的项目字段；工作区发现、结构视图与调试各取所需。 */
export interface ParsedProjectManifest {
    name?: string;
    source: string;
    binary: string;
    entry: string;
    pathAliases?: Record<string, string>;
    dependency?: string;
    local?: string;
    projectPath: string;
}

/** 将文件归属到项目时只需要稳定 ID 与源码根，不依赖 VS Code 类型。 */
export interface ProjectPathMatchCandidate {
    id: string;
    sourceRootPath: string;
}

/** JSON 边界的未验证记录，字段必须经下方读取器筛选后才能进入项目模型。 */
type ManifestRecord = Record<string, unknown>;

/** 统一宿主路径分隔符，供项目匹配和别名展示使用；不承担平台路径解析。 */
export function normalizeFilePath(value: string): string {
    // BUG: 根路径 / 会被删成空串；source 为 / 时 joinNormalizedPath 错把项目目录
    // 当作源码根，isPathInsideDirectory 也无法识别根目录下的源码文件。
    return value.replace(/[\\/]+/g, '/').replace(/\/+$/g, '');
}

/** 从清单路径取得项目目录，供相对 source 定位使用；调用方传入完整文件路径。 */
export function dirnameFromPath(value: string): string {
    const normalized = normalizeFilePath(value);
    const lastSlash = normalized.lastIndexOf('/');
    // BUG: 根目录下的 /demo.zrp 使 lastSlash 为 0，此处分支返回文件路径本身；
    // createWorkspaceProject 随后把 source 拼成 /demo.zrp/src，活动源码无法匹配该项目。
    return lastSlash > 0 ? normalized.slice(0, lastSlash) : normalized;
}

/** 将项目目录与清单 source 合成比较用路径，消去相对段以免误配源码目录。 */
export function joinNormalizedPath(basePath: string, relativePath: string): string {
    const normalizedBasePath = normalizeFilePath(basePath);
    const normalizedRelativePath = normalizeFilePath(relativePath);
    if (normalizedRelativePath.startsWith('/')) {
        return normalizedRelativePath;
    }

    // TODO: zrp.schema.json 对 source 仅规定字符串，DAP 的 path.resolve 接受绝对路径；需确认
    // Windows 盘符绝对路径是否有效。若有效，这里会把 E:/src 拼到 D:/project 后，导致项目归属判断失败。
    const combined = `${normalizedBasePath}/${normalizedRelativePath}`;
    const segments = combined.split('/');
    const resolved: string[] = [];

    for (const segment of segments) {
        if (!segment || segment === '.') {
            if (resolved.length === 0 && combined.startsWith('/')) {
                resolved.push('');
            }
            continue;
        }

        if (segment === '..') {
            // BUG: Windows 路径 D:/project 的 ../../src 会把 D: 也弹出，结果变成
            // 相对路径 src；createWorkspaceProject 的源码根与宿主解析的 D:/src 不一致。
            if (resolved.length > 1 || (resolved.length === 1 && resolved[0] !== '')) {
                resolved.pop();
            }
            continue;
        }

        resolved.push(segment);
    }

    return resolved.length === 1 && resolved[0] === ''
        ? '/'
        : resolved.join('/');
}

/** 解析项目发现和调试所需字段；语法错误或缺少必填字段时供调用方跳过该项目。 */
export function parseProjectManifestText(
    text: string,
    projectPath: string,
): ParsedProjectManifest | undefined {
    let parsed: ManifestRecord;

    try {
        parsed = JSON.parse(text) as ManifestRecord;
    } catch {
        return undefined;
    }

    // BUG: JSON 文本 "null" 可以解析成功却不是记录；字段读取抛 TypeError，
    // createWorkspaceProject/discoverWorkspaceProjects 会中断整批发现，而不是跳过此清单。
    const source = readRequiredString(parsed, 'source');
    const binary = readRequiredString(parsed, 'binary');
    const entry = readRequiredString(parsed, 'entry');
    if (!source || !binary || !entry) {
        return undefined;
    }

    return {
        name: readOptionalString(parsed, 'name'),
        source,
        binary,
        entry,
        pathAliases: readPathAliases(parsed),
        dependency: readOptionalString(parsed, 'dependency'),
        local: readOptionalString(parsed, 'local'),
        projectPath: normalizeFilePath(projectPath),
    };
}

/** 以目录边界而非字符串前缀判断归属，避免 src2 被当作 src 的子目录。 */
export function isPathInsideDirectory(filePath: string, directoryPath: string): boolean {
    const normalizedFilePath = normalizeFilePath(filePath);
    const normalizedDirectoryPath = normalizeFilePath(directoryPath);

    if (normalizedDirectoryPath.length === 0) {
        return false;
    }
    if (normalizedFilePath === normalizedDirectoryPath) {
        return true;
    }

    return normalizedFilePath.startsWith(`${normalizedDirectoryPath}/`);
}

/** 嵌套项目共存时优先选择最长源码根；调用方传入同一文件系统语义下的路径。 */
export function pickBestProjectForFile<T extends ProjectPathMatchCandidate>(
    filePath: string,
    candidates: T[],
): T | undefined {
    const matches = candidates
        .filter((candidate) => isPathInsideDirectory(filePath, candidate.sourceRootPath))
        .sort((left, right) => right.sourceRootPath.length - left.sourceRootPath.length);

    return matches[0];
}

/** 必填字段要求非空；上层以 undefined 拒绝不可定位的清单。 */
function readRequiredString(record: ManifestRecord, key: string): string | undefined {
    const value = readOptionalString(record, key);
    return value && value.length > 0 ? value : undefined;
}

/** 可选字段仅接受字符串，避免把任意 JSON 值传给项目与调试消费者。 */
function readOptionalString(record: ManifestRecord, key: string): string | undefined {
    const value = record[key];
    return typeof value === 'string' ? value.trim() : undefined;
}

/** 仅保留非空的别名前缀；无可用条目时让消费者看到缺省状态。 */
function readPathAliases(record: ManifestRecord): Record<string, string> | undefined {
    const value = record.pathAliases;
    const result: Record<string, string> = {};

    if (!value || typeof value !== 'object' || Array.isArray(value)) {
        return undefined;
    }

    for (const [alias, rawPrefix] of Object.entries(value)) {
        const trimmedAlias = alias.trim();
        const trimmedPrefix = typeof rawPrefix === 'string' ? normalizeFilePath(rawPrefix.trim()) : '';
        if (!trimmedAlias || !trimmedPrefix) {
            continue;
        }

        result[trimmedAlias] = trimmedPrefix;
    }

    return Object.keys(result).length > 0 ? result : undefined;
}
