const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

async function main() {
    const executable = process.argv[2];
    assert(executable, 'expected stdio server path');
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-defer-'));
    let client;
    try {
        const file = path.join(root, 'main.zr');
        const text = 'module main;\nfn main(): int { return missing; }\n';
        fs.writeFileSync(file, text);
        const uri = pathToFileURL(file).toString();
        const rootUri = pathToFileURL(root + path.sep).toString();
        const textDocument = { uri };
        client = new StdioProtocolClient(executable);
        const request = (method, params) => client.requestWithId(method, params, 10000).promise;
        await request('initialize', {
            processId: null, rootUri, workspaceFolders: [{ uri: rootUri, name: 'deferred' }], capabilities: {},
            initializationOptions: { zrDeferDiagnostics: true },
        });
        client.notify('initialized', {});
        client.notify('textDocument/didOpen', { textDocument: { uri, languageId: 'zr', version: 1, text } });
        const symbols = await request('textDocument/documentSymbol', { textDocument });
        assert(symbols.some(symbol => symbol.name === 'main'), 'interactive symbols must remain available');
        // The symbol response is a barrier after the preceding document notification.
        assert.equal(client.notificationBacklog.has('textDocument/publishDiagnostics'), false);
        const first = await request('textDocument/diagnostic', { textDocument });
        assert.equal(first.kind, 'full');
        assert(first.items.length > 0, 'explicit pull must still report the missing value');
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 2 },
            contentChanges: [{ text: 'module main;\nfn main(): int { return 1; }\n' }],
        });
        await request('textDocument/documentSymbol', { textDocument });
        assert.equal(client.notificationBacklog.has('textDocument/publishDiagnostics'), false);
        const changed = await request('textDocument/diagnostic', { textDocument, previousResultId: first.resultId });
        assert.equal(changed.kind, 'full');
        assert.notEqual(changed.resultId, first.resultId);
        assert(changed.items.length < first.items.length, 'pull must use the edited buffer');
        console.log('PASS: interactive requests and explicit pulls without automatic lint');
    } finally {
        try { await client?.terminate(); } finally { fs.rmSync(root, { recursive: true, force: true }); }
    }
}

main().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
