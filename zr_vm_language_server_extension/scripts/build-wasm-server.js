const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const { createArtifactLayout } = require('./artifact-layout');

// package.json 的 build:wasm 与 VSIX 预发布路径共用此布局，编译目录独立于桌面构建。
const layout = createArtifactLayout({
    repositoryRoot: path.resolve(__dirname, '..', '..'),
    extensionRoot: path.resolve(__dirname, '..'),
});
const repositoryRoot = layout.repositoryRoot;
const buildDir = layout.wasm.buildDir;
const target = process.env.ZR_WASM_BUILD_TARGET || 'zr_vm_language_server_wasm';
const jobs = process.env.ZR_WASM_BUILD_JOBS || process.env.ZR_BUILD_JOBS || '8';
const wasmBuildType = process.env.ZR_WASM_BUILD_TYPE || 'Release';
const wslEmsdkEnvPath = process.env.ZR_WASM_EMSDK_ENV_WSL || '/mnt/e/Git/emsdk/emsdk_env.sh';

// 先保证 Emscripten 专用 CMake cache 可用，再构建扩展需要的单个目标。
ensureWasmBuildDirectory();

let result;
if (process.platform === 'win32') {
    // BUG: 工作区或 EMSDK 路径含单引号时，下方 Bash 命令的引号失配；
    // WSL bash -n 已复现退出 2，正常 build:wasm/package 因而无法配置或构建。
    const repositoryRootWsl = toWslPath(repositoryRoot);
    // BUG: ZR_WASM_BUILD_DIR 指向其他盘符时相对路径经分隔符替换成为 D:/... 而非 /mnt/d/...；
    // package-vsix.ps1 的 -WasmBuildDir 可触发错误的 WSL CMake 构建路径。
    const relativeBuildDirWsl = path.relative(repositoryRoot, buildDir).replace(/\\/g, '/');
    const buildPathInCommand = relativeBuildDirWsl.length > 0 ? relativeBuildDirWsl : '.';
    result = spawnSync('wsl', [
        'bash',
        '-lc',
        [
            sourceWslEmsdkEnvironmentCommand(),
            `cd '${repositoryRootWsl}'`,
            `cmake --build '${buildPathInCommand}' --target '${target}' -j${jobs}`,
        ].join(' && '),
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

// 构建失败必须传回 npm/VSIX 打包流程，不能继续同步可能过期的产物。
if (result.status !== 0) {
    process.exit(result.status ?? 1);
}

// 不复用生成器不符或缺少 BUILD_WASM 配置键的 cache；重配使选定类型生效。
function ensureWasmBuildDirectory() {
    ensureWasmToolchainAvailable();

    const cachePath = path.join(buildDir, 'CMakeCache.txt');
    if (fs.existsSync(cachePath)) {
        const cacheText = fs.readFileSync(cachePath, 'utf8');
        if (!cacheText.includes('CMAKE_GENERATOR:INTERNAL=Ninja') || !cacheText.includes('BUILD_WASM')) {
            // TODO: ZR_WASM_BUILD_DIR 可指向仓外；需确认清除不兼容 cache 时可递归删除整个自定义目录。
            fs.rmSync(buildDir, { recursive: true, force: true });
        }
    }

    fs.mkdirSync(buildDir, { recursive: true });

    if (process.platform === 'win32') {
        const repositoryRootWsl = toWslPath(repositoryRoot);
        const relativeBuildDirWsl = path.relative(repositoryRoot, buildDir).replace(/\\/g, '/');
        const buildPathInCommand = relativeBuildDirWsl.length > 0 ? relativeBuildDirWsl : '.';
        const configureResult = spawnSync('wsl', [
            'bash',
            '-lc',
            [
                sourceWslEmsdkEnvironmentCommand(),
                `cd '${repositoryRootWsl}'`,
                `emcmake cmake -S . -B '${buildPathInCommand}' -G Ninja -DCMAKE_BUILD_TYPE=${wasmBuildType} -DBUILD_TESTS=OFF -DBUILD_WASM=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF`,
            ].join(' && '),
        ], {
            cwd: repositoryRoot,
            stdio: 'inherit',
        });
        if (configureResult.status !== 0) {
            process.exit(configureResult.status ?? 1);
        }
        return;
    }

    const configureResult = spawnSync('emcmake', [
        'cmake',
        '-S',
        repositoryRoot,
        '-B',
        buildDir,
        '-G',
        'Ninja',
        `-DCMAKE_BUILD_TYPE=${wasmBuildType}`,
        '-DBUILD_TESTS=OFF',
        '-DBUILD_WASM=ON',
        '-DBUILD_LANGUAGE_SERVER_EXTENSION=OFF',
    ], {
        cwd: repositoryRoot,
        stdio: 'inherit',
    });
    if (configureResult.status !== 0) {
        process.exit(configureResult.status ?? 1);
    }
}

// 在配置和构建前确认 emcmake 可用，以便报出可操作的缺失工具链原因。
function ensureWasmToolchainAvailable() {
    if (process.platform === 'win32') {
        const result = spawnSync('wsl', [
            'bash',
            '-lc',
            `${sourceWslEmsdkEnvironmentCommand()} && command -v emcmake >/dev/null 2>&1`,
        ], {
            cwd: repositoryRoot,
            stdio: 'ignore',
        });
        if (result.status === 0) {
            return;
        }

        console.error([
            'Unable to build ZR wasm assets because emcmake is not available inside WSL.',
            'Install Emscripten in WSL or provide prebuilt wasm outputs before packaging.',
            `Expected one of: ${layout.wasm.sourceCandidates.join(', ')}`,
        ].join('\n'));
        process.exit(1);
    }

    const result = spawnSync('emcmake', ['--version'], {
        cwd: repositoryRoot,
        stdio: 'ignore',
    });
    if (result.status === 0) {
        return;
    }

    console.error([
        'Unable to build ZR wasm assets because emcmake is not available on PATH.',
        'Install Emscripten or provide prebuilt wasm outputs before packaging.',
        `Expected one of: ${layout.wasm.sourceCandidates.join(', ')}`,
    ].join('\n'));
    process.exit(1);
}

// 仅 Windows→WSL 路径使用：优先沿用已激活环境，必要时加载可配置 emsdk_env。
function sourceWslEmsdkEnvironmentCommand() {
    return `export EMSDK_QUIET=1; if ! command -v emcmake >/dev/null 2>&1 && [ -f '${wslEmsdkEnvPath}' ]; then . '${wslEmsdkEnvPath}' >/dev/null; fi`;
}

// 将 Windows 盘符目录映射成当前构建约定的 WSL /mnt/<drive>/ 路径。
function toWslPath(nativePath) {
    const normalized = path.resolve(nativePath).replace(/\\/g, '/');
    const driveMatch = /^([A-Za-z]):\/(.*)$/.exec(normalized);
    if (!driveMatch) {
        return normalized;
    }

    return `/mnt/${driveMatch[1].toLowerCase()}/${driveMatch[2]}`;
}
