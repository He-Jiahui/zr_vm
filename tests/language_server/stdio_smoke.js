const { spawn, spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL, fileURLToPath } = require('url');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function diagnosticRelatedUriMatches(expectedUri, actualUri) {
    if (actualUri === expectedUri) {
        return true;
    }
    if (typeof actualUri !== 'string' || typeof expectedUri !== 'string') {
        return false;
    }
    try {
        const expectedPath = fileURLToPath(expectedUri);
        const actualPath = fileURLToPath(actualUri);
        if (process.platform === 'win32') {
            return expectedPath.toLowerCase() === actualPath.toLowerCase();
        }
        return expectedPath === actualPath;
    } catch {
        return false;
    }
}

async function waitForDiagnosticsUri(client, uri, message) {
    for (let attempt = 0; attempt < 16; attempt += 1) {
        const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        if (diagnosticRelatedUriMatches(uri, diagnostics.uri)) {
            return diagnostics;
        }
    }

    throw new Error(message);
}

function copyPathSync(sourcePath, targetPath) {
    const stats = fs.statSync(sourcePath);

    if (stats.isDirectory()) {
        fs.mkdirSync(targetPath, { recursive: true });
        fs.readdirSync(sourcePath).forEach((entry) => {
            copyPathSync(path.join(sourcePath, entry), path.join(targetPath, entry));
        });
        return;
    }

    fs.copyFileSync(sourcePath, targetPath);
}

function removePathSync(targetPath, options = {}) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, options);
        return;
    }

    if (!fs.existsSync(targetPath)) {
        return;
    }

    const stats = fs.statSync(targetPath);
    if (stats.isDirectory()) {
        fs.readdirSync(targetPath).forEach((entry) => {
            removePathSync(path.join(targetPath, entry), options);
        });
        fs.rmdirSync(targetPath);
        return;
    }

    fs.unlinkSync(targetPath);
}

function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
}

function findPosition(text, substring, occurrence = 0, offset = 0) {
    let fromIndex = 0;
    let index = -1;

    for (let current = 0; current <= occurrence; current += 1) {
        index = text.indexOf(substring, fromIndex);
        if (index < 0) {
            throw new Error(`Unable to find substring "${substring}"`);
        }
        fromIndex = index + substring.length;
    }

    const target = index + offset;
    const lines = text.slice(0, target).split('\n');
    return {
        line: lines.length - 1,
        character: lines[lines.length - 1].length,
    };
}

function createWatchedProjectFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-watch-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'watched_refresh.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'watched_refresh',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, [
        'module "main";',
        '',
        'pub func watched_before_refresh(): int {',
        '    return 1;',
        '}',
        '',
    ].join('\n'));

    return {
        rootPath,
        projectPath,
        mainPath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
    };
}

function regenerateWatchedBinaryMetadataFixture(serverPath, rootPath, cliPathOptional) {
    const cliPath =
        typeof cliPathOptional === 'string' && cliPathOptional.length > 0
            ? cliPathOptional
            : path.join(path.dirname(serverPath), `zr_vm_cli${path.extname(serverPath)}`);
    const projectPath = path.join(rootPath, 'binary_module_graph_pipeline.zrp');
    const tempBinarySourcePath = path.join(rootPath, 'src', 'graph_binary_stage.zr');
    const compileResult = spawnSync(cliPath, [
        '--compile',
        projectPath,
        '--intermediate',
    ], {
        cwd: process.cwd(),
        encoding: 'utf8',
        windowsHide: true,
    });

    removePathSync(tempBinarySourcePath, { force: true });

    if (compileResult.error) {
        throw compileResult.error;
    }

    if (compileResult.status !== 0) {
        const stderr = compileResult.stderr ? compileResult.stderr.trim() : '';
        const stdout = compileResult.stdout ? compileResult.stdout.trim() : '';
        throw new Error([
            `Failed to regenerate watched binary metadata fixture with ${cliPath}`,
            `status=${compileResult.status}`,
            stdout ? `stdout=${stdout}` : '',
            stderr ? `stderr=${stderr}` : '',
        ].filter(Boolean).join('\n'));
    }
}

function createWatchedBinaryMetadataFixture(serverPath, cliPathOptional) {
    const sourceFixtureRoot = path.join(__dirname,
        '..',
        'fixtures',
        'projects',
        'binary_module_graph_pipeline');
    const binarySourceFixturePath = path.join(sourceFixtureRoot,
        'fixtures',
        'graph_binary_stage_source.zr');
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-binary-watch-'));
    const projectPath = path.join(rootPath, 'binary_module_graph_pipeline.zrp');
    const mainPath = path.join(rootPath, 'src', 'main.zr');
    const binaryPath = path.join(rootPath, 'bin', 'graph_binary_stage.zro');
    const binaryIntermediatePath = path.join(rootPath, 'bin', 'graph_binary_stage.zri');
    const tempBinarySourcePath = path.join(rootPath, 'src', 'graph_binary_stage.zr');

    copyPathSync(sourceFixtureRoot, rootPath);
    fs.copyFileSync(binarySourceFixturePath, tempBinarySourcePath);
    removePathSync(binaryPath, { force: true });
    removePathSync(binaryIntermediatePath, { force: true });
    regenerateWatchedBinaryMetadataFixture(serverPath, rootPath, cliPathOptional);

    return {
        rootPath,
        projectPath,
        mainPath,
        binaryPath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        binaryUri: pathToFileURL(binaryPath).toString(),
    };
}

function createImportDiagnosticsFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-import-diag-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'import_diagnostics.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const modulePath = path.join(sourcePath, 'greet.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'import_diagnostics',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, [
        'var greet = %import("greet");',
        'var answer = greet.missing;',
        '',
    ].join('\n'));
    fs.writeFileSync(modulePath, [
        'pub var present = 1;',
        '',
    ].join('\n'));

    return {
        rootPath,
        projectPath,
        mainPath,
        modulePath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        moduleUri: pathToFileURL(modulePath).toString(),
    };
}

function sleepSync(milliseconds) {
    const waitArray = new Int32Array(new SharedArrayBuffer(4));
    Atomics.wait(waitArray, 0, 0, milliseconds);
}

function cleanupPath(targetPath) {
    let lastError = null;

    if (!targetPath) {
        return;
    }

    for (let attempt = 0; attempt < 8; attempt += 1) {
        try {
            removePathSync(targetPath, { recursive: true, force: true });
            return;
        } catch (error) {
            if (!error || (error.code !== 'EBUSY' && error.code !== 'EPERM')) {
                throw error;
            }

            lastError = error;
            sleepSync(25 * (attempt + 1));
        }
    }

    if (lastError) {
        console.warn(`Skipping cleanup for locked path ${targetPath}: ${lastError.code}`);
    }
}

let watchedFixtureRootToCleanup = null;
let watchedBinaryFixtureRootToCleanup = null;
let importDiagnosticsFixtureRootToCleanup = null;
let fileOperationsFixtureRootToCleanup = null;

