const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const vscode = require('vscode');

const CLASSES_FULL_SMOKE_SOURCE = [
    'module "classes_full";',
    '',
    'class BaseHero {',
    '    pri var _hp: int = 0;',
    '',
    '    pub @constructor(seed: int) {',
    '        this._hp = seed;',
    '    }',
    '',
    '    // Current hero hit points.',
    '    pub get hp: int {',
    '        return this._hp;',
    '    }',
    '',
    '    pub set hp(v: int) {',
    '        this._hp = v;',
    '    }',
    '',
    '    pub heal(amount: int): int {',
    '        this.hp = this.hp + amount;',
    '        return this.hp;',
    '    }',
    '}',
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
    'class BossHero: BaseHero {',
    '    pub static var created: int = 0;',
    '',
    '    pub @constructor(seed: int) super(seed) {',
    '        BossHero.created = BossHero.created + 1;',
    '    }',
    '',
    '    // Calculates the boss total score.',
    '    pub total(): int {',
    '        return this.hp + ScoreBoard.bonus + BossHero.created;',
    '    }',
    '}',
    '',
    '%test("classesFullProjectShape") {',
    '    var boss = new BossHero(30);',
    '    boss.hp = boss.hp + 7;',
    '    ScoreBoard.bonus = boss.heal(5);',
    '    return boss.total() + ScoreBoard.bonus;',
    '}',
    '',
].join('\n');

const PROJECT_INFERENCE_SMOKE_SOURCE = [
    'var greetModule = %import("greet");',
    '',
    'class Hero {',
    '    pub total(): int {',
    '        return 1;',
    '    }',
    '}',
    '',
    'take(): %unique Hero {',
    '    return %unique new Hero();',
    '}',
    '',
    'var hero = take();',
    'return greetModule.greet();',
    '',
].join('\n');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

async function sleep(milliseconds) {
    await new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function withRetry(action, predicate, timeoutMs, label) {
    const deadline = Date.now() + timeoutMs;
    let lastError;

    while (Date.now() < deadline) {
        try {
            const value = await action();
            if (predicate(value)) {
                return value;
            }
        } catch (error) {
            lastError = error;
        }

        await sleep(150);
    }

    if (lastError) {
        throw lastError;
    }

    throw new Error(`Timed out waiting for ${label}`);
}

function findPositionBySubstring(document, substring, occurrence = 0, offset = 0) {
    const text = document.getText();
    let fromIndex = 0;
    let index = -1;

    for (let current = 0; current <= occurrence; current += 1) {
        index = text.indexOf(substring, fromIndex);
        if (index < 0) {
            throw new Error(`Unable to find substring "${substring}" in ${document.uri.toString()}`);
        }
        fromIndex = index + substring.length;
    }

    return document.positionAt(index + offset);
}

async function openDocument(filePath) {
    const document = await vscode.workspace.openTextDocument(filePath);
    await vscode.window.showTextDocument(document);
    return document;
}

async function verifyDiagnostics(workspaceRoot) {
    const diagnosticUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'diagnostics_smoke.zr');
    await vscode.workspace.fs.writeFile(
        diagnosticUri,
        new TextEncoder().encode('var x = ;'),
    );

    const document = await openDocument(diagnosticUri);

    const diagnostics = await withRetry(
        async () => vscode.languages.getDiagnostics(document.uri),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'diagnostics',
    );

    assert(diagnostics[0].message.length > 0, 'Expected syntax diagnostics to include a message');

    await vscode.workspace.fs.delete(diagnosticUri, { useTrash: false });
}

