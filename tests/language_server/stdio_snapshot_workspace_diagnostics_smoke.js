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

function createProject(rootPath, name, withProvider) {
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, `${name}.zrp`);
    const mainPath = path.join(sourcePath, 'main.zr');
    const providerPath = path.join(sourcePath, 'provider.zr');
    const mainText = withProvider
        ? [
            'module main;',
            'var provider = import("provider");',
            'pub fn entry(): int { return provider.value(); }',
            '',
        ].join('\n')
        : [
            'module main;',
            'pub fn independent(): int { return 1; }',
            '',
        ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name,
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, mainText);
    if (withProvider) {
        fs.writeFileSync(providerPath, [
            'module provider;',
            'pub fn value(): int { return 1; }',
            '',
        ].join('\n'));
    }

    return {
        rootUri: pathToFileURL(rootPath + path.sep).toString(),
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        providerUri: withProvider ? pathToFileURL(providerPath).toString() : null,
        providerPath,
    };
}

function reportForUri(workspaceReport, uri) {
    return workspaceReport.items.find((item) => item && item.uri === uri);
}

async function request(client, method, params) {
    return client.requestWithId(method, params, REQUEST_TIMEOUT_MS).promise;
}

async function main() {
    const serverPath = process.argv[2];
    const workspaceRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-task7-workspace-diagnostics-'));
    const first = createProject(path.join(workspaceRoot, 'first'), 'first', true);
    const second = createProject(path.join(workspaceRoot, 'second'), 'second', false);
    const client = new StdioProtocolClient(serverPath);

    assert(serverPath, 'expected stdio server path');
    try {
        await request(client, 'initialize', {
            processId: null,
            workspaceFolders: [
                { uri: first.rootUri, name: 'first' },
                { uri: second.rootUri, name: 'second' },
            ],
            capabilities: {},
        });
        client.notify('workspace/didChangeWatchedFiles', {
            changes: [
                { uri: first.projectUri, type: 1 },
                { uri: second.projectUri, type: 1 },
            ],
        });

        const initial = await request(client, 'workspace/diagnostic', {});
        const initialMain = reportForUri(initial, first.mainUri);
        const initialProvider = reportForUri(initial, first.providerUri);
        const secondMain = reportForUri(initial, second.mainUri);
        assert(initialMain && initialProvider && secondMain,
            'multi-root diagnostics must include both roots and the unopened provider source');
        assert.equal(initialMain.version, null,
            'unopened importer diagnostics must not claim an editor version');
        assert.equal(initialProvider.version, null,
            'unopened provider diagnostics must not claim an editor version');
        assert.equal(secondMain.version, null,
            'unopened diagnostics from the second root must not claim an editor version');
        assert.equal(typeof initialMain.resultId, 'string');
        assert(initialMain.resultId.length > 0,
            'unopened importer diagnostics must have a snapshot-backed resultId');

        fs.writeFileSync(first.providerPath, [
            'module provider;',
            'pub fn value(): int { return 1; }',
            '// provider reload generation two',
            '',
        ].join('\n'));
        client.notify('workspace/didChangeWatchedFiles', {
            changes: [{ uri: first.providerUri, type: 2 }],
        });

        const reloaded = await request(client, 'workspace/diagnostic', {
            previousResultIds: [{ uri: first.mainUri, value: initialMain.resultId }],
        });
        const reloadedMain = reportForUri(reloaded, first.mainUri);
        assert(reloadedMain && reloadedMain.resultId !== initialMain.resultId,
            'unopened provider reload must change the importer diagnostic resultId');
        console.log('stdio snapshot workspace diagnostics smoke: 7/7 Pass');
    } finally {
        await client.terminate();
        removePathSync(workspaceRoot);
    }
}

main().catch((error) => {
    console.error(error.stack || error.message || String(error));
    process.exitCode = 1;
});
