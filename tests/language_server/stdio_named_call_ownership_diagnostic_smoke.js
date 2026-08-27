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
const serverArgs = process.argv.slice(3);
const documentUri = 'file:///zr-named-call-ownership-query-smoke.zr';
const documentText = [
    'resource class Resource {}',
    'fn consume(resource: Unique<Resource>) {}',
    'fn run(resource: Shared<Resource>) {',
    '    consume(resource);',
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

const result = spawnSync(serverPath, serverArgs, {
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
assert(publication, 'Expected named-call ownership diagnostic publication');

const diagnostics = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'ownership_mismatch');
assert(diagnostics.length === 1,
    `Expected exactly one canonical ownership mismatch: ${JSON.stringify(publication.params.diagnostics)}`);

const diagnostic = diagnostics[0];
assert(diagnostic.range &&
    diagnostic.range.start.line === 3 &&
    diagnostic.range.start.character === 12 &&
    diagnostic.range.end.line === 3 &&
    diagnostic.range.end.character === 20,
`Expected the argument token range: ${JSON.stringify(diagnostic)}`);
assert(diagnostic.severity === 1 &&
    diagnostic.message ===
        'Ownership qualifier mismatch\n' +
        'Cause: Actual value has type Shared<Resource>, but the target requires Unique<Resource>.\n' +
        'Suggestion: Provide a Unique<Resource> value, use an ownership builtin, or change the target annotation to match.',
`Expected canonical ownership mismatch payload: ${JSON.stringify(diagnostic)}`);
assert(diagnostic.data && diagnostic.data.descriptorId === 2008 &&
    diagnostic.data.noFixReason === 'requires_user_decision',
`Expected descriptor 2008 and no-fix disposition: ${JSON.stringify(diagnostic)}`);
assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
    'Ownership mismatch diagnostics must not publish a machine fix');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
'Expected registered diagnostic help URI');