async function verifyLanguageFeatures(workspaceRoot) {
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'lsp_smoke.zr');
    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode('var x = 10; var y = x;'),
    );

    const mainDocument = await openDocument(smokeUri);
    const definitionPosition = findPositionBySubstring(mainDocument, 'x', 0);
    const definition = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDefinitionProvider',
            mainDocument.uri,
            definitionPosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'definition provider',
    );
    assert(uriPath(definition[0].uri).endsWith('/src/lsp_smoke.zr'),
        'Definition should resolve into lsp_smoke.zr');

    const hover = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeHoverProvider',
            mainDocument.uri,
            definitionPosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'hover provider',
    );
    assert(hover.length > 0, 'Expected hover results');

    const completions = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeCompletionItemProvider',
            mainDocument.uri,
            new vscode.Position(0, 0),
        ),
        (items) => {
            if (!items) {
                return false;
            }
            const entries = Array.isArray(items) ? items : items.items;
            return Array.isArray(entries);
        },
        15000,
        'completion provider',
    );
    const completionItems = Array.isArray(completions) ? completions : completions.items;
    assert(Array.isArray(completionItems), 'Expected completion list array');

    const references = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeReferenceProvider',
            mainDocument.uri,
            definitionPosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'reference provider',
    );
    assert(references.some((item) => uriPath(item.uri).endsWith('/src/lsp_smoke.zr')),
        'Expected references to include lsp_smoke.zr');

    const documentSymbols = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDocumentSymbolProvider',
            mainDocument.uri,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'document symbols',
    );
    assert(documentSymbols.some((item) => item.name === 'x'),
        'Expected document symbols to include x');

    const workspaceSymbols = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeWorkspaceSymbolProvider',
            'x',
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'workspace symbols',
    );
    assert(workspaceSymbols.some((item) => item.name === 'x'),
        'Expected workspace symbols to include x');

    const renameEdit = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDocumentRenameProvider',
            mainDocument.uri,
            definitionPosition,
            'renamedX',
        ),
        (value) => Boolean(value),
        15000,
        'rename provider',
    );

    const renameEntries = renameEdit.entries();
    assert(renameEntries.some(([uri]) => uriPath(uri).endsWith('/src/lsp_smoke.zr')),
        'Rename should include lsp_smoke.zr edits');

    await vscode.workspace.fs.delete(smokeUri, { useTrash: false });
}

function completionEntries(items) {
    if (!items) {
        return [];
    }

    return Array.isArray(items) ? items : items.items;
}

function markdownLikeToString(value) {
    if (!value) {
        return '';
    }

    if (typeof value === 'string') {
        return value;
    }

    if (Array.isArray(value)) {
        return value.map(markdownLikeToString).join('\n');
    }

    if (typeof value.value === 'string') {
        return value.value;
    }

    return String(value);
}

function hoverText(items) {
    if (!Array.isArray(items)) {
        return '';
    }

    return items.map((item) => markdownLikeToString(item.contents)).join('\n');
}

function completionDocumentationText(item) {
    return markdownLikeToString(item?.documentation);
}

function detailText(item) {
    return item?.detail ?? item?.label?.detail ?? '';
}

function locationUri(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.uri ?? entry.targetUri ?? entry.location?.uri;
}

function locationRange(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.range ?? entry.targetSelectionRange ?? entry.targetRange ?? entry.location?.range;
}

function positionEquals(position, line, character) {
    return Boolean(position) && position.line === line && position.character === character;
}

function rangeEquals(range, startLine, startCharacter, endLine, endCharacter) {
    return Boolean(range) &&
        positionEquals(range.start, startLine, startCharacter) &&
        positionEquals(range.end, endLine, endCharacter);
}

function hasDocumentSymbol(items, name) {
    if (!Array.isArray(items)) {
        return false;
    }

    return items.some((item) =>
        item &&
        (item.name === name || hasDocumentSymbol(item.children, name)));
}

function semanticTokenData(tokens) {
    if (!tokens || !tokens.data) {
        return [];
    }

    if (Array.isArray(tokens.data)) {
        return tokens.data;
    }

    if (typeof tokens.data.length === 'number') {
        return Array.from(tokens.data);
    }

    return [];
}

function decodeSemanticTokens(document, legend, tokens) {
    const data = semanticTokenData(tokens);
    const tokenTypes = Array.isArray(legend?.tokenTypes) ? legend.tokenTypes : [];
    const decoded = [];
    let line = 0;
    let character = 0;

    for (let index = 0; index + 4 < data.length; index += 5) {
        const deltaLine = data[index];
        const deltaCharacter = data[index + 1];
        const length = data[index + 2];
        const typeIndex = data[index + 3];

        line += deltaLine;
        character = deltaLine === 0 ? character + deltaCharacter : deltaCharacter;
        decoded.push({
            line,
            character,
            length,
            type: tokenTypes[typeIndex] ?? `unknown:${typeIndex}`,
            text: document.getText(new vscode.Range(line, character, line, character + length)),
        });
    }

    return decoded;
}

function hasSemanticToken(decodedTokens, expectedType, expectedText) {
    return decodedTokens.some((token) =>
        token.type === expectedType && token.text === expectedText);
}

