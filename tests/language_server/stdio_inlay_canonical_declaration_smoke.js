const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (fs.existsSync(targetPath)) {
        fs.rmdirSync(targetPath, { recursive: true });
    }
}

async function request(client, method, params, timeoutMs = 10000) {
    return client.requestWithId(method, params, timeoutMs).promise;
}

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath,
        'Usage: node stdio_inlay_canonical_declaration_smoke.js <serverPath>');

    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-inlay-declaration-'));
    const sourcePath = path.join(rootPath, 'src');
    const documentPath = path.join(sourcePath, 'inlay_declaration.zr');
    const documentUri = pathToFileURL(documentPath).toString();
    const text = [
        'fn run(): void {',
        '    var inferred = 1;',
        '    var explicit: int = 2;',
        '}',
        '',
    ].join('\n');
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;

    try {
        fs.mkdirSync(sourcePath, { recursive: true });
        fs.writeFileSync(documentPath, text);

        const initialize = await request(client, 'initialize', {
            processId: process.pid,
            rootUri: pathToFileURL(rootPath).toString(),
            capabilities: {},
        });
        assert(initialize && initialize.capabilities &&
            initialize.capabilities.inlayHintProvider,
        'inlayHintProvider must be enabled');

        client.notify('initialized', {});
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: documentUri,
                languageId: 'zr',
                version: 1,
                text,
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics', 10000);

        const hints = await request(client, 'textDocument/inlayHint', {
            textDocument: { uri: documentUri },
            range: {
                start: { line: 0, character: 0 },
                end: { line: 4, character: 0 },
            },
        });
        assert(Array.isArray(hints) && hints.length === 1,
            `expected one canonical inferred-local hint, got ${JSON.stringify(hints)}`);
        assert(typeof hints[0].label === 'string' && hints[0].label.startsWith(': int'),
            `expected canonical int label, got ${JSON.stringify(hints[0])}`);
        assert(hints[0].position && hints[0].position.line === 1 &&
            hints[0].position.character === 16,
        `expected inferred declaration hint at 1:16, got ${JSON.stringify(hints[0].position)}`);

        const shutdown = await request(client, 'shutdown', undefined);
        assert(shutdown === null, 'shutdown must return null');
        client.notify('exit', undefined);
        const exitCode = await client.waitForExit(10000);
        assert(exitCode === 0,
            `server exited with ${exitCode}. stderr=${client.stderr()}`);
        assert(client.stderr().trim() === '',
            `language server stderr must stay empty. stderr=${client.stderr()}`);
        cleanExit = true;
    } finally {
        if (!cleanExit && !client.closed) {
            await client.terminate();
        }
        removePathSync(rootPath);
    }
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