class LspClient {
    constructor(serverPath) {
        this.serverPath = serverPath;
        this.nextId = 1;
        this.buffer = Buffer.alloc(0);
        this.pendingResponses = new Map();
        this.pendingNotifications = new Map();
        this.notificationBacklog = new Map();
        this.closed = false;
        this.exitCode = null;
        this.stderrChunks = [];

        this.child = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });

        this.child.stdout.on('data', (chunk) => this.onStdout(chunk));
        this.child.stderr.on('data', (chunk) => {
            this.stderrChunks.push(chunk.toString('utf8'));
        });
        this.child.on('exit', (code) => {
            this.exitCode = code;
        });
        this.child.on('close', (code) => {
            this.closed = true;
            if (this.exitCode === null) {
                this.exitCode = code;
            }

            for (const { reject, timer } of this.pendingResponses.values()) {
                clearTimeout(timer);
                reject(new Error(`Server closed before responding. stderr=${this.stderr()}`));
            }
            this.pendingResponses.clear();

            for (const waiters of this.pendingNotifications.values()) {
                for (const { reject, timer } of waiters) {
                    clearTimeout(timer);
                    reject(new Error(`Server closed before notification. stderr=${this.stderr()}`));
                }
            }
            this.pendingNotifications.clear();
        });
    }

    stderr() {
        return this.stderrChunks.join('');
    }

    onStdout(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);

        while (true) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd < 0) {
                return;
            }

            const header = this.buffer.subarray(0, headerEnd).toString('ascii');
            const headerLines = header.split('\r\n').filter((line) => line.length > 0);
            for (const line of headerLines) {
                if (!/^[A-Za-z-]+:\s*.+$/.test(line)) {
                    throw new Error(`Invalid LSP header line: ${line}`);
                }
            }
            const lengthMatch = header.match(/Content-Length:\s*(\d+)/i);
            if (!lengthMatch) {
                throw new Error(`Missing Content-Length header: ${header}`);
            }

            const contentLength = Number(lengthMatch[1]);
            const messageStart = headerEnd + 4;
            const totalLength = messageStart + contentLength;
            if (this.buffer.length < totalLength) {
                return;
            }

            const payload = this.buffer.subarray(messageStart, totalLength).toString('utf8');
            this.buffer = this.buffer.subarray(totalLength);

            const message = JSON.parse(payload);
            this.dispatch(message);
        }
    }

    dispatch(message) {
        if (Object.prototype.hasOwnProperty.call(message, 'id') &&
            (Object.prototype.hasOwnProperty.call(message, 'result') ||
             Object.prototype.hasOwnProperty.call(message, 'error'))) {
            const pending = this.pendingResponses.get(message.id);
            if (!pending) {
                return;
            }

            clearTimeout(pending.timer);
            this.pendingResponses.delete(message.id);

            if (message.error) {
                pending.reject(new Error(JSON.stringify(message.error)));
            } else {
                pending.resolve(message.result);
            }
            return;
        }

        if (!message.method) {
            return;
        }

        const backlog = this.notificationBacklog.get(message.method);
        if (backlog) {
            backlog.push(message.params);
        } else {
            this.notificationBacklog.set(message.method, [message.params]);
        }

        const waiters = this.pendingNotifications.get(message.method);
        if (!waiters || waiters.length === 0) {
            return;
        }

        const notificationBacklog = this.notificationBacklog.get(message.method);
        const nextParams = notificationBacklog ? notificationBacklog.shift() : undefined;
        if (notificationBacklog && notificationBacklog.length === 0) {
            this.notificationBacklog.delete(message.method);
        }

        const waiter = waiters.shift();
        if (waiters.length === 0) {
            this.pendingNotifications.delete(message.method);
        }

        clearTimeout(waiter.timer);
        waiter.resolve(nextParams);
    }

    request(method, params, timeoutMs = 10000) {
        if (this.closed) {
            return Promise.reject(new Error('Server already exited'));
        }

        const id = this.nextId++;
        const message = createMessage({
            jsonrpc: '2.0',
            id,
            method,
            params,
        });

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendingResponses.delete(id);
                reject(new Error(`Timed out waiting for response to ${method}. stderr=${this.stderr()}`));
            }, timeoutMs);

            this.pendingResponses.set(id, { resolve, reject, timer });
            this.child.stdin.write(message);
        });
    }

    notify(method, params) {
        if (this.closed) {
            throw new Error('Server already exited');
        }

        this.child.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

    waitForNotification(method, timeoutMs = 10000) {
        const backlog = this.notificationBacklog.get(method);
        if (backlog && backlog.length > 0) {
            const params = backlog.shift();
            if (backlog.length === 0) {
                this.notificationBacklog.delete(method);
            }
            return Promise.resolve(params);
        }

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                const waiters = this.pendingNotifications.get(method);
                if (waiters) {
                    const index = waiters.findIndex((entry) => entry.timer === timer);
                    if (index >= 0) {
                        waiters.splice(index, 1);
                    }
                    if (waiters.length === 0) {
                        this.pendingNotifications.delete(method);
                    }
                }
                reject(new Error(`Timed out waiting for notification ${method}. stderr=${this.stderr()}`));
            }, timeoutMs);

            const waiters = this.pendingNotifications.get(method) || [];
            waiters.push({ resolve, reject, timer });
            this.pendingNotifications.set(method, waiters);
        });
    }

    waitForExit(timeoutMs = 10000) {
        if (this.closed) {
            return Promise.resolve(this.exitCode);
        }

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                reject(new Error(`Timed out waiting for server exit. stderr=${this.stderr()}`));
            }, timeoutMs);

            this.child.once('close', (code) => {
                clearTimeout(timer);
                resolve(code);
            });
        });
    }
}

