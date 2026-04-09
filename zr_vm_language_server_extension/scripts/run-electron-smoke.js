const path = require('node:path');
const fs = require('node:fs');
const os = require('node:os');
const { runTests } = require('@vscode/test-electron');
const { spawnSync } = require('node:child_process');
const {
    executableName,
    resolveLatestRunnableNativeAssetDir,
} = require('./native-asset-selection');

function resolveNativeServerPath(repoRoot) {
    const configured = process.env.ZR_TEST_NATIVE_SERVER;
    if (configured && fs.existsSync(configured)) {
        return configured;
    }

    const extensionRoot = path.join(repoRoot, 'zr_vm_language_server_extension');
    const assetDir = resolveLatestRunnableNativeAssetDir(repoRoot, extensionRoot);
    return assetDir ? path.join(assetDir, executableName('languageServer')) : undefined;
}

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

async function main() {
    const extensionDevelopmentPath = path.resolve(__dirname, '..');
    const extensionTestsPath = path.resolve(__dirname, '..', 'test', 'smoke', 'electronRunner.js');
    const repoRoot = path.resolve(__dirname, '..', '..');
    const workspacePath = path.resolve(repoRoot, 'tests', 'fixtures', 'projects', 'import_basic');
    const nativeServerPath = resolveNativeServerPath(repoRoot);

    if (!nativeServerPath) {
        throw new Error('Unable to locate zr_vm_language_server_stdio for Electron smoke test.');
    }

    syncBundledNativeServer(extensionDevelopmentPath, nativeServerPath);
    const userDataDir = prepareUserDataDir(extensionDevelopmentPath);

    await runTests({
        extensionDevelopmentPath,
        extensionTestsPath,
        launchArgs: [
            workspacePath,
            '--disable-extensions',
            `--user-data-dir=${userDataDir}`,
        ],
        extensionTestsEnv: {
            ZR_TEST_SERVER_MODE: 'native',
            ZR_TEST_NATIVE_SERVER: nativeServerPath,
        },
    });
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
