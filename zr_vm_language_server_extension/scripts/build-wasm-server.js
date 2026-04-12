const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const { createArtifactLayout } = require('./artifact-layout');

const layout = createArtifactLayout({
    repositoryRoot: path.resolve(__dirname, '..', '..'),
    extensionRoot: path.resolve(__dirname, '..'),
});
const repositoryRoot = layout.repositoryRoot;
const buildDir = layout.wasm.buildDir;
const target = process.env.ZR_WASM_BUILD_TARGET || 'zr_vm_language_server_wasm';
const jobs = process.env.ZR_WASM_BUILD_JOBS || process.env.ZR_BUILD_JOBS || '8';
const wasmBuildType = process.env.ZR_WASM_BUILD_TYPE || 'Release';

ensureWasmBuildDirectory();

let result;
if (process.platform === 'win32') {
    const repositoryRootWsl = toWslPath(repositoryRoot);
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

function ensureWasmBuildDirectory() {
    ensureWasmToolchainAvailable();

    const cachePath = path.join(buildDir, 'CMakeCache.txt');
    if (fs.existsSync(cachePath)) {
        const cacheText = fs.readFileSync(cachePath, 'utf8');
        if (!cacheText.includes('CMAKE_GENERATOR:INTERNAL=Ninja') || !cacheText.includes('BUILD_WASM')) {
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

function ensureWasmToolchainAvailable() {
    if (process.platform === 'win32') {
        const result = spawnSync('wsl', [
            'bash',
            '-lc',
            'command -v emcmake >/dev/null 2>&1',
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

function toWslPath(nativePath) {
    const normalized = path.resolve(nativePath).replace(/\\/g, '/');
    const driveMatch = /^([A-Za-z]):\/(.*)$/.exec(normalized);
    if (!driveMatch) {
        return normalized;
    }

    return `/mnt/${driveMatch[1].toLowerCase()}/${driveMatch[2]}`;
}
