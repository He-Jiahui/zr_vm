const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const root = path.resolve(__dirname, '../..');
const cmake = fs.readFileSync(path.join(root, 'zr_vm_language_server/CMakeLists.txt'), 'utf8');
const publicNames = JSON.parse(cmake.match(/set\(EXPORTED_FUNCTIONS_JSON\s+"(\[[^\n]+\])"\)/)[1]
    .replace(/\\"/g, '"'));
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-wasm-linked-'));
const jsFile = path.join(directory, 'module.js');
const wasmFile = path.join(directory, 'module.wasm');
const leb = value => {
    const bytes = [];
    do { bytes.push((value & 127) | (value > 127 ? 128 : 0)); value >>>= 7; } while (value);
    return bytes;
};
const vector = bytes => [...leb(bytes.length), ...bytes];
const section = (id, bytes) => [id, ...vector(bytes)];

// Tiny real WASM module: one callable context stub, exported under every ABI name.
function binary(names, nonFunction = false) {
    const exports = names.flatMap((name, index) => [
        ...vector([...Buffer.from(name)]), nonFunction && index === 0 ? 2 : 0, 0,
    ]);
    return Buffer.from([
        0, 97, 115, 109, 1, 0, 0, 0,
        ...section(1, [1, 96, 0, 1, 127]),
        ...section(3, [1, 0]),
        ...section(5, [1, 0, 1]),
        ...section(7, [...leb(names.length), ...exports]),
        ...section(10, [1, 4, 0, 65, 7, 11]),
    ]);
}

function javascript(targets, wrapper = false) {
    const assignments = publicNames.map((name, index) =>
        `Module[${JSON.stringify(name)}] = ${wrapper
            ? `createExportWrapper(${JSON.stringify(targets[index])})`
            : `wasmExports[${JSON.stringify(targets[index])}]`};`).join('\n');
    return `var createZrLanguageServerModule = async function(Module) {
        let wasmExports;
        const createExportWrapper = name => (...args) => wasmExports[name](...args);
        Module.instantiateWasm({}, instance => { wasmExports = instance.exports; });
        ${assignments}
        Module.ccall = (name, type, types, args = []) => Module['_' + name](...args);
        Module.cwrap = name => (...args) => Module['_' + name](...args);
        Module.UTF8ToString = () => '';
        Module.stringToUTF8 = () => {};
        return Module;
    };`;
}

const minified = publicNames.map((_, index) => 'f' + index);
const plain = publicNames.map(name => name.slice(1));
const cases = [
    ['minified Release exports', true, javascript(minified), binary(minified)],
    ['unminified Release exports', true, javascript(plain), binary(plain)],
    ['Debug export wrappers', true, javascript(plain, true), binary(plain)],
    ['missing public assignment', false, javascript(minified).replace(/Module\["_malloc"\] = [^;]+;/, ''), binary(minified)],
    ['duplicate public assignment', false, javascript(minified).replace('return Module;',
        'Module["_malloc"] = wasmExports["f0"]; return Module;'), binary(minified)],
    ['missing binary target', false, javascript(minified), binary(minified.slice(1))],
    ['nonfunction binary target', false, javascript(minified), binary(minified, true)],
    ['empty JavaScript', false, '', binary(plain)],
    ['mismatched asset pair', false, javascript(minified), binary(plain)],
    ['missing runtime helper', false, javascript(minified).replace('Module.ccall =', 'Module.unused ='), binary(minified)],
    ['runtime public binding overwritten', false, javascript(minified).replace('return Module;',
        `delete Module[${JSON.stringify(publicNames[0])}]; return Module;`), binary(minified)],
];
let failures = 0;
try {
    for (const [name, valid, js, wasm] of cases) {
        fs.writeFileSync(jsFile, js);
        fs.writeFileSync(wasmFile, wasm);
        const result = spawnSync(process.execPath, [
            path.join(__dirname, 'wasm_capability_inventory.js'), root, jsFile, wasmFile,
        ], { encoding: 'utf8', timeout: 30000, windowsHide: true });
        try {
            assert.ifError(result.error);
            assert.equal(result.signal, null, result.stderr);
            assert.equal(result.status, valid ? 0 : 1, result.stderr || 'invalid linked assets were accepted');
            if (valid) assert.equal(JSON.parse(result.stdout).linkedAssetChecked, true);
            else assert.match(result.stderr, /AssertionError/);
            console.log('Pass - linked inventory: ' + name);
        } catch (error) {
            failures++;
            console.error('Fail - linked inventory: ' + name + ': ' + error.message);
        }
    }
} finally {
    fs.rmSync(directory, { recursive: true, force: true });
}
console.log(`Linked WASM inventory regression: ${cases.length - failures}/${cases.length}`);
process.exitCode = failures ? 1 : 0;
