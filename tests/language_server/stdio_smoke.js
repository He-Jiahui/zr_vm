const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

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

function createWatchedBinaryMetadataFixture() {
    const sourceFixtureRoot = path.join(__dirname,
        '..',
        'fixtures',
        'projects',
        'aot_module_graph_pipeline');
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-binary-watch-'));
    const projectPath = path.join(rootPath, 'aot_module_graph_pipeline.zrp');
    const mainPath = path.join(rootPath, 'src', 'main.zr');
    const binaryPath = path.join(rootPath, 'bin', 'graph_binary_stage.zro');

    fs.cpSync(sourceFixtureRoot, rootPath, { recursive: true });

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

function cleanupPath(targetPath) {
    if (!targetPath) {
        return;
    }

    fs.rmSync(targetPath, { recursive: true, force: true });
}

let watchedFixtureRootToCleanup = null;
let watchedBinaryFixtureRootToCleanup = null;

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

        const nextParams = this.notificationBacklog.get(message.method)?.shift();
        if (this.notificationBacklog.get(message.method)?.length === 0) {
            this.notificationBacklog.delete(message.method);
        }

        const waiter = waiters.shift();
        if (waiters.length === 0) {
            this.pendingNotifications.delete(message.method);
        }

        clearTimeout(waiter.timer);
        waiter.resolve(nextParams);
    }

    request(method, params, timeoutMs = 3000) {
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

    waitForNotification(method, timeoutMs = 3000) {
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

    waitForExit(timeoutMs = 3000) {
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
    assert(serverPath, 'Expected server executable path as argv[2]');
    const watchedFixture = createWatchedProjectFixture();
    const watchedBinaryFixture = createWatchedBinaryMetadataFixture();
    watchedFixtureRootToCleanup = watchedFixture.rootPath;
    watchedBinaryFixtureRootToCleanup = watchedBinaryFixture.rootPath;

    const client = new LspClient(serverPath);
    const documentUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-smoke.zr';
    const docsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-docs.zr';
    const genericUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-generic.zr';
    const initialText = 'var x = 10; var y = x;';
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
        'func use(): void {',
        '    var value: Derived<Item, 2 + 2> = null;',
        '    var box = new Box<int>();',
        '    var m = new Matrix<int, 2 + 2>();',
        '    value;',
        '    box.shape(m);',
        '}',
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
    assert(initializeResult.capabilities.semanticTokensProvider &&
        initializeResult.capabilities.semanticTokensProvider.full === true,
        'semanticTokensProvider.full must be enabled');
    assert(Array.isArray(initializeResult.capabilities.semanticTokensProvider.legend?.tokenTypes) &&
        initializeResult.capabilities.semanticTokensProvider.legend.tokenTypes.includes('keyword'),
        'semantic token legend must include keyword');

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
        item.location.uri === watchedBinaryFixture.mainUri &&
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

    const watchedBinaryDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(watchedBinaryDiagnostics.uri === watchedBinaryFixture.mainUri,
        'binary watched metadata diagnostics uri mismatch');
    assert(Array.isArray(watchedBinaryDiagnostics.diagnostics) && watchedBinaryDiagnostics.diagnostics.length === 0,
        'binary watched metadata fixture should open without diagnostics');

    const watchedBinaryImportDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryImportDefinitionPosition,
    });
    assert(Array.isArray(watchedBinaryImportDefinition) && watchedBinaryImportDefinition.some((location) =>
        location &&
        location.uri === watchedBinaryFixture.binaryUri &&
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
        location.uri === watchedBinaryFixture.binaryUri &&
        location.range &&
        location.range.start &&
        location.range.start.line === 0 &&
        location.range.start.character === 0),
    'binary imported member definition should navigate to the binary metadata module entry');

    const watchedBinaryMemberReferences = await client.request('textDocument/references', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
        context: { includeDeclaration: true },
    });
    assert(Array.isArray(watchedBinaryMemberReferences) && watchedBinaryMemberReferences.some((location) =>
        location &&
        location.uri === watchedBinaryFixture.mainUri &&
        location.range &&
        location.range.start &&
        location.range.end &&
        location.range.start.line === watchedBinaryHoverPosition.line &&
        location.range.start.character === watchedBinaryHoverPosition.character &&
        location.range.end.line === watchedBinaryHoverPosition.line &&
        location.range.end.character === watchedBinaryHoverPosition.character + 'binarySeed'.length) &&
        watchedBinaryMemberReferences.some((location) =>
            location &&
            location.uri === watchedBinaryFixture.binaryUri &&
            location.range &&
            location.range.start &&
            location.range.start.line === 0 &&
            location.range.start.character === 0),
    'binary imported member references should include the current project usage and the binary metadata module entry');

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
        item.location.uri === watchedFixture.mainUri &&
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

    const watchedChangeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(watchedChangeDiagnostics.uri === watchedFixture.mainUri,
        'workspace/didChangeWatchedFiles source change diagnostics uri mismatch');
    assert(Array.isArray(watchedChangeDiagnostics.diagnostics) && watchedChangeDiagnostics.diagnostics.length === 0,
        'workspace/didChangeWatchedFiles source change should publish empty diagnostics');

    const watchedUpdatedSymbols = await client.request('workspace/symbol', {
        query: 'watched_after_refresh',
    });
    assert(Array.isArray(watchedUpdatedSymbols) && watchedUpdatedSymbols.some((item) =>
        item &&
        item.location &&
        item.location.uri === watchedFixture.mainUri &&
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

    const watchedDeleteDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(watchedDeleteDiagnostics.uri === watchedFixture.projectUri,
        'workspace/didChangeWatchedFiles project delete diagnostics uri mismatch');
    assert(Array.isArray(watchedDeleteDiagnostics.diagnostics) && watchedDeleteDiagnostics.diagnostics.length === 0,
        'workspace/didChangeWatchedFiles project delete must clear diagnostics');

    const watchedDeletedSymbols = await client.request('workspace/symbol', {
        query: 'watched_after_refresh',
    });
    assert(Array.isArray(watchedDeletedSymbols) && watchedDeletedSymbols.length === 0,
        'workspace/didChangeWatchedFiles delete must clear the removed project index');

    const semanticTokens = await client.request('textDocument/semanticTokens/full', {
        textDocument: { uri: docsUri },
    });
    assert(semanticTokens && Array.isArray(semanticTokens.data),
        'semanticTokens/full must return a data array');

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

    const expectedClosedUris = new Set([
        watchedBinaryFixture.mainUri,
        documentUri,
        docsUri,
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
    cleanupPath(watchedFixtureRootToCleanup);
    cleanupPath(watchedBinaryFixtureRootToCleanup);
    watchedFixtureRootToCleanup = null;
    watchedBinaryFixtureRootToCleanup = null;
}

main().catch((error) => {
    cleanupPath(watchedFixtureRootToCleanup);
    cleanupPath(watchedBinaryFixtureRootToCleanup);
    watchedFixtureRootToCleanup = null;
    watchedBinaryFixtureRootToCleanup = null;
    console.error(error.stack || String(error));
    process.exit(1);
});
