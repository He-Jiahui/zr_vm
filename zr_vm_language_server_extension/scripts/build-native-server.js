const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const repositoryRoot = path.resolve(__dirname, '..', '..');
const buildDir = process.env.ZR_NATIVE_BUILD_DIR
    ? path.resolve(process.env.ZR_NATIVE_BUILD_DIR)
    : path.join(repositoryRoot, 'build', 'codex-lsp');
const buildConfig = process.env.ZR_NATIVE_BUILD_CONFIG || 'Debug';
const target = process.env.ZR_NATIVE_BUILD_TARGET || 'zr_vm_language_server_stdio';
const jobs = process.env.ZR_BUILD_JOBS || '8';

if (!fs.existsSync(buildDir)) {
    console.error(`Native build directory does not exist: ${buildDir}`);
    process.exit(1);
}

const args = ['--build', buildDir];
if (process.platform === 'win32') {
    args.push('--config', buildConfig);
}
args.push('--target', target, '--parallel', jobs);

const result = spawnSync('cmake', args, {
    cwd: repositoryRoot,
    stdio: 'inherit',
});

if (result.status !== 0) {
    process.exit(result.status ?? 1);
}
