const fs = require('node:fs');
const path = require('node:path');
const {
    collectNativeBuildCandidateDirs,
    createArtifactLayout,
    nativeExecutableName,
    nativeRequiredRuntimeFiles,
} = require('./artifact-layout');

/** 桌面 smoke 测试使用与打包清单相同的服务端入口名。 */
function executableName(kind) {
    return nativeExecutableName(kind);
}

/** Windows 同步脚本读取共享清单中的两入口与必需 DLL，作为复制前的目录要求。 */
function requiredRuntimeFiles() {
    return nativeRequiredRuntimeFiles();
}

/** 保持显式构建候选在扫描结果前的顺序。 */
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

/** 对外提供完整开发构建候选列表。
 * TODO: 仓内目前未发现直接调用；核查是否仍有外部脚本消费此导出。
 */
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

/** 同步与 smoke 调用方按必需文件完整性筛选目录，再按更新时间挑选产物。
 * @note 候选须是同一平台的目录；时间相同保持候选顺序。
 */
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

/** 同步脚本只从完整开发构建取默认源，避免把不同构建的依赖拼在一起。 */
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

/** Electron smoke 选最近更新的完整资产，可在开发构建与已有捆绑目录间择新。
 * 打包运行时另有捆绑优先策略；这里服务于测试当前构建。
 */
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
