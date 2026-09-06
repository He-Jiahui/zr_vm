const { StdioProtocolClient, encodeFrame } = require('./stdio_protocol_client');

const RESPONSE_TIMEOUT_MS = 3000;
const NO_RESPONSE_TIMEOUT_MS = 150;
const EXPECTED_CAPABILITY_KEYS = [
    'callHierarchyProvider', 'codeActionProvider', 'codeLensProvider',
    'completionProvider', 'definitionProvider', 'diagnosticProvider',
    'documentFormattingProvider', 'documentHighlightProvider', 'documentLinkProvider',
    'documentOnTypeFormattingProvider', 'documentRangeFormattingProvider',
    'documentSymbolProvider', 'foldingRangeProvider', 'hoverProvider', 'implementationProvider',
    'inlayHintProvider', 'inlineValueProvider',
    'linkedEditingRangeProvider', 'monikerProvider', 'positionEncoding', 'referencesProvider',
    'renameProvider', 'selectionRangeProvider', 'semanticTokensProvider', 'signatureHelpProvider',
    'textDocumentSync', 'typeHierarchyProvider', 'workspace',
    'workspaceSymbolProvider',
].sort();

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertResponseEnvelope(response, id, member, label) {
    assert(response && typeof response === 'object' && !Array.isArray(response),
           `${label}: response envelope must be an object`);
    assert(response.jsonrpc === '2.0', `${label}: response must have jsonrpc=2.0`);
    assert(Object.prototype.hasOwnProperty.call(response, 'id') && response.id === id,
           `${label}: response envelope id must exactly match ${String(id)}`);
    assert(JSON.stringify(Object.keys(response).sort()) ===
           JSON.stringify(['id', 'jsonrpc', member].sort()),
           `${label}: response envelope must contain only jsonrpc, id and ${member}`);
}

function assertSuccessEnvelope(response, id, label) {
    assertResponseEnvelope(response, id, 'result', label);
}

function assertErrorEnvelope(response, id, code, label) {
    assertResponseEnvelope(response, id, 'error', label);
    assert(response.error && typeof response.error === 'object' && !Array.isArray(response.error) &&
           response.error.code === code,
           `${label}: expected error ${code}, actual=${JSON.stringify(response)}`);
    assert(typeof response.error.message === 'string', `${label}: error message must be a string`);
}

function initializePayload(id) {
    return {
        jsonrpc: '2.0',
        id,
        method: 'initialize',
        params: {
            processId: null,
            rootUri: null,
            capabilities: {},
        },
    };
}

function encodeRawJsonFrame(payload) {
    const body = Buffer.from(payload, 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

async function initialize(client, id = 'initialize') {
    const response = await client.requestEnvelope(initializePayload(id), RESPONSE_TIMEOUT_MS);
    assertSuccessEnvelope(response, id, 'initialize');
    assert(response && response.jsonrpc === '2.0' && response.id === id && response.result,
           `initialize must return a JSON-RPC success envelope, actual=${JSON.stringify(response)}`);
    return response.result;
}

async function expectInvalidRequest(client, payload, id, label) {
    client.sendPayload(payload);
    const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
    assertErrorEnvelope(response, id, -32600, label);
}

async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    try {
        return await run(client);
    } finally {
        await client.terminate().catch(() => {});
    }
}

async function testCapabilityMatrix(serverPath) {
    await withClient(serverPath, async (client) => {
        const result = await initialize(client, 'matrix');
        assert(result.capabilities && typeof result.capabilities === 'object',
               'initialize must return a capabilities object');
        const keys = Object.keys(result.capabilities).sort();
        assert(JSON.stringify(keys) === JSON.stringify(EXPECTED_CAPABILITY_KEYS),
               `LSP 3.17 capability matrix changed: ${JSON.stringify(keys)}`);
    });
}

async function testRequestBeforeInitialize(serverPath) {
    await withClient(serverPath, async (client) => {
        const response = await client.request('textDocument/hover', {
            textDocument: { uri: 'file:///before-initialize.zr' },
            position: { line: 0, character: 0 },
        }, 'before-initialize', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'before-initialize', -32002, 'request before initialize');
    });
}

async function testNotificationBeforeInitializeIsIgnored(serverPath) {
    await withClient(serverPath, async (client) => {
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///ignored-before-initialize.zr',
                languageId: 'zr',
                version: 1,
                text: 'class IgnoredBeforeInitialize { }',
            },
        });
        await initialize(client, 'notification-before-initialize');
        client.notify('initialized', {});

        const response = await client.request('workspace/symbol', {
            query: 'IgnoredBeforeInitialize',
        }, 'ignored-before-initialize', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(response, 'ignored-before-initialize', 'ignored notification');
        assert(response && Array.isArray(response.result) && response.result.length === 0,
               `notification before initialize must be ignored, actual=${JSON.stringify(response)}`);
    });
}