async function verifyProjectInferenceAndSemanticTokens(workspaceRoot) {
    const projectMainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr');
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'project_inference_smoke.zr');
    const featureTimeoutMs = 30000;
    const projectDocument = await openDocument(projectMainUri);
    const aliasUsagePosition = findPositionBySubstring(projectDocument, 'greetModule.greet()', 0, 1);

    const hover = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeHoverProvider',
            projectDocument.uri,
            aliasUsagePosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        featureTimeoutMs,
        'project import hover provider',
    );
    assert(hoverText(hover).includes('module <greet>'),
        'Import alias hover should render module display text');

    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode(PROJECT_INFERENCE_SMOKE_SOURCE),
    );

    const document = await openDocument(smokeUri);
    const takeUsagePosition = findPositionBySubstring(document, 'var hero = take();', 0, 'var hero = '.length);

    const takeHover = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeHoverProvider',
            document.uri,
            takeUsagePosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        featureTimeoutMs,
        'ownership hover provider',
    );
    assert(hoverText(takeHover).includes('%unique Hero'),
        'Hover should preserve ownership qualifiers in function type display');

    const semanticLegend = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.provideDocumentSemanticTokensLegend',
            document.uri,
        ),
        (value) => Array.isArray(value?.tokenTypes) && value.tokenTypes.length > 0,
        featureTimeoutMs,
        'semantic token legend',
    );
    const semanticTokens = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.provideDocumentSemanticTokens',
            document.uri,
        ),
        (value) => semanticTokenData(value).length > 0,
        featureTimeoutMs,
        'semantic tokens',
    );
    const decodedTokens = decodeSemanticTokens(document, semanticLegend, semanticTokens);
    assert(hasSemanticToken(decodedTokens, 'keyword', '%import'),
        'Semantic tokens should mark %import as a keyword');
    assert(hasSemanticToken(decodedTokens, 'keyword', '%unique'),
        'Semantic tokens should mark ownership directives as keywords');
    assert(hasSemanticToken(decodedTokens, 'namespace', 'greetModule'),
        'Semantic tokens should mark imported module aliases as namespaces');
    assert(hasSemanticToken(decodedTokens, 'class', 'Hero'),
        'Semantic tokens should mark class names');
    assert(hasSemanticToken(decodedTokens, 'method', 'total'),
        'Semantic tokens should mark methods');

    await vscode.workspace.fs.delete(smokeUri, { useTrash: false });
}

