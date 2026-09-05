const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { fileURLToPath, pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const RESPONSE_TIMEOUT_MS = 5000;
const SYMBOL_NAME = 'file_operation_value';

function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (!fs.existsSync(targetPath)) {
        return;
    }
    if (fs.statSync(targetPath).isDirectory()) {
        fs.readdirSync(targetPath).forEach((entry) => {
            removePathSync(path.join(targetPath, entry));
        });
        fs.rmdirSync(targetPath);
        return;
    }
    fs.unlinkSync(targetPath);
}

function comparableUri(uri) {
    const nativePath = fileURLToPath(uri);
    return process.platform === 'win32' ? nativePath.toLowerCase() : nativePath;
}

function writeProject(rootPath) {
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'file_operations.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const providerPath = path.join(sourcePath, 'legacy.zr');
    const renamedProviderPath = path.join(sourcePath, 'modern.zr');
    const mainText = [
        'var legacy = import("legacy");',
        `var cached = legacy.${SYMBOL_NAME}();`,
        'return cached;',
        '',
    ].join('\n');
    const providerText = [
        'module legacy;',
        `pub fn ${SYMBOL_NAME}(): int {`,
        '    return 1;',
        '}',
        '',
    ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'file_operations',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, mainText);
    fs.writeFileSync(providerPath, providerText);
    return {
        projectPath,
        mainPath,
        providerPath,
        renamedProviderPath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        providerUri: pathToFileURL(providerPath).toString(),
        renamedProviderUri: pathToFileURL(renamedProviderPath).toString(),
        mainText,
        providerText,
    };
}

async function request(client, method, params) {
    return client.requestWithId(method, params, RESPONSE_TIMEOUT_MS).promise;
}

async function assertWorkspaceSymbol(client, expectedUri, message) {
    const symbols = await request(client, 'workspace/symbol', { query: SYMBOL_NAME });
    assert(Array.isArray(symbols), `${message}: ${JSON.stringify(symbols)}`);
    const locations = symbols.filter((symbol) => symbol.name === SYMBOL_NAME)
        .map((symbol) => comparableUri(symbol.location.uri));
    assert.deepStrictEqual(locations, expectedUri === null ? [] : [comparableUri(expectedUri)], message);
}

