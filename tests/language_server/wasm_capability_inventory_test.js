const assert = require('assert').strict;
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const repository = path.resolve(__dirname, '..', '..');
const worker = 'zr_vm_language_server_extension/src/browser/worker/server-worker.ts';
const bridge = 'zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts';
const inputs = [
    'zr_vm_language_server/CMakeLists.txt',
    'zr_vm_language_server/wasm/wasm_exports.cpp',
    'zr_vm_language_server/wasm/wasm_exports.h', worker, bridge,
];
const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-wasm-inventory-'));
const sources = new Map(inputs.map(file => [file, fs.readFileSync(path.join(repository, file), 'utf8')]));

function swap(source, first, second) {
    assert.ok(source.includes(first) && source.includes(second), 'mutation targets must exist');
    return source.replace(first, '__inventory_swap__').replace(second, first).replace('__inventory_swap__', second);
}

const cases = [
    ['accepts the production adapter wiring', null, null],
    ['rejects missing JSON-RPC error code', 'zr_vm_language_server/wasm/wasm_exports.cpp', source =>
        source.replace(/\s*cJSON_AddNumberToObject\(json, "code", error_code_for_message\(message\)\);/, '')],
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
