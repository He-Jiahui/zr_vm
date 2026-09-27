const fs = require('node:fs');
const path = require('node:path');
const esbuild = require('esbuild');

// 编译产物与 WASM 资产共享 out/web；浏览器入口仍须写在 package.json 指定的 out/browser.js。
const extensionRoot = path.resolve(__dirname, '..');
const outputDir = path.join(extensionRoot, 'out', 'web');

// compile 先处理扩展宿主入口，再生成无法依赖 Node 模块加载器的独立 worker 脚本。
async function main() {
    fs.mkdirSync(outputDir, { recursive: true });

    // browser.ts 在 VS Code Web 宿主执行，vscode API 由宿主提供而非打进 bundle。
    await esbuild.build({
        entryPoints: [path.join(extensionRoot, 'src', 'browser.ts')],
        bundle: true,
        format: 'cjs',
        platform: 'browser',
        target: 'es2020',
        outfile: path.join(extensionRoot, 'out', 'browser.js'),
        sourcemap: true,
        external: ['vscode'],
        logLevel: 'info',
    });

    // Worker URL 由 browser.ts 指向该固定位置；IIFE 可直接在 DedicatedWorker 中运行。
    await esbuild.build({
        entryPoints: [path.join(extensionRoot, 'src', 'browser', 'worker', 'server-worker.ts')],
        bundle: true,
        format: 'iife',
        platform: 'browser',
        target: 'es2020',
        outfile: path.join(outputDir, 'server-worker.js'),
        sourcemap: true,
        logLevel: 'info',
    });
}

// 任一 bundle 失败必须使 compile 中止，防止缺失或未更新的 worker 进入后续打包。
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