async function testRepeatedInitialize(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'first-initialize');
        const response = await client.requestEnvelope(
            initializePayload('second-initialize'), RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'second-initialize', -32600, 'repeated initialize');
    });
}

async function testExitBeforeShutdown(serverPath) {
    await withClient(serverPath, async (client) => {
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode === 1, `exit before shutdown must exit 1, actual=${exitCode}`);
    });
}

async function testShutdownExitOrdering(serverPath) {
    await withClient(serverPath, async (client) => {
        const beforeInitialize = await client.request('shutdown', undefined,
                                                       'shutdown-before-initialize');
        assertErrorEnvelope(beforeInitialize, 'shutdown-before-initialize', -32002,
                            'shutdown before initialize');
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode === 1, `exit before successful shutdown must exit 1, actual=${exitCode}`);
    });

    await withClient(serverPath, async (client) => {
        await initialize(client, 'shutdown-exit-initialize');
        const shutdown = await client.request('shutdown', undefined, 'shutdown-exit-shutdown');
        assertSuccessEnvelope(shutdown, 'shutdown-exit-shutdown', 'shutdown before exit');
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode === 0, `exit after successful shutdown must exit 0, actual=${exitCode}`);
    });
}

async function testRequestAfterShutdown(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'shutdown-initialize');
        const shutdown = await client.request('shutdown', undefined, 'shutdown', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(shutdown, 'shutdown', 'shutdown');
        assert(shutdown && shutdown.result === null,
               `shutdown must return a null result envelope, actual=${JSON.stringify(shutdown)}`);
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///ignored-after-shutdown.zr',
                languageId: 'zr',
                version: 1,
                text: 'class IgnoredAfterShutdown { }',
            },
        });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        const response = await client.request('workspace/symbol', { query: '' }, 'after-shutdown', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'after-shutdown', -32600, 'request after shutdown');
    });
}

async function testMissingJsonRpc(serverPath) {
    await withClient(serverPath, async (client) => {
        const response = await client.requestEnvelope({
            id: 'missing-jsonrpc',
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'missing-jsonrpc', -32600, 'missing jsonrpc');
    });
}

async function testWrongJsonRpc(serverPath) {
    await withClient(serverPath, async (client) => {
        const response = await client.requestEnvelope({
            jsonrpc: '1.0',
            id: 'wrong-jsonrpc',
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'wrong-jsonrpc', -32600, 'wrong jsonrpc');
    });
}

async function testInvalidBooleanId(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, {
            jsonrpc: '2.0',
            id: true,
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, null, 'boolean request id');
    });
}

async function testInvalidStructuredIds(serverPath) {
    const invalidIds = [
        ['object request id', {}],
        ['array request id', []],
    ];

    for (const [label, id] of invalidIds) {
        await withClient(serverPath, async (client) => {
            await expectInvalidRequest(client, {
                jsonrpc: '2.0',
                id,
                method: 'initialize',
                params: initializePayload('ignored').params,
            }, null, label);
        });
    }
}

async function testInvalidFractionalRequestId(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, {
            jsonrpc: '2.0',
            id: 1.5,
            method: 'initialize',
            params: initializePayload('ignored').params,
        }, null, 'fractional request id');
    });
}

async function testInvalidTopLevelMessages(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, [], null, 'array top-level message');
        await expectInvalidRequest(client, 17, null, 'scalar top-level message');
    });
}

async function testInvalidParams(serverPath) {
    const invalidParams = [
        ['missing params', undefined],
        ['scalar params', 'not-an-object'],
        ['null params', null],
        ['array params', []],
    ];

    for (const [label, params] of invalidParams) {
        await withClient(serverPath, async (client) => {
            const response = await client.requestEnvelope({
                jsonrpc: '2.0',
                id: `invalid-params-${label}`,
                method: 'initialize',
                params,
            }, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, `invalid-params-${label}`, -32602, label);
        });
    }
}

async function testInvalidPositionAndRangeNumbers(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///invalid-position.zr';
        const invalidPositions = [
            ['fractional line', { line: 0.5, character: 0 }],
            ['negative character', { line: 0, character: -1 }],
            ['overflow line', { line: 2147483648, character: 0 }],
        ];

        await initialize(client, 'invalid-position-initialize');
        for (const [label, position] of invalidPositions) {
            const id = `invalid-position-${label}`;
            const response = await client.request('textDocument/hover', {
                textDocument: { uri },
                position,
            }, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }

        const reverseRange = await client.request('textDocument/rangeFormatting', {
            textDocument: { uri },
            range: {
                start: { line: 1, character: 0 },
                end: { line: 0, character: 0 },
            },
            options: {},
        }, 'reverse-range', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(reverseRange, 'reverse-range', -32602, 'reverse range');
    });
}

