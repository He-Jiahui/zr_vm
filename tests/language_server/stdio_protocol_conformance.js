const { StdioProtocolClient, encodeFrame } = require('./stdio_protocol_client');

const RESPONSE_TIMEOUT_MS = 750;
const NO_RESPONSE_TIMEOUT_MS = 150;
const EXPECTED_CAPABILITY_KEYS = [
    'callHierarchyProvider', 'codeActionProvider', 'codeLensProvider', 'colorProvider',
    'completionProvider', 'declarationProvider', 'definitionProvider', 'diagnosticProvider',
    'documentFormattingProvider', 'documentHighlightProvider', 'documentLinkProvider',
    'documentOnTypeFormattingProvider', 'documentRangeFormattingProvider',
    'documentSymbolProvider', 'foldingRangeProvider', 'hoverProvider', 'implementationProvider',
    'inlayHintProvider', 'inlineCompletionProvider', 'inlineValueProvider',
    'linkedEditingRangeProvider', 'monikerProvider', 'positionEncoding', 'referencesProvider',
    'renameProvider', 'selectionRangeProvider', 'semanticTokensProvider', 'signatureHelpProvider',
    'textDocumentSync', 'typeDefinitionProvider', 'typeHierarchyProvider', 'workspace',
    'workspaceSymbolProvider',
].sort();

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertErrorEnvelope(response, id, code, label) {
    assert(response && response.jsonrpc === '2.0', `${label}: response must have jsonrpc=2.0`);
    assert(Object.prototype.hasOwnProperty.call(response, 'id') && response.id === id,
           `${label}: response id must exactly match ${String(id)}`);
    assert(response.error && response.error.code === code,
           `${label}: expected error ${code}, actual=${JSON.stringify(response)}`);
    assert(!Object.prototype.hasOwnProperty.call(response, 'result'),
           `${label}: error envelope must not also contain result`);
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

async function initialize(client, id = 'initialize') {
    const response = await client.requestEnvelope(initializePayload(id), RESPONSE_TIMEOUT_MS);
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

async function testRequestAfterShutdown(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'shutdown-initialize');
        const shutdown = await client.request('shutdown', undefined, 'shutdown', RESPONSE_TIMEOUT_MS);
        assert(shutdown && shutdown.result === null,
               `shutdown must return a null result envelope, actual=${JSON.stringify(shutdown)}`);
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

async function testInvalidTopLevelMessages(serverPath) {
    await withClient(serverPath, async (client) => {
        await expectInvalidRequest(client, [], null, 'array top-level message');
        await expectInvalidRequest(client, 17, null, 'scalar top-level message');
    });
}

async function testInvalidParams(serverPath) {
    const invalidParams = [
        ['scalar params', 'not-an-object'],
        ['null params', null],
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
        assert(responses.some((response) => response && response.id === 'duplicate-request' &&
                response.error && response.error.code === -32600),
               `duplicate request ids must produce -32600, actual=${JSON.stringify(responses)}`);
    });
}

async function testCancelUnknownIdHasNoResponse(serverPath) {
    await withClient(serverPath, async (client) => {
        await initialize(client, 'cancel-initialize');
        client.notify('$/cancelRequest', { id: 'not-active' });
        await client.expectNoMessage(NO_RESPONSE_TIMEOUT_MS);
    });
}

async function testOversizeFrameClosesWithFailure(serverPath) {
    await withClient(serverPath, async (client) => {
        client.sendRawFrame(Buffer.from('Content-Length: 16777217\r\n\r\n', 'ascii'));
        client.endInput();
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert(exitCode !== 0, `oversize frame must terminate with failure, actual=${exitCode}`);
    });
}

async function main() {
    const serverPath = process.argv[2];
    const cases = [
        ['LSP 3.17 capability matrix', testCapabilityMatrix],
        ['request before initialize', testRequestBeforeInitialize],
        ['repeated initialize', testRepeatedInitialize],
        ['exit before shutdown', testExitBeforeShutdown],
        ['request after shutdown', testRequestAfterShutdown],
        ['missing jsonrpc', testMissingJsonRpc],
        ['wrong jsonrpc', testWrongJsonRpc],
        ['boolean request id', testInvalidBooleanId],
        ['structured request ids', testInvalidStructuredIds],
        ['invalid top-level messages', testInvalidTopLevelMessages],
        ['invalid params', testInvalidParams],
        ['invalid position and range numbers', testInvalidPositionAndRangeNumbers],
        ['unknown method', testUnknownMethod],
        ['notification has no response', testNotificationHasNoResponse],
        ['malformed notification has no response', testMalformedNotificationHasNoResponse],
        ['malformed JSON payload', testMalformedJson],
        ['duplicate request id', testDuplicateRequestId],
        ['cancel unknown id has no response', testCancelUnknownIdHasNoResponse],
        ['oversize frame closes with failure', testOversizeFrameClosesWithFailure],
    ];
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

main().catch((error) => {
    console.error(`stdio protocol conformance failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
