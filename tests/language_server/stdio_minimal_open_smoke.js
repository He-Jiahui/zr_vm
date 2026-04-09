const { spawnSync } = require('node:child_process');

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

const serverPath = process.argv[2];

assert(serverPath, 'Expected stdio server executable path');

const documentUri = 'file:///zr-minimal-open-smoke.zr';
const documentText = [
    'var value = 1;',
    'var other = value + 1;',
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
        params: {
            textDocument: { uri: documentUri },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        id: 3,
        method: 'shutdown',
        params: {},
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'exit',
        params: {},
    }),
]);

const result = spawnSync(serverPath, [], {
    input: payload,
    encoding: 'utf8',
    timeout: 10000,
    windowsHide: true,
});

assert(result.status === 0, `Expected stdio server to exit cleanly, got status=${result.status} signal=${result.signal}`);
assert(typeof result.stdout === 'string' && result.stdout.includes('"method":"textDocument/publishDiagnostics"'),
    'Expected didOpen to publish diagnostics so live editor feedback stays in sync');
assert(typeof result.stdout === 'string' && result.stdout.includes('"id":2'),
    'Expected documentSymbol response after didOpen');
assert(result.stdout.includes('"id":3'),
    'Expected shutdown response after didOpen');
