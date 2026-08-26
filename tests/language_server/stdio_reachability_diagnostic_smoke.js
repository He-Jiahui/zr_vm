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
const documentUri = 'file:///zr-reachability-query-smoke.zr';
const documentText = [
    'fn choose(): int {',
    '    var skipped = true || false;',
    '    return 0;',
    '}',
    '',
].join('\n');

assert(serverPath, 'Expected stdio server executable path');

const payload = Buffer.concat([
    createMessage({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: { capabilities: {} },
    }),
    createMessage({ jsonrpc: '2.0', method: 'initialized', params: {} }),
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
    `Expected stdio server exit zero, got status=${result.status} signal=${result.signal}`);

const publication = parseMessages(result.stdout).find((message) =>
    message.method === 'textDocument/publishDiagnostics' &&
    message.params &&
    message.params.uri === documentUri &&
    message.params.version === 1 &&
    Array.isArray(message.params.diagnostics));
assert(publication, 'Expected reachability diagnostic publication');

const canonical = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'unreachable_code');
assert(canonical.length === 1,
    'Expected exactly one canonical unreachable_code diagnostic');
assert(!publication.params.diagnostics.some((diagnostic) =>
    diagnostic.code === 'short_circuit_unreachable'),
    'Legacy analyzer-owned short-circuit diagnostic must not be published');

const diagnostic = canonical[0];
assert(diagnostic.severity === 2 && diagnostic.message.includes('Unreachable code'),
    'Expected canonical warning severity and message');
assert(diagnostic.range.start.line === 1 &&
    diagnostic.range.start.character === 26 &&
    diagnostic.range.end.line === 1 &&
    diagnostic.range.end.character === 31,
    'Expected primary range on the unreachable right operand');
assert(diagnostic.data && diagnostic.data.noFixReason === 'unsafe_edit',
    'Expected canonical unsafe-edit no-fix reason');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected registered unreachable-code help URI');
