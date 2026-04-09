const fs = require('node:fs');
const path = require('node:path');
const {
    requiredRuntimeFiles,
    resolveLatestCompleteNativeAssetDir,
} = require('./native-asset-selection');

const extensionRoot = path.resolve(__dirname, '..');
const repositoryRoot = path.resolve(extensionRoot, '..');
const bundledFolder = `${process.platform}-${process.arch}`;
const destinationDir = path.join(extensionRoot, 'server', 'native', bundledFolder);

const sourceDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : resolveDefaultSourceDir();
const retryableCopyErrorCodes = new Set(['EBUSY', 'EPERM']);
const copyRetryDelaysMs = [150, 300, 600, 1200, 2000];

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

for (const fileName of [...requiredFiles, ...optionalFiles]) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        continue;
    }

    const destinationFile = path.join(destinationDir, fileName);
    copyFileWithRetry(sourceFile, destinationFile, fileName);
    console.log(`Copied ${fileName} -> ${path.relative(extensionRoot, destinationFile)}`);
}

function resolveDefaultSourceDir() {
    return resolveLatestCompleteNativeAssetDir(repositoryRoot, extensionRoot) ||
        path.join(repositoryRoot, 'build', 'codex-lsp', 'bin');
}

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

function sleep(durationMs) {
    const signal = new Int32Array(new SharedArrayBuffer(4));
    Atomics.wait(signal, 0, 0, durationMs);
}