async function testInvalidHierarchyParams(serverPath) {
    const cases = [
        ['call hierarchy prepare', 'textDocument/prepareCallHierarchy', {}],
        ['call hierarchy incoming', 'callHierarchy/incomingCalls', { item: {} }],
        ['call hierarchy outgoing', 'callHierarchy/outgoingCalls', { item: {} }],
        ['type hierarchy prepare', 'textDocument/prepareTypeHierarchy', {}],
        ['type hierarchy supertypes', 'typeHierarchy/supertypes', { item: {} }],
        ['type hierarchy subtypes', 'typeHierarchy/subtypes', { item: {} }],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-hierarchy-initialize');
        for (const [label, method, params] of cases) {
            const id = `invalid-hierarchy-${label}`;
            const response = await client.request(method, params, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

async function testInvalidEditorFeatureParams(serverPath) {
    const cases = [
        ['implementation', 'textDocument/implementation'],
        ['folding range', 'textDocument/foldingRange'],
        ['selection range', 'textDocument/selectionRange'],
        ['document link', 'textDocument/documentLink'],
        ['code lens', 'textDocument/codeLens'],
    ];

    await withClient(serverPath, async (client) => {
        await initialize(client, 'invalid-editor-feature-initialize');
        for (const [label, method] of cases) {
            const id = `invalid-editor-feature-${label}`;
            const response = await client.request(method, {}, id, RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, id, -32602, label);
        }
    });
}

async function testUnknownMethod(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'unknown-method-initialize');
        const response = await client.request('workspace/notReal', {}, 'unknown-method', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'unknown-method', -32601, 'unknown method');
    });
}

async function testNotificationHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'notification-initialize');
        client.notify('workspace/notReal', {});
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

async function testMalformedNotificationHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'malformed-notification-initialize');
        client.notify('textDocument/hover', 'not-an-object');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

async function testMalformedJson(serverPath) {
    await withClient(serverPath, async (client) => {
        client.sendRawFrame(Buffer.from('Content-Length: 1\r\n\r\n{', 'ascii'));
        const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, null, -32700, 'malformed JSON payload');
    });
}

async function testDuplicateRequestId(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'duplicate-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///duplicate-request-queue.zr',
                languageId: 'zr',
                version: 1,
                text: '// request registry queue\n'.repeat(8192),
            },
        });
        const payload = {
            jsonrpc: '2.0',
            id: 'duplicate-request',
            method: 'workspace/symbol',
            params: { query: '' },
        };
        client.sendPayload(payload);
        client.sendPayload(payload);
        const first = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const second = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const responses = [first, second];
        const errors = responses.filter((response) =>
            response && Object.prototype.hasOwnProperty.call(response, 'error'));
        const successes = responses.filter((response) =>
            response && !Object.prototype.hasOwnProperty.call(response, 'error'));
        assert(errors.length === 1 && successes.length === 1,
               `duplicate request ids require one error and one success envelope, actual=${JSON.stringify(responses)}`);
        assertErrorEnvelope(errors[0], 'duplicate-request', -32600, 'duplicate request id');
        assertSuccessEnvelope(successes[0], 'duplicate-request', 'original request id');
        assert(Array.isArray(successes[0].result) && successes[0].result.length === 0,
               `original workspace symbol request must have no symbols, actual=${JSON.stringify(successes[0])}`);
    });
}

async function testDistinctTypedRequestIds(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'typed-id-initialize');
        client.sendPayload({
            jsonrpc: '2.0',
            id: 1,
            method: 'workspace/symbol',
            params: { query: '' },
        });
        client.sendPayload({
            jsonrpc: '2.0',
            id: '1',
            method: 'workspace/symbol',
            params: { query: '' },
        });
        const first = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const second = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        const responses = [first, second];
        for (const id of [1, '1']) {
            const response = responses.find((item) => item && item.id === id);
            assertSuccessEnvelope(response, id, `distinct ${typeof id} request id`);
            assert(Array.isArray(response.result) && response.result.length === 0,
                   `typed workspace symbol request must have no symbols, actual=${JSON.stringify(response)}`);
        }
    });
}

