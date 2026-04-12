const fs = require('node:fs');
const path = require('node:path');
const {
    collectNativeBuildCandidateDirs,
    createArtifactLayout,
    nativeExecutableName,
    nativeRequiredRuntimeFiles,
} = require('./artifact-layout');

function executableName(kind) {
    return nativeExecutableName(kind);
}

function requiredRuntimeFiles() {
    return nativeRequiredRuntimeFiles();
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
    const layout = createArtifactLayout({ repositoryRoot });
    return dedupePaths(
        collectNativeBuildCandidateDirs(
            layout.buildRoot,
            layout.native.buildDir,
            layout.nativeBuildConfig,
        ),
    );
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
    const layout = createArtifactLayout({ repositoryRoot, extensionRoot });
    return pickLatestExistingDirectoryWithFiles(
        collectNativeBuildCandidateDirs(
            layout.buildRoot,
            layout.native.buildDir,
            layout.nativeBuildConfig,
        ),
        layout.native.requiredRuntimeFiles,
    );
}

function resolveLatestRunnableNativeAssetDir(repositoryRoot, extensionRoot) {
    const layout = createArtifactLayout({ repositoryRoot, extensionRoot });

    return pickLatestExistingDirectoryWithFiles(
        [
            ...collectNativeBuildCandidateDirs(
                layout.buildRoot,
                layout.native.buildDir,
                layout.nativeBuildConfig,
            ),
            layout.native.bundledDir,
        ],
        layout.native.requiredRuntimeFiles,
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
