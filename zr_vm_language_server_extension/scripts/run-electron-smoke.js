const path = require('node:path');
const fs = require('node:fs');
const os = require('node:os');
const { pathToFileURL } = require('node:url');
const { runTests } = require('@vscode/test-electron');
const { spawnSync } = require('node:child_process');
const {
    executableName,
    resolveLatestRunnableNativeAssetDir,
} = require('./native-asset-selection');

/** npm 的桌面 e2e 命令先构建服务端，再由此处选择资产同步源。
 * 显式 ZR_TEST_NATIVE_SERVER 优先；未设置或路径不存在时使用最新完整的开发/捆绑资产。
 * @note 指定路径须是当前平台语言服务端文件，且同目录须有同步脚本需要的资产。
 */
function resolveNativeServerPath(repoRoot) {
    const configured = process.env.ZR_TEST_NATIVE_SERVER;
    // BUG: 仅检验存在性会接受目录；该目录随后被传给 syncBundledNativeServer，
    // 其父目录被误认为资产源，导致同步缺件或误测父目录资产。将现有目录设为覆盖值即可触发。
    if (configured && fs.existsSync(configured)) {
        return configured;
    }

    const extensionRoot = path.join(repoRoot, 'zr_vm_language_server_extension');
    // BUG: test:e2e:desktop 虽先 build:native，此选择仍允许旧 bundledDir 参与按 mtime 竞选；
    // 当旧捆绑文件时间较晚且本次构建无更新时，会用旧资产完成 smoke，无法验证当前构建。
    const assetDir = resolveLatestRunnableNativeAssetDir(repoRoot, extensionRoot);
    return assetDir ? path.join(assetDir, executableName('languageServer')) : undefined;
}

/** 在启动 Electron 前把所选服务端的同目录资产同步到扩展运行时布局。
 * smokeSuite 与扩展在捆绑目录使用 CLI/服务端，二者应来自同一构建。
 */
function syncBundledNativeServer(extensionDevelopmentPath, nativeServerPath) {
    const sourceDir = path.dirname(nativeServerPath);
    const syncScript = path.join(extensionDevelopmentPath, 'scripts', 'sync-native-server.js');
    const result = spawnSync(process.execPath, [syncScript, sourceDir], {
        cwd: extensionDevelopmentPath,
        stdio: 'inherit',
    });

    if (result.status !== 0) {
        throw new Error(`sync-native-server.js failed with exit code ${result.status ?? 'unknown'}`);
    }
}

/** 每次桌面 smoke 使用隔离的用户设置，强制 native 模式并打开 LSP 日志。
 * @note 会删除扩展根下固定的测试 profile；同一工作树的并发 smoke 不应共享此目录。
 */
function prepareUserDataDir(extensionDevelopmentPath) {
    const userDataDir = path.join(extensionDevelopmentPath, '.vscode-test-smoke', 'electron-user-data');
    const userSettingsDir = path.join(userDataDir, 'User');
    const settingsPath = path.join(userSettingsDir, 'settings.json');

    fs.rmSync(userDataDir, { recursive: true, force: true });
    fs.mkdirSync(userSettingsDir, { recursive: true });
    fs.writeFileSync(settingsPath, JSON.stringify({
        'zr.languageServer.enable': true,
        'zr.languageServer.mode': 'native',
        'zr.languageServer.trace.server': 'verbose',
        'window.autoDetectColorScheme': true,
    }, null, 4) + os.EOL, 'utf8');

    return userDataDir;
}

/** 子进程须作为 VS Code 桌面宿主启动；移除可能由上游 Electron/开发会话继承的模式开关。 */
function clearElectronNodeEnvironment() {
    delete process.env.ELECTRON_RUN_AS_NODE;
    delete process.env.VSCODE_DEV;
}

/** package.json 桌面 e2e 脚本入口：验证 workspace、同步原生资产，再把 runner 和焦点传给 VS Code。
 * @note 默认 fixture 为 import_basic；ZR_TEST_WORKSPACE_PATH 应指向现存目录。
 */
async function main() {
    const extensionDevelopmentPath = path.resolve(__dirname, '..');
    const extensionTestsPath = path.resolve(__dirname, '..', 'test', 'smoke', 'electronRunner.js');
    const repoRoot = path.resolve(__dirname, '..', '..');
    const workspacePath = path.resolve(process.env.ZR_TEST_WORKSPACE_PATH ||
        path.join(repoRoot, 'tests', 'fixtures', 'projects', 'import_basic'));
    if (!fs.statSync(workspacePath).isDirectory()) {
        throw new Error(`Smoke workspace is not a directory: ${workspacePath}`);
    }
    const nativeServerPath = resolveNativeServerPath(repoRoot);

    if (!nativeServerPath) {
        throw new Error('Unable to locate zr_vm_language_server_stdio for Electron smoke test.');
    }

    syncBundledNativeServer(extensionDevelopmentPath, nativeServerPath);
    const userDataDir = prepareUserDataDir(extensionDevelopmentPath);
    clearElectronNodeEnvironment();

    // @vscode/test-electron 将 extensionTestsEnv 传入测试宿主；桌面 runner 据此选择 native 和用例集。
    // BUG: ZR_TEST_NATIVE_SERVER 只在本脚本用于选同步源，runner 和扩展均不读取该变量；
    // 若显式路径是同目录下的其他文件，仍同步并测试固定名称的服务端，可能产生假阳性。
    // 指向不存在文件时上方选择器还会静默回退到旧资产；需以实际启动路径验收覆盖值。
    await runTests({
        extensionDevelopmentPath,
        extensionTestsPath,
        launchArgs: [
            `--folder-uri=${pathToFileURL(workspacePath).toString()}`,
            '--disable-extensions',
            `--user-data-dir=${userDataDir}`,
        ],
        extensionTestsEnv: {
            ZR_TEST_SERVER_MODE: 'native',
            ZR_TEST_NATIVE_SERVER: nativeServerPath,
            ZR_TEST_SMOKE_FOCUS: process.env.ZR_TEST_SMOKE_FOCUS || 'all',
        },
    });
}

// 让 npm 的 e2e 命令收到失败状态，而不是只把异常打印在测试启动器中。
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
