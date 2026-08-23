const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const REQUEST_TIMEOUT_MS = 10000;

function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (!fs.existsSync(targetPath)) {
        return;
    }
    if (fs.statSync(targetPath).isDirectory()) {
        for (const entry of fs.readdirSync(targetPath)) {
            removePathSync(path.join(targetPath, entry));
        }
        fs.rmdirSync(targetPath);
        return;
    }
    fs.unlinkSync(targetPath);
}

function createWorkspaceDiagnosticFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-task6-workspace-diagnostics-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'workspace_diagnostics.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const providerPath = path.join(sourcePath, 'provider.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'workspace_diagnostics',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    const mainText = [
        'module main;',
        'var provider = import("provider");',
        'pub fn entry(): int { return provider.value(); }',
        '',
    ].join('\n');
    const providerText = [
        'module provider;',
        'pub fn value(): int { return missing_provider_value; }',
        '',
    ].join('\n');
    fs.writeFileSync(mainPath, mainText);
    fs.writeFileSync(providerPath, providerText);

    return {
        rootPath,
        rootUri: pathToFileURL(rootPath + path.sep).toString(),
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        providerUri: pathToFileURL(providerPath).toString(),
        mainText,
        providerText,
    };
}

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'expected stdio server path');

    const client = new StdioProtocolClient(serverPath);
    const workspaceFixture = createWorkspaceDiagnosticFixture();
    try {
        const initialize = await client.requestWithId('initialize', {
            processId: null,
            workspaceFolders: [{ uri: workspaceFixture.rootUri, name: 'diagnostics' }],
            rootUri: workspaceFixture.rootUri,
            capabilities: {},
        }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(initialize.capabilities.diagnosticProvider.interFileDependencies, true);

        await assert.rejects(
            client.requestWithId('textDocument/diagnostic', {}, REQUEST_TIMEOUT_MS).promise,
            (error) => {
                const response = JSON.parse(error.message);
                return response.code === -32602;
            },
            'invalid document diagnostic params must return InvalidParams',
        );

        const uri = 'file:///tmp/zr-task6-diagnostic-generation.zr';
        const text = 'fn broken(): int { return missing; }\n';
        client.notify('textDocument/didOpen', {
            textDocument: { uri, languageId: 'zr', version: 1, text },
        });
        const first = await client.requestWithId(
            'textDocument/diagnostic', { textDocument: { uri } }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(first.kind, 'full');
        assert.equal(typeof first.resultId, 'string');
        assert(first.resultId.length > 0);

        const unchanged = await client.requestWithId('textDocument/diagnostic', {
            textDocument: { uri },
            previousResultId: first.resultId,
        }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(unchanged.kind, 'unchanged');
        assert.equal(unchanged.resultId, first.resultId);

        const emptyUri = 'file:///tmp/zr-task6-diagnostic-empty.zrp';
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: emptyUri,
                languageId: 'json',
                version: 1,
                text: '{ "name": "diagnostic-empty", "source": "src" }\n',
            },
        });
        const empty = await client.requestWithId(
            'textDocument/diagnostic', { textDocument: { uri: emptyUri } }, REQUEST_TIMEOUT_MS).promise;
        assert.equal(empty.kind, 'full');
        assert.equal(typeof empty.resultId, 'string');
        assert(empty.resultId.length > 0,
            'non-semantic documents must use their document snapshot for diagnostics identity');
        assert.deepEqual(empty.items, []);

        client.notify('workspace/didChangeWatchedFiles', {
            changes: [{ uri: workspaceFixture.projectUri, type: 1 }],
        });
        const workspace = await client.requestWithId(
            'workspace/diagnostic', {}, REQUEST_TIMEOUT_MS).promise;
        const providerReport = workspace.items.find((item) => item && item.uri === workspaceFixture.providerUri);
        const overlayReport = workspace.items.find((item) => item && item.uri === emptyUri);
        assert(providerReport, 'workspace diagnostics must include an unopened indexed source file');
        assert(overlayReport, 'workspace diagnostics must include an open document outside the project index');
        assert.equal(providerReport.version, null,
            'unopened indexed sources must not claim an editor document version');
        assert.equal(overlayReport.version, 1,
            'open overlay diagnostics must retain the editor document version');
        assert.equal(typeof providerReport.resultId, 'string');
        assert(providerReport.resultId.length > 0);

        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: workspaceFixture.mainUri,
                languageId: 'zr',
                version: 1,
                text: workspaceFixture.mainText,
            },
        });
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: workspaceFixture.providerUri,
                languageId: 'zr',
                version: 1,
                text: workspaceFixture.providerText,
            },
        });
        const dependencyBaseline = await client.requestWithId('textDocument/diagnostic', {
            textDocument: { uri: workspaceFixture.mainUri },
        }, REQUEST_TIMEOUT_MS).promise;
        client.notify('textDocument/didChange', {
            textDocument: { uri: workspaceFixture.providerUri, version: 2 },
            contentChanges: [{ text: workspaceFixture.providerText + '// dependency generation two\n' }],
        });
        const dependencyChanged = await client.requestWithId('textDocument/diagnostic', {
            textDocument: { uri: workspaceFixture.mainUri },
        }, REQUEST_TIMEOUT_MS).promise;
        assert.notEqual(dependencyChanged.resultId, dependencyBaseline.resultId,
            'dependency changes must invalidate a stable importer diagnostic resultId');
    } finally {
        await client.terminate();
        removePathSync(workspaceFixture.rootPath);
    }
}

main().catch((error) => {
    console.error(error.stack || error.message || String(error));
    process.exitCode = 1;
});
