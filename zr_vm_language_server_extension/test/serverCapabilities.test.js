const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const ts = require('typescript');

const workerPath = path.join(__dirname, '..', 'src', 'browser', 'worker', 'server-worker.ts');
const workerJavaScript = ts.transpileModule(fs.readFileSync(workerPath, 'utf8'), {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
    fileName: workerPath,
}).outputText;

function loadWorker(bridgeResponses = {}) {
    const handlers = new Map();
    const requests = new Map();
    const bridgeCalls = [];
    const connection = {
        onRequest: (method, handler) => requests.set(method, handler),
        onNotification: () => {},
        listen: () => {},
        console: { warn: () => {} },
    };
    for (const event of [
        'onInitialize', 'onInitialized', 'onShutdown',
        'onDidOpenTextDocument', 'onDidChangeTextDocument',
        'onDidCloseTextDocument', 'onDidSaveTextDocument',
        'onCompletion', 'onHover', 'onDefinition', 'onReferences',
        'onDocumentSymbol', 'onWorkspaceSymbol', 'onDocumentHighlight',
        'onPrepareRename', 'onRenameRequest',
    ]) {
        connection[event] = (handler) => handlers.set(event, handler);
    }
    class TestBridge {
        async initialize(baseUrl) {
            bridgeCalls.push(['initialize', baseUrl]);
        }
    }
    for (const [method, data] of Object.entries(bridgeResponses)) {
        TestBridge.prototype[method] = async (...args) => {
            bridgeCalls.push([method, ...args]);
            return { success: true, data };
        };
    }

    vm.runInNewContext(workerJavaScript, {
        exports: {},
        require: (name) => {
            if (name === 'vscode-languageserver/browser') {
                return {
                    BrowserMessageReader: class {},
                    BrowserMessageWriter: class {},
                    createConnection: () => connection,
                    TextDocumentSyncKind: { Incremental: 2 },
                };
            }
            assert.equal(name, './wasm-bridge');
            return { ZrWasmBridge: TestBridge };
        },
        self: { addEventListener: () => {} },
        console,
    }, { filename: workerPath });
    return { handlers, requests, bridgeCalls };
}

for (const name of [
    'workspaceSymbolProvider',
    'inlayHintProvider',
    'documentLinkProvider',
    'codeLensProvider',
    'codeActionProvider',
]) {
    test(`browser initialize keeps ${name} without identity resolve`, async () => {
        const worker = loadWorker();
        const result = await worker.handlers.get('onInitialize')({
            capabilities: {},
            initializationOptions: { serverBaseUrl: 'https://example.test/server/' },
        });
        const provider = result.capabilities[name];
        assert.ok(provider === true || (provider !== null && typeof provider === 'object'));
        assert.notEqual(provider.resolveProvider, true);
        assert.deepEqual(worker.bridgeCalls, [['initialize', 'https://example.test/server/']]);
    });
}

test('browser worker does not register withdrawn identity resolve handlers', () => {
    const worker = loadWorker();
    for (const method of [
        'workspaceSymbol/resolve', 'inlayHint/resolve',
        'documentLink/resolve', 'codeLens/resolve', 'codeAction/resolve',
    ]) {
        assert.equal(worker.requests.has(method), false, method);
    }
});

test('browser navigation aliases are neither advertised nor registered', async () => {
    const worker = loadWorker();
    const result = await worker.handlers.get('onInitialize')({ capabilities: {} });
    for (const name of ['declarationProvider', 'typeDefinitionProvider']) {
        assert.equal(result.capabilities[name], undefined, name);
    }
    for (const method of ['textDocument/declaration', 'textDocument/typeDefinition']) {
        assert.equal(worker.requests.has(method), false, method);
    }
    assert.equal(result.capabilities.definitionProvider, true);
    assert.equal(worker.handlers.has('onDefinition'), true);
});

test('browser semantic-token legend matches the native registry', async () => {
    const worker = loadWorker();
    const result = await worker.handlers.get('onInitialize')({ capabilities: {} });
    const provider = result.capabilities.semanticTokensProvider;
    assert.deepEqual(Array.from(provider.legend.tokenTypes), [
        'namespace', 'class', 'struct', 'interface', 'enum', 'function',
        'method', 'property', 'variable', 'parameter', 'keyword',
        'decorator', 'metaMethod',
    ]);
    assert.deepEqual(Array.from(provider.legend.tokenModifiers), ['declaration']);
    assert.equal(provider.full, true);
    assert.equal(Object.prototype.hasOwnProperty.call(provider, 'range'), false);
    assert.equal(worker.requests.has('textDocument/semanticTokens/full'), true);
    assert.equal(worker.requests.has('textDocument/semanticTokens/full/delta'), false);
    assert.equal(worker.requests.has('textDocument/semanticTokens/range'), false);
});

test('browser color scanning is neither advertised nor registered', async () => {
    for (const capabilities of [{}, { textDocument: { colorProvider: { dynamicRegistration: false } } }]) {
        const worker = loadWorker();
        const result = await worker.handlers.get('onInitialize')({ capabilities });
        assert.equal(Object.prototype.hasOwnProperty.call(result.capabilities, 'colorProvider'), false);
        assert.equal(worker.requests.has('textDocument/documentColor'), false);
        assert.equal(worker.requests.has('textDocument/colorPresentation'), false);
        assert.equal(result.capabilities.hoverProvider, true);
        assert.equal(worker.handlers.has('onHover'), true);
        assert.equal(result.capabilities.definitionProvider, true);
        assert.equal(worker.handlers.has('onDefinition'), true);
    }
});

test('browser base requests return complete initial payloads without resolve', async () => {
    const uri = 'file:///workspace/main.zr';
    const range = { start: { line: 0, character: 0 }, end: { line: 0, character: 4 } };
    const link = { range, target: 'file:///workspace/module.zr' };
    const lens = { range, command: { title: 'Run', command: 'zr.runCurrentProject', arguments: [uri] } };
    const hint = { position: { line: 0, character: 4 }, label: ': int', kind: 1 };
    const symbol = { name: 'main', kind: 12, location: { uri, range } };
    const action = {
        title: 'Organize Imports',
        kind: 'source.organizeImports',
        edit: { documentChanges: [{ textDocument: { uri, version: 1 }, edits: [{ range, newText: '' }] }] },
    };
    const worker = loadWorker({
        getDocumentLinks: [link],
        getCodeLens: [lens],
        getInlayHints: [hint],
        getWorkspaceSymbols: [symbol],
        getCodeActions: [action],
    });
    const params = { textDocument: { uri }, range };
    assert.deepEqual(await worker.requests.get('textDocument/documentLink')(params), [link]);
    assert.deepEqual(await worker.requests.get('textDocument/codeLens')(params), [lens]);
    assert.deepEqual(await worker.requests.get('textDocument/inlayHint')(params), [hint]);
    assert.deepEqual(await worker.handlers.get('onWorkspaceSymbol')({ query: 'main' }), [symbol]);
    assert.deepEqual(await worker.requests.get('textDocument/codeAction')(params), [action]);
    assert.deepEqual(worker.bridgeCalls, [
        ['getDocumentLinks', uri],
        ['getCodeLens', uri],
        ['getInlayHints', uri, 0, 0, 0, 4],
        ['getWorkspaceSymbols', 'main'],
        ['getCodeActions', uri, 0, 0, 0, 4],
    ]);
});
