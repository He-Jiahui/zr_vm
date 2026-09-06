const assert = require('assert').strict;
const fs = require('fs');
const path = require('path');
const { probeWorker } = require('./lsp_wasm_worker_probe');

function read(filePath) {
    assert.ok(fs.existsSync(filePath), `missing inventory input: ${filePath}`);
    return fs.readFileSync(filePath, 'utf8');
}

function assertSetEqual(actual, expected, label) {
    assert.equal(new Set(actual).size, actual.length, `${label} has duplicates`);
    assert.deepEqual([...actual].sort(), [...expected].sort(), `${label} mismatch`);
}

async function main() {
    assert.ok(Number(process.versions.node.split('.')[0]) >= 18,
        'WASM worker wiring probe requires Node 18+; configure ZR_VM_NODE_EXECUTABLE with a compatible runtime');
    const [repositoryRootArg, wasmJavaScriptArg, wasmBinaryArg] = process.argv.slice(2);
    const root = path.resolve(repositoryRootArg || path.join(__dirname, '..', '..'));
    const cmake = read(path.join(root, 'zr_vm_language_server', 'CMakeLists.txt'));
    const exportsSource = read(path.join(root, 'zr_vm_language_server', 'wasm', 'wasm_exports.cpp'));
    const exportsHeader = read(path.join(root, 'zr_vm_language_server', 'wasm', 'wasm_exports.h'));
    const bridge = read(path.join(root, 'zr_vm_language_server_extension', 'src', 'browser', 'worker', 'wasm-bridge.ts'));
    const worker = read(path.join(root, 'zr_vm_language_server_extension', 'src', 'browser', 'worker', 'server-worker.ts'));
    const exportListMatch = cmake.match(/set\(EXPORTED_FUNCTIONS_JSON\s+"(\[[^\n]+\])"\)/);
    assert.ok(exportListMatch, 'CMake export list is missing');
    const exportedFunctions = JSON.parse(exportListMatch[1].replace(/\\"/g, '"'))
        .map(name => name.replace(/^_/, ''));
    const runtimeExports = exportedFunctions.filter(name => name.startsWith('wasm_'));
    const definitions = [...exportsSource.matchAll(/(?:const\s+char\s*\*|void\s*\*|void|int)\s+(wasm_[A-Za-z0-9_]+)\s*\(/g)]
        .map(match => match[1]);
    const declarations = [...exportsHeader.matchAll(/(?:const\s+char\s*\*|void\s*\*|void|int)\s+(wasm_[A-Za-z0-9_]+)\s*\(/g)]
        .map(match => match[1]);
    const bridgeCalls = [...bridge.matchAll(/['"](wasm_[A-Za-z0-9_]+)['"]/g)].map(match => match[1]);
    assertSetEqual(definitions, runtimeExports, 'C++ definitions and CMake exports');
    assertSetEqual(declarations, runtimeExports, 'C++ declarations and CMake exports');
    assertSetEqual(bridgeCalls, runtimeExports.filter(name => !['wasm_malloc', 'wasm_free'].includes(name)),
        'bridge ccall names and runtime exports');
    const workerReport = await probeWorker(worker, bridge, runtimeExports);

    let linkedAssetChecked = false;
    if (wasmJavaScriptArg || wasmBinaryArg) {
        assert.ok(wasmJavaScriptArg && wasmBinaryArg, 'WASM asset check requires both JS and binary paths');
        assert.ok(fs.existsSync(wasmJavaScriptArg), `missing generated WASM JavaScript: ${wasmJavaScriptArg}`);
        const module = new WebAssembly.Module(fs.readFileSync(wasmBinaryArg));
        const linkedExports = WebAssembly.Module.exports(module).map(entry => entry.name.replace(/^_/, ''));
        assertSetEqual(linkedExports.filter(name => name.startsWith('wasm_')), runtimeExports,
            'linked WASM exports and CMake exports');
        linkedAssetChecked = true;
    }
    console.log(JSON.stringify({
        schemaVersion: 2,
        status: linkedAssetChecked ? 'wasm-linked-contract-mapped' : 'wasm-static-contract-mapped',
        runtimeExports: runtimeExports.length,
        runtimeExportNames: runtimeExports,
        bridgeCalls: bridgeCalls.length,
        workerRoutes: workerReport.featureRoutes.length,
        semanticTokenTypes: workerReport.capabilities.semanticTokensProvider.legend.tokenTypes.length,
        semanticTokenModifiers: workerReport.capabilities.semanticTokensProvider.legend.tokenModifiers,
        linkedAssetChecked, worker: workerReport,
    }, null, 2));
}

main().catch(error => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
