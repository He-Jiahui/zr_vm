const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

function readPackageJson() {
    return JSON.parse(fs.readFileSync(path.resolve(__dirname, '..', 'package.json'), 'utf8'));
}

test('zr.organizeImports is exposed through activation, commands, and editor context menu', () => {
    const manifest = readPackageJson();
    const commands = manifest.contributes?.commands ?? [];
    const editorContext = manifest.contributes?.menus?.['editor/context'] ?? [];

    assert(manifest.activationEvents.includes('onCommand:zr.organizeImports'));
    assert(commands.some((entry) =>
        entry.command === 'zr.organizeImports' &&
        entry.title === 'Organize Imports' &&
        entry.category === 'Zr'));
    assert(editorContext.some((entry) =>
        entry.command === 'zr.organizeImports' &&
        entry.when === 'editorLangId == zr'));
});
