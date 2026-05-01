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

const STRUCTURE_SMOKE_MAIN_SOURCE = [
    'var helper = %import("structure_helper");',
    'var system = %import("zr.system");',
    '',
    'class StructureHero {',
    '    pub total(): int {',
    '        return helper.value();',
    '    }',
    '}',
    '',
    '%test("structureViewSmoke") {',
    '    return helper.value();',
    '}',
    '',
    'return helper.value();',
    '',
].join('\n');

const STRUCTURE_SMOKE_HELPER_SOURCE = [
    'var cycle = %import("structure_cycle");',
    '',
    'pub var value = () => {',
    '    return cycle.answer();',
    '};',
    '',
].join('\n');

const STRUCTURE_SMOKE_CYCLE_SOURCE = [
    'var helper = %import("structure_helper");',
    '',
    'pub var answer = () => {',
    '    return 42;',
    '};',
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

async function withActionTimeout(promise, timeoutMs, label) {
    let timeoutHandle;

    try {
        return await Promise.race([
            promise,
            new Promise((_, reject) => {
                timeoutHandle = setTimeout(() => {
                    reject(new Error(`Timed out executing ${label}`));
                }, timeoutMs);
            }),
        ]);
    } finally {
        if (timeoutHandle) {
            clearTimeout(timeoutHandle);
        }
    }
}

async function withRetry(action, predicate, timeoutMs, label) {
    const deadline = Date.now() + timeoutMs;
    let lastError;
    let lastValue;

    while (Date.now() < deadline) {
        try {
            const attemptTimeoutMs = Math.min(15000, Math.max(1000, timeoutMs));
            const value = await withActionTimeout(action(), attemptTimeoutMs, label);
            lastValue = value;
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

    throw new Error(`Timed out waiting for ${label}${summarizeRetryValue(lastValue)}`);
}

function summarizeRetryValue(value) {
    if (value === undefined) {
        return '';
    }

    if (Array.isArray(value)) {
        return ` (last value: array length ${value.length})`;
    }

    if (value && Array.isArray(value.items)) {
        return ` (last value: items length ${value.items.length})`;
    }

    if (value && typeof value === 'object') {
        const keys = Object.keys(value);
        return ` (last value keys: ${keys.join(', ')})`;
    }

    return ` (last value: ${String(value)})`;
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

async function deleteDocumentFile(uri, fallbackUri) {
    const workspaceFolder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
    const fallbackTarget = fallbackUri ?? vscode.Uri.joinPath(workspaceFolder.uri, 'src', 'main.zr');

    const openDocumentToDelete = vscode.workspace.textDocuments.find((document) => document.uri.toString() === uri.toString());
    if (uri.scheme === 'vscode-test-web' && openDocumentToDelete?.isDirty) {
        await vscode.window.showTextDocument(openDocumentToDelete, { preview: false });
        await vscode.commands.executeCommand('workbench.action.files.revert');
    }

    if (vscode.window.activeTextEditor?.document?.uri?.toString() === uri.toString()) {
        const fallbackDocument = await vscode.workspace.openTextDocument(fallbackTarget);
        await vscode.window.showTextDocument(fallbackDocument, { preview: false });
    }

    // vscode-test-web may emit ENOPRO noise after delete events while the smoke host shuts down.
    // Leaving the temporary files in the ephemeral web workspace keeps the run deterministic.
    if (uri.scheme === 'vscode-test-web') {
        return;
    }

    await vscode.workspace.fs.delete(uri, { useTrash: false });
}

async function withPatchedWindowMethod(methodName, replacement, action) {
    const original = vscode.window[methodName];
    let restored = false;

    assert(typeof original === 'function', `Expected vscode.window.${methodName} to be patchable`);
    vscode.window[methodName] = replacement;

    try {
        return await action();
    } finally {
        if (!restored) {
            vscode.window[methodName] = original;
            restored = true;
        }
    }
}

async function verifyDiagnostics(workspaceRoot) {
    console.log('[zr-web-smoke] verifyDiagnostics:start');
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

    await deleteDocumentFile(diagnosticUri);
    console.log('[zr-web-smoke] verifyDiagnostics:done');
}

async function verifyLanguageFeatures(workspaceRoot) {
    console.log('[zr-web-smoke] verifyLanguageFeatures:start');
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'lsp_smoke.zr');
    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode('var x = 10; var y = x;'),
    );

    const mainDocument = await openDocument(smokeUri);
    const definitionPosition = findPositionBySubstring(mainDocument, 'x', 1);
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
    assert(uriPath(locationUri(definition[0])).endsWith('/src/lsp_smoke.zr'),
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
        async () => executeCompletionItems(mainDocument.uri, new vscode.Position(0, 0)),
        (items) => completionEntries(items).length >= 0,
        15000,
        'completion provider',
    );
    const completionItems = completionEntries(completions);
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
    assert(references.some((item) => uriPath(locationUri(item)).endsWith('/src/lsp_smoke.zr')),
        'Expected references to include lsp_smoke.zr');

    const documentSymbols = await withRetry(
        async () => executeDocumentSymbols(mainDocument.uri),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'document symbols',
    );
    assert(hasDocumentSymbol(documentSymbols, 'x'),
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

    await deleteDocumentFile(smokeUri);
    console.log('[zr-web-smoke] verifyLanguageFeatures:done');
}

async function verifyAdvancedEditorProviders(workspaceRoot) {
    console.log('[zr-web-smoke] verifyAdvancedEditorProviders:start');
    const advancedUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_smoke_${Date.now()}.zr`);
    const actionUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_action_${Date.now()}.zr`);
    const organizeUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_imports_${Date.now()}.zr`);
    const cleanupUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_cleanup_${Date.now()}.zr`);
    const commands = await vscode.commands.getCommands(true);
    assert(commands.includes('zr.organizeImports'), 'Expected web zr.organizeImports command to be registered');
    assert(commands.includes('zr.removeUnusedImports'), 'Expected web zr.removeUnusedImports command to be registered');
    const advancedSource = [
        'var system = %import("zr.system");',
        'var tcp = %import("zr.network.tcp");',
        '',
        'class AdvancedSmoke {',
        'pub func run(value: int): int {',
        'let local = value;',
        'return local;',
        '}',
        '}',
        '',
        '%test("advancedEditorSmoke") {',
        'return 1;',
        '}',
        '',
    ].join('\n');

    try {
        await vscode.workspace.fs.writeFile(
            advancedUri,
            new TextEncoder().encode(advancedSource),
        );

        const document = await openDocument(advancedUri);
        const fullRange = new vscode.Range(
            new vscode.Position(0, 0),
            document.lineAt(document.lineCount - 1).range.end,
        );

        const formattingEdits = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeFormatDocumentProvider',
                document.uri,
                { tabSize: 4, insertSpaces: true },
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'document formatting provider',
        );
        assert(formattingEdits.some((edit) => edit.newText === '    ') &&
                formattingEdits.some((edit) => edit.newText === '        '),
            'Expected web format provider to produce nested indentation edits');

        const rangeFormattingEdits = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeFormatRangeProvider',
                document.uri,
                fullRange,
                { tabSize: 4, insertSpaces: true },
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'range formatting provider',
        );
        assert(rangeFormattingEdits.some((edit) => edit.newText === '        ') ||
                rangeFormattingEdits.some((edit) => edit.newText.includes('        return local;')),
            'Expected web range format provider to indent method body');

        const foldingRanges = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeFoldingRangeProvider',
                document.uri,
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'folding range provider',
        );
        assert(foldingRanges.some((item) => item.start <= 4 && item.end >= 6) ||
                foldingRanges.some((item) => item.start <= 3 && item.end >= 7),
            'Expected web folding ranges for class/function regions');

        const selectionRanges = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeSelectionRangeProvider',
                document.uri,
                [findPositionBySubstring(document, 'local;', 0, 0)],
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'selection range provider',
        );
        assert(selectionRanges[0]?.range && selectionRanges[0]?.parent,
            'Expected web semantic selection ranges with parent expansion');

        await vscode.workspace.fs.writeFile(
            actionUri,
            new TextEncoder().encode('var answer = 42\n'),
        );
        const actionDocument = await openDocument(actionUri);
        const codeActions = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeCodeActionProvider',
                actionDocument.uri,
                new vscode.Range(new vscode.Position(0, 0), new vscode.Position(0, 0)),
                'quickfix',
            ),
            (items) => Array.isArray(items) && items.some((item) =>
                (item.kind?.value ?? item.kind) === 'quickfix'),
            15000,
            'code action provider',
        );
        assert(codeActions.some((item) => (item.kind?.value ?? item.kind) === 'quickfix' &&
                (item.edit || item.command || /semicolon/i.test(item.title ?? ''))),
            'Expected web quick fix code action');

        await vscode.workspace.fs.writeFile(
            organizeUri,
            new TextEncoder().encode([
                '%import("zr.system");',
                '%import("zr.math");',
                '',
                'return 1;',
                '',
            ].join('\n')),
        );
        const organizeDocument = await openDocument(organizeUri);
        await vscode.commands.executeCommand('zr.organizeImports');
        await withRetry(
            async () => organizeDocument.getText(),
            (text) => text.indexOf('%import("zr.math");\n%import("zr.system");') >= 0,
            15000,
            'zr.organizeImports command',
        );

        await vscode.workspace.fs.writeFile(
            cleanupUri,
            new TextEncoder().encode([
                'var math = %import("zr.math");',
                'var system = %import("zr.system");',
                '',
                'return math.PI;',
                '',
            ].join('\n')),
        );
        const cleanupDocument = await openDocument(cleanupUri);
        await vscode.commands.executeCommand('zr.removeUnusedImports');
        await withRetry(
            async () => cleanupDocument.getText(),
            (text) => text.includes('var math = %import("zr.math");') &&
                !text.includes('var system = %import("zr.system");'),
            15000,
            'zr.removeUnusedImports command',
        );

        const documentLinks = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeLinkProvider',
                document.uri,
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'document link provider',
        );
        assert(documentLinks.some((item) => item.target && item.range),
            'Expected web document links with targets');

        const codeLens = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeCodeLensProvider',
                document.uri,
                10,
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'code lens provider',
        );
        assert(codeLens.some((item) => item.command?.command === 'zr.runCurrentProject'),
            'Expected web CodeLens to expose the Zr test run command');

        const pullDiagnostics = await sendRawLanguageServerRequest('textDocument/diagnostic', {
            textDocument: { uri: document.uri.toString(true) },
        });
        assert(pullDiagnostics &&
                pullDiagnostics.kind === 'full' &&
                Array.isArray(pullDiagnostics.items) &&
                typeof pullDiagnostics.resultId === 'string' &&
                pullDiagnostics.resultId.length > 0,
            'Expected web textDocument/diagnostic to return a full report');

        const unchangedDiagnostics = await sendRawLanguageServerRequest('textDocument/diagnostic', {
            textDocument: { uri: document.uri.toString(true) },
            previousResultId: pullDiagnostics.resultId,
        });
        assert(unchangedDiagnostics &&
                unchangedDiagnostics.kind === 'unchanged' &&
                unchangedDiagnostics.resultId === pullDiagnostics.resultId &&
                !Object.prototype.hasOwnProperty.call(unchangedDiagnostics, 'items'),
            'Expected web textDocument/diagnostic to return unchanged reports');

        const workspaceDiagnostics = await sendRawLanguageServerRequest('workspace/diagnostic', {
            previousResultIds: [
                {
                    uri: document.uri.toString(true),
                    value: pullDiagnostics.resultId,
                },
            ],
        });
        assert(workspaceDiagnostics &&
                Array.isArray(workspaceDiagnostics.items) &&
                workspaceDiagnostics.items.some((item) =>
                    item.uri === document.uri.toString(true) &&
                    item.kind === 'unchanged' &&
                    item.resultId === pullDiagnostics.resultId),
            'Expected web workspace/diagnostic to include opened document reports');
    } finally {
        try {
            await deleteDocumentFile(advancedUri);
        } catch {
        }
        try {
            await deleteDocumentFile(actionUri);
        } catch {
        }
        try {
            await deleteDocumentFile(organizeUri);
        } catch {
        }
        try {
            await deleteDocumentFile(cleanupUri);
        } catch {
        }
    }

    console.log('[zr-web-smoke] verifyAdvancedEditorProviders:done');
}

