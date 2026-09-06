const assert = require('assert').strict;
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { probeWorker } = require('./lsp_wasm_worker_probe');

function windowsPath(filePath) {
    if (process.platform !== 'linux' || !path.isAbsolute(filePath)) return filePath;
    const result = spawnSync('wslpath', ['-w', filePath], { encoding: 'utf8' });
    assert.ifError(result.error);
    assert.equal(result.status, 0, 'wslpath failed for ' + filePath + ': ' + result.stderr);
    return result.stdout.trim();
}

function useCompatibleNode(args) {
    const major = Number(process.versions.node.split('.')[0]);
    if (major >= 14 || process.env.ZR_WASM_INVENTORY_COMPAT_NODE === '1') return false;
    const candidates = [
        '/mnt/c/nvm4w/nodejs/node.exe',
        '/mnt/c/Program Files/nodejs/node.exe',
    ];
    const nodePath = candidates.find(candidate => fs.existsSync(candidate));
    assert.ok(nodePath,
        'WASM worker wiring probe requires Node 14+; no Windows Node executable was found for the WSL runner');
    const child = spawnSync(nodePath, [windowsPath(__filename)].concat(args.map(windowsPath)), {
        encoding: 'utf8', timeout: 60000, maxBuffer: 16 * 1024 * 1024, windowsHide: true,
        env: Object.assign({}, process.env, { ZR_WASM_INVENTORY_COMPAT_NODE: '1' }),
    });
    assert.ifError(child.error);
    if (child.stdout) process.stdout.write(child.stdout);
    if (child.stderr) process.stderr.write(child.stderr);
    process.exitCode = child.status === null ? 1 : child.status;
    return true;
}

function read(filePath) {
    assert.ok(fs.existsSync(filePath), `missing inventory input: ${filePath}`);
    return fs.readFileSync(filePath, 'utf8');
}

function assertSetEqual(actual, expected, label) {
    assert.equal(new Set(actual).size, actual.length, `${label} has duplicates`);
    assert.deepEqual([...actual].sort(), [...expected].sort(), `${label} mismatch`);
}

async function main() {
    const args = process.argv.slice(2);
    if (useCompatibleNode(args)) return;
    const [repositoryRootArg, wasmJavaScriptArg, wasmBinaryArg] = args;
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