async function verifyClassLanguageFeatures(workspaceRoot) {
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'classes_full_smoke.zr');
    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode(CLASSES_FULL_SMOKE_SOURCE),
    );

    const document = await openDocument(smokeUri);
    const bossHeroUsage = findPositionBySubstring(document, 'BossHero(30)', 0);
    const bossCompletionPosition = findPositionBySubstring(document, 'boss.hp =', 0, 5);
    const scoreBoardCompletionPosition = findPositionBySubstring(document, 'ScoreBoard.bonus =', 0, 11);
    const totalUsagePosition = findPositionBySubstring(document, 'boss.total() + ScoreBoard.bonus', 0, 5);
    const bossHeroDefinitionPosition = findPositionBySubstring(document, 'class BossHero: BaseHero', 0, 6);
    const totalDefinitionPosition = findPositionBySubstring(document, 'pub total(): int {', 0, 4);

    const bossHeroDefinition = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDefinitionProvider',
            document.uri,
            bossHeroUsage,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'BossHero definition provider',
    );
    assert(
        bossHeroDefinition.some((item) =>
            uriPath(locationUri(item)).endsWith('/src/classes_full_smoke.zr') &&
            rangeEquals(
                locationRange(item),
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character,
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character + 'BossHero'.length,
            )),
        'BossHero definition should resolve to the class identifier span',
    );

    const bossHeroReferences = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeReferenceProvider',
            document.uri,
            bossHeroUsage,
        ),
        (items) => Array.isArray(items) && items.length >= 2,
        15000,
        'BossHero reference provider',
    );
    assert(
        bossHeroReferences.some((item) =>
            uriPath(locationUri(item)).endsWith('/src/classes_full_smoke.zr') &&
            rangeEquals(
                locationRange(item),
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character,
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character + 'BossHero'.length,
            )),
        'BossHero references should include the class declaration',
    );

    const bossCompletions = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeCompletionItemProvider',
            document.uri,
            bossCompletionPosition,
        ),
        (items) => completionEntries(items).length > 0,
        15000,
        'boss member completion',
    );
    const bossCompletionLabels = completionEntries(bossCompletions).map((item) => item.label?.label ?? item.label);
    assert(bossCompletionLabels.includes('hp'), 'boss. completion should include property hp');
    assert(bossCompletionLabels.includes('heal'), 'boss. completion should include method heal');
    assert(bossCompletionLabels.includes('total'), 'boss. completion should include method total');

    const scoreBoardCompletions = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeCompletionItemProvider',
            document.uri,
            scoreBoardCompletionPosition,
        ),
        (items) => completionEntries(items).length > 0,
        15000,
        'ScoreBoard member completion',
    );
    const scoreBoardCompletionLabels = completionEntries(scoreBoardCompletions)
        .map((item) => item.label?.label ?? item.label);
    assert(scoreBoardCompletionLabels.includes('bonus'),
        'ScoreBoard. completion should include static property bonus');
    const bonusCompletion = completionEntries(scoreBoardCompletions)
        .find((item) => (item.label?.label ?? item.label) === 'bonus');
    assert(completionDocumentationText(bonusCompletion).includes('Shared bonus exposed through get/set.'),
        'ScoreBoard. completion should surface leading property comments in documentation');

    const totalDefinition = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDefinitionProvider',
            document.uri,
            totalUsagePosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'total definition provider',
    );
    assert(
        totalDefinition.some((item) =>
            uriPath(locationUri(item)).endsWith('/src/classes_full_smoke.zr') &&
            rangeEquals(
                locationRange(item),
                totalDefinitionPosition.line,
                totalDefinitionPosition.character,
                totalDefinitionPosition.line,
                totalDefinitionPosition.character + 'total'.length,
            )),
        'Function definition should resolve to the identifier span instead of the whole declaration',
    );

    const totalHover = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeHoverProvider',
            document.uri,
            totalUsagePosition,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'total hover provider',
    );
    const totalHoverText = hoverText(totalHover);
    assert(totalHoverText.includes('Calculates the boss total score.'),
        'Hover should include the leading method comment');
    assert(!totalHoverText.includes('[object Object]'),
        'Hover should render markdown instead of object placeholders');

    const renameEdit = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDocumentRenameProvider',
            document.uri,
            totalUsagePosition,
            'renamedTotal',
        ),
        (value) => Boolean(value),
        15000,
        'total rename provider',
    );
    const renameEntries = renameEdit.entries();
    assert(renameEntries.some(([uri, edits]) =>
        uriPath(uri).endsWith('/src/classes_full_smoke.zr') &&
        Array.isArray(edits) &&
        edits.length >= 2),
    'Function rename should include both declaration and usage edits');

    const documentSymbols = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeDocumentSymbolProvider',
            document.uri,
        ),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'classes document symbols',
    );
    assert(hasDocumentSymbol(documentSymbols, 'hp'),
        'Document symbols should include property hp');
    assert(hasDocumentSymbol(documentSymbols, 'bonus'),
        'Document symbols should include property bonus');
    assert(hasDocumentSymbol(documentSymbols, 'heal'),
        'Document symbols should include method heal');
    assert(hasDocumentSymbol(documentSymbols, 'total'),
        'Document symbols should include method total');

    await vscode.workspace.fs.delete(smokeUri, { useTrash: false });
}

async function waitForDebugEvent(eventName, timeoutMs, expectedSessionId) {
    return new Promise((resolve, reject) => {
        const trackerDisposable = vscode.debug.registerDebugAdapterTrackerFactory('zr', {
            createDebugAdapterTracker(debugSession) {
                if (expectedSessionId && debugSession.id !== expectedSessionId) {
                    return undefined;
                }

                return {
                    onDidSendMessage(message) {
                        if (message && message.type === 'event' && message.event === eventName) {
                            clearTimeout(timeoutHandle);
                            trackerDisposable.dispose();
                            resolve(message);
                        }
                    },
                };
            },
        });
        const timeoutHandle = setTimeout(() => {
            trackerDisposable.dispose();
            reject(new Error(`Timed out waiting for debug event ${eventName}`));
        }, timeoutMs);
    });
}

