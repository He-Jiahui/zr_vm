/**
 * ## Summary
 * 表示扩展从 `.zrp` 文本筛出的项目字段，供工作区发现、结构视图和调试配置共享；它把清单声明与宿主解析出的路径放在同一模型中。
 *
 * ## Fields
 * - `name` 是可选展示名；缺省时由工作区层从清单文件名生成标签。
 * - `source`、`binary`、`entry` 是必填的非空清单字符串，分别用于源码归属和调试启动配置。
 * - `pathAliases` 是可选的非空别名到路径前缀映射；本类型不编码 schema 中的别名前缀约定。
 * - `dependency`、`local` 是仅在值为字符串时保留的可选扩展字段。
 * - `projectPath` 是调用方提供的清单路径经分隔符归一化后的文本，不是已验证存在的文件句柄。
 *
 * ## Invariants and lifecycle
 * `parseProjectManifestText` 只从 JSON 中筛选字段并构造普通对象；该接口本身不验证文件存在、目录布局、路径可达性或二进制内容。字段读取器仅修剪 `source`、`binary`、`entry` 等字符串的首尾空白；`projectPath` 和别名前缀另经路径归一化处理。
 *
 * ## References
 * `parseProjectManifestText` 创建此对象；`WorkspaceProject.manifest` 持有它；结构视图和 `ZrDebugAdapter.createLaunchSourceContext` 分别消费其中的项目字段。
 */
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

/**
 * ## Summary
 * 为文件到项目的纯路径匹配定义最小候选形状，使 `projectSupport.ts` 无需依赖 VS Code 类型。
 *
 * ## Fields
 * - `id` 是调用方用于识别候选的稳定标识；匹配函数本身只把候选原样返回，不解析该标识。
 * - `sourceRootPath` 是参与目录边界匹配和长度排序的源码根文本。
 *
 * ## Invariants and lifecycle
 * 文件路径和源码根必须采用同一文件系统的比较语义。匹配器会统一分隔符，但不会折叠大小写、解析符号链接或验证磁盘布局；候选数组仅在同步调用期间读取。
 *
 * ## References
 * `pickBestProjectForFile` 消费此形状；`WorkspaceProject` 提供其 `id` 与 `sourceRootPath`。
 */
export interface ProjectPathMatchCandidate {
    id: string;
    sourceRootPath: string;
}

/**
 * ## Summary
 * 为 JSON 清单读取器提供“键到未知值”的静态视图，避免把未筛选值直接赋给项目字段。
 *
 * ## Value model
 * `Record<string, unknown>` 只约束 TypeScript 使用方如何访问字段，不会在运行时验证 `JSON.parse` 的结果确为普通对象；数组、`null` 和其他合法 JSON 值仍可能越过类型断言。
 *
 * ## Constraints and lifecycle
 * 只有 `readOptionalString`、`readRequiredString` 和 `readPathAliases` 的筛选结果才可写入已解析模型。`null` 未被对象形状检查拦截，是 `parseProjectManifestText` 当前记录的缺陷。
 *
 * ## References
 * `parseProjectManifestText` 在 JSON 边界取得该记录；三个读取器负责字段级筛选。
 */
type ManifestRecord = Record<string, unknown>;

/**
 * ## Summary
 * 将路径文本中的斜线与反斜线统一成 `/`，供清单路径组合、项目归属和别名展示共用；该 helper 不承担宿主文件系统解析。
 *
 * ## Parameters
 * @param value 输入路径文本；调用方负责提供字符串，函数不会检查盘符、URI scheme 或路径存在性。
 *
 * ## Returns
 * @returns 分隔符统一且尾部斜线已去除的路径文本；输入 `/` 当前会变为空串，见本函数内的 `BUG` 说明。
 *
 * ## Constraints and lifecycle
 * 仅做正则文本替换，不解析 `.` / `..`、不折叠盘符大小写，也不保留根路径尾斜线。返回值是新字符串，不持有输入资源。
 *
 * ## References
 * `joinNormalizedPath`、`isPathInsideDirectory`、`WorkspaceProject` 创建流程及 `lastPathSegment` 复用此文本规则。
 */
export function normalizeFilePath(value: string): string {
    // BUG: 根路径 / 会被删成空串；source 为 / 时 joinNormalizedPath 错把项目目录
    // 当作源码根，isPathInsideDirectory 也无法识别根目录下的源码文件。
    return value.replace(/[\\/]+/g, '/').replace(/\/+$/g, '');
}

