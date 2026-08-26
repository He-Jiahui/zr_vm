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
const documentUri = 'file:///zr-const-assignment-query-smoke.zr';
const documentText = [
    'class Meter {',
    '    pub const value: int;',
    '    pub @constructor(seed: int) {',
    '        this.value = seed;',
    '    }',
    '    pub fn update(next: int) {',
    '        this.value = next;',
    '    }',
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
assert(publication, 'Expected const assignment diagnostic publication');

const canonical = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'const_assignment');
assert(canonical.length === 1,
    'Expected exactly one canonical const_assignment diagnostic');

const diagnostic = canonical[0];
assert(diagnostic.severity === 1 &&
    diagnostic.message ===
        "Cannot assign to immutable field 'value' outside initialization\n" +
        "Cause: An immutable instance field can only initialize the current instance inside its declaring type's constructor.\n" +
        'Suggestion: Move the assignment into the constructor or make the field mutable.',
    'Expected canonical const assignment severity and message');
assert(diagnostic.range.start.line === 6 &&
    diagnostic.range.start.character === 8 &&
    diagnostic.range.end.line === 6 &&
    diagnostic.range.end.character === 25,
    'Expected primary range on the normal-method assignment');
assert(!canonical.some((entry) => entry.range.start.line === 3),
    'Constructor initialization must not publish const_assignment');
assert(Array.isArray(diagnostic.relatedInformation) &&
    diagnostic.relatedInformation.length === 1 &&
    diagnostic.relatedInformation[0].location &&
    diagnostic.relatedInformation[0].location.uri === documentUri &&
    diagnostic.relatedInformation[0].location.range.start.line === 1 &&
    diagnostic.relatedInformation[0].location.range.start.character === 14 &&
    diagnostic.relatedInformation[0].location.range.end.line === 1 &&
    diagnostic.relatedInformation[0].location.range.end.character === 19,
    'Expected related information on the immutable field declaration');
assert(diagnostic.data && diagnostic.data.descriptorId === 2012 &&
    diagnostic.data.noFixReason === 'requires_user_decision',
    'Expected canonical descriptor and no-fix disposition');
assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
    'Const assignment must not publish a machine fix');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected registered const-assignment help URI');
