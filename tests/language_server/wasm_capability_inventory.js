const assert = require('assert').strict;
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const { probeWorker } = require('./lsp_wasm_worker_probe');

function read(filePath) {
    assert.ok(fs.existsSync(filePath), `missing inventory input: ${filePath}`);
    return fs.readFileSync(filePath, 'utf8');
}

function assertSetEqual(actual, expected, label) {
    assert.equal(new Set(actual).size, actual.length, `${label} has duplicates`);
    assert.deepEqual([...actual].sort(), [...expected].sort(), `${label} mismatch`);
}

function linkedPublicBindings(javaScript, exportedFunctions) {
    const bindings = [];
    const assignment = /Module\[(["'])(_[A-Za-z0-9_]+)\1\]\s*=\s*(?:wasmExports\[(["'])([^"']+)\3\]|createExportWrapper\((["'])([^"']+)\5(?:\s*,\s*\d+)?\))/g;
    for (const match of javaScript.matchAll(assignment)) {
        if (match[2].startsWith('_wasm_') || match[2] === '_malloc' || match[2] === '_free') {
            bindings.push({ publicName: match[2], binaryName: match[4] || match[6], wrapper: Boolean(match[6]) });
        }
    }
    assertSetEqual(bindings.map(binding => binding.publicName), exportedFunctions,
        'generated JavaScript public exports and CMake exports');
    return bindings;
}

async function assertLinkedAssets(javaScript, binary, exportedFunctions) {
    const bindings = linkedPublicBindings(javaScript, exportedFunctions);
    const wasmModule = new WebAssembly.Module(binary);
    const binaryExports = new Map(WebAssembly.Module.exports(wasmModule)
        .map(entry => [entry.name, entry.kind]));
    for (const binding of bindings) {
        assert.equal(binaryExports.get(binding.binaryName), 'function',
            `${binding.publicName} must map to a callable binary export ${binding.binaryName}`);
    }

    const context = vm.createContext({
        console, WebAssembly, TextEncoder, TextDecoder, URL, setTimeout, clearTimeout,
        performance, crypto: globalThis.crypto,
        WorkerGlobalScope: function WorkerGlobalScope() {},
        location: { href: 'https://inventory.invalid/server.js' },
    });
    context.self = context;
    vm.runInContext(javaScript, context, { timeout: 5000 });
    assert.equal(typeof context.createZrLanguageServerModule, 'function',
        'generated JavaScript must expose the module factory');
    let instance;
    const module = await context.createZrLanguageServerModule({
        instantiateWasm(imports, receive) {
            instance = new WebAssembly.Instance(wasmModule, imports);
            receive(instance, wasmModule);
            return instance.exports;
        },
        print() {}, printErr() {},
    });
    assert.ok(instance, 'generated JavaScript must instantiate the supplied binary');
    for (const binding of bindings) {
        assert.equal(typeof module[binding.publicName], 'function',
            `${binding.publicName} must be callable on the public module`);
        if (!binding.wrapper) {
            assert.equal(module[binding.publicName], instance.exports[binding.binaryName],
                `${binding.publicName} must bind its declared binary export`);
        }
    }
    for (const helper of ['ccall', 'cwrap', 'UTF8ToString', 'stringToUTF8']) {
        assert.equal(typeof module[helper], 'function', `${helper} runtime helper is missing`);
    }
    const pointer = module.ccall('wasm_ZrLspContextNew', 'number', [], []);
    assert.ok(Number.isInteger(pointer) && pointer > 0, 'WASM context creation failed');
    module.ccall('wasm_ZrLspContextFree', null, ['number'], [pointer]);
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
    const exportedPublicNames = JSON.parse(exportListMatch[1].replace(/\\"/g, '"'));
    const exportedFunctions = exportedPublicNames.map(name => name.replace(/^_/, ''));
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
    const workerReport = await probeWorker(worker, bridge, runtimeExports,
        path.join(root, 'zr_vm_language_server_extension', 'src', 'browser', 'worker'));

    let linkedAssetChecked = false;
    if (wasmJavaScriptArg || wasmBinaryArg) {
        assert.ok(wasmJavaScriptArg && wasmBinaryArg, 'WASM asset check requires both JS and binary paths');
        assert.ok(fs.existsSync(wasmJavaScriptArg), `missing generated WASM JavaScript: ${wasmJavaScriptArg}`);
        await assertLinkedAssets(read(wasmJavaScriptArg), fs.readFileSync(wasmBinaryArg), exportedPublicNames);
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