function assertRenameEdit(edit, fixture, mainVersion, importLine) {
    assert(edit && Array.isArray(edit.documentChanges),
        `willRenameFiles must return documentChanges: ${JSON.stringify(edit)}`);
    const actual = edit.documentChanges.map((change) => ({
        uri: comparableUri(change.textDocument.uri),
        version: change.textDocument.version,
        edits: change.edits,
    })).sort((left, right) => left.uri.localeCompare(right.uri));
    const expected = [
        {
            uri: comparableUri(fixture.mainUri),
            version: mainVersion,
            edits: [{
                range: {
                    start: { line: importLine, character: 21 },
                    end: { line: importLine, character: 27 },
                },
                newText: 'modern',
            }],
        },
        {
            uri: comparableUri(fixture.providerUri),
            version: null,
            edits: [{
                range: {
                    start: { line: 0, character: 7 },
                    end: { line: 0, character: 13 },
                },
                newText: 'modern',
            }],
        },
    ].sort((left, right) => left.uri.localeCompare(right.uri));
    assert.deepStrictEqual(actual, expected,
        'willRenameFiles must edit exactly the import target and module declaration at their current versions');
}

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Expected stdio server executable path');
    const workspaceRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-lsp-file-operations-'));
    const client = new StdioProtocolClient(serverPath);

    try {
        const initialized = await request(client, 'initialize', {
            processId: null,
            rootUri: pathToFileURL(workspaceRoot + path.sep).toString(),
            capabilities: {
                workspace: {
                    workspaceEdit: { documentChanges: true },
                    fileOperations: {
                        willCreate: true,
                        didCreate: true,
                        willRename: true,
                        didRename: true,
                        willDelete: true,
                        didDelete: true,
                    },
                },
            },
        });
        client.notify('initialized', {});
        const registrations = initialized.capabilities.workspace.fileOperations;
        for (const method of ['didCreate', 'didDelete', 'didRename', 'willRename']) {
            assert.deepStrictEqual(registrations[method], {
                filters: [{ pattern: { glob: '**/*.{zr,zrp,zro,dll,so,dylib}' } }],
            }, `${method} must retain its file-operation registration`);
        }

        const withdrawnResponses = [];
        for (const method of ['workspace/willCreateFiles', 'workspace/willDeleteFiles']) {
            const response = await client.request(method, {
                files: [{ uri: pathToFileURL(path.join(workspaceRoot, 'future.zr')).toString() }],
            }, method, RESPONSE_TIMEOUT_MS);
            withdrawnResponses.push(response);
            console.log(`${method}: ${JSON.stringify(response)}`);
        }

        await assertWorkspaceSymbol(client, null,
            'the workspace must start without the future project symbol');
        const fixture = writeProject(workspaceRoot);
        client.notify('workspace/didCreateFiles', {
            files: [{ uri: fixture.projectUri }],
        });
        await assertWorkspaceSymbol(client, fixture.providerUri,
            'didCreateFiles must index newly created unopened project sources');

        client.notify('textDocument/didOpen', {
            textDocument: { uri: fixture.mainUri, languageId: 'zr', version: 7, text: fixture.mainText },
        });
        const renameParams = {
            files: [{ oldUri: fixture.providerUri, newUri: fixture.renamedProviderUri }],
        };
        assertRenameEdit(await request(client, 'workspace/willRenameFiles', renameParams), fixture, 7, 0);

        const changedMainText = '// current overlay\n' + fixture.mainText;
        client.notify('textDocument/didChange', {
            textDocument: { uri: fixture.mainUri, version: 8 },
            contentChanges: [{ text: changedMainText }],
        });
        assertRenameEdit(await request(client, 'workspace/willRenameFiles', renameParams), fixture, 8, 1);

        fs.writeFileSync(fixture.providerPath, fixture.providerText.replace('return 1;', 'return 2;'));
        assert.strictEqual(await request(client, 'workspace/willRenameFiles', renameParams), null,
            'willRenameFiles must reject unopened disk content that no longer matches the cached snapshot');
        fs.writeFileSync(fixture.providerPath, fixture.providerText);
        assertRenameEdit(await request(client, 'workspace/willRenameFiles', renameParams), fixture, 8, 1);
        assert.strictEqual(await request(client, 'workspace/willRenameFiles', {
            files: [{ oldUri: fixture.providerUri, newUri: fixture.providerUri }],
        }), null, 'a same-URI willRenameFiles request must not produce edits');

        const renamedMainText = changedMainText.replace('import("legacy")', 'import("modern")');
        fs.writeFileSync(fixture.mainPath, renamedMainText);
        fs.renameSync(fixture.providerPath, fixture.renamedProviderPath);
        fs.writeFileSync(fixture.renamedProviderPath,
            fixture.providerText.replace('module legacy;', 'module modern;'));
        client.notify('workspace/didRenameFiles', renameParams);
        client.notify('textDocument/didChange', {
            textDocument: { uri: fixture.mainUri, version: 9 },
            contentChanges: [{ text: renamedMainText }],
        });
        await assertWorkspaceSymbol(client, fixture.renamedProviderUri,
            'didRenameFiles must move the source symbol to the new URI without retaining the old index');
        const definitions = await request(client, 'textDocument/definition', {
            textDocument: { uri: fixture.mainUri },
            position: { line: 2, character: 20 },
        });
        assert(Array.isArray(definitions), `Expected definition locations: ${JSON.stringify(definitions)}`);
        assert.deepStrictEqual(definitions.map((location) => comparableUri(location.uri)),
            [comparableUri(fixture.renamedProviderUri)],
            'didRenameFiles must resolve the updated import to the renamed provider');

        client.notify('textDocument/didClose', { textDocument: { uri: fixture.mainUri } });
        fs.unlinkSync(fixture.projectPath);
        client.notify('workspace/didDeleteFiles', { files: [{ uri: fixture.projectUri }] });
        await assertWorkspaceSymbol(client, null,
            'didDeleteFiles must remove the deleted project index');

        assert.deepStrictEqual({
            registrations: Object.keys(registrations).sort(),
            withdrawnResponses,
        }, {
            registrations: ['didCreate', 'didDelete', 'didRename', 'willRename'],
            withdrawnResponses: ['workspace/willCreateFiles', 'workspace/willDeleteFiles'].map((method) => ({
                jsonrpc: '2.0',
                id: method,
                error: { code: -32601, message: 'Method not found' },
            })),
        }, 'null-only file-operation requests must be unregistered and return exact MethodNotFound envelopes');

        await request(client, 'shutdown', {});
        client.notify('exit', {});
        const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
        assert.strictEqual(exitCode, 0, `Expected clean server exit: ${client.stderr()}`);
        console.log('stdio file operation capabilities smoke: Pass');
    } finally {
        if (!client.closed) {
            await client.terminate();
        }
        removePathSync(workspaceRoot);
    }
}

main().catch((error) => {
    console.error(`stdio file operation capabilities smoke failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
