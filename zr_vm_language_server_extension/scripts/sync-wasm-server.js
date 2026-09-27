const fs = require('node:fs');
const path = require('node:path');
const { createArtifactLayout } = require('./artifact-layout');

// sync:wasm 在 build:wasm 后将两个 WASM 产物放进 Web worker 可加载的扩展目录。
const layout = createArtifactLayout({
    repositoryRoot: path.resolve(__dirname, '..', '..'),
    extensionRoot: path.resolve(__dirname, '..'),
});
const extensionRoot = layout.extensionRoot;
const destinationDir = layout.wasm.bundledDir;
const requiredFiles = layout.wasm.requiredFiles;

// 显式目录便于复用预编译产物；省略时按 artifact-layout 的候选顺序查找。
const sourceDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : resolveDefaultSourceDir();

if (!fs.existsSync(sourceDir)) {
    console.error(`WASM server source directory does not exist: ${sourceDir}`);
    process.exit(1);
}

// BUG: existsSync 也接受目录；若第二项是目录，首项已复制后 copyFileSync 失败，
// 隔离复现得到新 JS 与旧 WASM 混在 out/web，后续打包可能使用不匹配的资产。
for (const fileName of requiredFiles) {
    const sourceFile = path.join(sourceDir, fileName);
    if (!fs.existsSync(sourceFile)) {
        console.error(`Missing required WASM asset: ${sourceFile}`);
        process.exit(1);
    }
}

// 全部源项通过检查后才创建目标，但复制过程仍非事务性。
fs.mkdirSync(destinationDir, { recursive: true });

for (const fileName of requiredFiles) {
    const sourceFile = path.join(sourceDir, fileName);
    const destinationFile = path.join(destinationDir, fileName);
    fs.copyFileSync(sourceFile, destinationFile);
    console.log(`Copied ${fileName} -> ${path.relative(extensionRoot, destinationFile)}`);
}

// BUG: 首个存在的候选目录可能不含完整产物；即使下一候选包含两项资产，
// 隔离复现的无参 sync:wasm 仍在首目录报缺文件并退出 1。
function resolveDefaultSourceDir() {
    for (const candidate of layout.wasm.sourceCandidates) {
        if (fs.existsSync(candidate)) {
            return candidate;
        }
    }

    return layout.wasm.sourceCandidates[0];
}
