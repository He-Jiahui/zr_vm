const assert = require('assert').strict;
const fs = require('fs');
const path = require('path');

const [repositoryRootArg, wasmJavaScriptArg, wasmBinaryArg] = process.argv.slice(2);
const repositoryRoot = path.resolve(repositoryRootArg || path.join(__dirname, '..', '..'));
const cmakePath = path.join(repositoryRoot, 'zr_vm_language_server', 'CMakeLists.txt');
const exportsSourcePath = path.join(repositoryRoot, 'zr_vm_language_server', 'wasm', 'wasm_exports.cpp');
const exportsHeaderPath = path.join(repositoryRoot, 'zr_vm_language_server', 'wasm', 'wasm_exports.h');
const bridgePath = path.join(repositoryRoot, 'zr_vm_language_server_extension', 'src', 'browser', 'worker', 'wasm-bridge.ts');
const workerPath = path.join(repositoryRoot, 'zr_vm_language_server_extension', 'src', 'browser', 'worker', 'server-worker.ts');

function read(filePath) {
    assert.ok(fs.existsSync(filePath), `missing inventory input: ${filePath}`);
    return fs.readFileSync(filePath, 'utf8');
}

function sorted(values) {
    return [...new Set(values)].sort();
}

function assertSetEqual(actual, expected, label) {
    assert.deepEqual(sorted(actual), sorted(expected), `${label} mismatch`);
}

const cmake = read(cmakePath);
const exportsSource = read(exportsSourcePath);
const exportsHeader = read(exportsHeaderPath);
const bridge = read(bridgePath);
const worker = read(workerPath);

