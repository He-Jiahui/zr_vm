const assert = require('assert').strict;
const { StdioProtocolClient } = require('./stdio_protocol_client');

const REQUEST_TIMEOUT_MS = 10000;
const URI = 'file:///stdio-color-capability.zr';
const TEXT = [
    'var accent = "#336699";',
    '// #112233 accent',
    '/* #445566 accent */',
    'var ordinary = accent;',
    'var count = 7;',
    'var copied = count;',
    '',
].join('\n');

function range(line, start, end) {
    return { start: { line, character: start }, end: { line, character: end } };
}

function response(id, result) {
    return { jsonrpc: '2.0', id, result };
}

function methodNotFound(id) {
    return { jsonrpc: '2.0', id, error: { code: -32601, message: 'Method not found' } };
}

function hover(value, hoverRange) {
    return { contents: { kind: 'markdown', value }, range: hoverRange };
}

function request(client, method, params, id) {
    return client.request(method, params, id, REQUEST_TIMEOUT_MS);
}

async function withClient(serverPath, run) {
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;
    try {
        await run(client);
        assert.deepEqual(await request(client, 'shutdown', undefined, 'shutdown'), response('shutdown', null));
        client.notify('exit');
        client.endInput();
        assert.equal(await client.waitForExit(REQUEST_TIMEOUT_MS), 0, client.stderr());
        assert.equal(client.stderr().trim(), '', 'stdio stderr must remain empty');
        cleanExit = true;
    } finally {
        if (!cleanExit) await client.terminate();
    }
}

async function checkProfile(client, capabilities, check) {
    const initialized = await request(client, 'initialize', { capabilities }, 'initialize');
    assert.equal(initialized.jsonrpc, '2.0');
    assert.equal(initialized.id, 'initialize');
    assert.equal(initialized.error, undefined);
    assert.ok(initialized.result && initialized.result.capabilities);
    client.notify('initialized', {});
    client.notify('textDocument/didOpen', {
        textDocument: { uri: URI, languageId: 'zr', version: 1, text: TEXT },
    });
    const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics', REQUEST_TIMEOUT_MS);
    await check('valid string and comment source', async () => {
        assert.deepEqual(diagnostics, { uri: URI, version: 1, diagnostics: [] });
    });
    await check('colorProvider is absent', async () => {
        assert.equal(Object.prototype.hasOwnProperty.call(initialized.result.capabilities, 'colorProvider'), false,
                     'untyped colorProvider must be absent from initialize');
    });
    await check('documentColor is MethodNotFound', async () => {
        const id = 'document-color';
        assert.deepEqual(await request(client, 'textDocument/documentColor', { textDocument: { uri: URI } }, id),
                         methodNotFound(id));
    });
    for (const [name, selectedRange] of [
        ['string', range(0, 14, 21)],
        ['line comment', range(1, 3, 10)],
        ['block comment', range(2, 3, 10)],
        ['ordinary identifier', range(4, 4, 9)],
    ]) {
        await check(`colorPresentation for ${name} is MethodNotFound`, async () => {
            const id = `color-presentation-${name}`;
            assert.deepEqual(await request(client, 'textDocument/colorPresentation', {
                textDocument: { uri: URI }, color: { red: 0.2, green: 0.4, blue: 0.6, alpha: 1 },
                range: selectedRange,
            }, id), methodNotFound(id));
        });
    }
    const queries = [
        ['hex text remains an ordinary string literal', 'textDocument/hover', { line: 0, character: 16 },
            hover('**expression**\n\nType: string\n\nExpression: literal exact\n\nConstant: "#336699"',
                  range(0, 13, 22))],
        ['string variable hover retains its exact type and range', 'textDocument/hover', { line: 3, character: 17 },
            hover('**variable**: accent\n\nResolved Type: string\n\nExpression: identifier exact\n\n' +
                  'Reference: read\n\nSymbol: accent\n\nDeclared at: 1:5', range(3, 15, 21))],
        ['string variable definition retains its exact target', 'textDocument/definition', { line: 3, character: 17 },
            [{ uri: URI, range: range(0, 4, 10) }]],
        ['ordinary identifier hover retains its value and type', 'textDocument/hover', { line: 5, character: 15 },
            hover('**variable**: count\n\nResolved Type: int\n\nExpression: identifier exact\n\n' +
                  'Numeric range: 7..7\n\nUnsigned range: 7..7\n\nReference: read\n\n' +
                  'Symbol: count\n\nDeclared at: 5:5', range(5, 13, 18))],
        ['ordinary identifier definition retains its exact target', 'textDocument/definition', { line: 5, character: 15 },
            [{ uri: URI, range: range(4, 4, 9) }]],
        ['line comment does not become a symbol reference', 'textDocument/definition', { line: 1, character: 13 }, []],
        ['block comment does not become a symbol reference', 'textDocument/definition', { line: 2, character: 13 }, []],
    ];
    for (const [name, method, position, result] of queries) {
        await check(name, async () => {
            assert.deepEqual(await request(client, method, { textDocument: { uri: URI }, position }, name),
                             response(name, result));
        });
    }
}

async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_color_capability_smoke.js <stdio-server>');
    let checks = 0;
    let failures = 0;
    for (const [profile, capabilities] of [
        ['empty client', {}],
        ['explicit color client', { textDocument: { colorProvider: { dynamicRegistration: false } } }],
    ]) {
        const check = async (name, run) => {
            checks++;
            try {
                await run();
                console.log(`Pass - ${profile}: ${name}`);
            } catch (error) {
                failures++;
                console.error(`Fail - ${profile}: ${name}\n${error.stack || String(error)}`);
            }
        };
        await check('protocol lifecycle', async () => withClient(serverPath, async (client) => {
            await checkProfile(client, capabilities, check);
        }));
    }
    assert.equal(failures, 0, `${failures}/${checks} color capability checks failed`);
    console.log(`Pass - ${checks}/${checks} color capability checks`);
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
