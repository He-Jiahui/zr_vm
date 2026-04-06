const test = require('node:test');
const assert = require('node:assert/strict');

const {
    parseProjectManifestText,
    pickBestProjectForFile,
} = require('../out/projectSupport.js');

test('parseProjectManifestText reads the required zrp fields', () => {
    const manifest = parseProjectManifestText(
        JSON.stringify({
            name: 'demo-project',
            source: 'src',
            binary: 'bin',
            entry: 'app/main',
        }),
        'D:/repo/demo-project.zrp',
    );

    assert.equal(manifest?.name, 'demo-project');
    assert.equal(manifest?.source, 'src');
    assert.equal(manifest?.binary, 'bin');
    assert.equal(manifest?.entry, 'app/main');
});

test('pickBestProjectForFile prefers the deepest matching source root', () => {
    const winner = pickBestProjectForFile('D:/repo/packages/demo/src/main.zr', [
        {
            id: 'root-project',
            sourceRootPath: 'D:/repo/src',
        },
        {
            id: 'nested-project',
            sourceRootPath: 'D:/repo/packages/demo/src',
        },
    ]);

    assert.equal(winner?.id, 'nested-project');
});