async function testNumericRequestIdPrecision(serverPath) {
    const safeIdText = '9007199254740991';
    const safeId = Number(safeIdText);
    const negativeSafeIdText = '-9007199254740991';
    const initializeParams = JSON.stringify(initializePayload(safeId).params);

    await withClient(serverPath, async (client) => {
        client.sendRawFrame(encodeRawJsonFrame(
            `{"jsonrpc":"2.0","id":${safeIdText},"method":"initialize","params":${initializeParams}}`));
        const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(response, safeId, 'safe numeric request id');

        client.sendRawFrame(encodeRawJsonFrame(
            `{"jsonrpc":"2.0","id":${negativeSafeIdText},"method":"workspace/symbol","params":{"query":""}}`));
        const negativeResponse = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(negativeResponse, Number(negativeSafeIdText),
                              'negative safe numeric request id');
    });

    await withClient(serverPath, async (client) => {
        for (const unsafeIdText of ['9007199254740992', '-9007199254740992']) {
            client.sendRawFrame(encodeRawJsonFrame(
                `{"jsonrpc":"2.0","id":${unsafeIdText},"method":"initialize","params":${initializeParams}}`));
            const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
            assertErrorEnvelope(response, null, -32600, `unsafe numeric request id ${unsafeIdText}`);
        }
    });
}

async function testCancelUnknownIdHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'cancel-initialize');
        client.notify('$/cancelRequest', { id: 'not-active' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

async function testCancelKnownRequestId(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///cancel-known-request.zr';
        const version = 1;
        const documentSetupTimeoutMs = 10000;
        const symbols = Array.from(
            { length: 2048 },
            (_, index) => `class CancellationSymbol${index} { }`).join('\n');

        await initialize(client, 'cancel-known-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version,
                text: symbols,
            },
        });

        // Keep the request and cancellation queued behind the expensive didOpen setup.
        client.sendPayload({
            jsonrpc: '2.0',
            id: 'cancel-known-request',
            method: 'workspace/symbol',
            params: {
                query: 'CancellationSymbol',
                partialResultToken: 'cancel-known-progress',
            },
        });
        client.notify('$/cancelRequest', { id: 'cancel-known-request' });

        const diagnostics = await client.waitForNotification(
            'textDocument/publishDiagnostics', documentSetupTimeoutMs);
        assert(diagnostics && diagnostics.uri === uri && diagnostics.version === version,
               `cancel known request setup must publish the exact document/version, actual=${JSON.stringify(diagnostics)}`);

        // This response deadline excludes setup; active-query cancellation latency is a separate gate.
        const response = await client.nextMessage(RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(response, 'cancel-known-request', -32800, 'cancel known request id');
    });
}

