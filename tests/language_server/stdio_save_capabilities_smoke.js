const assert = require('assert').strict;
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const TIMEOUT_MS = 10000;
const URI = 'file:///save-capability-contract.zr';
const SOURCE = 'fn saveProbe(): int {\nreturn 42;\n}\n';
const FORMATTED = 'fn saveProbe(): int {\n    return 42;\n}\n';
const EDIT = {
    range: { start: { line: 0, character: 0 }, end: { line: 3, character: 0 } },
    newText: FORMATTED,
};

async function withDiskDocument(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-lsp-save-'));
    const filePath = path.join(directory, 'saved.zr');
    try {
        fs.writeFileSync(filePath, 'class BeforeSave { }\n', 'utf8');
        await run(filePath, pathToFileURL(filePath).href);
    } finally {
        if (fs.existsSync(filePath)) fs.unlinkSync(filePath);
        fs.rmdirSync(directory);
    }
}

async function withClient(serverPath, capabilities, run) {
    const client = new StdioProtocolClient(serverPath);
    let cleanExit = false;
    try {
        const response = await client.request('initialize', { capabilities }, 'initialize', TIMEOUT_MS);
        assert.equal(response.jsonrpc, '2.0');
        assert.equal(response.id, 'initialize');
        assert.equal(response.error, undefined);
        assert.ok(response.result && response.result.capabilities);
        client.notify('initialized', {});
        await run(client, response.result.capabilities);
        assert.deepEqual(await client.request('shutdown', undefined, 'shutdown', TIMEOUT_MS), {
            jsonrpc: '2.0', id: 'shutdown', result: null,
        });
        client.notify('exit');
        client.endInput();
        assert.equal(await client.waitForExit(TIMEOUT_MS), 0);
        assert.equal(client.stderr().trim(), '');
        cleanExit = true;
    } finally {
        if (!cleanExit) await client.terminate();
    }
}

async function main() {
    const serverPath = process.argv[2];
    assert.ok(serverPath, 'usage: node stdio_save_capabilities_smoke.js <stdio-server>');
    const profiles = [
        ['baseline', {}],
        ['save-aware', { textDocument: { synchronization: { willSave: true, willSaveWaitUntil: true } } }],
    ];
    let failures = 0;
    let checks = 0;
    const check = async (name, run) => {
        checks++;
        try {
            await run();
            console.log(`Pass - ${name}`);
        } catch (error) {
            failures++;
            console.error(`Fail - ${name}\n${error.stack || String(error)}`);
        }
    };
    for (const [name, capabilities] of profiles) {
        await check(`${name} save notification publication`, () => withClient(serverPath, capabilities,
            async (_client, advertised) => {
                assert.deepEqual(advertised.textDocumentSync, {
                    openClose: true, change: 2, willSaveWaitUntil: true, save: { includeText: false },
                });
            }));
        await check(`${name} retained save formatting`, () => withClient(serverPath, capabilities,
            async (client) => {
                client.notify('textDocument/didOpen', {
                    textDocument: { uri: URI, languageId: 'zr', version: 1, text: SOURCE },
                });
                const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics', TIMEOUT_MS);
                assert.equal(diagnostics.uri, URI);
                assert.equal(diagnostics.version, 1);
                assert.deepEqual(diagnostics.diagnostics, []);
                assert.deepEqual(await client.request('textDocument/willSaveWaitUntil', {
                    textDocument: { uri: URI }, reason: 1,
                }, 'before-save', TIMEOUT_MS), { jsonrpc: '2.0', id: 'before-save', result: [EDIT] });
                client.notify('textDocument/didChange', {
                    textDocument: { uri: URI, version: 2 }, contentChanges: [{ text: FORMATTED }],
                });
                const changed = await client.waitForNotification('textDocument/publishDiagnostics', TIMEOUT_MS);
                assert.equal(changed.uri, URI);
                assert.equal(changed.version, 2);
                assert.deepEqual(changed.diagnostics, []);
                client.notify('textDocument/didSave', { textDocument: { uri: URI } });
                assert.deepEqual(await client.request('textDocument/willSaveWaitUntil', {
                    textDocument: { uri: URI }, reason: 1,
                }, 'after-save', TIMEOUT_MS), { jsonrpc: '2.0', id: 'after-save', result: [] });
                assert.deepEqual(await client.request('textDocument/definition', {
                    textDocument: { uri: URI }, position: { line: 0, character: 5 },
                }, 'definition', TIMEOUT_MS), {
                    jsonrpc: '2.0', id: 'definition', result: [{ uri: URI, range: {
                        start: { line: 0, character: 3 }, end: { line: 0, character: 12 },
                    } }],
                });
            }));
        await check(`${name} disk save refresh`, () => withDiskDocument(async (filePath, diskUri) => {
            await withClient(serverPath, capabilities, async (client) => {
                const definition = async (id, end) => {
                    assert.deepEqual(await client.request('textDocument/definition', {
                        textDocument: { uri: diskUri }, position: { line: 0, character: 8 },
                    }, id, TIMEOUT_MS), {
                        jsonrpc: '2.0', id, result: [{ uri: diskUri, range: {
                            start: { line: 0, character: 6 }, end: { line: 0, character: end },
                        } }],
                    });
                };
                const save = async () => {
                    client.notify('textDocument/didSave', { textDocument: { uri: diskUri } });
                    const published = await client.waitForNotification('textDocument/publishDiagnostics', TIMEOUT_MS);
                    assert.equal(published.uri, diskUri);
                    assert.deepEqual(published.diagnostics, []);
                    return published.version;
                };
                const beforeVersion = await save();
                await definition('before-disk-save', 16);
                fs.writeFileSync(filePath, 'class AfterSave { }\n', 'utf8');
                const afterVersion = await save();
                assert.ok(Number.isInteger(beforeVersion) && afterVersion === beforeVersion + 1,
                          'didSave must publish the next disk snapshot generation');
                await definition('after-disk-save', 15);
            });
        }));
    }
    assert.equal(failures, 0, `${failures}/${checks} save capability checks failed`);
    console.log(`Pass - ${checks}/${checks} save capability checks`);
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exitCode = 1;
});
