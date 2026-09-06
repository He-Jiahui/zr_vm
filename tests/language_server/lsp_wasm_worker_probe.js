const assert = require('assert').strict;
const path = require('path');
const vm = require('vm');
const { TextEncoder } = require('util');

const REQUESTS = [
    ['textDocument/completion', 'completionProvider', 'wasm_ZrLspGetCompletion'],
    ['textDocument/hover', 'hoverProvider', 'wasm_ZrLspGetHover'],
    ['textDocument/definition', 'definitionProvider', 'wasm_ZrLspGetDefinition'],
    ['textDocument/references', 'referencesProvider', 'wasm_ZrLspFindReferences'],
    ['textDocument/documentSymbol', 'documentSymbolProvider', 'wasm_ZrLspGetDocumentSymbols'],
    ['workspace/symbol', 'workspaceSymbolProvider', 'wasm_ZrLspGetWorkspaceSymbols'],
    ['textDocument/documentHighlight', 'documentHighlightProvider', 'wasm_ZrLspGetDocumentHighlights'],
    ['textDocument/inlayHint', 'inlayHintProvider', 'wasm_ZrLspGetInlayHints'],
    ['textDocument/semanticTokens/full', 'semanticTokensProvider', 'wasm_ZrLspGetSemanticTokens'],
    ['textDocument/prepareRename', 'renameProvider', 'wasm_ZrLspPrepareRename'],
    ['textDocument/rename', 'renameProvider', 'wasm_ZrLspRename'],
    ['textDocument/formatting', 'documentFormattingProvider', 'wasm_ZrLspGetFormatting'],
    ['textDocument/rangeFormatting', 'documentRangeFormattingProvider', 'wasm_ZrLspGetRangeFormatting'],
    ['textDocument/codeAction', 'codeActionProvider', 'wasm_ZrLspGetCodeActions'],
    ['textDocument/foldingRange', 'foldingRangeProvider', 'wasm_ZrLspGetFoldingRanges'],
    ['textDocument/selectionRange', 'selectionRangeProvider', 'wasm_ZrLspGetSelectionRange'],
    ['textDocument/documentLink', 'documentLinkProvider', 'wasm_ZrLspGetDocumentLinks'],
    ['textDocument/codeLens', 'codeLensProvider', 'wasm_ZrLspGetCodeLens'],
    ['textDocument/diagnostic', 'diagnosticProvider', 'wasm_ZrLspGetDiagnosticReport'],
    ['workspace/diagnostic', 'diagnosticProvider', 'wasm_ZrLspGetWorkspaceDiagnosticReports'],
    ['zr/richHover', null, 'wasm_ZrLspGetRichHover'],
    ['zr/nativeDeclarationDocument', null, 'wasm_ZrLspGetNativeDeclarationDocument'],
    ['zr/projectModules', null, 'wasm_ZrLspGetProjectModules'],
];

const EVENTS = {
    onInitialize: 'initialize', onInitialized: 'initialized', onShutdown: 'shutdown',
    onDidOpenTextDocument: 'textDocument/didOpen', onDidChangeTextDocument: 'textDocument/didChange',
    onDidCloseTextDocument: 'textDocument/didClose', onDidSaveTextDocument: 'textDocument/didSave',
    onCompletion: 'textDocument/completion', onHover: 'textDocument/hover',
    onDefinition: 'textDocument/definition', onReferences: 'textDocument/references',
    onDocumentSymbol: 'textDocument/documentSymbol', onWorkspaceSymbol: 'workspace/symbol',
    onDocumentHighlight: 'textDocument/documentHighlight', onPrepareRename: 'textDocument/prepareRename',
    onRenameRequest: 'textDocument/rename',
};
const CONTROLS = ['initialize', 'initialized', 'shutdown', 'exit'];
const DOCUMENTS = ['textDocument/didOpen', 'textDocument/didChange', 'textDocument/didClose', 'textDocument/didSave'];
const TOKEN_TYPES = ['namespace', 'class', 'struct', 'interface', 'enum', 'function', 'method',
    'property', 'variable', 'parameter', 'keyword', 'decorator', 'metaMethod'];

