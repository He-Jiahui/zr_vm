const fs = require('node:fs');
const path = require('node:path');
const { createArtifactLayout } = require('./artifact-layout');
const {
    requiredRuntimeFiles,
    resolveLatestCompleteNativeAssetDir,
} = require('./native-asset-selection');

// vscode:prepublish 构建完成后进入此脚本；目标目录与运行时读取的布局一致。
const layout = createArtifactLayout({
    repositoryRoot: path.resolve(__dirname, '..', '..'),
    extensionRoot: path.resolve(__dirname, '..'),
});
const extensionRoot = layout.extensionRoot;
const repositoryRoot = layout.repositoryRoot;
const destinationDir = layout.native.bundledDir;

// Electron smoke 可显式指定源目录；发布流程默认选最近的完整开发构建。
const sourceDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : resolveDefaultSourceDir();
// Windows 下已加载的 DLL 可能短暂锁定；重试间隔只作用于可恢复复制错误。
const retryableCopyErrorCodes = new Set(['EBUSY', 'EPERM']);
const copyRetryDelaysMs = [150, 300, 600, 1200, 2000];

// 源目录缺少必需文件时先终止，避免因此发生部分写入。
if (!fs.existsSync(sourceDir)) {
    console.error(`Native server source directory does not exist: ${sourceDir}`);
    process.exit(1);
}

const { requiredFiles, optionalFiles } = process.platform === 'win32'
    ? collectWindowsNativeAssets(sourceDir)
    : collectUnixNativeAssets(sourceDir);

for (const fileName of requiredFiles) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        console.error(`Missing required native asset: ${sourceFile}`);
        process.exit(1);
    }
}

fs.mkdirSync(destinationDir, { recursive: true });

// TODO: 同步只覆盖源目录当前存在的文件，不清理旧捆绑目录中的额外 DLL/so；
// 核查连续打包不同构建时是否会留下可被加载的过期依赖。
for (const fileName of [...requiredFiles, ...optionalFiles]) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        continue;
    }

    const destinationFile = path.join(destinationDir, fileName);
    copyFileWithRetry(sourceFile, destinationFile, fileName);
    console.log(`Copied ${fileName} -> ${path.relative(extensionRoot, destinationFile)}`);
}

/** 默认从完整开发构建复制；若无完整目录，仍返回已知构建输出以报告缺件。 */
function resolveDefaultSourceDir() {
    return resolveLatestCompleteNativeAssetDir(repositoryRoot, extensionRoot) ||
        layout.native.buildOutputCandidates.find((candidate) => fs.existsSync(candidate)) ||
        layout.native.buildOutputCandidates[0];
}

/** Windows 同步两入口、平台必需 DLL 以及同目录的其他模块，PDB 仅作可选辅助。 */
function collectWindowsNativeAssets(nativeSourceDir) {
    const entries = fs.readdirSync(nativeSourceDir, { withFileTypes: true });
    const dllFiles = entries
        .filter((entry) => entry.isFile() && (/^zr_vm_.*\.dll$/i.test(entry.name) || /^(uv|libuv)\.dll$/i.test(entry.name)))
        .map((entry) => entry.name)
        .sort();
    const pdbFiles = [
        'zr_vm_language_server_stdio.pdb',
        'zr_vm_cli.pdb',
        ...dllFiles.map((fileName) => fileName.replace(/\.dll$/i, '.pdb')),
    ]
        .filter((fileName, index, allFileNames) => allFileNames.indexOf(fileName) === index)
        .filter((fileName) => entries.some((entry) => entry.isFile() && entry.name === fileName));

    return {
        requiredFiles: [...new Set([...requiredRuntimeFiles(), ...dllFiles])],
        optionalFiles: pdbFiles,
    };
}

/** Unix 同步两入口并尝试携带同目录动态库。
 * BUG: 默认 shared 构建将 libzr_vm_*.so 放在 build/lib，而默认源是 build/bin；
 * 此处仅扫描源目录且过滤器不接受 libzr_vm_ 前缀，发布包缺少首方共享库。
 */
function collectUnixNativeAssets(nativeSourceDir) {
    const entries = fs.readdirSync(nativeSourceDir, { withFileTypes: true });
    const optionalRuntimeFiles = entries
        .filter((entry) =>
            entry.isFile() &&
            (/^zr_vm_.*\.(so(\..*)?|dylib)$/i.test(entry.name) || /^libuv(\..*)?\.(so|dylib)$/i.test(entry.name)))
        .map((entry) => entry.name)
        .sort();

    return {
        requiredFiles: ['zr_vm_language_server_stdio', 'zr_vm_cli'],
        optionalFiles: optionalRuntimeFiles,
    };
}

/** 锁定目标仅在内容已相同的情况下视为成功；变更中的文件重试后仍失败即上报。 */
function copyFileWithRetry(sourceFile, destinationFile, fileName) {
    for (let attempt = 0; attempt <= copyRetryDelaysMs.length; attempt += 1) {
        try {
            fs.copyFileSync(sourceFile, destinationFile);
            return;
        } catch (error) {
            const errorCode = error && typeof error === 'object' ? error.code : undefined;
            if (!retryableCopyErrorCodes.has(errorCode)) {
                throw error;
            }

            if (filesAreIdentical(sourceFile, destinationFile)) {
                console.warn(`Skipped locked unchanged asset: ${fileName}`);
                return;
            }

            if (attempt >= copyRetryDelaysMs.length) {
                throw error;
            }

            sleep(copyRetryDelaysMs[attempt]);
        }
    }
}

/** 在跳过被锁资产前比较完整字节，避免旧版本被误认为已同步。 */
function filesAreIdentical(sourceFile, destinationFile) {
    try {
        if (!fs.existsSync(destinationFile)) {
            return false;
        }

        const sourceStat = fs.statSync(sourceFile);
        const destinationStat = fs.statSync(destinationFile);
        if (sourceStat.size !== destinationStat.size) {
            return false;
        }

        const sourceContent = fs.readFileSync(sourceFile);
        const destinationContent = fs.readFileSync(destinationFile);
        return sourceContent.equals(destinationContent);
    } catch {
        return false;
    }
}

/** 同步脚本的短暂阻塞退避，仅由锁定复制重试调用。 */
function sleep(durationMs) {
    const signal = new Int32Array(new SharedArrayBuffer(4));
    Atomics.wait(signal, 0, 0, durationMs);
}
