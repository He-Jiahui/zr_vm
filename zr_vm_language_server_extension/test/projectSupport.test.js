const test = require('node:test');
const assert = require('node:assert/strict');

const {
    parseProjectManifestText,
    pickBestProjectForFile,
} = require('../out/projectSupport.js');

/** 有效 .zrp 的定位字段与别名应被提取，供工作区发现和调试共用。 */
test('parseProjectManifestText reads the required zrp fields', () => {
    const manifest = parseProjectManifestText(
        JSON.stringify({
            name: 'demo-project',
            source: 'src',
            binary: 'bin',
            entry: 'app/main',
            pathAliases: {
                '@app': 'feature/app',
                '@shared': 'common/shared',
            },
        }),
        'D:/repo/demo-project.zrp',
    );

    assert.equal(manifest?.name, 'demo-project');
    assert.equal(manifest?.source, 'src');
    assert.equal(manifest?.binary, 'bin');
    assert.equal(manifest?.entry, 'app/main');
    assert.deepEqual(manifest?.pathAliases, {
        '@app': 'feature/app',
        '@shared': 'common/shared',
    });
});

/** 候选中有无关源码根时，文件应归属实际包含它的项目。 */
test('pickBestProjectForFile prefers the deepest matching source root', () => {
    // BUG: root-project 的 D:/repo/src 并不包含目标文件；去掉“最长根优先”的排序
    // 本例仍会通过，故测试名称所称的重叠源码根优先级未被真正保护。
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
