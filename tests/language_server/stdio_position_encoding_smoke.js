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

function parseMessages(buffer) {
    const messages = [];
    let offset = 0;

    while (offset < buffer.length) {
        const headerEnd = buffer.indexOf(Buffer.from('\r\n\r\n', 'ascii'), offset);
        if (headerEnd < 0) {
            break;
        }

        const header = buffer.slice(offset, headerEnd).toString('ascii');
        const match = /Content-Length:\s*(\d+)/i.exec(header);
        assert(match, `Missing Content-Length header: ${header}`);

        const bodyStart = headerEnd + 4;
        const bodyLength = Number(match[1]);
        const bodyEnd = bodyStart + bodyLength;
        assert(bodyEnd <= buffer.length, 'Truncated LSP response body');

        messages.push(JSON.parse(buffer.slice(bodyStart, bodyEnd).toString('utf8')));
        offset = bodyEnd;
    }

    return messages;
}

function findResponse(messages, id) {
    return messages.find((message) => message && message.id === id);
}

function utf8ColumnForIndex(text, index) {
    return Buffer.byteLength(text.slice(0, index), 'utf8');
}

const serverPath = process.argv[2];

assert(serverPath, 'Expected stdio server executable path');

const documentUri = 'file:///zr-position-encoding-smoke.zr';
const documentText = '/* \u03bb */ var system = %import("zr.system");\n';
const importLiteralIndex = documentText.indexOf('"zr.system"');
const hoverIndex = documentText.indexOf('zr.system') + 1;
const expectedRangeStart = utf8ColumnForIndex(documentText, importLiteralIndex);
const expectedRangeEnd = expectedRangeStart + Buffer.byteLength('"zr.system"', 'utf8');

const payload = Buffer.concat([
    createMessage({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
            capabilities: {
                general: {
                    positionEncodings: ['utf-8', 'utf-16'],
                },
            },
        },
    }),
    createMessage({
        jsonrpc: '2.0',
        method: 'initialized',
        params: {},
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
        method: 'textDocument/hover',
        params: {
            textDocument: { uri: documentUri },
            position: {
                line: 0,
                character: utf8ColumnForIndex(documentText, hoverIndex),
            },
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
    timeout: 10000,
    windowsHide: true,
});

assert(result.status === 0,
    `Expected stdio server to exit cleanly, got status=${result.status} signal=${result.signal}`);

const messages = parseMessages(result.stdout);
const initialize = findResponse(messages, 1);
assert(initialize && initialize.result && initialize.result.capabilities,
    'initialize response missing capabilities');
assert(initialize.result.capabilities.positionEncoding === 'utf-8',
    `server must negotiate utf-8 positionEncoding, got ${JSON.stringify(
        initialize.result.capabilities.positionEncoding)}`);

const hover = findResponse(messages, 2);
assert(hover && hover.result && hover.result.range,
    `hover response missing range: ${JSON.stringify(hover)}`);
assert(hover.result.range.start.line === 0 && hover.result.range.end.line === 0,
    `hover range should stay on line 0: ${JSON.stringify(hover.result.range)}`);
assert(hover.result.range.start.character === expectedRangeStart,
    `hover range start must be UTF-8 byte column ${expectedRangeStart}, got ${
        hover.result.range.start.character}`);
assert(hover.result.range.end.character === expectedRangeEnd,
    `hover range end must be UTF-8 byte column ${expectedRangeEnd}, got ${
        hover.result.range.end.character}`);
