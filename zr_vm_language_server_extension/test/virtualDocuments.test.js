// 核对虚拟 URI 的身份参数在 VS Code 序列化后仍可还原。
const test = require('node:test');
const assert = require('node:assert/strict');
const { URI } = require('vscode-uri');

// TODO: 本例仅覆盖 URI 往返；仍需检查 provider 注销后监听器是否释放及重启刷新行为。
test('scoped virtual declaration identity survives VS Code URI serialization', () => {
    const identity = {
        project: 'file:///tmp/project & #%".zrp',
        origin: 'file:///tmp/native & #%".so',
        generation: '18446744073709551615',
    };
    const moduleName = 'zr.test.\u{20000}';
    const uri = `zr-decompiled:/${encodeURIComponent(moduleName)}.zr?${encodeURIComponent(JSON.stringify(identity))}`;
    const clientUri = URI.parse(uri).toString();
    const query = clientUri.slice(clientUri.indexOf('?') + 1);
    assert.deepEqual(JSON.parse(decodeURIComponent(query)), identity);
    assert.equal(URI.parse(clientUri).path, `/${moduleName}.zr`);
    assert.equal(URI.parse(clientUri).scheme, 'zr-decompiled');
});