/**
 * ## Summary
 * 从完整清单文件路径的文本形式推导相对 `source` 的基目录，由工作区项目构造器在解析清单后调用。
 *
 * ## Parameters
 * @param value 完整清单路径；调用方通常传 `Uri.fsPath || Uri.path`，本函数不判断该 URI 是否指向文件。
 *
 * ## Returns
 * @returns 按最后一个 `/` 切出的前缀；若分隔符不存在或位于索引 0，当前实现返回归一化后的整串，因此根目录清单的行为受本函数内 `BUG` 影响。
 *
 * ## Constraints and lifecycle
 * 先调用 `normalizeFilePath`，再执行字符串切分；不会访问文件系统，也不处理盘符/UNC 语义。输入和返回值都只是路径文本。
 *
 * ## References
 * `createWorkspaceProject` 用返回值作为 `joinNormalizedPath` 的基路径；`pickWorkspaceProjectForUri` 及项目选择流程消费其产生的源码根。
 */
export function dirnameFromPath(value: string): string {
    const normalized = normalizeFilePath(value);
    const lastSlash = normalized.lastIndexOf('/');
    // BUG: 根目录下的 /demo.zrp 使 lastSlash 为 0，此处分支返回文件路径本身；
    // createWorkspaceProject 随后把 source 拼成 /demo.zrp/src，活动源码无法匹配该项目。
    return lastSlash > 0 ? normalized.slice(0, lastSlash) : normalized;
}

/**
 * ## Summary
 * 将项目目录和清单 `source` 合成为用于源码归属比较的路径文本，并折叠相对段；工作区层用它建立 `WorkspaceProject.sourceRootPath`。
 *
 * ## Parameters
 * @param basePath 清单所在目录的路径文本，通常来自 `dirnameFromPath`。
 * @param relativePath 清单中的 `source` 字符串。归一化后仍非空且以 `/` 开头的路径可走绝对路径分支；Windows 盘符绝对路径是否受 schema 支持仍是待核事项。
 *
 * ## Returns
 * @returns 折叠空段、`.` 和可处理的 `..` 后得到的比较路径；结果不代表路径存在或已由操作系统解析。
 *
 * ## Constraints and lifecycle
 * 参数只统一分隔符，不执行 `path.resolve`。当前 `..` 折叠会把盘符段也弹出，Windows 盘符根溢出已有 `BUG`；盘符绝对 `source` 的期望契约仍由本函数内 `TODO` 跟进。该函数不读写输入或磁盘状态。
 *
 * ## References
 * `createWorkspaceProject` 将此结果存入 `WorkspaceProject.sourceRootPath`；`pickBestProjectForFile` 后续以目录边界作纯文本归属判断；`ZrDebugAdapter.createLaunchSourceContext` 使用独立的宿主路径解析。
 */
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

/**
 * ## Summary
 * 解析 `.zrp` JSON 文本并提取工作区发现、结构视图和调试所需字段；调用方用 `undefined` 跳过语法错误或缺少必填字符串的清单。
 *
 * ## Parameters
 * @param text 清单的原始 JSON 文本；文件读取和 UTF-8 解码由工作区层负责。
 * @param projectPath 清单的路径文本，调用方提供 `Uri.fsPath || Uri.path`；返回对象会对其统一分隔符。
 *
 * ## Returns
 * @returns 对象顶层的 `source`、`binary`、`entry` 都是修剪后非空字符串时返回新 `ParsedProjectManifest`；JSON 语法错误或必填项缺失时返回 `undefined`。合法 JSON `null` 当前会在字段读取时抛出 `TypeError`，而非返回 `undefined`，见下方 `BUG`。
 *
 * ## Constraints and lifecycle
 * 只做字段类型筛选，不执行完整 schema 校验，不验证路径、别名语法或文件存在性；可选字符串修剪后为空时仍作为空串保留，别名映射则丢弃空条目。输入文本和返回对象均由调用方持有，本函数不保存它们。
 *
 * ## References
 * `createWorkspaceProject` 用当前编辑器文本或工作区字节解码结果调用此函数；`ZrDebugAdapter.createLaunchSourceContext` 也独立解析清单字段。
 */
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

/**
 * ## Summary
 * 判断文件路径是否等于目录，或位于目录分隔符边界以下，供嵌套项目归属筛选使用。
 *
 * ## Parameters
 * @param filePath 待归属的路径文本。
 * @param directoryPath 候选源码根目录文本；归一化后为空串时不匹配任何文件。
 *
 * ## Returns
 * @returns 两个归一化字符串相等，或文件以 `directory/` 为前缀时返回 `true`；单纯字符串前缀如 `src2` 不匹配 `src`。
 *
 * ## Constraints and lifecycle
 * 只比较分隔符归一化后的文本，不折叠大小写、解析 `..`、校验磁盘边界或跟随符号链接。根斜线会被 `normalizeFilePath` 去掉，相关空根缺陷见该 helper 的 `BUG`。
 *
 * ## References
 * `pickBestProjectForFile` 对每个候选调用此谓词；`pickWorkspaceProjectForUri` 将 VS Code URI 路径转换后交给匹配器。
 */
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

