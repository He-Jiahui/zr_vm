const fs = require('node:fs');
const path = require('node:path');

function executableName(kind) {
    if (kind === 'cli') {
        return process.platform === 'win32' ? 'zr_vm_cli.exe' : 'zr_vm_cli';
    }

    return process.platform === 'win32'
        ? 'zr_vm_language_server_stdio.exe'
        : 'zr_vm_language_server_stdio';
}

function requiredRuntimeFiles() {
    if (process.platform === 'win32') {
        return [
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
    }

    return [
        executableName('languageServer'),
        executableName('cli'),
    ];
}

function dedupePaths(values) {
    const seen = new Set();
    const result = [];

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

function collectBuildCandidateDirs(repositoryRoot) {
    const buildRoot = path.join(repositoryRoot, 'build');
    const seedDirs = [
        path.join(buildRoot, 'codex-lsp', 'bin', 'Debug'),
        path.join(buildRoot, 'codex-lsp', 'bin'),
        path.join(buildRoot, 'codex-msvc-debug', 'bin', 'Debug'),
        path.join(buildRoot, 'codex-msvc-debug', 'bin'),
    ];
    const scannedDirs = [];

    try {
        for (const entry of fs.readdirSync(buildRoot, { withFileTypes: true })) {
            if (!entry.isDirectory()) {
                continue;
            }

            const candidateBinDir = path.join(buildRoot, entry.name, 'bin');
            scannedDirs.push(candidateBinDir);
            scannedDirs.push(path.join(candidateBinDir, 'Debug'));
            scannedDirs.push(path.join(candidateBinDir, 'Release'));
            scannedDirs.push(path.join(candidateBinDir, 'RelWithDebInfo'));
        }
    } catch {
        // Ignore missing build roots and fall back to bundled assets.
    }

    return dedupePaths([...seedDirs, ...scannedDirs]);
}

function pickLatestExistingDirectoryWithFiles(candidates, requiredFiles) {
    let bestCandidate;
    let bestMtime = Number.NEGATIVE_INFINITY;

    for (const candidate of candidates) {
        if (!candidate || !fs.existsSync(candidate)) {
            continue;
        }

        const normalizedCandidate = path.resolve(candidate);
        if (!requiredFiles.every((fileName) => fs.existsSync(path.join(normalizedCandidate, fileName)))) {
            continue;
        }

        let candidateMtime = 0;
        for (const fileName of requiredFiles) {
            try {
                const fileMtime = fs.statSync(path.join(normalizedCandidate, fileName)).mtimeMs;
                if (fileMtime > candidateMtime) {
                    candidateMtime = fileMtime;
                }
            } catch {
                candidateMtime = 0;
            }
        }

        if (!bestCandidate || candidateMtime > bestMtime) {
            bestCandidate = normalizedCandidate;
            bestMtime = candidateMtime;
        }
    }

    return bestCandidate;
}

function resolveLatestCompleteNativeAssetDir(repositoryRoot, extensionRoot) {
    return pickLatestExistingDirectoryWithFiles(
        collectBuildCandidateDirs(repositoryRoot),
        requiredRuntimeFiles(),
    );
}

function resolveLatestRunnableNativeAssetDir(repositoryRoot, extensionRoot) {
    const bundledDir = path.join(extensionRoot, 'server', 'native', `${process.platform}-${process.arch}`);
    const extensionServerDir = path.join(extensionRoot, 'server');

    return pickLatestExistingDirectoryWithFiles(
        [
            ...collectBuildCandidateDirs(repositoryRoot),
            bundledDir,
            extensionServerDir,
        ],
        requiredRuntimeFiles(),
    );
}

module.exports = {
    collectBuildCandidateDirs,
    executableName,
    pickLatestExistingDirectoryWithFiles,
    requiredRuntimeFiles,
    resolveLatestCompleteNativeAssetDir,
    resolveLatestRunnableNativeAssetDir,
};