async function startExternalDebugTarget(cliPath, workspaceRoot) {
    const projectPath = path.join(workspaceRoot.fsPath, 'import_basic.zrp');
    const child = spawn(cliPath, [
        projectPath,
        '--debug',
        '--debug-address',
        '127.0.0.1:0',
        '--debug-wait',
        '--debug-print-endpoint',
    ], {
        cwd: workspaceRoot.fsPath,
        stdio: ['ignore', 'pipe', 'pipe'],
    });

    return new Promise((resolve, reject) => {
        let stdoutBuffer = '';
        let stderrBuffer = '';
        let resolved = false;
        const timeoutHandle = setTimeout(() => {
            if (!resolved) {
                child.kill();
                reject(new Error(`Timed out waiting for external debug endpoint.\nstdout:\n${stdoutBuffer}\nstderr:\n${stderrBuffer}`));
            }
        }, 15000);

        function finish(error, value) {
            clearTimeout(timeoutHandle);
            if (error) {
                reject(error);
                return;
            }
            resolve(value);
        }

        child.stdout.on('data', (chunk) => {
            stdoutBuffer += chunk.toString();
            const match = stdoutBuffer.match(/debug_endpoint=([^\r\n]+)/);
            if (match && !resolved) {
                resolved = true;
                finish(undefined, {
                    child,
                    endpoint: match[1].trim(),
                });
            }
        });
        child.stderr.on('data', (chunk) => {
            stderrBuffer += chunk.toString();
        });
        child.on('exit', (code) => {
            if (!resolved) {
                finish(new Error(`External debug target exited before endpoint became available (code=${code}).\nstdout:\n${stdoutBuffer}\nstderr:\n${stderrBuffer}`));
            }
        });
        child.on('error', (error) => {
            if (!resolved) {
                finish(error);
            }
        });
    });
}

async function verifyDebugStateInspection(session, expectedSourcePath) {
    const stackTrace = await session.customRequest('stackTrace', { threadId: 1 });
    const stackFrames = Array.isArray(stackTrace?.stackFrames) ? stackTrace.stackFrames : [];

    assert(stackFrames.length > 0, 'Expected stackTrace to return at least one frame');
    assert(typeof stackFrames[0].line === 'number' && stackFrames[0].line > 0,
        'Expected top stack frame to expose a source line');
    assert(normalizePath(stackFrames[0]?.source?.path ?? '') === normalizePath(expectedSourcePath),
        'Expected top stack frame source path to match the launched ZR source');

    const scopesResult = await session.customRequest('scopes', { frameId: stackFrames[0].id });
    const scopes = Array.isArray(scopesResult?.scopes) ? scopesResult.scopes : [];

    assert(scopes.length > 0, 'Expected scopes request to return at least one scope');

    for (const scope of scopes) {
        if (!scope || typeof scope.variablesReference !== 'number' || scope.variablesReference <= 0) {
            continue;
        }

        const variablesResult = await session.customRequest('variables', {
            variablesReference: scope.variablesReference,
        });
        const variables = Array.isArray(variablesResult?.variables) ? variablesResult.variables : [];

        if (variables.some((item) => item?.name === 'greetModule')) {
            return;
        }
    }

    throw new Error('Expected variables request to expose greetModule in at least one scope');
}