/**
 * ## Summary
 * 在多个候选源码根同时包含文件时，选出原始 `sourceRootPath` 字符串最长的候选，使嵌套项目优先；工作区 URI 选择和清单归属共用此策略。
 *
 * ## Parameters
 * @param filePath 要归属的路径文本，须与候选根采用同一文件系统比较语义。
 * @param candidates 候选数组；本函数仅读取并筛选它，排序发生在新建的匹配数组中，不重排调用方数组。
 *
 * ## Returns
 * @returns 匹配候选按根路径原始字符串长度降序排序后的首项；没有边界匹配时为 `undefined`。返回候选仍由调用方管理。
 *
 * ## Constraints and lifecycle
 * 目录包含关系由 `isPathInsideDirectory` 的斜线边界文本比较决定，排序长度则直接使用候选原字符串，不进行 canonical path、大小写或文件系统身份比较。相同长度的候选没有额外优先级合同。
 *
 * ## References
 * `pickWorkspaceProjectForUri` 提供工作区项目候选；`.zrp` 清单和活动源码的归属策略由 `resolveSelectedProjectUri` 调用链使用。
 */
export function pickBestProjectForFile<T extends ProjectPathMatchCandidate>(
    filePath: string,
    candidates: T[],
): T | undefined {
    const matches = candidates
        .filter((candidate) => isPathInsideDirectory(filePath, candidate.sourceRootPath))
        .sort((left, right) => right.sourceRootPath.length - left.sourceRootPath.length);

    return matches[0];
}

/**
 * ## Summary
 * 从未验证清单记录读取必填字符串，并把空值转换成 `undefined`，让解析器统一拒绝无法定位的项目。
 *
 * ## Parameters
 * @param record `JSON.parse` 的静态记录视图；调用前必须可按键索引，类型别名不会做运行时对象校验。
 * @param key 清单字段名，例如 `source`、`binary` 或 `entry`。
 *
 * ## Returns
 * @returns 对应值为字符串且修剪后非空时返回修剪字符串；否则返回 `undefined`。
 *
 * ## Constraints and lifecycle
 * 委托 `readOptionalString` 读取字段；`null` 记录仍会在属性访问处抛错。只在同步调用期间读取记录，不保留引用。
 *
 * ## References
 * `parseProjectManifestText` 对三个必填定位字段调用此 helper；`readOptionalString` 实施类型和修剪规则。
 */
function readRequiredString(record: ManifestRecord, key: string): string | undefined {
    const value = readOptionalString(record, key);
    return value && value.length > 0 ? value : undefined;
}

/**
 * ## Summary
 * 从清单记录中取可选字符串字段，阻止数字、数组和对象等任意 JSON 值进入项目或调试模型。
 *
 * ## Parameters
 * @param record 可按键索引的未验证清单记录；运行时 `null` 不满足此调用前提。
 * @param key 要读取的属性名。
 *
 * ## Returns
 * @returns 属性值为字符串时返回去掉首尾空白后的新字符串（可能为空串）；非字符串时返回 `undefined`。
 *
 * ## Constraints and lifecycle
 * 不校验记录整体形状，不区分属性缺失与显式非字符串；返回的是不可变字符串值，本函数不保留记录。
 *
 * ## References
 * `readRequiredString` 在此基础上增加非空门槛；`parseProjectManifestText` 用它读取可选展示及扩展字段。
 */
function readOptionalString(record: ManifestRecord, key: string): string | undefined {
    const value = record[key];
    return typeof value === 'string' ? value.trim() : undefined;
}

/**
 * ## Summary
 * 从 `pathAliases` 对象生成仅含可用字符串键值的普通映射，供项目结构和调试路径消费者读取。
 *
 * ## Parameters
 * @param record 可按键索引的清单记录；`pathAliases` 缺失、为数组或不是非空对象时返回缺省结果。
 *
 * ## Returns
 * @returns 保留非空修剪别名及非空字符串前缀的映射；没有可用条目时返回 `undefined`。
 *
 * ## Constraints and lifecycle
 * 别名名只修剪空白，前缀先修剪再调用 `normalizeFilePath`；此 helper 不检查 schema 所述 `@` 前缀规则，也不验证目标目录。结果对象新建并由调用方持有，不引用原 JSON 子对象。
 *
 * ## References
 * `parseProjectManifestText` 将结果写入 `ParsedProjectManifest.pathAliases`；结构视图和调试入口消费该可选映射。
 */
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
