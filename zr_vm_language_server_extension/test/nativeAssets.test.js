const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const {
    configuredPathCandidates,
    resolveConfiguredPath,
    resolvePreferredCliSetting,
} = require('../out/executablePath.js');
const {
    pickFirstExistingDirectoryWithFiles,
    pickFirstExistingPath,
    pickLatestExistingPath,
    pickLatestExistingDirectoryWithFiles,
} = require('../out/nativePathSelection.js');
const {
    createArtifactLayout,
} = require('../scripts/artifact-layout.js');

// 公共运行设置与旧调试设置并存时，运行和调试必须选择同一首选 CLI。
test('resolvePreferredCliSetting prefers zr.executablePath over the legacy debug setting', () => {
    const resolved = resolvePreferredCliSetting({
        executablePath: 'D:/tools/new-zr_vm_cli.exe',
        legacyDebugCliPath: 'D:/tools/old-zr_vm_cli.exe',
    });

    assert.equal(resolved, 'D:/tools/new-zr_vm_cli.exe');
});

// 迁移期旧设置仍可用，空白新设置不应遮蔽它。
test('resolvePreferredCliSetting falls back to zr.debug.cli.path when zr.executablePath is empty', () => {
    const resolved = resolvePreferredCliSetting({
        executablePath: '   ',
        legacyDebugCliPath: 'D:/tools/old-zr_vm_cli.exe',
    });

    assert.equal(resolved, 'D:/tools/old-zr_vm_cli.exe');
});

// 多根工作区先于扩展安装目录解析相对设置，避免同名工具意外落入捆绑目录。
test('configuredPathCandidates resolves relative executable settings against workspace folders before the extension install', () => {
    const workspaceFolderPaths = [
        path.join('E:', 'Git', 'workspace-a'),
        path.join('E:', 'Git', 'workspace-b'),
    ];
    const extensionPath = path.join('E:', 'Git', 'zr_vm', 'zr_vm_language_server_extension');

    const candidates = configuredPathCandidates({
        configuredPath: path.join('tools', 'zr_vm_cli.exe'),
        workspaceFolderPaths,
        extensionPath,
    });

    assert.deepEqual(candidates, [
        path.resolve(workspaceFolderPaths[0], 'tools', 'zr_vm_cli.exe'),
        path.resolve(workspaceFolderPaths[1], 'tools', 'zr_vm_cli.exe'),
        path.resolve(extensionPath, 'tools', 'zr_vm_cli.exe'),
    ]);
});

// 真实文件系统验证优先级，保证相对配置最终选到工作区里的可执行文件。
test('resolveConfiguredPath prefers a workspace-relative executable over the extension-relative fallback', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-configured-path-'));

    try {
        const workspaceRoot = path.join(tempRoot, 'workspace');
        const extensionRoot = path.join(tempRoot, 'extension');
        const relativeExecutable = path.join('tools', 'zr_vm_cli.exe');
        const workspaceExecutable = path.join(workspaceRoot, relativeExecutable);
        const extensionExecutable = path.join(extensionRoot, relativeExecutable);

        fs.mkdirSync(path.dirname(workspaceExecutable), { recursive: true });
        fs.mkdirSync(path.dirname(extensionExecutable), { recursive: true });
        fs.writeFileSync(workspaceExecutable, 'workspace');
        fs.writeFileSync(extensionExecutable, 'extension');

        const resolved = resolveConfiguredPath({
            configuredPath: relativeExecutable,
            workspaceFolderPaths: [workspaceRoot],
            extensionPath: extensionRoot,
        });

        assert.equal(resolved, workspaceExecutable);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});

// 没有完整运行时可用时，单文件开发构建回退按文件时间选最新候选。
test('pickLatestExistingPath prefers the newest existing native asset candidate', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-native-assets-'));

    try {
        const olderPath = path.join(tempRoot, 'older.exe');
        const newerPath = path.join(tempRoot, 'newer.exe');
        fs.writeFileSync(olderPath, 'older');
        fs.writeFileSync(newerPath, 'newer');
        fs.utimesSync(olderPath, new Date('2025-01-01T00:00:00Z'), new Date('2025-01-01T00:00:00Z'));
        fs.utimesSync(newerPath, new Date('2026-01-01T00:00:00Z'), new Date('2026-01-01T00:00:00Z'));

        const resolved = pickLatestExistingPath([
            path.join(tempRoot, 'missing.exe'),
            olderPath,
            newerPath,
        ]);

        assert.equal(resolved, newerPath);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});

