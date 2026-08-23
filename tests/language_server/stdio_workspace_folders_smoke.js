const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const RESPONSE_TIMEOUT_MS = 5000;

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

function writeProject(rootPath, projectFileName, symbolName) {
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, projectFileName);
    const mainPath = path.join(sourcePath, 'main.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: symbolName,
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, [
        'module main;',
        '',
        `pub fn ${symbolName}(): int {`,
        '    return 1;',
        '}',
        '',
    ].join('\n'));

    return {
        rootPath,
        projectPath,
        mainPath,
        rootUri: pathToFileURL(rootPath + path.sep).toString(),
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        symbolName,
    };
}

async function request(client, method, params) {
    return client.requestWithId(method, params, RESPONSE_TIMEOUT_MS).promise;
}

async function assertWorkspaceSymbol(client, symbolName, expected, message) {
    const result = await request(client, 'workspace/symbol', { query: symbolName });
    const found = Array.isArray(result) && result.some((item) => item && item.name === symbolName);
    assert(found === expected, message + ': ' + JSON.stringify(result));
}

async function shutdown(client) {
    await request(client, 'shutdown', {});
    client.notify('exit', {});
    const exitCode = await client.waitForExit(RESPONSE_TIMEOUT_MS);
    assert(exitCode === 0, `Expected clean server exit, got ${exitCode}: ${client.stderr()}`);
}

async function main() {
    const serverPath = process.argv[2];
    const workspaceRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-lsp-workspace-folders-'));
    const first = writeProject(path.join(workspaceRoot, 'first'), 'shared.zrp', 'from_first_root');
    const second = writeProject(path.join(workspaceRoot, 'second'), 'shared.zrp', 'from_second_root');
    const nested = writeProject(path.join(first.rootPath, 'nested'), 'shared.zrp', 'from_nested_root');
    const outside = writeProject(path.join(workspaceRoot, 'outside'), 'outside.zrp', 'from_outside_root');
    const client = new StdioProtocolClient(serverPath);

    assert(serverPath, 'Expected stdio server executable path');

    try {
        const initializeResult = await request(client, 'initialize', {
            processId: null,
            workspaceFolders: [
                { uri: first.rootUri, name: 'first' },
                { uri: second.rootUri, name: 'second' },
            ],
            rootUri: outside.rootUri,
            rootPath: outside.rootPath,
            initializationOptions: {
                zrSelectedProjectUri: first.projectUri,
            },
            capabilities: {},
        });
        const workspaceFolders = initializeResult && initializeResult.capabilities &&
            initializeResult.capabilities.workspace &&
            initializeResult.capabilities.workspace.workspaceFolders;

        assert(workspaceFolders && workspaceFolders.supported === true &&
            workspaceFolders.changeNotifications === true,
        'workspaceFolders change notifications must only be advertised with the active handler');

        client.notify('workspace/didChangeWatchedFiles', {
            changes: [
                { uri: first.projectUri, type: 1 },
                { uri: second.projectUri, type: 1 },
                { uri: outside.projectUri, type: 1 },
                { uri: 'vscode-test-web://workspace/foreign.zrp', type: 1 },
            ],
        });

        await assertWorkspaceSymbol(client, first.symbolName, true,
            'workspaceFolders must take priority over rootUri/rootPath for the first root');
        await assertWorkspaceSymbol(client, second.symbolName, true,
            'workspaceFolders must register each independent root');
        await assertWorkspaceSymbol(client, outside.symbolName, false,
            'rootUri/rootPath must not override workspaceFolders and root-external events must be ignored');

        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: first.mainUri,
                languageId: 'zr',
                version: 1,
                text: fs.readFileSync(first.mainPath, 'utf8'),
            },
        });

        client.notify('workspace/didChangeWorkspaceFolders', {
            event: {
                added: [{ uri: nested.rootUri, name: 'nested' }],
                removed: [],
            },
        });
        client.notify('workspace/didChangeWatchedFiles', {
            changes: [{ uri: nested.projectUri, type: 1 }],
        });
        await assertWorkspaceSymbol(client, nested.symbolName, true,
            'added nested workspace root must load its own same-name project');

        client.notify('workspace/didChangeWorkspaceFolders', {
            event: {
                added: [],
                removed: [{ uri: first.rootUri, name: 'first' }],
            },
        });

        await assertWorkspaceSymbol(client, first.symbolName, true,
            'removing a workspace root must preserve symbols from an explicitly opened overlay');
        await assertWorkspaceSymbol(client, nested.symbolName, true,
            'a nested root must survive removal of its parent root');
        await assertWorkspaceSymbol(client, second.symbolName, true,
            'an unrelated workspace root must survive removal of another root');

        const retainedOverlaySymbols = await request(client, 'textDocument/documentSymbol', {
            textDocument: { uri: first.mainUri },
        });
        assert(Array.isArray(retainedOverlaySymbols) && retainedOverlaySymbols.some((item) =>
            item && item.name === first.symbolName),
        'removing a root must retain an explicitly opened document overlay');
        client.notify('textDocument/didClose', {
            textDocument: { uri: first.mainUri },
        });
        await assertWorkspaceSymbol(client, first.symbolName, false,
            'closing the retained overlay must release the removed root-owned document state');

        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: nested.mainUri,
                languageId: 'zr',
                version: 1,
                text: fs.readFileSync(nested.mainPath, 'utf8'),
            },
        });
        const nestedModules = await request(client, 'zr/projectModules', { uri: nested.projectUri });
        assert(Array.isArray(nestedModules) && nestedModules.some((item) => item && item.moduleName === 'main'),
            'removing the selected parent project must clear selection before nested project resolution');

        const renamedProjectPath = path.join(second.rootPath, 'renamed.zrp');
        const renamedProjectUri = pathToFileURL(renamedProjectPath).toString();
        fs.renameSync(second.projectPath, renamedProjectPath);
        client.notify('workspace/didRenameFiles', {
            files: [{ oldUri: second.projectUri, newUri: renamedProjectUri }],
        });
        await assertWorkspaceSymbol(client, second.symbolName, true,
            'project file rename under a registered root must preserve the project index');

        client.notify('workspace/didChangeWatchedFiles', {
            changes: [{ uri: outside.projectUri, type: 1 }],
        });
        await assertWorkspaceSymbol(client, outside.symbolName, false,
            'removed and root-external file events must not trigger local disk reads or indexing');

        await shutdown(client);
        console.log('stdio workspace folders smoke: 12/12 Pass');
    } finally {
        if (!client.closed) {
            await client.terminate();
        }
        removePathSync(workspaceRoot);
    }
}

main().catch((error) => {
    console.error(`stdio workspace folders smoke failed: ${error.stack || error.message}`);
    process.exitCode = 1;
});
