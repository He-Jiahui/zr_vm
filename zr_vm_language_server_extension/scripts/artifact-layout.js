const fs = require('node:fs');
const path = require('node:path');
const assetLayout = require('../asset-layout.json');

function templatePathSegments(template, replacements) {
    const rendered = Object.entries(replacements).reduce(
        (value, [key, replacement]) => value.replace(new RegExp(`\\{${key}\\}`, 'g'), replacement),
        template,
    );
    return rendered
        .split(/[\\/]+/)
        .filter((segment) => segment.length > 0 && segment !== '.');
}

function resolveRelativePathTemplate(template, replacements = {}) {
    const segments = templatePathSegments(template, replacements);
    if (segments.length === 0) {
        return '.';
    }

    return path.join(...segments);
}

function resolveSubdirTemplates(baseDir, templates, replacements = {}) {
    return templates.map((template) => {
        const relativePath = resolveRelativePathTemplate(template, replacements);
        return relativePath === '.'
            ? path.resolve(baseDir)
            : path.resolve(baseDir, relativePath);
    });
}

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

function nativeExecutableName(kind, platform = process.platform) {
    const entry = assetLayout.native.executables[kind];
    if (!entry) {
        throw new Error(`Unsupported native executable kind: ${kind}`);
    }

    return entry[platform] ?? entry.default;
}

function nativeRequiredRuntimeFiles(platform = process.platform) {
    const files = assetLayout.native.requiredRuntimeFiles[platform] ?? assetLayout.native.requiredRuntimeFiles.default;
    return Array.isArray(files) ? [...files] : [];
}

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
        // Ignore missing build roots and fall back to bundled assets.
    }

    return dedupeAbsolutePaths(candidates);
}

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
