const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const { createArtifactLayout } = require('../scripts/artifact-layout.js');

// 验证 package.json 同步脚本将布局声明的两项 WASM 资产复制到 VSIX 目录。
// TODO: 本例直接改写共享 out/web 后再恢复；并行构建/打包可观察到测试桩，
// 需把扩展布局注入临时根目录或串行化此类测试。
test('sync-wasm-server copies the required wasm assets into the extension bundle', () => {
    const extensionRoot = path.resolve(__dirname, '..');
    const repositoryRoot = path.resolve(extensionRoot, '..');
    const layout = createArtifactLayout({
        repositoryRoot,
        extensionRoot,
    });
    const sourceDir = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-wasm-sync-source-'));
    const previousFiles = new Map();

    // 保存既有文件以减小单次测试对开发工作树的影响；正常退出时完整还原。
    try {
        for (const fileName of layout.wasm.requiredFiles) {
            fs.writeFileSync(path.join(sourceDir, fileName), `test-${fileName}`);

            const destinationFile = path.join(layout.wasm.bundledDir, fileName);
            if (fs.existsSync(destinationFile)) {
                previousFiles.set(fileName, fs.readFileSync(destinationFile));
            }
        }

        // 用真实子进程覆盖命令行入口、布局解析和复制，不只模拟文件 API。
        const result = spawnSync(process.execPath, [
            path.join(extensionRoot, 'scripts', 'sync-wasm-server.js'),
            sourceDir,
        ], {
            cwd: extensionRoot,
            encoding: 'utf8',
        });

        assert.equal(result.status, 0, result.stderr || result.stdout);

        for (const fileName of layout.wasm.requiredFiles) {
            const destinationFile = path.join(layout.wasm.bundledDir, fileName);
            assert.equal(fs.readFileSync(destinationFile, 'utf8'), `test-${fileName}`);
        }
    } finally {
        for (const fileName of layout.wasm.requiredFiles) {
            const destinationFile = path.join(layout.wasm.bundledDir, fileName);
            if (previousFiles.has(fileName)) {
                fs.mkdirSync(path.dirname(destinationFile), { recursive: true });
                fs.writeFileSync(destinationFile, previousFiles.get(fileName));
            } else if (fs.existsSync(destinationFile)) {
                fs.rmSync(destinationFile, { force: true });
            }
        }

        fs.rmSync(sourceDir, { recursive: true, force: true });
    }
});
