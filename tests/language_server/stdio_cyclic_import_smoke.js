const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server path');
    const projectRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-lsp-cyclic-import-'));
    const sourceRoot = path.join(projectRoot, 'src');
    fs.mkdirSync(sourceRoot);
    fs.writeFileSync(path.join(projectRoot, 'cyclic.zrp'), JSON.stringify({
        name: 'cyclic', source: 'src', binary: 'bin', entry: 'main',
    }));
    fs.writeFileSync(path.join(sourceRoot, 'main.zr'), [
        'let helper = import("helper");',
        'class StructureHero {',
        '    pub fn total(): int { return helper.value(); }',
        '}',
        'return helper.value();',
        '',
    ].join('\n'));
    fs.writeFileSync(path.join(sourceRoot, 'helper.zr'), [
        'let cycle = import("cycle");',
        'pub var value = fn() => { return cycle.answer(); };',
        '',
    ].join('\n'));
    fs.writeFileSync(path.join(sourceRoot, 'cycle.zr'), [
        'let helper = import("helper");',
        'pub var answer = fn() => { return 42; };',
        '',
    ].join('\n'));

    const client = new StdioProtocolClient(serverPath);
    const request = async (method, params) =>
        client.requestWithId(method, params, 10000).promise;
    try {
        const projectUri = pathToFileURL(path.join(projectRoot, 'cyclic.zrp')).toString();
        const rootUri = pathToFileURL(projectRoot).toString();
        const documentUri = pathToFileURL(path.join(sourceRoot, 'main.zr')).toString();
        const initialized = await request('initialize', {
            processId: null,
            rootUri,
            workspaceFolders: [{ uri: rootUri, name: 'cyclic' }],
            capabilities: { textDocument: { diagnostic: {}, semanticTokens: {} } },
            initializationOptions: { zrSelectedProjectUri: projectUri },
        });
        assert(initialized?.capabilities?.documentSymbolProvider);
        client.notify('initialized', {});
        client.notify('textDocument/didOpen', { textDocument: {
            uri: documentUri, languageId: 'zr', version: 1,
            text: fs.readFileSync(path.join(sourceRoot, 'main.zr'), 'utf8'),
        } });

        const diagnostics = await request('textDocument/diagnostic', {
            textDocument: { uri: documentUri },
        });
        assert(Array.isArray(diagnostics?.items), 'Cyclic imports must return diagnostics');
        const symbols = await request('textDocument/documentSymbol', {
            textDocument: { uri: documentUri },
        });
        assert(Array.isArray(symbols), 'Cyclic imports must keep document symbols responsive');
        assert(symbols.some((symbol) => symbol.name === 'StructureHero'),
            'Document symbols must retain declarations beside a cyclic import');
        const tokens = await request('textDocument/semanticTokens/full', {
            textDocument: { uri: documentUri },
        });
        assert(Array.isArray(tokens?.data), 'Semantic tokens must remain responsive');
        assert(!client.closed, 'Language server must survive cyclic imports');
        console.log('Cyclic import stdio smoke passed');
    } finally {
        client.child.kill();
        fs.rmSync(projectRoot, { recursive: true, force: true });
    }
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
