const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');

const REQUEST_TIMEOUT_MS = 10000;

async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_navigation_capabilities_smoke.js <stdio-server>');
    const client = new StdioProtocolClient(serverPath);
    const request = (method, params, id) => client.request(method, params, id, REQUEST_TIMEOUT_MS);
    const implementationUri = 'file:///stdio-navigation-implementation.zr';
    const implementationText = [
        'interface Readable { fn read(): int; }',
        'class Device : Readable {',
        '    pub fn read(): int { return 1; }',
        '}',
        'class Sensor : Readable {',
        '    pub fn read(): int { return 2; }',
        '}',
        'class Other {',
        '    pub fn read(): int { return 3; }',
        '}',
        '',
    ].join('\n');
    let cleanExit = false;

    try {
        const initialize = await request('initialize', { capabilities: {} }, 'initialize');
        assert.ok(initialize.result && initialize.result.capabilities,
                  'initialize must return capabilities');
        assert.equal(initialize.result.capabilities.definitionProvider, true,
                     'definitionProvider must remain advertised');
        assert.equal(initialize.result.capabilities.implementationProvider, true,
                     'implementationProvider must remain advertised');
        client.notify('initialized', {});

        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: implementationUri, languageId: 'zr', version: 1, text: implementationText,
            },
        });
        const diagnostics = await client.waitForNotification(
            'textDocument/publishDiagnostics', REQUEST_TIMEOUT_MS);
        assert.equal(diagnostics.uri, implementationUri);
        assert.deepEqual(diagnostics.diagnostics, [], 'navigation fixture must have no diagnostics');

        const definitionParams = {
            textDocument: { uri: implementationUri },
            position: { line: 1, character: 7 },
        };
        const definition = await request('textDocument/definition', definitionParams, 'definition');
        assert.deepEqual(definition.result, [{
            uri: implementationUri,
            range: {
                start: { line: 1, character: 6 },
                end: { line: 1, character: 12 },
            },
        }], 'definition must retain the exact Device declaration target');

        const implementation = await request('textDocument/implementation', {
            textDocument: { uri: implementationUri },
            position: { line: 0, character: 11 },
        }, 'implementation');
        assert.ok(Array.isArray(implementation.result), 'implementation must return an array');
        assert.deepEqual(implementation.result.slice().sort((left, right) =>
            left.range.start.line - right.range.start.line), [
            {
                uri: implementationUri,
                range: {
                    start: { line: 1, character: 0 },
                    end: { line: 3, character: 1 },
                },
            },
            {
                uri: implementationUri,
                range: {
                    start: { line: 4, character: 0 },
                    end: { line: 6, character: 1 },
                },
            },
        ], 'implementation must return both canonical targets and exclude unrelated Other.read');
        const unrelatedImplementation = await request('textDocument/implementation', {
            textDocument: { uri: implementationUri },
            position: { line: 7, character: 7 },
        }, 'unrelated-implementation');
        assert.deepEqual(unrelatedImplementation.result, [],
                         'unrelated type with a same-name read method has no implementations');
        console.log('Pass - exact definition and implementation target set/ranges');

        assert.equal(initialize.result.capabilities.declarationProvider, undefined,
                     'declarationProvider must be withdrawn');
        assert.equal(initialize.result.capabilities.typeDefinitionProvider, undefined,
                     'typeDefinitionProvider must be withdrawn');
        for (const [method, id] of [['textDocument/declaration', 'declaration'],
                                    ['textDocument/typeDefinition', 'type-definition']]) {
            const withdrawn = await request(method, definitionParams, id);
            assert.deepEqual(withdrawn, {
                jsonrpc: '2.0',
                id,
                error: { code: -32601, message: 'Method not found' },
            }, `${method} must return the exact MethodNotFound envelope`);
        }

        console.log('Pass - navigation capabilities withdraw aliases and retain definition/implementation');
        const shutdown = await request('shutdown', undefined, 'shutdown');
        assert.equal(shutdown.result, null);
        client.notify('exit', undefined);
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0, client.stderr());
        assert.equal(client.stderr().trim(), '', 'stdio stderr must remain empty');
        cleanExit = true;
    } finally {
        if (!cleanExit) {
            await client.terminate();
        }
    }
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
