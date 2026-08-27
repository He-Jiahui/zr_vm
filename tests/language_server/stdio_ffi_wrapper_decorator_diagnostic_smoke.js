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
const documentUri = 'file:///zr-ffi-wrapper-decorator-query-smoke.zr';
const documentText = [
    '#zr.ffi.ownerMode("shared")#',
    'class Handle {}',
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
assert(publication, 'Expected FFI wrapper decorator diagnostic publication');

const canonical = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'invalid_decorator');
assert(canonical.length === 1,
    `Expected one canonical invalid_decorator diagnostic: ${JSON.stringify(publication.params.diagnostics)}`);
assert(!publication.params.diagnostics.some((diagnostic) =>
    diagnostic.code === 'compiler_error'),
    'FFI wrapper decorator projection must not retain a parallel compiler_error');

const diagnostic = canonical[0];
assert(diagnostic.severity === 1 &&
    diagnostic.range.start.line === 0 &&
    diagnostic.range.start.character === 0 &&
    diagnostic.range.end.line === 0 &&
    diagnostic.range.end.character === 28,
    `Expected exact wrapper decorator range: ${JSON.stringify(diagnostic)}`);
assert(diagnostic.message.includes(
    'zr.ffi.ownerMode on class wrappers requires one of: borrowed, owned'),
    `Expected compiler-owned wrapper message: ${JSON.stringify(diagnostic)}`);
assert(diagnostic.data && diagnostic.data.descriptorId === 2019 &&
    diagnostic.data.noFixReason === 'requires_user_decision',
    `Expected descriptor 2019 and canonical no-fix reason: ${JSON.stringify(diagnostic)}`);
assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
    'Invalid FFI wrapper decorator diagnostics must not publish a machine fix');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected registered invalid-decorator help URI');
