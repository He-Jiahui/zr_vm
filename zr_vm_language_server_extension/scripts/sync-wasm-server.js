const fs = require('node:fs');
const path = require('node:path');

const extensionRoot = path.resolve(__dirname, '..');
const repositoryRoot = path.resolve(extensionRoot, '..');
const destinationDir = path.join(extensionRoot, 'out', 'web');
const requiredFiles = [
    'zr_vm_language_server.js',
    'zr_vm_language_server.wasm',
];

const sourceDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : resolveDefaultSourceDir();

if (!fs.existsSync(sourceDir)) {
    console.error(`WASM server source directory does not exist: ${sourceDir}`);
    process.exit(1);
}

for (const fileName of requiredFiles) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        console.error(`Missing required WASM asset: ${sourceFile}`);
        process.exit(1);
    }
}

fs.mkdirSync(destinationDir, { recursive: true });

for (const fileName of requiredFiles) {
    const sourceFile = path.join(sourceDir, fileName);
    const destinationFile = path.join(destinationDir, fileName);
    fs.copyFileSync(sourceFile, destinationFile);
    console.log(`Copied ${fileName} -> ${path.relative(extensionRoot, destinationFile)}`);
}

function resolveDefaultSourceDir() {
    const candidates = [
        path.join(repositoryRoot, 'build', 'codex-lsp-wasm', 'wasm'),
        path.join(repositoryRoot, 'build', 'wasm', 'wasm'),
        path.join(repositoryRoot, 'build', 'wasm'),
    ];

    for (const candidate of candidates) {
        if (fs.existsSync(candidate)) {
            return candidate;
        }
    }

    return candidates[0];
}