function completionEntries(items) {
    if (!items) {
        return [];
    }

    return Array.isArray(items) ? items : items.items;
}

async function sendRawLanguageServerRequest(method, params) {
    try {
        return await vscode.commands.executeCommand('zr.__sendLanguageServerRequest', method, params);
    } catch {
        return undefined;
    }
}

async function executeCompletionItems(uri, position, triggerCharacter = undefined, itemResolveCount = 100) {
    const primary = await vscode.commands.executeCommand(
        'vscode.executeCompletionItemProvider',
        uri,
        position,
        triggerCharacter,
        itemResolveCount,
    );
    const direct = await sendRawLanguageServerRequest('textDocument/completion', {
        textDocument: { uri: uri.toString(true) },
        position: { line: position.line, character: position.character },
        context: triggerCharacter
            ? { triggerKind: 2, triggerCharacter }
            : { triggerKind: 1 },
    });
    const directEntries = completionEntries(direct);
    const primaryEntries = completionEntries(primary);
    if (directEntries.length > 0 && primaryEntries.length > 0) {
        return { items: [...directEntries, ...primaryEntries] };
    }
    if (directEntries.length > 0) {
        return direct;
    }
    if (primaryEntries.length > 0) {
        return primary;
    }
    return direct ?? primary;
}

