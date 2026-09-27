const test = require('node:test');
const assert = require('node:assert/strict');
const { loadWorker } = require('./helpers/workerHost');

// Web 当前不宣告项目级请求；能力宣告与未注册的请求路由必须同时收缩。
test('Web advertises document diagnostics and omits project indexing routes', async () => {
    const worker = loadWorker();
    const result = await worker.handlers.get('onInitialize')({ capabilities: {} });
    assert.equal(result.capabilities.workspaceSymbolProvider, undefined);
    assert.equal(result.capabilities.diagnosticProvider.workspaceDiagnostics, false);
    assert.equal(result.capabilities.diagnosticProvider.interFileDependencies, false);
    assert.equal(worker.handlers.has('onWorkspaceSymbol'), false);
    assert.equal(worker.requests.has('workspace/diagnostic'), false);
    assert.equal(worker.requests.has('zr/projectModules'), false);
    assert.equal(worker.requests.has('zr/nativeDeclarationDocument'), true);
});

// 失败封装的错误码和数据必须穿过 worker，否则客户端会误认为返回了空功能结果。
test('browser worker propagates WASM error envelopes as JSON-RPC ResponseError', async () => {
    const uri = 'file:///workspace/main.zr';
    const position = { line: 0, character: 0 };
    const cases = [
        {
            handler: 'onCompletion',
            bridgeMethod: 'getCompletion',
            params: { textDocument: { uri }, position },
            code: -32602,
            message: 'Invalid parameters',
        },
        {
            handler: 'onHover',
            bridgeMethod: 'getHover',
            params: { textDocument: { uri }, position },
            code: -32800,
            message: 'Request cancelled',
        },
        {
            handler: 'onDefinition',
            bridgeMethod: 'getDefinition',
            params: { textDocument: { uri }, position },
            code: -32801,
            message: 'Content modified',
        },
    ];

    for (const fixture of cases) {
        const data = {
            success: false,
            code: fixture.code,
            error: fixture.message,
            data: { reason: fixture.message },
        };
        const worker = loadWorker({ [fixture.bridgeMethod]: data });
        await worker.handlers.get('onDidOpenTextDocument')({ textDocument: { uri, version: 1, text: 'var seed: int = 1;' } });
        await assert.rejects(
            worker.handlers.get(fixture.handler)(fixture.params),
            (error) => {
                assert.equal(error.code, fixture.code);
                assert.equal(error.message, fixture.message);
                assert.deepEqual(error.data, { reason: fixture.message });
                return true;
            },
        );
    }
});

// 基础响应由 WASM 一次给齐；Web 端不承诺尚未实现的 resolve 往返。
for (const name of [
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

// 定义导航仍可用，而无 WASM 映射的别名路由不能被客户端发现。
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

// WASM 的数字 token 流依赖同一类别顺序；Web 只实现全量请求。
// TODO: 此测试仅与本文件中的硬编码列表对照；需增加读取 C token registry
// 的契约核查，防止原生类别顺序变化而本测试仍通过。
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

// 客户端不同动态注册能力下，未实现的颜色请求都不得被宣告。
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

// 以真实 handler 的基础响应验证链接、lens、hint、符号和动作均可直接消费。
test('browser base requests return complete initial payloads without resolve', async () => {
    const uri = 'file:///workspace/main.zr';
    const range = { start: { line: 0, character: 0 }, end: { line: 0, character: 4 } };
    const link = { range, target: 'file:///workspace/module.zr' };
    const lens = { range, command: { title: '1 reference', command: 'zr.showReferences', arguments: [uri] } };
    const unavailableLens = { range, command: { title: 'Run', command: 'zr.runCurrentProject', arguments: [uri] } };
    const hint = { position: { line: 0, character: 4 }, label: ': int', kind: 1 };
    const symbol = { name: 'main', kind: 12, location: { uri, range } };
    const action = {
        title: 'Organize Imports',
        kind: 'source.organizeImports',
        edit: { documentChanges: [{ textDocument: { uri, version: 1 }, edits: [{ range, newText: '' }] }] },
    };
    const worker = loadWorker({
        getDocumentLinks: [link],
        getCodeLens: [lens, unavailableLens],
        getInlayHints: [hint],
        getDocumentSymbols: [symbol],
        getCodeActions: [action],
    });
    await worker.handlers.get('onDidOpenTextDocument')({ textDocument: { uri, version: 1, text: 'var seed: int = 1;' } });
    worker.bridgeCalls.length = 0;
    const params = { textDocument: { uri }, range };
    assert.deepEqual(await worker.requests.get('textDocument/documentLink')(params), [link]);
    assert.deepEqual(await worker.requests.get('textDocument/codeLens')(params), [lens]);
    assert.deepEqual(await worker.requests.get('textDocument/inlayHint')(params), [hint]);
    assert.deepEqual(await worker.handlers.get('onDocumentSymbol')(params), [symbol]);
    assert.deepEqual(await worker.requests.get('textDocument/codeAction')(params), [action]);
    assert.deepEqual(worker.bridgeCalls, [
        ['getDocumentLinks', uri],
        ['getCodeLens', uri],
        ['getInlayHints', uri, 0, 0, 0, 4],
        ['getDocumentSymbols', uri],
        ['getCodeActions', uri, 0, 0, 0, 4],
    ]);
});
