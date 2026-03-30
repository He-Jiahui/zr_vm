const fs = require('node:fs');
const path = require('node:path');

const extensionRoot = path.resolve(__dirname, '..');
const outputDir = path.join(extensionRoot, 'out');

fs.rmSync(outputDir, { recursive: true, force: true });