async function testSetTraceWritesOnlyStderr(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'trace-initialize');
        client.notify('$/setTrace', { value: 'messages' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);

        const tracedResponse = await client.request('workspace/symbol', { query: '' }, 'trace-request');
        assertSuccessEnvelope(tracedResponse, 'trace-request', 'traced request');
        assert(tracedResponse && Array.isArray(tracedResponse.result),
               `traced request must retain its framed result, actual=${JSON.stringify(tracedResponse)}`);
        assert(client.stderr().includes('LSP trace inbound request workspace/symbol'),
               `messages trace must write inbound request to stderr, stderr=${client.stderr()}`);
        assert(client.stderr().includes('LSP trace outbound response workspace/symbol'),
               `messages trace must write outbound response to stderr, stderr=${client.stderr()}`);

        client.notify('$/setTrace', { value: 'verbose' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        client.notify('workspace/notReal', {});
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        assert(client.stderr().includes('LSP trace inbound notification workspace/notReal'),
               `verbose trace must write notification metadata to stderr, stderr=${client.stderr()}`);

        client.notify('$/setTrace', { value: 'off' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        const stderrBeforeOffRequest = client.stderr();
        const offResponse = await client.request('workspace/symbol', { query: '' }, 'trace-off-request');
        assertSuccessEnvelope(offResponse, 'trace-off-request', 'trace off request');
        assert(client.stderr() === stderrBeforeOffRequest,
               `off trace must not add stderr records, stderr=${client.stderr()}`);
    });
}

async function testControlNotificationsOutsideLifecycleAreIgnored(serverPath) {
    await withClient(serverPath, async (client) => {
        client.notify('$/setTrace', { value: 'messages' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        await initialize(client, 'trace-before-initialize');

        const response = await client.request('workspace/symbol', { query: '' },
                                               'trace-before-initialize-request');
        assertSuccessEnvelope(response, 'trace-before-initialize-request',
                              'request after pre-initialize setTrace');
        assert(!client.stderr().includes('LSP trace inbound request workspace/symbol'),
               `setTrace before initialize must be ignored, stderr=${client.stderr()}`);

        const shutdown = await client.request('shutdown', undefined, 'trace-shutdown');
        assertSuccessEnvelope(shutdown, 'trace-shutdown', 'trace lifecycle shutdown');
        client.notify('$/setTrace', { value: 'verbose' });
        client.notify('workspace/notReal', {});
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
        assert(!client.stderr().includes('LSP trace inbound notification workspace/notReal'),
               `setTrace after shutdown must be ignored, stderr=${client.stderr()}`);
    });
}

async function testRequestWorkDoneProgress(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'work-done-initialize');
        const stringResponse = client.request('workspace/symbol', {
            query: '',
            workDoneToken: 'workspace-symbol-work',
        }, 'work-done-string', RESPONSE_TIMEOUT_MS);
        const stringBegin = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const stringEnd = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const stringResult = await stringResponse;
        assertSuccessEnvelope(stringResult, 'work-done-string', 'string work-done result');

        assert(stringBegin && stringBegin.token === 'workspace-symbol-work' &&
               stringBegin.value && stringBegin.value.kind === 'begin',
               `string work-done begin must preserve token, actual=${JSON.stringify(stringBegin)}`);
        assert(stringEnd && stringEnd.token === 'workspace-symbol-work' &&
               stringEnd.value && stringEnd.value.kind === 'end',
               `string work-done end must preserve token, actual=${JSON.stringify(stringEnd)}`);
        assert(stringResult && stringResult.id === 'work-done-string' && Array.isArray(stringResult.result),
               `work-done request must retain its ordinary response, actual=${JSON.stringify(stringResult)}`);

        const numberResponse = client.request('workspace/symbol', {
            query: '',
            workDoneToken: 9,
        }, 'work-done-number', RESPONSE_TIMEOUT_MS);
        const numberBegin = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const numberEnd = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const numberResult = await numberResponse;
        assertSuccessEnvelope(numberResult, 'work-done-number', 'numeric work-done result');

        assert(numberBegin && numberBegin.token === 9 && numberBegin.value && numberBegin.value.kind === 'begin',
               `numeric work-done begin must preserve token, actual=${JSON.stringify(numberBegin)}`);
        assert(numberEnd && numberEnd.token === 9 && numberEnd.value && numberEnd.value.kind === 'end',
               `numeric work-done end must preserve token, actual=${JSON.stringify(numberEnd)}`);
        assert(numberResult && numberResult.id === 'work-done-number' && Array.isArray(numberResult.result),
               `numeric work-done request must retain its ordinary response, actual=${JSON.stringify(numberResult)}`);

        for (const [label, token] of [
            ['positive-boundary', 9007199254740991],
            ['negative-boundary', -9007199254740991],
        ]) {
            const requestId = `work-done-${label}`;
            const responsePromise = client.request('workspace/symbol', {
                query: '',
                workDoneToken: token,
            }, requestId, RESPONSE_TIMEOUT_MS);
            const begin = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
            const end = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
            const response = await responsePromise;
            assertSuccessEnvelope(response, requestId, `${label} work-done result`);
            assert(begin && begin.token === token && begin.value && begin.value.kind === 'begin',
                   `${label} work-done begin must preserve token, actual=${JSON.stringify(begin)}`);
            assert(end && end.token === token && end.value && end.value.kind === 'end',
                   `${label} work-done end must preserve token, actual=${JSON.stringify(end)}`);
        }

        const invalidPartial = await client.request('workspace/symbol', {
            query: '',
            partialResultToken: false,
        }, 'work-done-invalid-partial', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(invalidPartial,
                            'work-done-invalid-partial',
                            -32602,
                            'invalid partial result token');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);

        const invalidWorkDoneBoundary = await client.request('workspace/symbol', {
            query: '',
            workDoneToken: 9007199254740992,
        }, 'work-done-invalid-work-boundary', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(invalidWorkDoneBoundary,
                            'work-done-invalid-work-boundary',
                            -32602,
                            'invalid work-done token boundary');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);

        const invalidPartialBoundary = await client.request('workspace/symbol', {
            query: '',
            partialResultToken: -9007199254740992,
        }, 'work-done-invalid-partial-boundary', RESPONSE_TIMEOUT_MS);
        assertErrorEnvelope(invalidPartialBoundary,
                            'work-done-invalid-partial-boundary',
                            -32602,
                            'invalid partial-result token boundary');
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

async function testWorkspaceSymbolPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'partial-symbol-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: 'file:///partial-progress-symbol.zr',
                languageId: 'zr',
                version: 1,
                text: Array.from(
                    { length: 65 },
                    (_, index) => `class PartialProgressSymbol${index} { }`).join('\n'),
            },
        });

        const responsePromise = client.request('workspace/symbol', {
            query: 'PartialProgressSymbol',
            partialResultToken: 17,
        }, 'workspace-symbol-partial', RESPONSE_TIMEOUT_MS);
        const firstPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const secondPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'workspace-symbol-partial', 'workspace symbol partial result');

        assert(firstPartial && firstPartial.token === 17 &&
               Array.isArray(firstPartial.value) && firstPartial.value.length === 64 &&
               firstPartial.value.some((item) => item && item.name === 'PartialProgressSymbol0'),
               `workspace symbol first partial batch must preserve 64 results, actual=${JSON.stringify(firstPartial)}`);
        assert(secondPartial && secondPartial.token === 17 &&
               Array.isArray(secondPartial.value) && secondPartial.value.length === 1 &&
               secondPartial.value[0] && secondPartial.value[0].name === 'PartialProgressSymbol64',
               `workspace symbol second partial batch must preserve the remaining result, actual=${JSON.stringify(secondPartial)}`);
        assert(response && response.id === 'workspace-symbol-partial' && response.result === null,
               `workspace symbol partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

async function testReferencesPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-references.zr';
        await initialize(client, 'partial-references-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: [
                    'fn partialReference(value: int): int {',
                    '    return value;',
                    '}',
                    'fn usePartialReference(): int {',
                    '    return partialReference(partialReference(1));',
                    '}',
                ].join('\n'),
            },
        });

        const responsePromise = client.request('textDocument/references', {
            textDocument: { uri },
            position: { line: 0, character: 3 },
            context: { includeDeclaration: true },
            partialResultToken: 'references-partial',
        }, 'references-partial', RESPONSE_TIMEOUT_MS);
        const partial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'references-partial', 'references partial result');

        assert(partial && partial.token === 'references-partial' && Array.isArray(partial.value) &&
               partial.value.length >= 3 && partial.value.every((location) => location && location.uri === uri),
               `references partial result must preserve all reference locations, actual=${JSON.stringify(partial)}`);
        assert(response && response.id === 'references-partial' && response.result === null,
               `references partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

async function testCallHierarchyPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-call-hierarchy.zr';
        await initialize(client, 'partial-call-hierarchy-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: [
                    'fn partialCallee(value: int): int {',
                    '    return value;',
                    '}',
                    'fn partialCaller(): int {',
                    '    return partialCallee(1);',
                    '}',
                ].join('\n'),
            },
        });

        const prepared = await client.request('textDocument/prepareCallHierarchy', {
            textDocument: { uri },
            position: { line: 0, character: 3 },
        }, 'prepare-call-hierarchy-partial', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(prepared, 'prepare-call-hierarchy-partial', 'prepare call hierarchy');
        assert(prepared && Array.isArray(prepared.result) && prepared.result.length > 0,
               `prepare call hierarchy must yield the callee item, actual=${JSON.stringify(prepared)}`);

        const responsePromise = client.request('callHierarchy/incomingCalls', {
            item: prepared.result[0],
            partialResultToken: 'call-hierarchy-partial',
        }, 'call-hierarchy-partial', RESPONSE_TIMEOUT_MS);
        const partial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'call-hierarchy-partial', 'call hierarchy partial result');

        assert(partial && partial.token === 'call-hierarchy-partial' && Array.isArray(partial.value) &&
               partial.value.some((call) => call && call.from && call.from.name === 'partialCaller'),
               `call hierarchy partial result must preserve incoming calls, actual=${JSON.stringify(partial)}`);
        assert(response && response.id === 'call-hierarchy-partial' && response.result === null,
               `call hierarchy partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

async function testTypeHierarchyPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-type-hierarchy.zr';
        await initialize(client, 'partial-type-hierarchy-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: [
                    'class PartialBase {',
                    '}',
                    'class PartialDerived : PartialBase {',
                    '}',
                ].join('\n'),
            },
        });

        const derived = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri },
            position: { line: 2, character: 7 },
        }, 'prepare-type-hierarchy-derived', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(derived, 'prepare-type-hierarchy-derived', 'prepare derived type hierarchy');
        assert(derived && Array.isArray(derived.result) && derived.result.length > 0,
               `prepare type hierarchy must yield PartialDerived, actual=${JSON.stringify(derived)}`);

        const supertypesPromise = client.request('typeHierarchy/supertypes', {
            item: derived.result[0],
            partialResultToken: 'type-hierarchy-supertypes-partial',
        }, 'type-hierarchy-supertypes-partial', RESPONSE_TIMEOUT_MS);
        const supertypesPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const supertypes = await supertypesPromise;
        assertSuccessEnvelope(supertypes, 'type-hierarchy-supertypes-partial', 'supertypes partial result');
        assert(supertypesPartial && supertypesPartial.token === 'type-hierarchy-supertypes-partial' &&
               Array.isArray(supertypesPartial.value) &&
               supertypesPartial.value.some((item) => item && item.name === 'PartialBase'),
               `type hierarchy supertypes partial result must preserve PartialBase, actual=${JSON.stringify(supertypesPartial)}`);
        assert(supertypes && supertypes.id === 'type-hierarchy-supertypes-partial' && supertypes.result === null,
               `type hierarchy supertypes partial result must consume the ordinary response, actual=${JSON.stringify(supertypes)}`);

        const base = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri },
            position: { line: 0, character: 7 },
        }, 'prepare-type-hierarchy-base', RESPONSE_TIMEOUT_MS);
        assertSuccessEnvelope(base, 'prepare-type-hierarchy-base', 'prepare base type hierarchy');
        assert(base && Array.isArray(base.result) && base.result.length > 0,
               `prepare type hierarchy must yield PartialBase, actual=${JSON.stringify(base)}`);

        const subtypesPromise = client.request('typeHierarchy/subtypes', {
            item: base.result[0],
            partialResultToken: 'type-hierarchy-subtypes-partial',
        }, 'type-hierarchy-subtypes-partial', RESPONSE_TIMEOUT_MS);
        const subtypesPartial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const subtypes = await subtypesPromise;
        assertSuccessEnvelope(subtypes, 'type-hierarchy-subtypes-partial', 'subtypes partial result');
        assert(subtypesPartial && subtypesPartial.token === 'type-hierarchy-subtypes-partial' &&
               Array.isArray(subtypesPartial.value) &&
               subtypesPartial.value.some((item) => item && item.name === 'PartialDerived'),
               `type hierarchy subtypes partial result must preserve PartialDerived, actual=${JSON.stringify(subtypesPartial)}`);
        assert(subtypes && subtypes.id === 'type-hierarchy-subtypes-partial' && subtypes.result === null,
               `type hierarchy subtypes partial result must consume the ordinary response, actual=${JSON.stringify(subtypes)}`);
    });
}

async function testWorkspaceDiagnosticPartialResults(serverPath) {
    await withClient(serverPath, async (client) => {
        const uri = 'file:///partial-progress-workspace-diagnostic.zr';
        await initialize(client, 'partial-workspace-diagnostic-initialize');
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'fn partialWorkspaceDiagnostic(value: int): int { return value; }',
            },
        });

        const responsePromise = client.request('workspace/diagnostic', {
            partialResultToken: 'workspace-diagnostic-partial',
        }, 'workspace-diagnostic-partial', RESPONSE_TIMEOUT_MS);
        const partial = await client.waitForNotification('$/progress', RESPONSE_TIMEOUT_MS);
        const response = await responsePromise;
        assertSuccessEnvelope(response, 'workspace-diagnostic-partial', 'workspace diagnostic partial result');

        assert(partial && partial.token === 'workspace-diagnostic-partial' &&
               partial.value && Array.isArray(partial.value.items) &&
               partial.value.items.some((report) => report && report.uri === uri),
               `workspace diagnostic partial result must preserve report items, actual=${JSON.stringify(partial)}`);
        assert(response && response.id === 'workspace-diagnostic-partial' && response.result === null,
               `workspace diagnostic partial result must consume the ordinary response, actual=${JSON.stringify(response)}`);
    });
}

async function testOversizeFrameClosesWithFailure(serverPath) {
    await withClient(serverPath, async (client) => {
        client.sendRawFrame(Buffer.from('Content-Length: 16777217\r\n\r\n', 'ascii'));
        client.endInput();
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode !== 0, `oversize frame must terminate with failure, actual=${exitCode}`);
        assert(client.stderr().includes('TOO_LARGE'),
               `oversize frame must report TOO_LARGE, stderr=${client.stderr()}`);
    });
}

async function testMalformedFramesCloseWithFailure(serverPath) {
    const cases = [
        ['missing content length', 'MALFORMED_HEADER', 'Content-Type: application/vscode-jsonrpc\r\n\r\n'],
        ['duplicate content length', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}'],
        ['negative content length', 'MALFORMED_HEADER', 'Content-Length: -1\r\n\r\n'],
        ['content length suffix', 'MALFORMED_HEADER', 'Content-Length: 2oops\r\n\r\n{}'],
        ['content length overflow', 'TOO_LARGE', 'Content-Length: 184467440737095516160\r\n\r\n'],
        ['NUL in content length', 'MALFORMED_HEADER', 'Content-Length: 2\0junk\r\n\r\n{}'],
        ['wrong newline', 'MALFORMED_HEADER', 'Content-Length: 2\n\n{}'],
        ['non utf8 charset', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-16\r\n\r\n{}'],
        ['charset without value', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset\r\n\r\n{}'],
        ['conflicting charset parameters', 'MALFORMED_HEADER',
         'Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8; charset=utf-16\r\n\r\n{}'],
        ['truncated payload', 'PAYLOAD_TRUNCATED', 'Content-Length: 4\r\n\r\n{}'],
        ['too many headers', 'TOO_LARGE',
         `${Array.from({ length: 33 }, (_, index) => `X-Test-${index}: value\r\n`).join('')}Content-Length: 2\r\n\r\n{}`],
    ];

    for (const [label, expectedStatus, frame] of cases) {
        await withClient(serverPath, async (client) => {
            client.sendRawFrame(Buffer.from(frame, 'ascii'));
            client.endInput();
            const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
            assert(exitCode !== 0, `${label}: malformed frame must exit non-zero, actual=${exitCode}`);
            assert(client.stderr().includes(expectedStatus),
                   `${label}: expected ${expectedStatus}, stderr=${client.stderr()}`);
        });
    }
}

function protocolCases() {
    return [
        ['LSP 3.17 capability matrix', testCapabilityMatrix],
        ['request before initialize', testRequestBeforeInitialize],
        ['notification before initialize is ignored', testNotificationBeforeInitializeIsIgnored],
        ['repeated initialize', testRepeatedInitialize],
        ['exit before shutdown', testExitBeforeShutdown],
        ['shutdown and exit ordering', testShutdownExitOrdering],
        ['request after shutdown', testRequestAfterShutdown],
        ['missing jsonrpc', testMissingJsonRpc],
        ['wrong jsonrpc', testWrongJsonRpc],
        ['boolean request id', testInvalidBooleanId],
        ['structured request ids', testInvalidStructuredIds],
        ['fractional request id', testInvalidFractionalRequestId],
        ['invalid top-level messages', testInvalidTopLevelMessages],
        ['invalid params', testInvalidParams],
        ['invalid position and range numbers', testInvalidPositionAndRangeNumbers],
        ['invalid hierarchy params', testInvalidHierarchyParams],
        ['invalid editor feature params', testInvalidEditorFeatureParams],
        ['unknown method', testUnknownMethod],
        ['notification has no response', testNotificationHasNoResponse],
        ['malformed notification has no response', testMalformedNotificationHasNoResponse],
        ['malformed JSON payload', testMalformedJson],
        ['duplicate request id', testDuplicateRequestId],
        ['distinct typed request ids', testDistinctTypedRequestIds],
        ['numeric request id precision', testNumericRequestIdPrecision],
        ['cancel unknown id has no response', testCancelUnknownIdHasNoResponse],
        ['cancel known request id', testCancelKnownRequestId],
        ['set trace writes only stderr', testSetTraceWritesOnlyStderr],
        ['control notifications outside lifecycle are ignored', testControlNotificationsOutsideLifecycleAreIgnored],
        ['request work-done progress', testRequestWorkDoneProgress],
        ['workspace symbol partial results', testWorkspaceSymbolPartialResults],
        ['references partial results', testReferencesPartialResults],
        ['call hierarchy partial results', testCallHierarchyPartialResults],
        ['type hierarchy partial results', testTypeHierarchyPartialResults],
        ['workspace diagnostic partial results', testWorkspaceDiagnosticPartialResults],
        ['oversize frame closes with failure', testOversizeFrameClosesWithFailure],
        ['malformed frames close with classified failure', testMalformedFramesCloseWithFailure],
    ];
}

async function main() {
    const serverPath = process.argv[2];
    const cases = protocolCases();
    let failures = 0;

    assert(serverPath, 'usage: node stdio_protocol_conformance.js <stdio-server>');
    for (const [name, run] of cases) {
        try {
            await run(serverPath);
            console.log(`Pass - ${name}`);
        } catch (error) {
            console.error(`Fail - ${name}: ${error.stack || error.message}`);
            failures += 1;
        }
    }

    if (failures !== 0) {
        throw new Error(`stdio protocol conformance has ${failures} failing cases`);
    }
}

module.exports = { protocolCases };

if (require.main === module) {
    main().catch((error) => {
        console.error(`stdio protocol conformance failed: ${error.stack || error.message}`);
        process.exitCode = 1;
    });
}