const exportListMatch = cmake.match(/set\(EXPORTED_FUNCTIONS_JSON\s+"(\[[^\n]+\])"\)/);
assert.ok(exportListMatch, 'CMake export list is missing');
const exportedFunctions = JSON.parse(exportListMatch[1].replace(/\\"/g, '"'))
    .map((name) => name.replace(/^_/, ''));
const wasmDefinitions = [...exportsSource.matchAll(/(?:const\s+char\s*\*|void\s*\*|void|int)\s+(wasm_[A-Za-z0-9_]+)\s*\(/g)]
    .map((match) => match[1]);
const wasmDeclarations = [...exportsHeader.matchAll(/(?:const\s+char\s*\*|void\s*\*|void|int)\s+(wasm_[A-Za-z0-9_]+)\s*\(/g)]
    .map((match) => match[1]);
const bridgeCalls = [...bridge.matchAll(/['"](wasm_[A-Za-z0-9_]+)['"]/g)]
    .map((match) => match[1]);
const bridgeMethods = [...bridge.matchAll(/^\s*(?:async\s+)?([A-Za-z0-9_]+)\s*\(/gm)]
    .map((match) => match[1]);
const workerBridgeCalls = [...worker.matchAll(/bridge\.([A-Za-z0-9_]+)\s*\(/g)]
    .map((match) => match[1]);

const runtimeExports = exportedFunctions.filter((name) => name.startsWith('wasm_'));
assertSetEqual(wasmDefinitions, runtimeExports, 'C++ definitions and CMake exports');
assertSetEqual(wasmDeclarations, runtimeExports, 'C++ declarations and CMake exports');
assertSetEqual(bridgeCalls, runtimeExports.filter((name) => !['wasm_malloc', 'wasm_free'].includes(name)),
    'bridge ccall names and runtime exports');
for (const method of sorted(workerBridgeCalls)) {
    assert.ok(bridgeMethods.includes(method), `worker calls missing bridge method ${method}`);
}

const requiredWorkerRoutes = [
    ['connection.onCompletion(', 'getCompletion', 'wasm_ZrLspGetCompletion'],
    ['connection.onHover(', 'getHover', 'wasm_ZrLspGetHover'],
    ['connection.onDefinition(', 'getDefinition', 'wasm_ZrLspGetDefinition'],
    ['connection.onReferences(', 'findReferences', 'wasm_ZrLspFindReferences'],
    ['connection.onDocumentSymbol(', 'getDocumentSymbols', 'wasm_ZrLspGetDocumentSymbols'],
    ['connection.onWorkspaceSymbol(', 'getWorkspaceSymbols', 'wasm_ZrLspGetWorkspaceSymbols'],
    ['connection.onDocumentHighlight(', 'getDocumentHighlights', 'wasm_ZrLspGetDocumentHighlights'],
    ["connection.onRequest('textDocument/semanticTokens/full'", 'getSemanticTokens', 'wasm_ZrLspGetSemanticTokens'],
    ['connection.onPrepareRename(', 'prepareRename', 'wasm_ZrLspPrepareRename'],
    ['connection.onRenameRequest(', 'rename', 'wasm_ZrLspRename'],
    ["connection.onRequest('textDocument/formatting'", 'getFormatting', 'wasm_ZrLspGetFormatting'],
    ["connection.onRequest('textDocument/rangeFormatting'", 'getRangeFormatting', 'wasm_ZrLspGetRangeFormatting'],
    ["connection.onRequest('textDocument/codeAction'", 'getCodeActions', 'wasm_ZrLspGetCodeActions'],
    ["connection.onRequest('textDocument/foldingRange'", 'getFoldingRanges', 'wasm_ZrLspGetFoldingRanges'],
    ["connection.onRequest('textDocument/selectionRange'", 'getSelectionRange', 'wasm_ZrLspGetSelectionRange'],
    ["connection.onRequest('textDocument/documentLink'", 'getDocumentLinks', 'wasm_ZrLspGetDocumentLinks'],
    ["connection.onRequest('textDocument/codeLens'", 'getCodeLens', 'wasm_ZrLspGetCodeLens'],
    ["connection.onRequest('zr/richHover'", 'getRichHover', 'wasm_ZrLspGetRichHover'],
    ["connection.onRequest('zr/nativeDeclarationDocument'", 'getNativeDeclarationDocument', 'wasm_ZrLspGetNativeDeclarationDocument'],
    ["connection.onRequest('zr/projectModules'", 'getProjectModules', 'wasm_ZrLspGetProjectModules'],
    ["connection.onRequest('textDocument/diagnostic'", 'getDiagnosticReport', 'wasm_ZrLspGetDiagnosticReport'],
    ["connection.onRequest('workspace/diagnostic'", 'getWorkspaceDiagnosticReports', 'wasm_ZrLspGetWorkspaceDiagnosticReports'],
];
for (const [route, bridgeMethod, exportName] of requiredWorkerRoutes) {
    assert.ok(worker.includes(route), `worker route missing ${route}`);
    assert.ok(worker.includes(`bridge.${bridgeMethod}(`), `${route} does not call bridge.${bridgeMethod}`);
    assert.ok(runtimeExports.includes(exportName), `${route} points to unexported ${exportName}`);
}

const legend = worker.match(/const semanticTokenLegend[\s\S]*?tokenTypes:\s*\[([\s\S]*?)\][\s\S]*?tokenModifiers:\s*\[([\s\S]*?)\]/);
assert.ok(legend, 'worker semantic-token legend is missing');
for (const tokenType of ['namespace', 'class', 'struct', 'interface', 'enum', 'function', 'method',
    'property', 'variable', 'parameter', 'keyword', 'decorator', 'metaMethod']) {
    assert.match(legend[1], new RegExp(`['"]${tokenType}['"]`), `worker legend omits ${tokenType}`);
}
assert.match(legend[2], /['"]declaration['"]/, 'worker legend omits declaration modifier');
assert.match(worker, /semanticTokensProvider:\s*\{[\s\S]*?full:\s*true/, 'worker semantic full capability missing');
assert.doesNotMatch(worker, /semanticTokensProvider:\s*\{[\s\S]*?delta:\s*true/, 'worker overclaims semantic delta');
assert.doesNotMatch(worker, /semanticTokensProvider:\s*\{[\s\S]*?range:\s*true/, 'worker overclaims semantic range');

const assetReport = { linkedAssetChecked: false };
if (wasmJavaScriptArg || wasmBinaryArg) {
    assert.ok(wasmJavaScriptArg && wasmBinaryArg, 'WASM asset check requires both JS and binary paths');
    const wasmJavaScriptPath = path.resolve(wasmJavaScriptArg);
    const wasmBinaryPath = path.resolve(wasmBinaryArg);
    assert.ok(fs.existsSync(wasmJavaScriptPath), `missing generated WASM JavaScript: ${wasmJavaScriptPath}`);
    assert.ok(fs.existsSync(wasmBinaryPath), `missing generated WASM binary: ${wasmBinaryPath}`);
    const module = new WebAssembly.Module(fs.readFileSync(wasmBinaryPath));
    const linkedExports = WebAssembly.Module.exports(module).map((entry) => entry.name.replace(/^_/, ''));
    assertSetEqual(linkedExports.filter((name) => name.startsWith('wasm_')), runtimeExports,
        'linked WASM exports and CMake exports');
    assetReport.linkedAssetChecked = true;
    assetReport.wasmJavaScript = wasmJavaScriptPath;
    assetReport.wasmBinary = wasmBinaryPath;
}

console.log(JSON.stringify({
    schemaVersion: 1,
    status: assetReport.linkedAssetChecked ? 'wasm-linked-contract-mapped' : 'wasm-static-contract-mapped',
    runtimeExports: runtimeExports.length,
    bridgeCalls: bridgeCalls.length,
    workerRoutes: requiredWorkerRoutes.length,
    semanticTokenTypes: 13,
    semanticTokenModifiers: ['declaration'],
    linkedAssetChecked: assetReport.linkedAssetChecked,
}, null, 2));