// 新构建若缺少依赖文件不能顶替完整目录，避免在启动时才遇到缺件。
test('pickLatestExistingDirectoryWithFiles ignores newer incomplete native asset directories', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-native-asset-dir-'));

    try {
        const olderCompleteDir = path.join(tempRoot, 'older-complete');
        const newerIncompleteDir = path.join(tempRoot, 'newer-incomplete');
        const newerCompleteDir = path.join(tempRoot, 'newer-complete');
        fs.mkdirSync(olderCompleteDir, { recursive: true });
        fs.mkdirSync(newerIncompleteDir, { recursive: true });
        fs.mkdirSync(newerCompleteDir, { recursive: true });

        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.writeFileSync(path.join(olderCompleteDir, fileName), `older-${fileName}`);
        }
        for (const fileName of ['zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.writeFileSync(path.join(newerIncompleteDir, fileName), `incomplete-${fileName}`);
        }
        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.writeFileSync(path.join(newerCompleteDir, fileName), `newer-${fileName}`);
        }

        fs.utimesSync(olderCompleteDir, new Date('2025-01-01T00:00:00Z'), new Date('2025-01-01T00:00:00Z'));
        fs.utimesSync(newerIncompleteDir, new Date('2026-01-01T00:00:00Z'), new Date('2026-01-01T00:00:00Z'));
        fs.utimesSync(newerCompleteDir, new Date('2026-02-01T00:00:00Z'), new Date('2026-02-01T00:00:00Z'));
        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.utimesSync(
                path.join(olderCompleteDir, fileName),
                new Date('2025-01-01T00:00:00Z'),
                new Date('2025-01-01T00:00:00Z'),
            );
        }
        for (const fileName of ['zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.utimesSync(
                path.join(newerIncompleteDir, fileName),
                new Date('2026-01-01T00:00:00Z'),
                new Date('2026-01-01T00:00:00Z'),
            );
        }
        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.utimesSync(
                path.join(newerCompleteDir, fileName),
                new Date('2026-02-01T00:00:00Z'),
                new Date('2026-02-01T00:00:00Z'),
            );
        }

        const resolved = pickLatestExistingDirectoryWithFiles(
            [
                path.join(tempRoot, 'missing'),
                olderCompleteDir,
                newerIncompleteDir,
                newerCompleteDir,
            ],
            ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll'],
        );

        assert.equal(resolved, newerCompleteDir);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});

// 捆绑入口优先级固定，开发产物时间戳不应改变其顺序选择。
test('pickFirstExistingPath returns the first existing candidate without preferring newer timestamps', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-first-path-'));

    try {
        const firstPath = path.join(tempRoot, 'first.exe');
        const secondPath = path.join(tempRoot, 'second.exe');
        fs.writeFileSync(firstPath, 'first');
        fs.writeFileSync(secondPath, 'second');
        fs.utimesSync(firstPath, new Date('2025-01-01T00:00:00Z'), new Date('2025-01-01T00:00:00Z'));
        fs.utimesSync(secondPath, new Date('2026-01-01T00:00:00Z'), new Date('2026-01-01T00:00:00Z'));

        const resolved = pickFirstExistingPath([
            path.join(tempRoot, 'missing.exe'),
            firstPath,
            secondPath,
        ]);

        assert.equal(resolved, firstPath);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});

// 完整捆绑目录应保持优先，防止已安装扩展加载工作树中的二进制。
// BUG: 该用例只把开发文件时间固定在 2026-01-01，捆绑文件保留创建时间；当
// 测试时钟晚于该日期，开发文件实际更旧，断言不能覆盖标题声称的较新构建场景。
test('pickFirstExistingDirectoryWithFiles prefers bundled native assets over newer development builds', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-first-dir-'));

    try {
        const bundledDir = path.join(tempRoot, 'bundled');
        const devBuildDir = path.join(tempRoot, 'dev-build');
        const requiredFiles = ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll'];
        fs.mkdirSync(bundledDir, { recursive: true });
        fs.mkdirSync(devBuildDir, { recursive: true });

        for (const fileName of requiredFiles) {
            fs.writeFileSync(path.join(bundledDir, fileName), `bundled-${fileName}`);
            fs.writeFileSync(path.join(devBuildDir, fileName), `dev-${fileName}`);
        }

        fs.utimesSync(devBuildDir, new Date('2026-01-01T00:00:00Z'), new Date('2026-01-01T00:00:00Z'));
        for (const fileName of requiredFiles) {
            fs.utimesSync(
                path.join(devBuildDir, fileName),
                new Date('2026-01-01T00:00:00Z'),
                new Date('2026-01-01T00:00:00Z'),
            );
        }

        const resolved = pickFirstExistingDirectoryWithFiles([
            path.join(tempRoot, 'missing'),
            bundledDir,
            devBuildDir,
        ], requiredFiles);

        assert.equal(resolved, bundledDir);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});

// 打包布局按共享清单给出平台目录和显式构建候选；宿主另读取同一清单。
test('createArtifactLayout exposes a single relative bundle layout for native and wasm assets', () => {
    const repositoryRoot = path.join('E:', 'Git', 'zr_vm');
    const extensionRoot = path.join(repositoryRoot, 'zr_vm_language_server_extension');
    const nativeBuildDir = path.join(repositoryRoot, 'build', 'custom-native');
    const wasmBuildDir = path.join(repositoryRoot, 'build', 'custom-wasm');

    const layout = createArtifactLayout({
        repositoryRoot,
        extensionRoot,
        nativeBuildDir,
        wasmBuildDir,
        nativeBuildConfig: 'Release',
        platform: 'win32',
        arch: 'x64',
    });

    assert.equal(layout.native.bundledRelativeDir.replace(/\\/g, '/'), 'server/native/win32-x64');
    assert.equal(layout.wasm.bundledRelativeDir.replace(/\\/g, '/'), 'out/web');
    assert.deepEqual(layout.native.buildOutputCandidates.slice(0, 2), [
        path.join(nativeBuildDir, 'bin', 'Release'),
        path.join(nativeBuildDir, 'bin'),
    ]);
    assert.deepEqual(layout.wasm.sourceCandidates.slice(0, 2), [
        path.join(wasmBuildDir, 'wasm'),
        wasmBuildDir,
    ]);
});