async function executeDocumentSymbols(uri) {
    const primary = await vscode.commands.executeCommand(
        'vscode.executeDocumentSymbolProvider',
        uri,
    );
    if (Array.isArray(primary) && primary.length > 0) {
        return primary;
    }

    const direct = await sendRawLanguageServerRequest('textDocument/documentSymbol', {
        textDocument: { uri: uri.toString(true) },
    });
    return Array.isArray(direct) ? direct : primary;
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

function locationUri(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.uri ?? entry.targetUri;
}

function locationRange(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.range ?? entry.targetSelectionRange ?? entry.targetRange;
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

function structureChildren(node) {
    return Array.isArray(node?.children) ? node.children : [];
}

function findImmediateStructureNode(items, predicate) {
    return Array.isArray(items) ? items.find((item) => item && predicate(item)) : undefined;
}

function findStructureNode(items, predicate) {
    if (!Array.isArray(items)) {
        return undefined;
    }

    for (const item of items) {
        if (!item) {
            continue;
        }
        if (predicate(item)) {
            return item;
        }

        const nested = findStructureNode(structureChildren(item), predicate);
        if (nested) {
            return nested;
        }
    }

    return undefined;
}

function commandArguments(node) {
    return Array.isArray(node?.commandArguments) ? node.commandArguments : [];
}

function findImmediateGroupNode(node, label) {
    return findImmediateStructureNode(
        structureChildren(node),
        (child) => child?.nodeType === 'group' && child.label === label,
    );
}

async function verifyActiveSelection(uriSuffix, line, character, label) {
    await withRetry(
        async () => vscode.window.activeTextEditor,
        (editor) => {
            const active = editor?.selection?.active;
            return Boolean(editor) &&
                uriPath(editor.document.uri).endsWith(uriSuffix) &&
                Boolean(active) &&
                active.line === line &&
                active.character === character;
        },
        15000,
        label,
    );
}

async function verifyClassLanguageFeatures(workspaceRoot) {
    console.log('[zr-web-smoke] verifyClassLanguageFeatures:start');
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
        async () => executeCompletionItems(document.uri, bossCompletionPosition, '.'),
        (items) => completionEntries(items).length > 0,
        15000,
        'boss member completion',
    );
    const bossCompletionLabels = completionEntries(bossCompletions).map((item) => item.label?.label ?? item.label);
    assert(bossCompletionLabels.includes('hp'), 'boss. completion should include property hp');
    assert(bossCompletionLabels.includes('heal'), 'boss. completion should include method heal');
    assert(bossCompletionLabels.includes('total'), 'boss. completion should include method total');

    const scoreBoardCompletions = await withRetry(
        async () => executeCompletionItems(document.uri, scoreBoardCompletionPosition, '.'),
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
        async () => executeDocumentSymbols(document.uri),
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

    await deleteDocumentFile(smokeUri);
    console.log('[zr-web-smoke] verifyClassLanguageFeatures:done');
}

async function verifyStructureViews(workspaceRoot) {
    console.log('[zr-web-smoke] verifyStructureViews:start');
    const mainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_smoke_main.zr');
    const helperUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_helper.zr');
    const cycleUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_cycle.zr');
    const alternateProjectRootUri = vscode.Uri.joinPath(workspaceRoot, '.structure_selected_project_smoke');
    const alternateProjectUri = vscode.Uri.joinPath(alternateProjectRootUri, 'structure_selected_project_smoke.zrp');
    const alternateProjectSrcUri = vscode.Uri.joinPath(alternateProjectRootUri, 'src');
    const alternateProjectMainUri = vscode.Uri.joinPath(alternateProjectSrcUri, 'main.zr');

    await vscode.workspace.fs.writeFile(mainUri, new TextEncoder().encode(STRUCTURE_SMOKE_MAIN_SOURCE));
    await vscode.workspace.fs.writeFile(helperUri, new TextEncoder().encode(STRUCTURE_SMOKE_HELPER_SOURCE));
    await vscode.workspace.fs.writeFile(cycleUri, new TextEncoder().encode(STRUCTURE_SMOKE_CYCLE_SOURCE));
    try {
        const mainDocument = await openDocument(mainUri);
        const totalDefinitionPosition = findPositionBySubstring(mainDocument, 'pub total(): int {', 0, 4);
        const nativeImportPosition = findPositionBySubstring(
            mainDocument,
            '"zr.system"',
            0,
            1,
        );
        const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');

        assert(extension?.packageJSON?.contributes?.views?.zr?.some((view) =>
            view.id === 'zrFiles' && view.name === 'Current File Structure'),
        'Expected zrFiles view to be renamed to Current File Structure');
        assert(extension?.packageJSON?.contributes?.views?.zr?.some((view) =>
            view.id === 'zrImports' && view.name === 'Selected Project'),
        'Expected zrImports view to be renamed to Selected Project');
        assert((await vscode.commands.getCommands(true)).includes('zr.selectProject'),
            'Expected zr.selectProject to be registered in web mode');
        assert((await vscode.commands.getCommands(true)).includes('zr.runSelectedProject'),
            'Expected zr.runSelectedProject to be registered in web mode');
        assert((await vscode.commands.getCommands(true)).includes('zr.debugSelectedProject'),
            'Expected zr.debugSelectedProject to be registered in web mode');

        await vscode.commands.executeCommand('zr.structure.refresh');
        await withRetry(
            async () => vscode.window.activeTextEditor,
            (editor) => editor?.document?.uri?.toString() === mainUri.toString(),
            15000,
            'structure refresh preserves active editor',
        );
        const snapshot = await withRetry(
            async () => vscode.commands.executeCommand('zr.__inspectStructureViews'),
            (value) => Array.isArray(value?.files) && Array.isArray(value?.project),
            15000,
            'structure view snapshot',
        );

        const mainFileNode = findImmediateStructureNode(
            snapshot.files,
            (node) => node.nodeType === 'file' && node.label === 'structure_smoke_main',
        );
        assert(mainFileNode, 'Expected the Current File Structure view to render the active .zr file as the root node');

        const mainImportsGroup = findImmediateGroupNode(mainFileNode, 'Imports');
        const mainDeclarationsGroup = findImmediateGroupNode(mainFileNode, 'Declarations');
        assert(mainImportsGroup, 'Expected structure_smoke_main to include an Imports group');
        assert(mainDeclarationsGroup, 'Expected structure_smoke_main to include a Declarations group');
        assert(
            findStructureNode(
                structureChildren(mainImportsGroup),
                (node) => node.nodeType === 'import' && node.label === 'structure_helper',
            ),
            'Expected Imports group to include workspace import structure_helper',
        );
        assert(
            findStructureNode(
                structureChildren(mainImportsGroup),
                (node) => node.nodeType === 'import' && node.label === 'zr.system',
            ),
            'Expected Imports group to include builtin import zr.system',
        );
        assert(
            findStructureNode(
                structureChildren(mainDeclarationsGroup),
                (node) => node.nodeType === 'declaration' && node.label === 'StructureHero',
            ),
            'Expected Declarations group to include StructureHero',
        );
        assert(
            findStructureNode(
                structureChildren(mainDeclarationsGroup),
                (node) => node.nodeType === 'declaration' && node.label === 'total',
            ),
            'Expected Declarations group to include total',
        );
        assert(
            findStructureNode(
                structureChildren(mainDeclarationsGroup),
                (node) => node.nodeType === 'declaration' && node.label === 'structureViewSmoke',
            ),
            'Expected Declarations group to include %test symbol structureViewSmoke',
        );

        const projectActionSelectNode = findImmediateStructureNode(
            snapshot.project,
            (node) => node.nodeType === 'action' && node.label === 'Select Project',
        );
        const projectActionRunNode = findImmediateStructureNode(
            snapshot.project,
            (node) => node.nodeType === 'action' && node.label === 'Run Selected Project',
        );
        const projectActionDebugNode = findImmediateStructureNode(
            snapshot.project,
            (node) => node.nodeType === 'action' && node.label === 'Debug Selected Project',
        );
        assert(
            projectActionSelectNode?.commandId === 'zr.selectProject' &&
            projectActionRunNode?.commandId === 'zr.runSelectedProject' &&
            projectActionDebugNode?.commandId === 'zr.debugSelectedProject',
            'Expected Selected Project view actions to expose select/run/debug commands in web mode',
        );

        const selectedProjectNode = findStructureNode(
            snapshot.project,
            (node) => node.nodeType === 'project' && node.label === 'import_basic',
        );
        assert(selectedProjectNode, 'Expected the Selected Project view to render the auto-selected import_basic project');

        const projectModulesGroup = findImmediateGroupNode(selectedProjectNode, 'Project Modules');
        const nativeModulesGroup = findImmediateGroupNode(selectedProjectNode, 'Native Modules');
        const binaryModulesGroup = findImmediateGroupNode(selectedProjectNode, 'Binary Modules');
        assert(projectModulesGroup, 'Expected the selected project view to include a Project Modules group');
        assert(nativeModulesGroup, 'Expected the selected project view to include a Native Modules group');
        assert(binaryModulesGroup, 'Expected the selected project view to include a Binary Modules group');
        assert(
            findStructureNode(
                structureChildren(projectModulesGroup),
                (node) => node.nodeType === 'module' && node.label === 'main',
            ),
            'Expected Project Modules to include the selected project entry module',
        );
        const nativeProjectModuleNode = findStructureNode(
            structureChildren(nativeModulesGroup),
            (node) => node.nodeType === 'module',
        );

        const totalNode = findStructureNode(
            structureChildren(mainDeclarationsGroup),
            (node) => node.nodeType === 'declaration' && node.label === 'total',
        );
        assert(totalNode?.commandId, 'Expected declaration node total to expose a navigation command');
        await vscode.commands.executeCommand(totalNode.commandId, ...commandArguments(totalNode));
        await withRetry(
            async () => vscode.window.activeTextEditor,
            (editor) => uriPath(editor?.document?.uri).endsWith('/src/structure_smoke_main.zr') &&
                editor?.selection?.active?.line === totalDefinitionPosition.line,
            15000,
            'structure declaration navigation',
        );

        const helperImportNode = findStructureNode(
            structureChildren(mainImportsGroup),
            (node) => node.nodeType === 'import' && node.label === 'structure_helper',
        );
        assert(helperImportNode?.commandId, 'Expected import node structure_helper to expose a navigation command');
        await vscode.commands.executeCommand(helperImportNode.commandId, ...commandArguments(helperImportNode));
        await withRetry(
            async () => vscode.window.activeTextEditor,
            (editor) => uriPath(editor?.document?.uri).endsWith('/src/structure_helper.zr'),
            15000,
            'workspace import definition navigation',
        );

        await vscode.window.showTextDocument(mainDocument, { preview: false });
        const nativeImportNode = findStructureNode(
            structureChildren(mainImportsGroup),
            (node) => node.nodeType === 'import' && node.label === 'zr.system',
        );
        assert(nativeImportNode?.commandId, 'Expected native import node zr.system to expose a navigation command');
        await vscode.commands.executeCommand(nativeImportNode.commandId, ...commandArguments(nativeImportNode));
        await withRetry(
            async () => vscode.window.activeTextEditor,
            (editor) => editor?.document?.uri?.scheme === 'zr-decompiled' &&
                uriPath(editor.document.uri).endsWith('/zr.system.zr'),
            15000,
            'native import opens zr-decompiled document',
        );

        const nativeDocument = await withRetry(
            async () => vscode.workspace.openTextDocument(vscode.Uri.parse('zr-decompiled:/zr.system.zr')),
            (document) => typeof document?.getText === 'function' &&
                document.getText().includes('%extern("zr.system")'),
            15000,
            'native declaration virtual document load',
        );
        assert(nativeDocument.getText().includes('%extern("zr.system")'),
            'Expected zr-decompiled virtual documents to be backed by native declaration rendering');

        const nativeDefinitions = await withRetry(
            async () => vscode.commands.executeCommand(
                'vscode.executeDefinitionProvider',
                mainDocument.uri,
                nativeImportPosition,
            ),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'native import definition provider',
        );
        assert(
            nativeDefinitions.some((item) =>
                locationUri(item)?.scheme === 'zr-decompiled' &&
                uriPath(locationUri(item)).endsWith('/zr.system.zr')),
            'Expected native import goto definition to resolve into zr-decompiled virtual documents',
        );

        if (nativeProjectModuleNode) {
            assert(nativeProjectModuleNode.commandId, 'Expected selected-project native module nodes to expose navigation commands');
            await vscode.commands.executeCommand(nativeProjectModuleNode.commandId, ...commandArguments(nativeProjectModuleNode));
            await withRetry(
                async () => vscode.window.activeTextEditor,
                (editor) => editor?.document?.uri?.scheme === 'zr-decompiled',
                15000,
                'selected project native module navigation',
            );
        }

        await vscode.workspace.fs.createDirectory(alternateProjectSrcUri);
        await vscode.workspace.fs.writeFile(
            alternateProjectUri,
            new TextEncoder().encode(JSON.stringify({
                name: 'structure_selected_project_smoke',
                source: 'src',
                binary: 'bin',
                entry: 'main',
            }, null, 2) + '\n'),
        );
        await vscode.workspace.fs.writeFile(
            alternateProjectMainUri,
            new TextEncoder().encode('return 7;\n'),
        );

        await withPatchedWindowMethod('showQuickPick', async (items) => items.find((item) => item.label === 'structure_selected_project_smoke'), async () => {
            await vscode.commands.executeCommand('zr.selectProject');
        });
        await withRetry(
            async () => vscode.commands.executeCommand('zr.__inspectStructureViews'),
            (value) => Boolean(findStructureNode(
                value?.project,
                (node) => node.nodeType === 'project' && node.label === 'structure_selected_project_smoke',
            )),
            15000,
            'selected project updates after zr.selectProject',
        );

        await vscode.commands.executeCommand('zr.structure.refresh');
        await withRetry(
            async () => vscode.commands.executeCommand('zr.__inspectStructureViews'),
            (value) => Boolean(findStructureNode(
                value?.project,
                (node) => node.nodeType === 'project' && node.label === 'structure_selected_project_smoke',
            )),
            15000,
            'selected project persists across refresh',
        );

        await withPatchedWindowMethod('showQuickPick', async (items) => items.find((item) => item.label === 'import_basic'), async () => {
            await vscode.commands.executeCommand('zr.selectProject');
        });
    } finally {
        await vscode.workspace.fs.delete(alternateProjectRootUri, { recursive: true, useTrash: false });
        await deleteDocumentFile(mainUri);
        await deleteDocumentFile(helperUri);
        await deleteDocumentFile(cycleUri);
        await vscode.commands.executeCommand('zr.structure.refresh');
    }
    console.log('[zr-web-smoke] verifyStructureViews:done');
}

async function run() {
    const workspaceFolder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
    const focus = typeof process !== 'undefined' && process?.env?.ZR_TEST_SMOKE_FOCUS
        ? process.env.ZR_TEST_SMOKE_FOCUS
        : 'all';
    assert(workspaceFolder, 'Expected a workspace folder for smoke test');
    const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');
    assert(extension, 'Expected Zr extension to be present in extension host');

    console.log(`[zr-web-smoke] run:start focus=${focus}`);
    await extension.activate();
    assert(extension.isActive, 'Expected Zr extension to remain active after activation');
    console.log('[zr-web-smoke] extension:activated');

    if (focus === 'all' || focus === 'lsp') {
        await verifyLanguageFeatures(workspaceFolder.uri);
        await verifyAdvancedEditorProviders(workspaceFolder.uri);
        await verifyClassLanguageFeatures(workspaceFolder.uri);
        await verifyStructureViews(workspaceFolder.uri);
        await verifyDiagnostics(workspaceFolder.uri);
    } else if (focus === 'structure') {
        await verifyStructureViews(workspaceFolder.uri);
    }
    console.log('[zr-web-smoke] run:done');
}

function uriPath(uri) {
    return typeof uri?.path === 'string' ? uri.path : uri?.toString() ?? '';
}

module.exports = {
    run,
};
