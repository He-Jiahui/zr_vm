const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

async function main() {
    const [executable, scratch] = process.argv.slice(2);
    assert(executable && scratch, 'expected stdio executable and scratch directory');
    const root = fs.mkdtempSync(path.join(scratch, 'zr-strict-no-gc-'));
    let client;
    try {
        fs.mkdirSync(path.join(root, 'src'));
        fs.writeFileSync(path.join(root, 'strict.zrp'), JSON.stringify({
            manifestVersion: 3, StrictNoGC: true, name: 'strict', version: '1.0.0',
            kind: 'app', source: 'src', binary: 'bin', entry: 'main',
        }));
        const file = path.join(root, 'src', 'main.zr');
        const text = `resource class Student { pub @constructor() {} }
fn unused(): int {
    if (false) { let rejected = new Student(); }
    let owned = own Student();
    let managed = intoGc(owned);
    return 0;
}
return 1;
`;
        fs.writeFileSync(file, text);
        const uri = pathToFileURL(file).toString();
        const rootUri = pathToFileURL(root + path.sep).toString();
        const textDocument = { uri };
        client = new StdioProtocolClient(executable);
        const request = (method, params) => client.requestWithId(method, params, 10000).promise;
        await request('initialize', {
            processId: null, rootUri, workspaceFolders: [{ uri: rootUri, name: 'strict' }], capabilities: {},
            initializationOptions: { zrDeferDiagnostics: true },
        });
        client.notify('initialized', {});
        client.notify('textDocument/didOpen', { textDocument: { uri, languageId: 'zr', version: 1, text } });
        const first = await request('textDocument/diagnostic', { textDocument });
        assert.equal(first.kind, 'full');
        const strict = first.items.filter(({ code }) => typeof code === 'string' && code.startsWith('strict_no_gc_'));
        assert.deepEqual(strict.map(({ code, range }) => [code, range.start.line]), [
            ['strict_no_gc_new', 2], ['strict_no_gc_into_gc', 4],
        ]);
        assert.match(strict[0].message, /Use init .* or own/);
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 2 },
            contentChanges: [{ text: text.replace('new Student()', 'own Student()').replace('let managed = intoGc(owned);', 'drop(owned);') }],
        });
        const changed = await request('textDocument/diagnostic', { textDocument });
        assert.equal(changed.kind, 'full');
        assert(!changed.items.some(({ code }) => typeof code === 'string' && code.startsWith('strict_no_gc_')));
    } finally {
        try { await client?.terminate(); } finally { fs.rmSync(root, { recursive: true, force: true }); }
    }
}

main().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
