const fs = require('node:fs');
const path = require('node:path');
const esbuild = require('esbuild');

const extensionRoot = path.resolve(__dirname, '..');
const outputDir = path.join(extensionRoot, 'out', 'web');

async function main() {
    fs.mkdirSync(outputDir, { recursive: true });

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

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
