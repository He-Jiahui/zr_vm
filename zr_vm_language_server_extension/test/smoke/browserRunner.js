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
        async () => vscode.commands.executeCommand(
            'vscode.executeCompletionItemProvider',
            mainDocument.uri,
            new vscode.Position(0, 0),
        ),
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
        async () => vscode.commands.executeCommand(
            'vscode.executeDocumentSymbolProvider',
            mainDocument.uri,
        ),
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

    await deleteDocumentFile(smokeUri);
    console.log('[zr-web-smoke] verifyClassLanguageFeatures:done');
}

async function verifyStructureViews(workspaceRoot) {
    console.log('[zr-web-smoke] verifyStructureViews:start');
    const mainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_smoke_main.zr');
    const helperUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_helper.zr');
    const cycleUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_cycle.zr');

    await vscode.workspace.fs.writeFile(mainUri, new TextEncoder().encode(STRUCTURE_SMOKE_MAIN_SOURCE));
    await vscode.workspace.fs.writeFile(helperUri, new TextEncoder().encode(STRUCTURE_SMOKE_HELPER_SOURCE));
    await vscode.workspace.fs.writeFile(cycleUri, new TextEncoder().encode(STRUCTURE_SMOKE_CYCLE_SOURCE));

    try {
        const mainDocument = await openDocument(mainUri);
        const totalDefinitionPosition = findPositionBySubstring(mainDocument, 'pub total(): int {', 0, 4);
        const helperImportPosition = findPositionBySubstring(
            mainDocument,
            'var helper = %import("structure_helper");',
            0,
            0,
        );
        const helperDocument = await vscode.workspace.openTextDocument(helperUri);
        const helperValuePosition = findPositionBySubstring(helperDocument, 'pub var value = () => {', 0, 8);

        await vscode.commands.executeCommand('zr.structure.refresh');
        const snapshot = await withRetry(
            async () => vscode.commands.executeCommand('zr.__inspectStructureViews'),
            (value) => Array.isArray(value?.files) && Array.isArray(value?.imports),
            15000,
            'structure view snapshot',
        );

        const projectNode = findStructureNode(
            snapshot.files,
            (node) => node.nodeType === 'project' && node.label === 'import_basic',
        );
        assert(projectNode, 'Expected structure views to group files under the owning .zrp project');

        const mainFileNode = findStructureNode(
            snapshot.files,
            (node) => node.nodeType === 'file' && node.moduleName === 'structure_smoke_main',
        );
        assert(mainFileNode, 'Expected ZR Files view to include structure_smoke_main');

        const mainImportsGroup = findStructureNode(
            structureChildren(mainFileNode),
            (node) => node.nodeType === 'imports',
        );
        const mainDeclarationsGroup = findImmediateStructureNode(
            structureChildren(mainFileNode),
            (node) => node.nodeType === 'declarations',
        );
        assert(mainImportsGroup, 'Expected structure_smoke_main to include an Imports group');
        assert(mainDeclarationsGroup, 'Expected structure_smoke_main to include a Declarations group');
        assert(
            findStructureNode(
                structureChildren(mainImportsGroup),
                (node) => node.nodeType === 'import' && node.moduleName === 'structure_helper',
            ),
            'Expected Imports group to include workspace import structure_helper',
        );
        assert(
            findStructureNode(
                structureChildren(mainImportsGroup),
                (node) => node.nodeType === 'import' && node.moduleName === 'zr.system' && node.description === 'builtin',
            ),
            'Expected Imports group to include builtin import zr.system',
        );
        const builtinSystemImportNode = findStructureNode(
            structureChildren(mainImportsGroup),
            (node) => node.nodeType === 'import' && node.moduleName === 'zr.system',
        );
        assert(
            findStructureNode(
                structureChildren(builtinSystemImportNode),
                (node) => node.nodeType === 'module' && node.moduleName === 'zr.system.fs',
            ),
            'Expected builtin import zr.system to expose builtin child modules',
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

        const mainImportRoot = findStructureNode(
            snapshot.imports,
            (node) => node.nodeType === 'module' && node.moduleName === 'structure_smoke_main',
        );
        assert(mainImportRoot, 'Expected ZR Imports view to include structure_smoke_main root module');

        const helperModuleNode = findStructureNode(
            structureChildren(mainImportRoot),
            (node) => node.nodeType === 'module' && node.moduleName === 'structure_helper',
        );
        assert(helperModuleNode, 'Expected import tree to include structure_helper module');
        const builtinSystemModuleNode = findStructureNode(
            structureChildren(mainImportRoot),
            (node) => node.nodeType === 'module' && node.moduleName === 'zr.system',
        );
        assert(
            findStructureNode(
                structureChildren(builtinSystemModuleNode),
                (node) => node.nodeType === 'module' && node.moduleName === 'zr.system.vm',
            ),
            'Expected import tree to expand builtin modules such as zr.system',
        );

        const helperDeclarationsGroup = findImmediateStructureNode(
            structureChildren(helperModuleNode),
            (node) => node.nodeType === 'declarations',
        );
        assert(helperDeclarationsGroup, 'Expected structure_helper module to expose a Declarations group');
        assert(
            findStructureNode(
                structureChildren(helperDeclarationsGroup),
                (node) => node.nodeType === 'declaration' && node.label === 'value',
            ),
            'Expected helper module Declarations group to include value',
        );

        const cycleModuleNode = findStructureNode(
            structureChildren(helperModuleNode),
            (node) => node.nodeType === 'module' && node.moduleName === 'structure_cycle',
        );
        assert(cycleModuleNode, 'Expected import tree to include structure_cycle under structure_helper');
        assert(
            findStructureNode(
                structureChildren(cycleModuleNode),
                (node) => node.nodeType === 'module' &&
                    node.moduleName === 'structure_helper' &&
                    node.isRecursiveReference === true,
            ),
            'Expected cycle back-edge to structure_helper to be marked as a recursive reference',
        );

        const totalNode = findStructureNode(
            structureChildren(mainDeclarationsGroup),
            (node) => node.nodeType === 'declaration' && node.label === 'total',
        );
        assert(totalNode?.commandId, 'Expected declaration node total to expose a navigation command');
        await vscode.commands.executeCommand(totalNode.commandId, ...commandArguments(totalNode));
        await verifyActiveSelection(
            '/src/structure_smoke_main.zr',
            totalDefinitionPosition.line,
            totalDefinitionPosition.character,
            'structure declaration navigation',
        );

        const helperImportNode = findStructureNode(
            structureChildren(mainImportsGroup),
            (node) => node.nodeType === 'import' && node.moduleName === 'structure_helper',
        );
        assert(helperImportNode?.commandId, 'Expected import node structure_helper to expose a navigation command');
        await vscode.commands.executeCommand(helperImportNode.commandId, ...commandArguments(helperImportNode));
        await verifyActiveSelection(
            '/src/structure_smoke_main.zr',
            helperImportPosition.line,
            helperImportPosition.character,
            'structure import navigation',
        );

        assert(helperModuleNode?.commandId, 'Expected helper module node to expose a navigation command');
        await vscode.commands.executeCommand(helperModuleNode.commandId, ...commandArguments(helperModuleNode));
        await verifyActiveSelection(
            '/src/structure_helper.zr',
            helperValuePosition.line,
            helperValuePosition.character,
            'structure module navigation',
        );
    } finally {
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
