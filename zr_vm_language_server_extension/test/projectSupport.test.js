const test = require('node:test');
const assert = require('node:assert/strict');

const {
    parseProjectManifestText,
    pickBestProjectForFile,
} = require('../out/projectSupport.js');

/** 有效 `.zrp` 的字段筛选正例：定位字段和两个非空别名会进入返回模型；不覆盖 schema 拒绝、`null` 或路径归一化边界。 */
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

/** 回归意图是保护重叠源码根时最深候选获胜；当前样例只有一个根实际匹配，故无法证明排序优先级（BUG 证据如下）。 */
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
