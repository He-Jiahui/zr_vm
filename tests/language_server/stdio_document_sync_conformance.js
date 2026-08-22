const { StdioProtocolClient } = require('./stdio_protocol_client');

const RESPONSE_TIMEOUT_MS = 3000;

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    try {
        return await run(client);
    } finally {
        await client.terminate().catch(() => {});
    }
}

async function initialize(client) {
    const response = await client.request('initialize', {
        processId: null,
        rootUri: null,
        capabilities: {},
    }, 'document-sync-initialize', RESPONSE_TIMEOUT_MS);
    assert(response && response.result && response.result.capabilities,
           `initialize must succeed, actual=${JSON.stringify(response)}`);
    client.notify('initialized', {});
}

async function queryWorkspaceSymbols(client, query, id) {
    const response = await client.request('workspace/symbol', { query }, id, RESPONSE_TIMEOUT_MS);
    assert(response && !response.error && Array.isArray(response.result),
           `workspace/symbol must return a normal result, actual=${JSON.stringify(response)}`);
    return response.result;
}

async function main() {
    const serverPath = process.argv[2];
    const uri = 'file:///stdio-document-sync-conformance.zr';

    assert(serverPath, 'usage: node stdio_document_sync_conformance.js <stdio-server>');
    await withClient(serverPath, async (client) => {
        await initialize(client);
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri,
                languageId: 'zr',
                version: 1,
                text: 'class DocumentSyncVersionOne { }',
            },
        });

        const versionOne = await queryWorkspaceSymbols(
            client,
            'DocumentSyncVersionOne',
            'document-sync-v1');
        assert(versionOne.some((symbol) => symbol && symbol.name === 'DocumentSyncVersionOne'),
               `didOpen version 1 must be queryable, actual=${JSON.stringify(versionOne)}`);

        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 2 },
            contentChanges: [{ text: 'class DocumentSyncVersionTwo { }' }],
        });

        const versionTwo = await queryWorkspaceSymbols(
            client,
            'DocumentSyncVersionTwo',
            'document-sync-v2');
        assert(versionTwo.some((symbol) => symbol && symbol.name === 'DocumentSyncVersionTwo'),
               `didChange version 2 must replace the queryable document content, actual=${JSON.stringify(versionTwo)}`);

        const stale = await queryWorkspaceSymbols(
            client,
            'DocumentSyncVersionOne',
            'document-sync-stale');
        assert(stale.length === 0,
               `replaced document content must not remain in the workspace index, actual=${JSON.stringify(stale)}`);
    });
    console.log('stdio document sync conformance passed');
}

main().catch((error) => {
    console.error(`stdio document sync conformance failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
