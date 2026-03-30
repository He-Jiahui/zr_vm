const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const repositoryRoot = path.resolve(__dirname, '..', '..');
const buildDir = process.env.ZR_WASM_BUILD_DIR
    ? path.resolve(process.env.ZR_WASM_BUILD_DIR)
    : path.join(repositoryRoot, 'build', 'codex-lsp-wasm');
const target = process.env.ZR_WASM_BUILD_TARGET || 'zr_vm_language_server_wasm';
const jobs = process.env.ZR_WASM_BUILD_JOBS || process.env.ZR_BUILD_JOBS || '8';

if (!fs.existsSync(buildDir)) {
    console.error(`WASM build directory does not exist: ${buildDir}`);
    process.exit(1);
}

let result;
if (process.platform === 'win32') {
    const repositoryRootWsl = toWslPath(repositoryRoot);
    const buildDirWsl = toWslPath(path.relative(repositoryRoot, buildDir).length === 0
        ? repositoryRoot
        : buildDir);
    const relativeBuildDirWsl = path.relative(repositoryRoot, buildDir).replace(/\\/g, '/');
    const buildPathInCommand = relativeBuildDirWsl.length > 0 ? relativeBuildDirWsl : '.';
    result = spawnSync('wsl', [
        'bash',
        '-lc',
        `cd '${repositoryRootWsl}' && cmake --build '${buildPathInCommand}' --target '${target}' -j${jobs}`,
    ], {
        cwd: repositoryRoot,
        stdio: 'inherit',
    });
} else {
    result = spawnSync('cmake', [
        '--build',
        buildDir,
        '--target',
        target,
        '-j',
        jobs,
    ], {
        cwd: repositoryRoot,
        stdio: 'inherit',
    });
}

if (result.status !== 0) {
    process.exit(result.status ?? 1);
}

function toWslPath(nativePath) {
    const normalized = path.resolve(nativePath).replace(/\\/g, '/');
    const driveMatch = /^([A-Za-z]):\/(.*)$/.exec(normalized);
    if (!driveMatch) {
        return normalized;
    }

    return `/mnt/${driveMatch[1].toLowerCase()}/${driveMatch[2]}`;
}
