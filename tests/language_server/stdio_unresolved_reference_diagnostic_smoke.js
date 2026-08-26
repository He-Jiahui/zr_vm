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

function assertRange(range, startLine, startCharacter, endLine, endCharacter) {
    assert(range &&
        range.start.line === startLine &&
        range.start.character === startCharacter &&
        range.end.line === endLine &&
        range.end.character === endCharacter,
    `Unexpected unresolved member range: ${JSON.stringify(range)}`);
}

const serverPath = process.argv[2];
const serverArgs = process.argv.slice(3);
const documentUri = 'file:///zr-unresolved-reference-query-smoke.zr';
const documentText = [
    'class Meter {',
    '    var value: int;',
    '}',
    'fn read(meter: Meter): int {',
    '    return meter.missingField;',
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
assert(publication, 'Expected unresolved-reference diagnostic publication');

const diagnostics = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'member_not_found');
assert(diagnostics.length === 1,
    `Expected exactly one canonical member diagnostic: ${JSON.stringify(publication.params.diagnostics)}`);

const diagnostic = diagnostics[0];
const memberRangeDiagnostics = publication.params.diagnostics.filter((item) =>
    item.range &&
    item.range.start.line === 4 &&
    item.range.start.character === 17 &&
    item.range.end.line === 4 &&
    item.range.end.character === 29);
assert(memberRangeDiagnostics.length === 1 &&
    memberRangeDiagnostics[0].code === 'member_not_found',
`Expected the member token to have one canonical diagnostic: ${JSON.stringify(publication.params.diagnostics)}`);
assert(diagnostic.severity === 1 &&
    diagnostic.message ===
        "Member 'missingField' could not be resolved\n" +
        'Cause: Canonical member binding found no field, property, or method for this reference.\n' +
        "Suggestion: Declare the member or use a member exposed by the receiver's canonical type.",
`Expected canonical member diagnostic: ${JSON.stringify(diagnostic)}`);
assertRange(diagnostic.range, 4, 17, 4, 29);
assert(diagnostic.data && diagnostic.data.descriptorId === 2016 &&
    diagnostic.data.noFixReason === 'requires_user_decision',
`Expected descriptor 2016 and no-fix disposition: ${JSON.stringify(diagnostic)}`);
assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
    'Unresolved member diagnostics must not publish a machine fix');
assert(diagnostic.codeDescription &&
    diagnostic.codeDescription.href ===
        'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
'Expected registered diagnostic help URI');
