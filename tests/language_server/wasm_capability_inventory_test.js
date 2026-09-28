const assert = require('assert').strict;
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const repository = path.resolve(__dirname, '..', '..');
const worker = 'zr_vm_language_server_extension/src/browser/worker/server-worker.ts';
const bridge = 'zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts';
const response = 'zr_vm_language_server_extension/src/browser/worker/wasm-response.ts';
const inputs = [
    'zr_vm_language_server/CMakeLists.txt',
    'zr_vm_language_server/wasm/wasm_exports.cpp',
    'zr_vm_language_server/wasm/wasm_exports.h', worker, bridge, response,
    'zr_vm_language_server_extension/src/browser/worker/document-sync.ts',
];
const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-wasm-inventory-'));
const sources = new Map(inputs.map(file => [file, fs.readFileSync(path.join(repository, file), 'utf8')]));

/** 只交换现有源码片段，确保拒绝来自能力接线漂移而非缺失测试目标。 */
function swap(source, first, second) {
    assert.ok(source.includes(first) && source.includes(second), 'mutation targets must exist');
    return source.replace(first, '__inventory_swap__').replace(second, first).replace('__inventory_swap__', second);
}

// 同一份生产输入逐例变异，覆盖响应契约、路由、导出、legend 与能力广告。
const cases = [
    ['accepts the production adapter wiring', null, null],
    ['rejects errors converted to empty success', response, source =>
        source.replace('throw new ResponseError(code, message, response.data);', 'return fallback;')],
    ['rejects swapped worker providers', worker, source =>
        swap(source, 'bridge.getCompletion(', 'bridge.getHover(')],
    ['rejects swapped bridge exports', bridge, source =>
        swap(source, "'wasm_ZrLspGetCompletion'", "'wasm_ZrLspGetHover'")],
    ['rejects missing inlay hint route', worker, source =>
        source.replace("connection.onRequest('textDocument/inlayHint'", "connection.onRequest('textDocument/unregisteredHint'")],
    ['rejects duplicate worker route', worker, source =>
        source + "\nconnection.onHover(async () => null);\n"],
    ['rejects orphan worker route', worker, source =>
        source + "\nconnection.onRequest('textDocument/unregistered', async () => []);\n"],
    ['rejects reordered semantic legend', worker, source => swap(source, "'namespace'", "'class'")],
    ['rejects extra semantic token', worker, source => source.replace("'metaMethod',", "'metaMethod', 'unregistered',")],
    ['rejects a capability without a provider', worker, source =>
        source.replace('hoverProvider: true,', 'hoverProvider: true, signatureHelpProvider: {},')],
];

// 每轮写入独立临时副本，真实库存 CLI 不接触仓库构建目录。
let failures = 0;
try {
    for (const [name, mutatedFile, mutate] of cases) {
        try {
            for (const [file, source] of sources) {
                const destination = path.join(fixture, file);
                fs.mkdirSync(path.dirname(destination), { recursive: true });
                const content = file === mutatedFile ? mutate(source) : source;
                if (file === mutatedFile) assert.notEqual(content, source, 'mutation must change its fixture');
                fs.writeFileSync(destination, content);
            }
            const result = spawnSync(process.execPath, [
                path.join(__dirname, 'wasm_capability_inventory.js'), fixture,
            ], { encoding: 'utf8', timeout: 30000, maxBuffer: 4 * 1024 * 1024, windowsHide: true });
            assert.ifError(result.error);
            assert.equal(result.signal, null, result.stderr);
            if (mutatedFile === null) {
                assert.equal(result.status, 0, result.stderr);
                assert.equal(JSON.parse(result.stdout).linkedAssetChecked, false);
            } else {
                assert.equal(result.status, 1, 'the real inventory CLI must reject this drift');
                assert.match(result.stderr, /AssertionError/, 'a loader or syntax error is not a contract rejection');
            }
            console.log('Pass - ' + name);
        } catch (error) {
            failures++;
            console.error('Fail - ' + name + ': ' + error.message);
        }
    }
} finally {
    if (fs.rmSync) fs.rmSync(fixture, { recursive: true, force: true });
    else fs.rmdirSync(fixture, { recursive: true });
}
console.log(`WASM inventory regression: ${cases.length - failures}/${cases.length}`);
process.exitCode = failures ? 1 : 0;
