const fs = require('node:fs');
const path = require('node:path');

const extensionRoot = path.resolve(__dirname, '..');
const repositoryRoot = path.resolve(extensionRoot, '..');
const bundledFolder = `${process.platform}-${process.arch}`;
const destinationDir = path.join(extensionRoot, 'server', 'native', bundledFolder);

const sourceDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : resolveDefaultSourceDir();

const { requiredFiles, optionalFiles } = process.platform === 'win32'
    ? collectWindowsNativeAssets(sourceDir)
    : {
        requiredFiles: ['zr_vm_language_server_stdio'],
        optionalFiles: [],
    };

if (!fs.existsSync(sourceDir)) {
    console.error(`Native server source directory does not exist: ${sourceDir}`);
    process.exit(1);
}

for (const fileName of requiredFiles) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        console.error(`Missing required native asset: ${sourceFile}`);
        process.exit(1);
    }
}

fs.mkdirSync(destinationDir, { recursive: true });

for (const fileName of [...requiredFiles, ...optionalFiles]) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        continue;
    }

    const destinationFile = path.join(destinationDir, fileName);
    fs.copyFileSync(sourceFile, destinationFile);
    console.log(`Copied ${fileName} -> ${path.relative(extensionRoot, destinationFile)}`);
}

function resolveDefaultSourceDir() {
    const candidates = [
        path.join(repositoryRoot, 'build', 'codex-lsp', 'bin', 'Debug'),
        path.join(repositoryRoot, 'build', 'codex-lsp', 'bin'),
    ];

    for (const candidate of candidates) {
        if (fs.existsSync(candidate)) {
            return candidate;
        }
    }

    return candidates[0];
}

function collectWindowsNativeAssets(nativeSourceDir) {
    const entries = fs.readdirSync(nativeSourceDir, { withFileTypes: true });
    const dllFiles = entries
        .filter((entry) => entry.isFile() && /^zr_vm_.*\.dll$/i.test(entry.name))
        .map((entry) => entry.name)
        .sort();
    const pdbFiles = ['zr_vm_language_server_stdio.pdb', ...dllFiles.map((fileName) => fileName.replace(/\.dll$/i, '.pdb'))]
        .filter((fileName, index, allFileNames) => allFileNames.indexOf(fileName) === index)
        .filter((fileName) => entries.some((entry) => entry.isFile() && entry.name === fileName));

    return {
        requiredFiles: ['zr_vm_language_server_stdio.exe', ...dllFiles],
        optionalFiles: pdbFiles,
    };
}