async function main() {
    const serverPath = process.argv[2];
    const cliPathOptional = process.argv[3];
    assert(serverPath, 'Expected server executable path as argv[2]');
    const watchedFixture = createWatchedProjectFixture();
    const watchedBinaryFixture = createWatchedBinaryMetadataFixture(serverPath, cliPathOptional);
    const importDiagnosticsFixture = createImportDiagnosticsFixture();
    const fileOperationsFixture = createWatchedProjectFixture();
    watchedFixtureRootToCleanup = watchedFixture.rootPath;
    watchedBinaryFixtureRootToCleanup = watchedBinaryFixture.rootPath;
    importDiagnosticsFixtureRootToCleanup = importDiagnosticsFixture.rootPath;
    fileOperationsFixtureRootToCleanup = fileOperationsFixture.rootPath;

    const client = new LspClient(serverPath);
    const documentUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-smoke.zr';
    const docsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-docs.zr';
    const genericUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-generic.zr';
    const formatEditUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-format-edit.zr';
    const noopFormatUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-format-noop.zr';
    const importFoldingUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-import-folding.zr';
    const moduleImportsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-module-imports.zr';
    const colorUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-colors.zr';
    const inlineCompletionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-completion.zr';
    const initialText = 'var x = 10; var y = x;';
    const colorText = 'var accent = "#336699";';
    const inlineCompletionText = [
        'func main(): int {',
        '    ret',
        '}',
        '',
    ].join('\n');
    const documentationText = [
        'module "documentation";',
        '',
        'class ScoreBoard {',
        '    pri static var _bonus: int = 5;',
        '',
        '    // Shared bonus exposed through get/set.',
        '    pub static get bonus: int {',
        '        return ScoreBoard._bonus;',
        '    }',
        '',
        '    pub static set bonus(v: int) {',
        '        ScoreBoard._bonus = v;',
        '    }',
        '}',
        '',
        '%test("documentation") {',
        '    return ScoreBoard.bonus;',
        '}',
        '',
    ].join('\n');
    const genericText = [
        'class Item {',
        '    pub @constructor() { }',
        '}',
        'class Derived<T, const N: int> {',
        '}',
        'class Matrix<T, const N: int> { }',
        'class Box<T> {',
        '    func shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }',
        '}',
        'func inferNumber() {',
        '    return 42;',
        '}',
        'func use(): void {',
        '    var value: Derived<Item, 2 + 2> = null;',
        '    var box = new Box<int>();',
        '    var m = new Matrix<int, 2 + 2>();',
        '    value;',
        '    box.shape(m);',
        '}',
        '',
    ].join('\n');
    const noopFormatText = [
        'class Sample {',
        '    pub func run(value: int): int {',
        '        return value;',
        '    }',
        '}',
        '',
    ].join('\n');
    const formatEditText = [
        'class Sample {',
        'pub func run(value: int): int {',
        'return value;',
        '}',
        '}',
        '',
    ].join('\n');
    const importFoldingText = [
        '%import("zr.system");',
        '%import("zr.math");',
        '%import("zr.container");',
        '',
        '// first note',
        '// second note',
        '',
        '//#region setup',
        'func main(): int {',
        '    return 0;',
        '}',
        '//#endregion',
        '',
    ].join('\n');
    const moduleImportsText = [
        'module "stdio";',
        '',
        '%import("zr.system");',
        '%import("zr.math");',
        '',
        'func main(): int { return 0; }',
        '',
    ].join('\n');

    const initializeResult = await client.request('initialize', {
        processId: null,
        rootUri: null,
        capabilities: {},
        clientInfo: {
            name: 'stdio-smoke',
            version: '0.0.1',
        },
    });

    assert(initializeResult, 'initialize returned null');
    assert(initializeResult.capabilities, 'initialize missing capabilities');
    assert(initializeResult.capabilities.textDocumentSync.change === 2,
        'server must advertise incremental sync');
    assert(initializeResult.capabilities.signatureHelpProvider &&
        Array.isArray(initializeResult.capabilities.signatureHelpProvider.triggerCharacters) &&
        initializeResult.capabilities.signatureHelpProvider.triggerCharacters.includes('('),
        'signatureHelpProvider must advertise trigger characters');
    assert(initializeResult.capabilities.definitionProvider === true,
        'definitionProvider must be enabled');
    assert(initializeResult.capabilities.renameProvider.prepareProvider === true,
        'renameProvider.prepareProvider must be enabled');
    assert(initializeResult.capabilities.documentSymbolProvider === true,
        'documentSymbolProvider must be enabled');
    assert(initializeResult.capabilities.workspaceSymbolProvider &&
        initializeResult.capabilities.workspaceSymbolProvider.resolveProvider === true,
        'workspaceSymbolProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.semanticTokensProvider &&
        initializeResult.capabilities.semanticTokensProvider.full &&
        initializeResult.capabilities.semanticTokensProvider.full.delta === true,
        'semanticTokensProvider.full.delta must be enabled');
    assert(initializeResult.capabilities.semanticTokensProvider.range === true,
        'semanticTokensProvider.range must be enabled');
    const semanticTokensProvider = initializeResult.capabilities.semanticTokensProvider;
    const semanticTokenTypes = semanticTokensProvider &&
        semanticTokensProvider.legend &&
        semanticTokensProvider.legend.tokenTypes;
    assert(Array.isArray(semanticTokenTypes) &&
        semanticTokenTypes.includes('keyword'),
        'semantic token legend must include keyword');
    assert(initializeResult.capabilities.inlayHintProvider &&
        initializeResult.capabilities.inlayHintProvider.resolveProvider === true,
        'inlayHintProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.codeActionProvider &&
        Array.isArray(initializeResult.capabilities.codeActionProvider.codeActionKinds) &&
        initializeResult.capabilities.codeActionProvider.codeActionKinds.includes('source.organizeImports'),
        'codeActionProvider must advertise organize imports');
    assert(initializeResult.capabilities.codeActionProvider.resolveProvider === true,
        'codeActionProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.codeActionProvider.codeActionKinds.includes('quickfix'),
        'codeActionProvider must advertise quick fixes');
    assert(initializeResult.capabilities.documentFormattingProvider === true,
        'documentFormattingProvider must be enabled');
    assert(initializeResult.capabilities.documentRangeFormattingProvider === true,
        'documentRangeFormattingProvider must be enabled');
    assert(initializeResult.capabilities.textDocumentSync &&
        initializeResult.capabilities.textDocumentSync.willSaveWaitUntil === true,
        'textDocumentSync.willSaveWaitUntil must be enabled');
    assert(initializeResult.capabilities.documentOnTypeFormattingProvider &&
        initializeResult.capabilities.documentOnTypeFormattingProvider.firstTriggerCharacter === '}',
        'documentOnTypeFormattingProvider must be enabled');
    assert(initializeResult.capabilities.foldingRangeProvider === true,
        'foldingRangeProvider must be enabled');
    assert(initializeResult.capabilities.selectionRangeProvider === true,
        'selectionRangeProvider must be enabled');
    assert(initializeResult.capabilities.linkedEditingRangeProvider === true,
        'linkedEditingRangeProvider must be enabled');
    assert(initializeResult.capabilities.monikerProvider === true,
        'monikerProvider must be enabled');
    assert(initializeResult.capabilities.inlineValueProvider === true,
        'inlineValueProvider must be enabled');
    assert(initializeResult.capabilities.colorProvider === true,
        'colorProvider must be enabled');
    assert(initializeResult.capabilities.inlineCompletionProvider === true,
        'inlineCompletionProvider must be enabled');
    assert(initializeResult.capabilities.documentLinkProvider &&
        initializeResult.capabilities.documentLinkProvider.resolveProvider === true,
        'documentLinkProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.declarationProvider === true,
        'declarationProvider must be enabled');
    assert(initializeResult.capabilities.typeDefinitionProvider === true,
        'typeDefinitionProvider must be enabled');
    assert(initializeResult.capabilities.implementationProvider === true,
        'implementationProvider must be enabled');
    assert(initializeResult.capabilities.codeLensProvider &&
        initializeResult.capabilities.codeLensProvider.resolveProvider === true,
        'codeLensProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.executeCommandProvider &&
        Array.isArray(initializeResult.capabilities.executeCommandProvider.commands) &&
        initializeResult.capabilities.executeCommandProvider.commands.includes('zr.runCurrentProject') &&
        initializeResult.capabilities.executeCommandProvider.commands.includes('zr.showReferences'),
        'executeCommandProvider must advertise server-visible ZR commands');
    assert(initializeResult.capabilities.callHierarchyProvider === true,
        'callHierarchyProvider must be enabled');
    assert(initializeResult.capabilities.typeHierarchyProvider === true,
        'typeHierarchyProvider must be enabled');
    assert(initializeResult.capabilities.diagnosticProvider &&
        initializeResult.capabilities.diagnosticProvider.workspaceDiagnostics === true,
        'diagnosticProvider must support workspace diagnostics');
    assert(initializeResult.capabilities.completionProvider.resolveProvider === true,
        'completionProvider.resolveProvider must be enabled');
    assert(initializeResult.capabilities.workspace &&
        initializeResult.capabilities.workspace.fileOperations &&
        initializeResult.capabilities.workspace.fileOperations.willCreate &&
        initializeResult.capabilities.workspace.fileOperations.didCreate &&
        initializeResult.capabilities.workspace.fileOperations.willRename &&
        initializeResult.capabilities.workspace.fileOperations.didRename &&
        initializeResult.capabilities.workspace.fileOperations.willDelete &&
        initializeResult.capabilities.workspace.fileOperations.didDelete &&
        initializeResult.capabilities.workspace.fileOperations.didRename,
    'workspace.fileOperations must advertise create/delete/rename requests and notifications');

    client.notify('initialized', {});
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: documentUri,
            languageId: 'zr',
            version: 1,
            text: initialText,
        },
    });

    const openDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(openDiagnostics.uri === documentUri, 'didOpen diagnostics uri mismatch');
    assert(Array.isArray(openDiagnostics.diagnostics), 'didOpen diagnostics must be an array');

    client.notify('textDocument/didChange', {
        textDocument: {
            uri: documentUri,
            version: 2,
        },
        contentChanges: [
            {
                range: {
                    start: { line: 0, character: 8 },
                    end: { line: 0, character: 10 },
                },
                text: '20',
            },
        ],
    });

    const changeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(changeDiagnostics.uri === documentUri, 'didChange diagnostics uri mismatch');
    assert(changeDiagnostics.version === 2, 'didChange diagnostics version mismatch');

    const definition = await client.request('textDocument/definition', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(definition) && definition.length > 0, 'definition must return at least one location');

    const references = await client.request('textDocument/references', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
        context: { includeDeclaration: true },
    });
    assert(Array.isArray(references) && references.length > 0, 'references must return at least one location');
    const linkedEditing = await client.request('textDocument/linkedEditingRange', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(linkedEditing &&
        Array.isArray(linkedEditing.ranges) &&
        linkedEditing.ranges.some((range) =>
            range && range.start && range.start.line === 0 && range.start.character === 4) &&
        linkedEditing.ranges.some((range) =>
            range && range.start && range.start.line === 0 && range.start.character === 20) &&
        linkedEditing.wordPattern,
    'linkedEditingRange must return same-document ranges for the edited symbol');
    const monikers = await client.request('textDocument/moniker', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(monikers) &&
        monikers.some((moniker) =>
            moniker &&
            moniker.scheme === 'zr' &&
            moniker.identifier.endsWith('#x') &&
            moniker.unique === 'document' &&
            moniker.kind === 'local'),
    'textDocument/moniker must return a document-scoped symbol identity');
    const inlineValues = await client.request('textDocument/inlineValue', {
        textDocument: { uri: documentUri },
        range: {
            start: { line: 0, character: 0 },
            end: { line: 0, character: initialText.length },
        },
        context: {
            frameId: 1,
            stoppedLocation: {
                start: { line: 0, character: 0 },
                end: { line: 0, character: initialText.length },
            },
        },
    });
    assert(Array.isArray(inlineValues) &&
        inlineValues.some((value) =>
            value &&
            value.variableName === 'x' &&
            value.caseSensitiveLookup === true &&
            value.range &&
            value.range.start.line === 0 &&
            value.range.start.character === 4) &&
        inlineValues.some((value) =>
            value &&
            value.variableName === 'y' &&
            value.caseSensitiveLookup === true &&
            value.range &&
            value.range.start.line === 0 &&
            value.range.start.character === 16),
    'textDocument/inlineValue must expose local variable lookups in the requested range');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: colorUri,
            languageId: 'zr',
            version: 1,
            text: colorText,
        },
    });
    const colorDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(colorDiagnostics.uri === colorUri, 'color didOpen diagnostics uri mismatch');
    const documentColors = await client.request('textDocument/documentColor', {
        textDocument: { uri: colorUri },
    });
    assert(Array.isArray(documentColors) &&
        documentColors.some((entry) =>
            entry &&
            entry.range &&
            entry.range.start.line === 0 &&
            entry.range.start.character === 14 &&
            Math.abs(entry.color.red - 0.2) < 0.001 &&
            Math.abs(entry.color.green - 0.4) < 0.001 &&
            Math.abs(entry.color.blue - 0.6) < 0.001 &&
            entry.color.alpha === 1),
    'textDocument/documentColor must expose hex color literals');
    const colorPresentation = await client.request('textDocument/colorPresentation', {
        textDocument: { uri: colorUri },
        color: { red: 0.2, green: 0.4, blue: 0.6, alpha: 1 },
        range: {
            start: { line: 0, character: 14 },
            end: { line: 0, character: 21 },
        },
    });
    assert(Array.isArray(colorPresentation) &&
        colorPresentation.some((presentation) =>
            presentation &&
            presentation.label === '#336699' &&
            presentation.textEdit &&
            presentation.textEdit.newText === '#336699'),
    'textDocument/colorPresentation must format the selected color as a hex edit');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: inlineCompletionUri,
            languageId: 'zr',
            version: 1,
            text: inlineCompletionText,
        },
    });
    const inlineCompletionDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(inlineCompletionDiagnostics.uri === inlineCompletionUri,
        'inline completion didOpen diagnostics uri mismatch');
    const inlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 1, character: 7 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(inlineCompletions) &&
        inlineCompletions.some((item) =>
            item &&
            item.insertText === 'return ' &&
            item.range &&
            item.range.start.line === 1 &&
            item.range.start.character === 4 &&
            item.range.end.character === 7),
    'textDocument/inlineCompletion must expand statement prefixes');

    const hover = await client.request('textDocument/hover', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(hover && hover.contents, 'hover must return contents');

    const completions = await client.request('textDocument/completion', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 0 },
    });
    assert(Array.isArray(completions), 'completion must return an array');

    const documentSymbols = await client.request('textDocument/documentSymbol', {
        textDocument: { uri: documentUri },
    });
    assert(Array.isArray(documentSymbols) && documentSymbols.length > 0,
        'documentSymbol must return at least one symbol');

    const workspaceSymbols = await client.request('workspace/symbol', {
        query: 'x',
    });
    assert(Array.isArray(workspaceSymbols) && workspaceSymbols.length > 0,
        'workspace/symbol must return at least one symbol');
    const resolvedWorkspaceSymbol = await client.request('workspaceSymbol/resolve', workspaceSymbols[0]);
    assert(resolvedWorkspaceSymbol &&
        resolvedWorkspaceSymbol.name === workspaceSymbols[0].name &&
        resolvedWorkspaceSymbol.location &&
        resolvedWorkspaceSymbol.location.uri === workspaceSymbols[0].location.uri,
    'workspaceSymbol/resolve must preserve resolved workspace symbols');

    const highlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(highlights) && highlights.length > 0,
        'documentHighlight must return at least one highlight');

    const prepareRename = await client.request('textDocument/prepareRename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(prepareRename && prepareRename.range && prepareRename.placeholder === 'x',
        'prepareRename must return range and placeholder');

    const rename = await client.request('textDocument/rename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
        newName: 'renamedX',
    });
    assert(rename && rename.changes && Array.isArray(rename.changes[documentUri]) &&
        rename.changes[documentUri].length > 0,
        'rename must return workspace edits for the document');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: docsUri,
            languageId: 'zr',
            version: 1,
            text: documentationText,
        },
    });

    const docsDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(docsDiagnostics.uri === docsUri, 'documentation diagnostics uri mismatch');
    assert(Array.isArray(docsDiagnostics.diagnostics) && docsDiagnostics.diagnostics.length === 0,
        'documentation fixture should open without diagnostics');

    const docsHoverPosition = findPosition(documentationText, 'ScoreBoard.bonus;', 0, 11);
    const docsCompletionPosition = findPosition(documentationText, 'ScoreBoard.bonus;', 0, 11);

    const docsHover = await client.request('textDocument/hover', {
        textDocument: { uri: docsUri },
        position: docsHoverPosition,
    });
    assert(docsHover && docsHover.contents && !Array.isArray(docsHover.contents),
        'hover.contents must be a MarkupContent object');
    assert(docsHover.contents.kind === 'markdown',
        'hover.contents.kind must be markdown');
    assert(typeof docsHover.contents.value === 'string' &&
        docsHover.contents.value.includes('Shared bonus exposed through get/set.'),
        'hover markdown should include the leading property comment');

    const docsCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: docsUri },
        position: docsCompletionPosition,
    });
    assert(Array.isArray(docsCompletions), 'documentation completion must return an array');
    const bonusCompletion = docsCompletions.find((item) => item && item.label === 'bonus');
    assert(bonusCompletion, 'documentation completion must include bonus');
    assert(bonusCompletion.documentation && bonusCompletion.documentation.kind === 'markdown',
        'completion documentation must use markdown');
    assert(typeof bonusCompletion.documentation.value === 'string' &&
        bonusCompletion.documentation.value.includes('Shared bonus exposed through get/set.'),
        'completion documentation should include the leading property comment');
    assert(bonusCompletion.data && bonusCompletion.data.uri === docsUri && bonusCompletion.data.position,
        'completion items must include resolve data');
    const resolvedBonusCompletion = await client.request('completionItem/resolve', {
        label: bonusCompletion.label,
        kind: bonusCompletion.kind,
        insertText: bonusCompletion.insertText,
        insertTextFormat: bonusCompletion.insertTextFormat,
        data: bonusCompletion.data,
    });
    assert(resolvedBonusCompletion &&
        resolvedBonusCompletion.documentation &&
        resolvedBonusCompletion.documentation.kind === 'markdown' &&
        resolvedBonusCompletion.documentation.value.includes('Shared bonus exposed through get/set.'),
    'completionItem/resolve must restore markdown documentation from resolve data');

    const docsCodeLens = await client.request('textDocument/codeLens', {
        textDocument: { uri: docsUri },
    });
    assert(Array.isArray(docsCodeLens) && docsCodeLens.some((lens) =>
        lens &&
        lens.command &&
        lens.command.command === 'zr.runCurrentProject'),
        'textDocument/codeLens must expose a run command for %test blocks');
    const resolvedDocsCodeLens = await client.request('codeLens/resolve', docsCodeLens[0]);
    assert(resolvedDocsCodeLens &&
        resolvedDocsCodeLens.command &&
        resolvedDocsCodeLens.command.command === docsCodeLens[0].command.command &&
        resolvedDocsCodeLens.range &&
        resolvedDocsCodeLens.range.start &&
        resolvedDocsCodeLens.range.start.line === docsCodeLens[0].range.start.line,
    'codeLens/resolve must preserve resolved command lenses');
    const runCommandResult = await client.request('workspace/executeCommand', {
        command: 'zr.runCurrentProject',
        arguments: [docsUri],
    });
    assert(runCommandResult === null,
        'workspace/executeCommand must acknowledge advertised run commands');

    const importDiagnosticsText = fs.readFileSync(importDiagnosticsFixture.mainPath, 'utf8');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: importDiagnosticsFixture.mainUri,
            languageId: 'zr',
            version: 1,
            text: importDiagnosticsText,
        },
    });

    const importDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(diagnosticRelatedUriMatches(importDiagnosticsFixture.mainUri, importDiagnostics.uri),
        'import diagnostics uri mismatch');
    assert(Array.isArray(importDiagnostics.diagnostics) && importDiagnostics.diagnostics.length > 0,
        'import diagnostics fixture should publish at least one diagnostic');
    const missingImportDiagnostic = importDiagnostics.diagnostics.find((diagnostic) =>
        diagnostic &&
        typeof diagnostic.message === 'string' &&
        diagnostic.message.includes("Import member 'greet.missing' could not be resolved"));
    assert(missingImportDiagnostic,
        'import diagnostics fixture should publish the missing imported member diagnostic');
    assert(Array.isArray(missingImportDiagnostic.relatedInformation) &&
        missingImportDiagnostic.relatedInformation.some((item) =>
            item &&
            item.location &&
            diagnosticRelatedUriMatches(importDiagnosticsFixture.mainUri, item.location.uri)) &&
        missingImportDiagnostic.relatedInformation.some((item) =>
            item &&
            item.location &&
            diagnosticRelatedUriMatches(importDiagnosticsFixture.moduleUri, item.location.uri)),
    'import diagnostics should serialize cross-file relatedInformation trace locations');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: genericUri,
            languageId: 'zr',
            version: 1,
            text: genericText,
        },
    });

    const genericDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(genericDiagnostics.uri === genericUri, 'generic diagnostics uri mismatch');
    assert(Array.isArray(genericDiagnostics.diagnostics) && genericDiagnostics.diagnostics.length === 0,
        'generic fixture should open without diagnostics');

    const genericTypePosition = findPosition(genericText, 'Derived<Item, 2 + 2>', 0, 0);
    const genericDefinitionPosition = findPosition(genericText, 'class Derived<T, const N: int>', 0, 6);
    const genericCallPosition = findPosition(genericText, 'box.shape(m);', 0, 10);

    const genericCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: genericUri },
        position: genericTypePosition,
    });
    assert(Array.isArray(genericCompletions), 'generic completion must return an array');
    const derivedCompletion = genericCompletions.find((item) => item && item.label === 'Derived');
    assert(derivedCompletion && typeof derivedCompletion.detail === 'string' &&
        derivedCompletion.detail.includes('Resolved Type: Derived<Item, 4>'),
        'generic completion detail should include the normalized closed instantiation');

    const genericDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: genericUri },
        position: genericTypePosition,
    });
    assert(Array.isArray(genericDefinition) && genericDefinition.length > 0,
        'generic definition must return at least one location');
    assert(genericDefinition.some((location) =>
        location &&
        location.uri === genericUri &&
        location.range &&
        location.range.start &&
        location.range.start.line === genericDefinitionPosition.line &&
        location.range.start.character === genericDefinitionPosition.character),
        'generic definition should jump from closed use to open generic declaration');

    const signatureHelp = await client.request('textDocument/signatureHelp', {
        textDocument: { uri: genericUri },
        position: genericCallPosition,
    });
    assert(signatureHelp && Array.isArray(signatureHelp.signatures) && signatureHelp.signatures.length > 0,
        'signatureHelp must return at least one signature');
    assert(signatureHelp.activeParameter === 0,
        'signatureHelp activeParameter must resolve to the current argument index');
    assert(signatureHelp.signatures.some((signature) =>
        signature &&
        typeof signature.label === 'string' &&
        signature.label.includes('shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>')),
        'signatureHelp should show the closed generic method signature with normalized const generics');

    const genericInlayHints = await client.request('textDocument/inlayHint', {
        textDocument: { uri: genericUri },
        range: {
            start: { line: 0, character: 0 },
            end: { line: genericText.split('\n').length, character: 0 },
        },
    });
    assert(Array.isArray(genericInlayHints),
        'textDocument/inlayHint must return an array');
    assert(genericInlayHints.some((hint) => hint && hint.label === ': Box<int>'),
        'inlay hints should include the exact inferred closed generic type for box');
    assert(genericInlayHints.some((hint) => hint && hint.label === ': Matrix<int, 4>'),
        'inlay hints should include the normalized exact inferred closed generic type for m');
    assert(genericInlayHints.some((hint) => hint && hint.label === ': int'),
        'inlay hints should include the exact inferred return type for inferNumber');
    const boxInlayHint = genericInlayHints.find((hint) => hint && hint.label === ': Box<int>');
    const resolvedBoxInlayHint = await client.request('inlayHint/resolve', boxInlayHint);
    assert(resolvedBoxInlayHint &&
        resolvedBoxInlayHint.label === boxInlayHint.label &&
        resolvedBoxInlayHint.position &&
        resolvedBoxInlayHint.position.line === boxInlayHint.position.line,
    'inlayHint/resolve must preserve resolved hints');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: formatEditUri,
            languageId: 'zr',
            version: 1,
            text: formatEditText,
        },
    });
    await waitForDiagnosticsUri(client, formatEditUri, 'format edit diagnostics uri mismatch');
    const formatted = await client.request('textDocument/formatting', {
        textDocument: { uri: formatEditUri },
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(formatted) && formatted.length === 1 &&
        formatted[0].newText.includes('    pub func run') &&
        formatted[0].newText.includes('        return value;'),
        'textDocument/formatting must return a full-document indented edit');
    const willSaveEdits = await client.request('textDocument/willSaveWaitUntil', {
        textDocument: { uri: formatEditUri },
        reason: 1,
    });
    assert(Array.isArray(willSaveEdits) && willSaveEdits.length === 1 &&
        willSaveEdits[0].newText.includes('    pub func run') &&
        willSaveEdits[0].newText.includes('        return value;'),
        'textDocument/willSaveWaitUntil must return save-time formatting edits');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: noopFormatUri,
            languageId: 'zr',
            version: 1,
            text: noopFormatText,
        },
    });
    await waitForDiagnosticsUri(client, noopFormatUri, 'noop formatting diagnostics uri mismatch');
    const noopFormatted = await client.request('textDocument/formatting', {
        textDocument: { uri: noopFormatUri },
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(noopFormatted) && noopFormatted.length === 0,
        'textDocument/formatting must skip already formatted documents');
    const noopRangeFormatted = await client.request('textDocument/rangeFormatting', {
        textDocument: { uri: noopFormatUri },
        range: { start: { line: 1, character: 0 }, end: { line: 3, character: 0 } },
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(noopRangeFormatted) && noopRangeFormatted.length === 0,
        'textDocument/rangeFormatting must skip already formatted ranges');
    const noopRangesFormatted = await client.request('textDocument/rangesFormatting', {
        textDocument: { uri: noopFormatUri },
        ranges: [
            { start: { line: 1, character: 0 }, end: { line: 2, character: 0 } },
            { start: { line: 2, character: 0 }, end: { line: 3, character: 0 } },
        ],
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(noopRangesFormatted) && noopRangesFormatted.length === 0,
        'textDocument/rangesFormatting must return aggregated range edits');

    const onTypeFormatted = await client.request('textDocument/onTypeFormatting', {
        textDocument: { uri: genericUri },
        position: { line: 8, character: 1 },
        ch: '}',
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(onTypeFormatted),
        'textDocument/onTypeFormatting must return an edit array');

    const folds = await client.request('textDocument/foldingRange', {
        textDocument: { uri: genericUri },
    });
    assert(Array.isArray(folds) && folds.length > 0,
        'textDocument/foldingRange must return structural ranges');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: importFoldingUri,
            languageId: 'zr',
            version: 1,
            text: importFoldingText,
        },
    });
    await waitForDiagnosticsUri(client, importFoldingUri, 'import folding diagnostics uri mismatch');
    const importFolds = await client.request('textDocument/foldingRange', {
        textDocument: { uri: importFoldingUri },
    });
    assert(Array.isArray(importFolds) && importFolds.some((range) =>
        range &&
        range.kind === 'imports' &&
        range.startLine === 0 &&
        range.endLine === 2) &&
        importFolds.some((range) =>
            range &&
        range.kind === 'comment' &&
        range.startLine === 4 &&
        range.endLine === 5) &&
        importFolds.some((range) =>
            range &&
            range.kind === 'region' &&
            range.startLine === 7 &&
            range.endLine === 11),
    'textDocument/foldingRange must include import, comment, and explicit marker regions');

    const selections = await client.request('textDocument/selectionRange', {
        textDocument: { uri: genericUri },
        positions: [genericDefinitionPosition],
    });
    assert(Array.isArray(selections) &&
        selections.length === 1 &&
        selections[0].range &&
        selections[0].parent &&
        selections[0].parent.parent,
    'textDocument/selectionRange must return word, line, and block parent ranges');

    const importLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: genericUri },
    });
    assert(Array.isArray(importLinks),
        'textDocument/documentLink must return an array');

    const zrpLinksPath = path.join(watchedFixture.rootPath, 'linked_paths.zrp');
    const zrpLinksUri = pathToFileURL(zrpLinksPath).toString();
    const zrpLinksText = [
        '{',
        '  "name": "linked_paths",',
        '  "source": "src",',
        '  "binary": "bin",',
        '  "entry": "main",',
        '  "dependency": "deps",',
        '  "local": "local_modules"',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(zrpLinksPath, zrpLinksText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: zrpLinksUri,
            languageId: 'json',
            version: 1,
            text: zrpLinksText,
        },
    });
    await waitForDiagnosticsUri(client, zrpLinksUri, 'zrp documentLink diagnostics uri mismatch');
    const zrpLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: zrpLinksUri },
    });
    assert(Array.isArray(zrpLinks) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/src')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/bin')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/src/main.zr')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/deps')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/local_modules')),
    'textDocument/documentLink must expose all zrp project path fields');
    const resolvedZrpLink = await client.request('documentLink/resolve', zrpLinks[0]);
    assert(resolvedZrpLink &&
        resolvedZrpLink.target === zrpLinks[0].target &&
        resolvedZrpLink.range &&
        resolvedZrpLink.range.start &&
        resolvedZrpLink.range.start.line === zrpLinks[0].range.start.line,
    'documentLink/resolve must preserve resolved target links');

    const virtualNetworkUri = 'zr-decompiled:/zr.network.zr';
    const virtualNetworkText = await client.request('zr/nativeDeclarationDocument', {
        uri: virtualNetworkUri,
    });
    assert(typeof virtualNetworkText === 'string' && virtualNetworkText.includes('pub module tcp: zr.network.tcp;'),
        'zr/nativeDeclarationDocument must render native module links');
    const virtualNetworkLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: virtualNetworkUri },
    });
    assert(Array.isArray(virtualNetworkLinks) && virtualNetworkLinks.some((link) =>
        link && link.target === 'zr-decompiled:/zr.network.tcp.zr'),
    'textDocument/documentLink must expose virtual native module links');

    const codeActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: genericUri },
        range: { start: { line: 0, character: 0 }, end: { line: genericText.split('\n').length, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(codeActions),
        'textDocument/codeAction must return an array');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: moduleImportsUri,
            languageId: 'zr',
            version: 1,
            text: moduleImportsText,
        },
    });
    await waitForDiagnosticsUri(client, moduleImportsUri, 'module imports diagnostics uri mismatch');
    const moduleImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: moduleImportsUri },
        range: { start: { line: 0, character: 0 }, end: { line: moduleImportsText.split('\n').length, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(moduleImportActions) && moduleImportActions.some((action) =>
        action &&
        action.kind === 'source.organizeImports' &&
        action.edit &&
        action.edit.changes &&
        Array.isArray(action.edit.changes[moduleImportsUri]) &&
        action.edit.changes[moduleImportsUri].some((edit) =>
            edit.newText.includes('%import("zr.math");\n%import("zr.system");'))),
    'textDocument/codeAction must organize imports after module declarations');
    const organizeImportAction = moduleImportActions.find((action) =>
        action && action.kind === 'source.organizeImports' && action.edit);
    const resolvedOrganizeImportAction = await client.request('codeAction/resolve', organizeImportAction);
    assert(resolvedOrganizeImportAction &&
        resolvedOrganizeImportAction.title === organizeImportAction.title &&
        resolvedOrganizeImportAction.edit &&
        resolvedOrganizeImportAction.edit.changes &&
        Array.isArray(resolvedOrganizeImportAction.edit.changes[moduleImportsUri]),
    'codeAction/resolve must preserve resolved edits');

    const semicolonFixturePath = path.join(watchedFixture.rootPath, 'semicolon_action.zr');
    const semicolonFixtureUri = pathToFileURL(semicolonFixturePath).toString();
    const semicolonFixtureText = 'var answer = 42\n';
    fs.writeFileSync(semicolonFixturePath, semicolonFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: semicolonFixtureUri,
            languageId: 'zr',
            version: 1,
            text: semicolonFixtureText,
        },
    });
    await waitForDiagnosticsUri(client, semicolonFixtureUri, 'semicolon quickfix diagnostics uri mismatch');

    const quickFixActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: semicolonFixtureUri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [], only: ['quickfix'] },
    });
    assert(Array.isArray(quickFixActions) && quickFixActions.some((action) =>
        action &&
        action.kind === 'quickfix' &&
        action.edit &&
        action.edit.changes &&
        Array.isArray(action.edit.changes[semicolonFixtureUri]) &&
        action.edit.changes[semicolonFixtureUri].some((edit) => edit.newText === ';')),
    'textDocument/codeAction must return a semicolon quickfix edit');

    const sourceOnlyActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: semicolonFixtureUri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(sourceOnlyActions) &&
        !sourceOnlyActions.some((action) => action && action.kind === 'quickfix'),
    'textDocument/codeAction must honor context.only filters');

    const declaration = await client.request('textDocument/declaration', {
        textDocument: { uri: genericUri },
        position: genericDefinitionPosition,
    });
    assert(Array.isArray(declaration),
        'textDocument/declaration must return an array');

    const callHierarchyItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: genericUri },
        position: genericCallPosition,
    });
    assert(Array.isArray(callHierarchyItems),
        'textDocument/prepareCallHierarchy must return an array');
    const outgoingCalls = await client.request('callHierarchy/outgoingCalls', {
        item: callHierarchyItems[0] || {
            name: 'shape',
            kind: 6,
            uri: genericUri,
            range: { start: genericCallPosition, end: genericCallPosition },
            selectionRange: { start: genericCallPosition, end: genericCallPosition },
        },
    });
    assert(Array.isArray(outgoingCalls),
        'callHierarchy/outgoingCalls must return an array');
    const incomingCalls = await client.request('callHierarchy/incomingCalls', {
        item: callHierarchyItems[0] || {
            name: 'shape',
            kind: 6,
            uri: genericUri,
            range: { start: genericCallPosition, end: genericCallPosition },
            selectionRange: { start: genericCallPosition, end: genericCallPosition },
        },
    });
    assert(Array.isArray(incomingCalls),
        'callHierarchy/incomingCalls must return an array');

    const hierarchyFixturePath = path.join(watchedFixture.rootPath, 'src', 'call_hierarchy.zr');
    const hierarchyFixtureUri = pathToFileURL(hierarchyFixturePath).toString();
    const hierarchyFixtureText = [
        'func helper(value: int): int {',
        '    return value;',
        '}',
        '',
        'func run(value: int): int {',
        '    return helper(value);',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(hierarchyFixturePath, hierarchyFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: hierarchyFixtureUri,
            languageId: 'zr',
            version: 1,
            text: hierarchyFixtureText,
        },
    });
    await waitForDiagnosticsUri(client, hierarchyFixtureUri, 'call hierarchy fixture diagnostics uri mismatch');
    const hierarchyCodeLens = await client.request('textDocument/codeLens', {
        textDocument: { uri: hierarchyFixtureUri },
    });
    assert(Array.isArray(hierarchyCodeLens) && hierarchyCodeLens.some((lens) =>
        lens &&
        lens.command &&
        lens.command.title === '1 reference' &&
        lens.command.command === 'zr.showReferences' &&
        Array.isArray(lens.command.arguments) &&
        lens.command.arguments[0] === hierarchyFixtureUri &&
        lens.command.arguments[1] &&
        lens.command.arguments[1].line === 0 &&
        lens.command.arguments[1].character === 5),
    'textDocument/codeLens must expose callable reference counts with a reference command');
    const hierarchyRunPosition = findPosition(hierarchyFixtureText, 'run(value', 0, 1);
    const preparedRunItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: hierarchyFixtureUri },
        position: hierarchyRunPosition,
    });
    assert(Array.isArray(preparedRunItems),
        'textDocument/prepareCallHierarchy must return an array in the call fixture');
    const preparedRunOutgoing = await client.request('callHierarchy/outgoingCalls', {
        item: preparedRunItems[0] || {
            name: 'run',
            kind: 12,
            uri: hierarchyFixtureUri,
            range: {
                start: { line: 4, character: 0 },
                end: { line: 6, character: 1 },
            },
            selectionRange: {
                start: { line: 4, character: 5 },
                end: { line: 4, character: 8 },
            },
        },
    });
    assert(Array.isArray(preparedRunOutgoing) && preparedRunOutgoing.some((call) =>
        call &&
        call.to &&
        call.to.name === 'helper' &&
        Array.isArray(call.fromRanges) &&
        call.fromRanges.length > 0),
    'callHierarchy/outgoingCalls must return direct helper() calls over stdio');

    const hierarchyHelperPosition = findPosition(hierarchyFixtureText, 'helper(value', 0, 1);
    const preparedHelperItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: hierarchyFixtureUri },
        position: hierarchyHelperPosition,
    });
    assert(Array.isArray(preparedHelperItems),
        'textDocument/prepareCallHierarchy must return helper in the call fixture');
    const preparedHelperIncoming = await client.request('callHierarchy/incomingCalls', {
        item: preparedHelperItems[0] || {
            name: 'helper',
            kind: 12,
            uri: hierarchyFixtureUri,
            range: {
                start: { line: 0, character: 0 },
                end: { line: 2, character: 1 },
            },
            selectionRange: {
                start: { line: 0, character: 5 },
                end: { line: 0, character: 11 },
            },
        },
    });
    assert(Array.isArray(preparedHelperIncoming) && preparedHelperIncoming.some((call) =>
        call &&
        call.from &&
        call.from.name === 'run' &&
        Array.isArray(call.fromRanges) &&
        call.fromRanges.length > 0),
    'callHierarchy/incomingCalls must return direct run() callers over stdio');

    const typeHierarchyItems = await client.request('textDocument/prepareTypeHierarchy', {
        textDocument: { uri: genericUri },
        position: genericDefinitionPosition,
    });
    assert(Array.isArray(typeHierarchyItems),
        'textDocument/prepareTypeHierarchy must return an array');
    const supertypes = await client.request('typeHierarchy/supertypes', {
        item: typeHierarchyItems[0] || {
            name: 'Derived',
            kind: 5,
            uri: genericUri,
            range: { start: genericDefinitionPosition, end: genericDefinitionPosition },
            selectionRange: { start: genericDefinitionPosition, end: genericDefinitionPosition },
        },
    });
    assert(Array.isArray(supertypes),
        'typeHierarchy/supertypes must return an array');
    const subtypes = await client.request('typeHierarchy/subtypes', {
        item: typeHierarchyItems[0] || {
            name: 'Derived',
            kind: 5,
            uri: genericUri,
            range: { start: genericDefinitionPosition, end: genericDefinitionPosition },
            selectionRange: { start: genericDefinitionPosition, end: genericDefinitionPosition },
        },
    });
    assert(Array.isArray(subtypes),
        'typeHierarchy/subtypes must return an array');

    const pullDiagnostics = await client.request('textDocument/diagnostic', {
        textDocument: { uri: genericUri },
    });
    assert(pullDiagnostics &&
        pullDiagnostics.kind === 'full' &&
        Array.isArray(pullDiagnostics.items) &&
        typeof pullDiagnostics.resultId === 'string' &&
        pullDiagnostics.resultId.length > 0,
    'textDocument/diagnostic must return a full diagnostic report with resultId');
    const unchangedDiagnostics = await client.request('textDocument/diagnostic', {
        textDocument: { uri: genericUri },
        previousResultId: pullDiagnostics.resultId,
    });
    assert(unchangedDiagnostics &&
        unchangedDiagnostics.kind === 'unchanged' &&
        unchangedDiagnostics.resultId === pullDiagnostics.resultId &&
        !Object.prototype.hasOwnProperty.call(unchangedDiagnostics, 'items'),
    'textDocument/diagnostic must return unchanged reports for matching previousResultId');

    const workspaceDiagnostics = await client.request('workspace/diagnostic', {});
    assert(workspaceDiagnostics && Array.isArray(workspaceDiagnostics.items),
        'workspace/diagnostic must return a workspace diagnostic report');
    assert(workspaceDiagnostics.items.some((report) =>
        report &&
        report.kind === 'full' &&
        report.version === 1 &&
        typeof report.resultId === 'string' &&
        report.resultId.length > 0 &&
        diagnosticRelatedUriMatches(genericUri, report.uri) &&
        Array.isArray(report.items)),
    'workspace/diagnostic must include opened document diagnostic reports');
    const genericWorkspaceReport = workspaceDiagnostics.items.find((report) =>
        report &&
        report.kind === 'full' &&
        diagnosticRelatedUriMatches(genericUri, report.uri) &&
        typeof report.resultId === 'string');
    assert(genericWorkspaceReport && genericWorkspaceReport.version === 1,
        'workspace/diagnostic full reports must include the document version');
    const unchangedWorkspaceDiagnostics = await client.request('workspace/diagnostic', {
        previousResultIds: [
            { uri: genericWorkspaceReport.uri, value: genericWorkspaceReport.resultId },
        ],
    });
    assert(unchangedWorkspaceDiagnostics &&
        Array.isArray(unchangedWorkspaceDiagnostics.items) &&
        unchangedWorkspaceDiagnostics.items.some((report) =>
            report &&
            report.uri === genericWorkspaceReport.uri &&
            report.kind === 'unchanged' &&
            report.version === genericWorkspaceReport.version &&
            report.resultId === genericWorkspaceReport.resultId &&
            !Object.prototype.hasOwnProperty.call(report, 'items')),
    'workspace/diagnostic must return unchanged reports for matching previousResultIds');

    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedBinaryFixture.binaryUri, type: 2 },
        ],
    });

    const watchedBootstrapSymbols = await client.request('workspace/symbol', {
        query: 'merged',
    });
    assert(Array.isArray(watchedBootstrapSymbols) && watchedBootstrapSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.mainUri, item.location.uri) &&
        item.name === 'merged'),
    'workspace/didChangeWatchedFiles binary metadata change must bootstrap unopened project indexes');

    const watchedBinaryText = fs.readFileSync(watchedBinaryFixture.mainPath, 'utf8');
    const watchedBinaryImportDefinitionPosition = findPosition(watchedBinaryText, '"graph_binary_stage"', 0, 1);
    const watchedBinaryHoverPosition = findPosition(watchedBinaryText, 'binarySeed', 0, 0);

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: watchedBinaryFixture.mainUri,
            languageId: 'zr',
            version: 1,
            text: watchedBinaryText,
        },
    });

    const watchedBinaryDiagnostics = await waitForDiagnosticsUri(
        client,
        watchedBinaryFixture.mainUri,
        'binary watched metadata diagnostics uri mismatch');
    assert(Array.isArray(watchedBinaryDiagnostics.diagnostics) && watchedBinaryDiagnostics.diagnostics.length === 0,
        'binary watched metadata fixture should open without diagnostics');

    const watchedBinaryImportDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryImportDefinitionPosition,
    });
    assert(Array.isArray(watchedBinaryImportDefinition) && watchedBinaryImportDefinition.some((location) =>
        location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.binaryUri, location.uri) &&
        location.range &&
        location.range.start &&
        location.range.start.line === 0 &&
        location.range.start.character === 0),
    'binary import literal definition should navigate to the binary metadata file entry');

    const watchedBinaryMemberDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(Array.isArray(watchedBinaryMemberDefinition) && watchedBinaryMemberDefinition.some((location) =>
        location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.binaryUri, location.uri) &&
        location.range),
    'binary imported member definition should navigate to the binary metadata declaration');

    const watchedBinaryMemberReferences = await client.request('textDocument/references', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
        context: { includeDeclaration: true },
    });
    assert(Array.isArray(watchedBinaryMemberReferences) && watchedBinaryMemberReferences.some((location) =>
        location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.mainUri, location.uri) &&
        location.range &&
        location.range.start &&
        location.range.end &&
        location.range.start.line === watchedBinaryHoverPosition.line &&
        location.range.start.character === watchedBinaryHoverPosition.character &&
        location.range.end.line === watchedBinaryHoverPosition.line &&
        location.range.end.character === watchedBinaryHoverPosition.character + 'binarySeed'.length) &&
        watchedBinaryMemberReferences.some((location) =>
            location &&
            diagnosticRelatedUriMatches(watchedBinaryFixture.binaryUri, location.uri) &&
            location.range),
    'binary imported member references should include the current project usage and the binary metadata declaration');

    const watchedBinaryHighlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(Array.isArray(watchedBinaryHighlights) && watchedBinaryHighlights.some((highlight) =>
        highlight &&
        highlight.range &&
        highlight.range.start &&
        highlight.range.end &&
        highlight.range.start.line === watchedBinaryHoverPosition.line &&
        highlight.range.start.character === watchedBinaryHoverPosition.character &&
        highlight.range.end.line === watchedBinaryHoverPosition.line &&
        highlight.range.end.character === watchedBinaryHoverPosition.character + 'binarySeed'.length),
    'binary imported member documentHighlight should include the current document usage');

    const watchedBinaryHoverBefore = await client.request('textDocument/hover', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(watchedBinaryHoverBefore &&
        watchedBinaryHoverBefore.contents &&
        watchedBinaryHoverBefore.contents.value.includes('Type: int') &&
        watchedBinaryHoverBefore.contents.value.includes('Source: binary metadata'),
    'binary metadata hover should resolve the initial inferred return type');

    fs.writeFileSync(watchedBinaryFixture.binaryPath, fs.readFileSync(watchedBinaryFixture.binaryPath));
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedBinaryFixture.binaryUri, type: 2 },
        ],
    });

    const watchedBinaryHoverAfter = await client.request('textDocument/hover', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(watchedBinaryHoverAfter &&
        watchedBinaryHoverAfter.contents &&
        watchedBinaryHoverAfter.contents.value.includes('Type: int') &&
        watchedBinaryHoverAfter.contents.value.includes('Source: binary metadata'),
    'workspace/didChangeWatchedFiles binary refresh must keep open analyzers usable for imported binary metadata');

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: genericUri,
        },
    });

    const genericCloseDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(genericCloseDiagnostics.uri === genericUri, 'generic didClose diagnostics uri mismatch');
    assert(Array.isArray(genericCloseDiagnostics.diagnostics) && genericCloseDiagnostics.diagnostics.length === 0,
        'generic didClose must clear diagnostics');

    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedFixture.projectUri, type: 1 },
        ],
    });

    const watchedCreateSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(watchedCreateSymbols) && watchedCreateSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedFixture.mainUri, item.location.uri) &&
        item.name === 'watched_before_refresh'),
    'workspace/didChangeWatchedFiles create must index unopened project sources');

    fs.writeFileSync(watchedFixture.mainPath, [
        'module "main";',
        '',
        'pub func watched_after_refresh(): int {',
        '    return 2;',
        '}',
        '',
    ].join('\n'));
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedFixture.mainUri, type: 2 },
        ],
    });

    const watchedChangeDiagnostics = await waitForDiagnosticsUri(
        client,
        watchedFixture.mainUri,
        'workspace/didChangeWatchedFiles source change diagnostics uri mismatch');
    assert(Array.isArray(watchedChangeDiagnostics.diagnostics) && watchedChangeDiagnostics.diagnostics.length === 0,
        'workspace/didChangeWatchedFiles source change should publish empty diagnostics');

    const watchedUpdatedSymbols = await client.request('workspace/symbol', {
        query: 'watched_after_refresh',
    });
    assert(Array.isArray(watchedUpdatedSymbols) && watchedUpdatedSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedFixture.mainUri, item.location.uri) &&
        item.name === 'watched_after_refresh'),
    'workspace/didChangeWatchedFiles change must refresh unopened source analyzers');

    const watchedRemovedOldSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(watchedRemovedOldSymbols) && watchedRemovedOldSymbols.length === 0,
        'workspace/didChangeWatchedFiles change must replace stale unopened source symbols');

    fs.unlinkSync(watchedFixture.projectPath);
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedFixture.projectUri, type: 3 },
        ],
    });

    const watchedDeleteDiagnostics = await waitForDiagnosticsUri(
        client,
        watchedFixture.projectUri,
        'workspace/didChangeWatchedFiles project delete diagnostics uri mismatch');
    assert(Array.isArray(watchedDeleteDiagnostics.diagnostics) && watchedDeleteDiagnostics.diagnostics.length === 0,
        'workspace/didChangeWatchedFiles project delete must clear diagnostics');

    const watchedDeletedSymbols = await client.request('workspace/symbol', {
        query: 'watched_after_refresh',
    });
    assert(Array.isArray(watchedDeletedSymbols) && watchedDeletedSymbols.length === 0,
        'workspace/didChangeWatchedFiles delete must clear the removed project index');

    const willCreateFiles = await client.request('workspace/willCreateFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    assert(willCreateFiles === null, 'workspace/willCreateFiles must return null when no edits are needed');
    client.notify('workspace/didCreateFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    const fileOperationCreateDiagnostics = await waitForDiagnosticsUri(
        client,
        fileOperationsFixture.projectUri,
        'workspace/didCreateFiles project diagnostics uri mismatch');
    assert(Array.isArray(fileOperationCreateDiagnostics.diagnostics),
        'workspace/didCreateFiles must publish project diagnostics');
    const fileOperationCreateSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(fileOperationCreateSymbols) && fileOperationCreateSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(fileOperationsFixture.mainUri, item.location.uri) &&
        item.name === 'watched_before_refresh'),
    'workspace/didCreateFiles must index newly created unopened project sources');

    const willRenameFiles = await client.request('workspace/willRenameFiles', {
        files: [
            {
                oldUri: fileOperationsFixture.mainUri,
                newUri: fileOperationsFixture.mainUri,
            },
        ],
    });
    assert(willRenameFiles === null, 'workspace/willRenameFiles must return null when no edits are needed');
    const willDeleteFiles = await client.request('workspace/willDeleteFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    assert(willDeleteFiles === null, 'workspace/willDeleteFiles must return null when no edits are needed');
    fs.unlinkSync(fileOperationsFixture.projectPath);
    client.notify('workspace/didDeleteFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    const fileOperationDeleteDiagnostics = await waitForDiagnosticsUri(
        client,
        fileOperationsFixture.projectUri,
        'workspace/didDeleteFiles project delete diagnostics uri mismatch');
    assert(Array.isArray(fileOperationDeleteDiagnostics.diagnostics) &&
        fileOperationDeleteDiagnostics.diagnostics.length === 0,
    'workspace/didDeleteFiles must clear diagnostics for deleted projects');
    const fileOperationDeletedSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(fileOperationDeletedSymbols) && fileOperationDeletedSymbols.length === 0,
        'workspace/didDeleteFiles must clear deleted project indexes');

    const semanticTokens = await client.request('textDocument/semanticTokens/full', {
        textDocument: { uri: docsUri },
    });
    assert(semanticTokens &&
        Array.isArray(semanticTokens.data) &&
        typeof semanticTokens.resultId === 'string' &&
        semanticTokens.resultId.length > 0,
    'semanticTokens/full must return a data array with a resultId');
    const semanticDeltaTokens = await client.request('textDocument/semanticTokens/full/delta', {
        textDocument: { uri: docsUri },
        previousResultId: semanticTokens.resultId,
    });
    assert(semanticDeltaTokens &&
        typeof semanticDeltaTokens.resultId === 'string' &&
        Array.isArray(semanticDeltaTokens.edits) &&
        semanticDeltaTokens.edits.some((edit) =>
            edit &&
            edit.start === 0 &&
            edit.deleteCount === semanticTokens.data.length &&
            Array.isArray(edit.data) &&
            edit.data.length === semanticTokens.data.length),
    'semanticTokens/full/delta must return a full replacement delta edit');
    const semanticRangeTokens = await client.request('textDocument/semanticTokens/range', {
        textDocument: { uri: docsUri },
        range: {
            start: { line: 0, character: 0 },
            end: { line: 6, character: 0 },
        },
    });
    assert(semanticRangeTokens &&
        Array.isArray(semanticRangeTokens.data) &&
        semanticRangeTokens.data.length > 0 &&
        semanticRangeTokens.data.length <= semanticTokens.data.length,
    'semanticTokens/range must return filtered semantic token data');

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: watchedBinaryFixture.mainUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: documentUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: docsUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: colorUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: inlineCompletionUri,
        },
    });

    const expectedClosedUris = new Set([
        watchedBinaryFixture.mainUri,
        documentUri,
        docsUri,
        colorUri,
        inlineCompletionUri,
        importDiagnosticsFixture.mainUri,
    ]);
    let clearedCloseCount = 0;
    while (clearedCloseCount < expectedClosedUris.size) {
        const closeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        assert(expectedClosedUris.has(closeDiagnostics.uri),
            `didClose diagnostics uri mismatch: ${closeDiagnostics.uri}`);
        assert(Array.isArray(closeDiagnostics.diagnostics) && closeDiagnostics.diagnostics.length === 0,
            'didClose must clear diagnostics');
        expectedClosedUris.delete(closeDiagnostics.uri);
        clearedCloseCount += 1;
    }

    const shutdown = await client.request('shutdown', null);
    assert(shutdown === null, 'shutdown must return null');

    client.notify('exit', null);
    const exitCode = await client.waitForExit();
    assert(exitCode === 0, `server exited with ${exitCode}. stderr=${client.stderr()}`);
    assert(client.stderr().trim() === '', `language server stderr must stay empty during stdio smoke. stderr=${client.stderr()}`);
    cleanupPath(watchedFixtureRootToCleanup);
    cleanupPath(watchedBinaryFixtureRootToCleanup);
    cleanupPath(importDiagnosticsFixtureRootToCleanup);
    cleanupPath(fileOperationsFixtureRootToCleanup);
    watchedFixtureRootToCleanup = null;
    watchedBinaryFixtureRootToCleanup = null;
    importDiagnosticsFixtureRootToCleanup = null;
    fileOperationsFixtureRootToCleanup = null;
}

main().catch((error) => {
    cleanupPath(watchedFixtureRootToCleanup);
    cleanupPath(watchedBinaryFixtureRootToCleanup);
    cleanupPath(importDiagnosticsFixtureRootToCleanup);
    cleanupPath(fileOperationsFixtureRootToCleanup);
    watchedFixtureRootToCleanup = null;
    watchedBinaryFixtureRootToCleanup = null;
    importDiagnosticsFixtureRootToCleanup = null;
    fileOperationsFixtureRootToCleanup = null;
    console.error(error.stack || String(error));
    process.exit(1);
});
