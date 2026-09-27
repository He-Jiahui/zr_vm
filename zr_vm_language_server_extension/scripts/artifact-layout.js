const fs = require('node:fs');
const path = require('node:path');
const assetLayout = require('../asset-layout.json');

/** 将共享清单中的路径模板限制为相对片段，供构建和安装目录复用。 */
function templatePathSegments(template, replacements) {
    const rendered = Object.entries(replacements).reduce(
        (value, [key, replacement]) => value.replace(new RegExp(`\\{${key}\\}`, 'g'), replacement),
        template,
    );
    return rendered
        .split(/[\\/]+/)
        .filter((segment) => segment.length > 0 && segment !== '.');
}

/** 打包目录与源目录均使用同一模板语义；空模板代表当前基目录。 */
function resolveRelativePathTemplate(template, replacements = {}) {
    const segments = templatePathSegments(template, replacements);
    if (segments.length === 0) {
        return '.';
    }

    return path.join(...segments);
}

/** 把清单里的候选子目录锚定到指定构建根，保留声明顺序。 */
function resolveSubdirTemplates(baseDir, templates, replacements = {}) {
    return templates.map((template) => {
        const relativePath = resolveRelativePathTemplate(template, replacements);
        return relativePath === '.'
            ? path.resolve(baseDir)
            : path.resolve(baseDir, relativePath);
    });
}

/** 合并显式和扫描候选时去重，但不改变前者优先级。 */
function dedupeAbsolutePaths(values) {
    const seen = new Set();
    const result = [];

    for (const value of values) {
        if (!value) {
            continue;
        }

        const normalized = path.resolve(value);
        if (seen.has(normalized)) {
            continue;
        }

        seen.add(normalized);
        result.push(normalized);
    }

    return result;
}

/** 按目标平台读取打包入口名，使运行时与构建脚本不用各自猜测后缀。 */
function nativeExecutableName(kind, platform = process.platform) {
    const entry = assetLayout.native.executables[kind];
    if (!entry) {
        throw new Error(`Unsupported native executable kind: ${kind}`);
    }

    return entry[platform] ?? entry.default;
}

/** 返回完整原生目录所需的文件清单，供同步和开发资产筛选共用。
 * TODO: 清单无有效平台数组时这里返回空数组；核查调用方是否应拒绝空要求，
 * 以免把任意现存目录视为完整资产。
 */
function nativeRequiredRuntimeFiles(platform = process.platform) {
    const files = assetLayout.native.requiredRuntimeFiles[platform] ?? assetLayout.native.requiredRuntimeFiles.default;
    return Array.isArray(files) ? [...files] : [];
}

/** 首先纳入调用者指定的构建目录，再扫描仓库 build 下其他配置产物。
 * 缺失的扫描根不妨碍已知构建目录被后续完整性检查使用。
 */
function collectNativeBuildCandidateDirs(buildRoot, nativeBuildDir, nativeBuildConfig) {
    const candidates = [
        ...resolveSubdirTemplates(nativeBuildDir, assetLayout.native.buildSubdirs, {
            config: nativeBuildConfig,
        }),
    ];

    try {
        for (const entry of fs.readdirSync(buildRoot, { withFileTypes: true })) {
            if (!entry.isDirectory()) {
                continue;
            }

            candidates.push(
                ...resolveSubdirTemplates(
                    path.join(buildRoot, entry.name),
                    assetLayout.native.scannedBuildSubdirs,
                ),
            );
        }
    } catch {
        // 扫描失败仍保留显式候选和已收集候选，交由完整性检查决定能否使用。
    }

    return dedupeAbsolutePaths(candidates);
}

/** 发布构建、同步脚本与测试共享的资产布局快照。
 * @note options 可覆盖仓库根、构建根、配置和目标平台；此函数不要求产物已存在。
 */
function createArtifactLayout(options = {}) {
    const repositoryRoot = path.resolve(options.repositoryRoot ?? path.join(__dirname, '..', '..'));
    const extensionRoot = path.resolve(options.extensionRoot ?? path.join(repositoryRoot, 'zr_vm_language_server_extension'));
    const buildRoot = path.resolve(options.buildRoot ?? path.join(repositoryRoot, 'build'));
    const nativeBuildConfig = options.nativeBuildConfig ?? process.env.ZR_NATIVE_BUILD_CONFIG ?? 'Debug';
    const nativeBuildDir = path.resolve(options.nativeBuildDir ?? process.env.ZR_NATIVE_BUILD_DIR ?? path.join(repositoryRoot, 'build', 'codex-lsp'));
    const wasmBuildDir = path.resolve(options.wasmBuildDir ?? process.env.ZR_WASM_BUILD_DIR ?? path.join(repositoryRoot, 'build', 'codex-lsp-wasm'));
    const platform = options.platform ?? process.platform;
    const arch = options.arch ?? process.arch;
    const nativeBundledRelativeDir = resolveRelativePathTemplate(assetLayout.native.bundledRelativeDir, {
        platform,
        arch,
    });
    const wasmBundledRelativeDir = resolveRelativePathTemplate(assetLayout.wasm.bundledRelativeDir);
    const nativeBuildOutputCandidates = resolveSubdirTemplates(nativeBuildDir, assetLayout.native.buildSubdirs, {
        config: nativeBuildConfig,
    });
    const wasmSourceCandidates = [
        ...resolveSubdirTemplates(wasmBuildDir, assetLayout.wasm.buildSubdirs),
        ...assetLayout.wasm.fallbackBuildDirectories.flatMap((relativeBuildDirectory) =>
            resolveSubdirTemplates(
                path.join(repositoryRoot, ...relativeBuildDirectory.split(/[\\/]+/)),
                assetLayout.wasm.buildSubdirs,
            )),
    ];

    // 同一仓库与扩展根下的 native/wasm 布局供构建、同步及测试跨阶段传递。
    return {
        repositoryRoot,
        extensionRoot,
        buildRoot,
        nativeBuildConfig,
        native: {
            bundledRelativeDir: nativeBundledRelativeDir,
            bundledDir: path.join(extensionRoot, nativeBundledRelativeDir),
            buildDir: nativeBuildDir,
            buildOutputCandidates: dedupeAbsolutePaths(nativeBuildOutputCandidates),
            developmentAssetCandidates: collectNativeBuildCandidateDirs(buildRoot, nativeBuildDir, nativeBuildConfig),
            executableNames: {
                cli: nativeExecutableName('cli', platform),
                languageServer: nativeExecutableName('languageServer', platform),
            },
            requiredRuntimeFiles: nativeRequiredRuntimeFiles(platform),
        },
        wasm: {
            bundledRelativeDir: wasmBundledRelativeDir,
            bundledDir: path.join(extensionRoot, wasmBundledRelativeDir),
            buildDir: wasmBuildDir,
            sourceCandidates: dedupeAbsolutePaths(wasmSourceCandidates),
            requiredFiles: [...assetLayout.wasm.requiredFiles],
        },
    };
}

module.exports = {
    collectNativeBuildCandidateDirs,
    createArtifactLayout,
    dedupeAbsolutePaths,
    nativeExecutableName,
    nativeRequiredRuntimeFiles,
    resolveRelativePathTemplate,
    resolveSubdirTemplates,
};
