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

function assertRange(range, startLine, startCharacter, endLine, endCharacter,
    label) {
    assert(range &&
        range.start.line === startLine &&
        range.start.character === startCharacter &&
        range.end.line === endLine &&
        range.end.character === endCharacter,
    `${label}: ${JSON.stringify(range)}`);
}

function assertCanonicalDiagnostic(diagnostic, expectedRange, suggestion) {
    assert(diagnostic.severity === 1 &&
        diagnostic.message ===
            "Interface const field 'version' must remain const in implementing class\n" +
            'Cause: The interface requires this field to preserve const access in every implementation.\n' +
            `Suggestion: ${suggestion}`,
    `Expected canonical severity and message: ${JSON.stringify(diagnostic)}`);
    assertRange(diagnostic.range, ...expectedRange, 'Unexpected primary range');
    assert(Array.isArray(diagnostic.relatedInformation) &&
        diagnostic.relatedInformation.length === 1 &&
        diagnostic.relatedInformation[0].location &&
        diagnostic.relatedInformation[0].location.uri === documentUri,
    `Expected one related interface declaration: ${JSON.stringify(diagnostic)}`);
    assertRange(diagnostic.relatedInformation[0].location.range,
        1, 14, 1, 21, 'Unexpected related interface field range');
    assert(diagnostic.relatedInformation[0].message ===
        'Const field is required by this interface declaration',
    'Expected canonical related-information message');
    assert(diagnostic.data && diagnostic.data.descriptorId === 2014 &&
        diagnostic.data.noFixReason === 'requires_user_decision',
    `Expected descriptor 2014 and no-fix disposition: ${JSON.stringify(diagnostic)}`);
    assert(!diagnostic.data.fixes || diagnostic.data.fixes.length === 0,
        'Interface const-field diagnostics must not publish a machine fix');
    assert(diagnostic.codeDescription &&
        diagnostic.codeDescription.href ===
            'https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md',
    'Expected registered diagnostic help URI');
}

const serverPath = process.argv[2];
const serverArgs = process.argv.slice(3);
const documentUri = 'file:///zr-interface-const-field-query-smoke.zr';
const documentText = [
    'interface Versioned {',
    '    pub const version: int;',
    '}',
    'class MutableVersion: Versioned {',
    '    pub var version: int;',
    '}',
    'class MissingVersion: Versioned {',
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
assert(publication, 'Expected interface const-field diagnostic publication');

const canonical = publication.params.diagnostics.filter((diagnostic) =>
    diagnostic.code === 'const_interface_mismatch');
assert(canonical.length === 2,
    `Expected exactly two canonical diagnostics: ${JSON.stringify(publication.params.diagnostics)}`);

assertCanonicalDiagnostic(
    canonical[0], [4, 12, 4, 19], 'Mark the implementing field const.');
assertCanonicalDiagnostic(
    canonical[1], [6, 6, 6, 20],
    'Declare a const field with the required name and type.');