async function probeWorker(workerSource, bridgeSource, runtimeExports) {
    // Execute production adapters; only the browser connection and WASM ABI are test doubles.
    const ts = require(path.join(__dirname, '..', '..', 'zr_vm_language_server_extension', 'node_modules', 'typescript'));
    const { ResponseError, ErrorCodes } = require(path.join(__dirname, '..', '..',
        'zr_vm_language_server_extension', 'node_modules', 'vscode-languageserver', 'browser'));
    const handlers = new Map();
    const calls = [];
    const responses = new Map();
    const logs = [];
    let nextPointer = 1;
    let closed = false;
    let responseFixture;
    const register = (method, handler) => {
        assert.equal(typeof method, 'string', 'worker route must have a protocol method');
        assert.equal(handlers.has(method), false, 'duplicate worker route ' + method);
        handlers.set(method, handler);
    };
    const connection = {
        onRequest: register, onNotification: register, listen() {}, sendDiagnostics() {},
        console: { warn: message => logs.push(message) },
    };
    for (const [event, method] of Object.entries(EVENTS)) connection[event] = handler => register(method, handler);
    const mockModule = {
        ccall(name) {
            assert.ok(runtimeExports.includes(name), 'worker calls unexported WASM function ' + name);
            calls.push(name);
            if (name === 'wasm_ZrLspContextNew') return 1;
            if (name === 'wasm_ZrLspContextFree') return 0;
            if (responseFixture && responseFixture.nullPointer) return 0;
            const data = name === 'wasm_ZrLspGetDiagnosticReport' ? { resultId: 'probe', items: [] } : [];
            const pointer = ++nextPointer;
            responses.set(pointer, responseFixture ? responseFixture.raw : JSON.stringify({ success: true, data }));
            return pointer;
        },
        UTF8ToString(pointer) {
            assert.ok(responses.has(pointer), 'bridge reads an unknown response pointer');
            if (responseFixture && responseFixture.decodeFailure) throw new Error('injected decode failure');
            return responses.get(pointer);
        },
        _free(pointer) {
            assert.equal(responses.delete(pointer), true, 'bridge frees an unknown response pointer');
        },
    };
    const workerSelf = {
        location: { href: 'https://inventory.test/server/worker.js' },
        addEventListener() {}, close() { closed = true; },
        importScripts() { this.createZrLanguageServerModule = async () => mockModule; },
    };
    function execute(source, filename, requireModule) {
        const compiled = ts.transpileModule(source, {
            compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2018 },
            fileName: filename, reportDiagnostics: true,
        });
        assert.deepEqual(compiled.diagnostics, [], filename + ' must transpile');
        const exports = {};
        vm.runInNewContext(compiled.outputText, {
            exports, require: requireModule, self: workerSelf, URL, TextEncoder,
            console: { error: (...args) => logs.push(args.join(' ')) },
        }, { filename, timeout: 5000 });
        return exports;
    }
    const bridge = execute(bridgeSource, 'wasm-bridge.ts', name => {
        assert.equal(name, 'vscode-languageserver/browser', 'unexpected bridge import');
        return { ResponseError, ErrorCodes };
    });
    execute(workerSource, 'server-worker.ts', name => {
        if (name === './wasm-bridge') return bridge;
        if (name === 'vscode-jsonrpc') {
            return { ResponseError, ErrorCodes };
        }
        assert.equal(name, 'vscode-languageserver/browser', 'unexpected worker import');
        return {
            BrowserMessageReader: class {}, BrowserMessageWriter: class {},
            createConnection: () => connection, TextDocumentSyncKind: { Incremental: 2 },
            ResponseError, ErrorCodes,
        };
    });
    assert.deepEqual([...handlers.keys()].sort(),
        REQUESTS.map(row => row[0]).concat(CONTROLS, DOCUMENTS).sort(), 'worker route set mismatch');
    async function invoke(method, params, expectedExports) {
        const start = calls.length;
        const result = await handlers.get(method)(params);
        const exportNames = calls.slice(start);
        assert.deepEqual(exportNames, expectedExports, 'worker export route mismatch for ' + method);
        assert.equal(responses.size, 0, 'bridge must release response pointers for ' + method);
        return { result, route: { method, exportNames } };
    }
    const initialized = await invoke('initialize', {
        capabilities: {}, initializationOptions: { serverBaseUrl: 'https://inventory.test/server/' },
    }, ['wasm_ZrLspContextNew']);
    const capabilities = JSON.parse(JSON.stringify(initialized.result.capabilities));
    const expectedKeys = [...new Set(REQUESTS.map(row => row[1]).filter(Boolean).concat('textDocumentSync'))].sort();
    assert.deepEqual(Object.keys(capabilities).sort(), expectedKeys, 'worker capability set mismatch');
    for (const key of expectedKeys.filter(key => key !== 'textDocumentSync')) {
        const provider = capabilities[key];
        assert.ok(provider === true || (provider && typeof provider === 'object' && !Array.isArray(provider)),
            'missing worker provider ' + key);
        assert.notEqual(provider.resolveProvider, true, 'worker must not publish unimplemented resolve ' + key);
    }
    assert.equal(capabilities.textDocumentSync, 2, 'worker document synchronization mismatch');
    assert.equal(capabilities.renameProvider.prepareProvider, true);
    assert.equal(capabilities.diagnosticProvider.workspaceDiagnostics, true);
    assert.deepEqual(capabilities.semanticTokensProvider, {
        legend: { tokenTypes: TOKEN_TYPES, tokenModifiers: ['declaration'] }, full: true,
    }, 'worker semantic-token legend or full/range/delta capability mismatch');
    await invoke('initialized', {}, []);
    const uri = 'file:///inventory/main.zr';
    const textDocument = { uri, languageId: 'zr', version: 1, text: 'var seed: int = 1;\n' };
    const updateExports = ['wasm_ZrLspUpdateDocument', 'wasm_ZrLspGetDiagnosticReport'];
    const documentRoutes = [(await invoke('textDocument/didOpen', { textDocument }, updateExports)).route];
    documentRoutes.push((await invoke('textDocument/didChange', {
        textDocument: { uri, version: 2 }, contentChanges: [{ text: 'var seed: int = 2;\n' }],
    }, updateExports)).route);
    const position = { line: 0, character: 4 };
    const params = { textDocument: { uri }, position, positions: [position],
        range: { start: position, end: { line: 0, character: 8 } },
        context: { includeDeclaration: true }, newName: 'next', query: 'seed', uri, line: 0, character: 4 };
    const featureRoutes = [];
    for (const [method, capabilityKey, exportName] of REQUESTS) {
        const { route } = await invoke(method, params, [exportName]);
        featureRoutes.push(Object.assign({ capabilityKey }, route));
    }
    const errorFixtures = [-32602, -32603, -32800, -32801].map(code => ({
        label: 'structured error ' + code, code, message: 'same message for every code',
        data: { reason: 'fixture', generation: 7 },
        raw: JSON.stringify({ success: false, code, error: 'same message for every code',
            data: { reason: 'fixture', generation: 7 } }),
    })).concat([
        { label: 'success without data', raw: '{"success":true}' },
        { label: 'non-boolean success', raw: '{"success":"true","data":[]}' },
        { label: 'success with error', raw: '{"success":true,"data":[],"error":"failure"}' },
        { label: 'invalid JSON', raw: '{' },
        { label: 'null pointer', nullPointer: true },
        { label: 'decode exception', raw: '', decodeFailure: true },
    ]);
    for (const fixture of errorFixtures) {
        responseFixture = fixture;
        for (const [method] of REQUESTS) {
            await assert.rejects(handlers.get(method)(params), error => {
                assert.ok(error instanceof ResponseError, method + ': must use the connection ResponseError class');
                assert.equal(error.code, fixture.code || ErrorCodes.InternalError, method + ': ' + fixture.label);
                if (fixture.message) assert.equal(error.message, fixture.message);
                if (fixture.data) assert.deepEqual(JSON.parse(JSON.stringify(error.data)), fixture.data);
                return true;
            }, method + ': ' + fixture.label + ' must not become a success result');
            assert.equal(responses.size, 0, method + ': release response even when decoding fails');
        }
    }
    responseFixture = undefined;
    documentRoutes.push((await invoke('textDocument/didSave', { textDocument: { uri }, text: textDocument.text }, updateExports)).route);
    documentRoutes.push((await invoke('textDocument/didClose', { textDocument: { uri } }, ['wasm_ZrLspCloseDocument'])).route);
    const shutdown = await invoke('shutdown', undefined, ['wasm_ZrLspContextFree']);
    await invoke('exit', undefined, []);
    assert.equal(closed, true, 'worker exit must close its host');
    assert.deepEqual(logs, [], 'worker emitted errors during wiring probe');
    return { capabilities, featureRoutes, documentRoutes,
        controlRoutes: [initialized.route, shutdown.route], mockedWasm: true, workerAssetLoaded: false };
}

module.exports = { probeWorker };