async function verifyDebugIntegration(workspaceRoot) {
    const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');
    const contributedDebuggers = extension?.packageJSON?.contributes?.debuggers ?? [];
    const bundledFolder = `${process.platform}-${process.arch}`;
    const bundledCliPath = process.platform === 'win32'
        ? path.join(extension.extensionPath, 'server', 'native', bundledFolder, 'zr_vm_cli.exe')
        : path.join(extension.extensionPath, 'server', 'native', bundledFolder, 'zr_vm_cli');
    const debugMainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr');
    const debugDocument = await openDocument(debugMainUri);
    const debugBreakpointPosition = findPositionBySubstring(debugDocument, 'return greetModule.greet()');
    const debugBreakpoint = new vscode.SourceBreakpoint(
        new vscode.Location(debugDocument.uri, debugBreakpointPosition),
    );
    let attachTarget;
    let session;
    let started;

    assert(extension, 'Expected Zr extension to be present before debug verification');
    assert((await vscode.commands.getCommands(true)).includes('zr.debugCurrentProject'),
        'Expected zr.debugCurrentProject to be registered');
    assert((await vscode.commands.getCommands(true)).includes('zr.attachDebugEndpoint'),
        'Expected zr.attachDebugEndpoint to be registered');
    assert(Array.isArray(contributedDebuggers) && contributedDebuggers.some((item) => item.type === 'zr'),
        'Expected ZR debugger contributions to be present');
    assert(fs.existsSync(bundledCliPath),
        `Expected bundled zr_vm_cli at ${bundledCliPath}`);

    vscode.debug.addBreakpoints([debugBreakpoint]);

    try {
        const breakpointResolved = waitForDebugEvent('breakpoint', 15000);
        const launchStopped = waitForDebugEvent('stopped', 15000);
        const launchTerminated = waitForDebugEvent('terminated', 30000);
        started = await vscode.debug.startDebugging(workspaceRoot, {
            type: 'zr',
            name: 'ZR Smoke Launch',
            request: 'launch',
            project: debugMainUri.fsPath.replace(/[\\\/]src[\\\/]main\.zr$/i, `${path.sep}import_basic.zrp`),
            cwd: workspaceRoot.fsPath,
            executionMode: 'interp',
            stopOnEntry: false,
        });
        assert(started, 'Expected ZR launch debug session to start');
        session = await withRetry(
            async () => vscode.debug.activeDebugSession,
            (value) => value && value.type === 'zr',
            15000,
            'ZR debug session start',
        );
        const breakpointEvent = await breakpointResolved;
        assert(breakpointEvent?.body?.breakpoint?.verified !== false,
            'Expected launch breakpoint to resolve before continuing');
        const stoppedEvent = await launchStopped;
        assert(stoppedEvent?.body?.reason === 'breakpoint',
            'Expected launch session to stop because the source breakpoint was hit');
        await verifyDebugStateInspection(session, debugDocument.uri.fsPath);
        await session.customRequest('continue', { threadId: 1 });
        await launchTerminated;
    } finally {
        if (session) {
            await vscode.debug.stopDebugging(session);
        }
        vscode.debug.removeBreakpoints([debugBreakpoint]);
    }

    attachTarget = await startExternalDebugTarget(bundledCliPath, workspaceRoot);
    session = undefined;
    try {
        const attachStopped = waitForDebugEvent('stopped', 15000);
        const attachTerminated = waitForDebugEvent('terminated', 30000);
        started = await vscode.debug.startDebugging(workspaceRoot, {
            type: 'zr',
            name: 'ZR Smoke Attach',
            request: 'attach',
            endpoint: attachTarget.endpoint,
        });
        assert(started, 'Expected ZR attach debug session to start');
        session = await withRetry(
            async () => vscode.debug.activeDebugSession,
            (value) => value && value.type === 'zr',
            15000,
            'ZR attach session start',
        );
        const attachStoppedEvent = await attachStopped;
        assert(attachStoppedEvent?.body?.reason === 'entry',
            'Expected attach session to observe the runtime entry stop');
        await session.customRequest('continue', { threadId: 1 });
        await attachTerminated;
    } finally {
        if (session) {
            await vscode.debug.stopDebugging(session);
        }
        if (attachTarget) {
            attachTarget.child.kill();
        }
    }
}

async function runSmokeSuite({ expectedMode, focus = 'all' }) {
    const workspaceFolder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
    assert(workspaceFolder, 'Expected a workspace folder for smoke test');
    const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');
    assert(extension, 'Expected Zr extension to be present in extension host');

    await extension.activate();
    assert(extension.isActive, 'Expected Zr extension to remain active after activation');
    assert(expectedMode === 'native' || expectedMode === 'web',
        `Unexpected smoke mode: ${expectedMode}`);

    if (focus === 'all' || focus === 'lsp') {
        await verifyLanguageFeatures(workspaceFolder.uri);
        await verifyProjectInferenceAndSemanticTokens(workspaceFolder.uri);
        await verifyClassLanguageFeatures(workspaceFolder.uri);
        await verifyDiagnostics(workspaceFolder.uri);
    }
    await verifyDebugIntegration(workspaceFolder.uri);
}

function uriPath(uri) {
    return typeof uri.path === 'string' ? uri.path : uri.toString();
}

function normalizePath(value) {
    return typeof value === 'string' ? value.replace(/[\\/]+/g, '/').toLowerCase() : '';
}

module.exports = {
    runSmokeSuite,
};
