const fs = require('node:fs');
const path = require('node:path');

// package.json 的 compile 在 tsc 与 Web worker 打包前调用此脚本。
// 仅把扩展根下的 out 视为可重建产物；原生与 WASM 服务端资产由各自同步脚本管理。
const extensionRoot = path.resolve(__dirname, '..');
const outputDir = path.join(extensionRoot, 'out');

// 预发布、桌面及 Web smoke 都会经过 compile；先移除旧输出，避免过期 JS 进入 VSIX 或测试宿主。
fs.rmSync(outputDir, { recursive: true, force: true });
