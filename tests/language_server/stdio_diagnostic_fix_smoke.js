const { spawnSync } = require('child_process');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

function parseMessages(output) {
    const messages = [];
    let offset = 0;

    while (offset < output.length) {
        const headerEnd = output.indexOf('\r\n\r\n', offset);
        if (headerEnd < 0) {
            break;
        }

        const header = output.slice(offset, headerEnd);
        const lengthMatch = /Content-Length: (\d+)/i.exec(header);
        assert(lengthMatch, `Malformed LSP header: ${header}`);

        const bodyStart = headerEnd + 4;
        const bodyEnd = bodyStart + Number(lengthMatch[1]);
        assert(bodyEnd <= output.length, 'Truncated LSP response body');
        messages.push(JSON.parse(output.slice(bodyStart, bodyEnd)));
        offset = bodyEnd;
    }

    return messages;
}

const serverPath = process.argv[2];
assert(serverPath, 'Expected stdio server executable path');

const documentUri = 'file:///zr-diagnostic-fix-smoke.zr';
const documentText = [
    'func choose(flag: bool): int {',
    '    var seed: int;',
    '    if (flag) {',
    '        seed = 1;',
    '    }',
    '    return seed;',
    '}',
    '',
].join('\n');

const payload = Buffer.concat([
    createMessage({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: { capabilities: {} },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'textDocument/didOpen',
        params: {
            textDocument: {
                uri: documentUri,
                languageId: 'zr',
                version: 1,
                text: documentText,
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 2,
        method: 'textDocument/documentSymbol',
        params: { textDocument: { uri: documentUri } },
    }),
    createMessage({ jsonrpc: '2.0', id: 3, method: 'shutdown', params: {} }),
    createMessage({ jsonrpc: '2.0', method: 'exit', params: {} }),
]);

const result = spawnSync(serverPath, [], {
    input: payload,
    encoding: 'utf8',
    timeout: 10000,
    windowsHide: true,
});

assert(result.status === 0,
    `Expected stdio server to exit cleanly, got status=${result.status} signal=${result.signal}`);

const messages = parseMessages(result.stdout);
const publication = messages.find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === documentUri &&
    Array.isArray(message.params.diagnostics) &&
    message.params.diagnostics.some((diagnostic) =>
        diagnostic.code === 'possibly_uninitialized_read'));
assert(publication, 'Expected possibly_uninitialized_read publication');

const diagnostic = publication.params.diagnostics.find((entry) =>
    entry.code === 'possibly_uninitialized_read');
assert(diagnostic.data && diagnostic.data.descriptorId === 3003,
    'Expected the registered descriptorId to survive Diagnostic.data serialization');
assert(Array.isArray(diagnostic.data.fixes) && diagnostic.data.fixes.length === 1,
    'Expected one serialized diagnostic fix');

const fix = diagnostic.data.fixes[0];
assert(fix.title === 'Replace with an initialized value',
    'Expected serialized diagnostic fix title');
assert(fix.edit && fix.edit.newText === '<value>',
    'Expected serialized placeholder edit text');
assert(fix.applicability === 2,
    'Expected HAS_PLACEHOLDERS applicability');
assert(fix.edit.range.start.line === 5 && fix.edit.range.start.character === 11 &&
    fix.edit.range.end.line === 5 && fix.edit.range.end.character === 15,
    'Expected serialized fix range for seed read');
