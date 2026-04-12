const fs = require('node:fs');
const path = require('node:path');
const { createArtifactLayout } = require('./artifact-layout');

const layout = createArtifactLayout({
    repositoryRoot: path.resolve(__dirname, '..', '..'),
    extensionRoot: path.resolve(__dirname, '..'),
});
const extensionRoot = layout.extensionRoot;
const destinationDir = layout.wasm.bundledDir;
const requiredFiles = layout.wasm.requiredFiles;

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
    for (const candidate of layout.wasm.sourceCandidates) {
        if (fs.existsSync(candidate)) {
            return candidate;
        }
    }

    return layout.wasm.sourceCandidates[0];
}
