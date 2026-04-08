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

const CLASSES_FULL_DEBUG_SOURCE = [
    CLASSES_FULL_SMOKE_SOURCE,
    'var runtimeBoss = new BossHero(30);',
    'runtimeBoss.hp = runtimeBoss.hp + 7;',
    'ScoreBoard.bonus = runtimeBoss.heal(5);',
    'return runtimeBoss.total() + ScoreBoard.bonus;',
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

async function withRetry(action, predicate, timeoutMs, label) {
    const deadline = Date.now() + timeoutMs;
    let lastError;
    let lastValue;

    while (Date.now() < deadline) {
        try {
            const value = await action();
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

    await vscode.workspace.fs.delete(uri, { useTrash: false });
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

    await deleteDocumentFile(diagnosticUri);
}

async function verifyLanguageFeatures(workspaceRoot) {
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

    await deleteDocumentFile(smokeUri);
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

function sanitizeNetworkLoopbackDebugSource(text) {
    return text.replace(
        'var r = new lib.Record(3, 4);\nvar sum = r();',
        'var r = 0;\nvar sum = r;',
    );
}

async function verifyProjectInferenceAndSemanticTokens(workspaceRoot) {
    const projectMainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr');
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'project_inference_smoke.zr');
    const featureTimeoutMs = 30000;
    const projectDocument = await openDocument(projectMainUri);
    const aliasUsagePosition = findPositionBySubstring(projectDocument, 'greetModule.greet()', 0, 1);
    let lastProjectImportHoverText = '';

    const hover = await withRetry(
        async () => vscode.commands.executeCommand(
            'vscode.executeHoverProvider',
            projectDocument.uri,
            aliasUsagePosition,
        ),
        (items) => {
            if (!Array.isArray(items) || items.length === 0) {
                return false;
            }

            lastProjectImportHoverText = hoverText(items);
            console.log('[zr-smoke] project import hover:', JSON.stringify(lastProjectImportHoverText));
            return lastProjectImportHoverText.includes('module <greet>');
        },
        featureTimeoutMs,
        'project import hover provider',
    );
    if (!hoverText(hover).includes('module <greet>')) {
        throw new Error(`Import alias hover mismatch: ${lastProjectImportHoverText}`);
    }
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

    await deleteDocumentFile(smokeUri);
}

async function verifyStructureViews(workspaceRoot) {
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
        const helperDocument = await vscode.workspace.openTextDocument(helperUri);
        const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');

        assert(extension?.packageJSON?.contributes?.views?.zr?.some((view) =>
            view.id === 'zrFiles' && view.name === 'Current File Structure'),
        'Expected zrFiles view to be renamed to Current File Structure');
        assert(extension?.packageJSON?.contributes?.views?.zr?.some((view) =>
            view.id === 'zrImports' && view.name === 'Selected Project'),
        'Expected zrImports view to be renamed to Selected Project');

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
            'Expected Selected Project view title actions to expose select/run/debug commands',
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

        const alternateSnapshot = await withRetry(
            async () => vscode.commands.executeCommand('zr.__inspectStructureViews'),
            (value) => Boolean(findStructureNode(
                value?.project,
                (node) => node.nodeType === 'project' && node.label === 'structure_selected_project_smoke',
            )),
            15000,
            'selected project updates after zr.selectProject',
        );
        assert(
            findStructureNode(
                alternateSnapshot.project,
                (node) => node.nodeType === 'project' && node.label === 'structure_selected_project_smoke',
            ),
            'Expected the Selected Project view to switch to the explicitly chosen .zrp project',
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

    await deleteDocumentFile(smokeUri);
}

async function waitForDebugEvent(eventName, timeoutMs, expectedSessionId, label) {
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
            reject(new Error(`Timed out waiting for debug event ${eventName}${label ? ` (${label})` : ''}`));
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

function debugVariableByName(variables, name) {
    return Array.isArray(variables) ? variables.find((item) => item?.name === name) : undefined;
}

async function readDebugVariables(session, variablesReference) {
    const variablesResult = await session.customRequest('variables', {
        variablesReference,
    });
    return Array.isArray(variablesResult?.variables) ? variablesResult.variables : [];
}

async function readDebugScopeVariables(session, frameId, scopeName) {
    const scopesResult = await session.customRequest('scopes', { frameId });
    const scopes = Array.isArray(scopesResult?.scopes) ? scopesResult.scopes : [];
    const scope = scopes.find((item) => item?.name === scopeName);

    assert(scope && typeof scope.variablesReference === 'number' && scope.variablesReference > 0,
        `Expected ${scopeName} scope to expose variables`);
    return readDebugVariables(session, scope.variablesReference);
}

async function verifyRichDebugInspection(session, expectedSourcePath) {
    const stackTrace = await session.customRequest('stackTrace', { threadId: 1 });
    const stackFrames = Array.isArray(stackTrace?.stackFrames) ? stackTrace.stackFrames : [];
    const topFrame = stackFrames[0];

    assert(topFrame, 'Expected rich inspection stack to expose a top frame');
    assert(normalizePath(topFrame?.source?.path ?? '') === normalizePath(expectedSourcePath),
        'Expected rich inspection frame source path to match the hero module');
    assert(String(topFrame?.name ?? '').includes('[method]'),
        'Expected rich inspection frame name to expose method call kind');
    assert(String(topFrame?.name ?? '').includes('this.heal'),
        'Expected rich inspection frame name to expose receiver and function name');
    assert(String(topFrame?.name ?? '').includes('@'),
        'Expected rich inspection frame name to include module information');
    assert(String(topFrame?.name ?? '').includes('depth=0'),
        'Expected rich inspection frame name to include frame depth');

    const argumentsVariables = await readDebugScopeVariables(session, topFrame.id, 'Arguments');
    const globalsVariables = await readDebugScopeVariables(session, topFrame.id, 'Globals');
    const prototypeVariables = await readDebugScopeVariables(session, topFrame.id, 'Prototype');

    assert(debugVariableByName(argumentsVariables, 'amount')?.value === '5',
        'Expected method arguments scope to expose amount=5');
    assert(debugVariableByName(globalsVariables, 'zrState')?.variablesReference > 0,
        'Expected globals scope to expose zrState');
    assert(debugVariableByName(globalsVariables, 'loadedModules')?.variablesReference > 0,
        'Expected globals scope to expose loadedModules');
    assert(debugVariableByName(globalsVariables, 'zr')?.variablesReference > 0,
        'Expected globals scope to expose zr runtime helpers');
    assert(debugVariableByName(prototypeVariables, 'memberDescriptorCount'),
        'Expected prototype scope to expose memberDescriptorCount');
    assert(debugVariableByName(prototypeVariables, 'managedFieldCount'),
        'Expected prototype scope to expose managedFieldCount');
    assert(debugVariableByName(prototypeVariables, 'indexContract'),
        'Expected prototype scope to expose indexContract');

    const evaluateValue = await session.customRequest('evaluate', {
        expression: 'amount + 1',
        frameId: topFrame.id,
        context: 'watch',
    });
    assert(String(evaluateValue?.result ?? '') === '6',
        'Expected paused readonly evaluate to compute amount + 1');

    const evaluateThis = await session.customRequest('evaluate', {
        expression: 'this',
        frameId: topFrame.id,
        context: 'watch',
    });
    assert(typeof evaluateThis?.variablesReference === 'number' && evaluateThis.variablesReference > 0,
        'Expected paused readonly evaluate(this) to expose an expandable object');

    const objectVariables = await readDebugVariables(session, evaluateThis.variablesReference);
    const prototypeEntry = debugVariableByName(objectVariables, '$prototype');
    const stateEntry = debugVariableByName(globalsVariables, 'zrState');
    const loadedModulesEntry = debugVariableByName(globalsVariables, 'loadedModules');
    assert(typeof prototypeEntry?.variablesReference === 'number' && prototypeEntry.variablesReference > 0,
        'Expected instance expansion to expose $prototype');
    assert(!debugVariableByName(objectVariables, '$metadata'),
        'Expected ordinary instances to hide $metadata');
    assert(!debugVariableByName(objectVariables, '$methods'),
        'Expected ordinary instances to hide $methods');

    const typeVariables = await readDebugVariables(session, prototypeEntry.variablesReference);
    const zrStateVariables = await readDebugVariables(session, stateEntry.variablesReference);
    const loadedModuleVariables = await readDebugVariables(session, loadedModulesEntry.variablesReference);
    const metadataEntry = debugVariableByName(typeVariables, '$metadata');
    const membersEntry = debugVariableByName(typeVariables, '$members');
    const methodsEntry = debugVariableByName(typeVariables, '$methods');

    assert(typeof metadataEntry?.variablesReference === 'number' && metadataEntry.variablesReference > 0,
        'Expected type-object expansion to expose $metadata');
    assert(typeof membersEntry?.variablesReference === 'number' && membersEntry.variablesReference > 0,
        'Expected type-object $members expansion to remain expandable');
    assert(typeof methodsEntry?.variablesReference === 'number' && methodsEntry.variablesReference > 0,
        'Expected type-object expansion to expose $methods');
    assert(zrStateVariables.length > 0,
        'Expected zrState expansion to expose runtime state metadata');
    assert(loadedModuleVariables.length > 0,
        'Expected loadedModules expansion to expose at least one loaded module');

    const metadataVariables = await readDebugVariables(session, metadataEntry.variablesReference);
    assert(debugVariableByName(metadataVariables, 'name') || debugVariableByName(metadataVariables, 'memberDescriptorCount'),
        'Expected $metadata expansion to expose prototype metadata');
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

async function withPatchedObjectMethod(target, methodName, replacement, action) {
    const original = target[methodName];
    let restored = false;

    assert(typeof original === 'function', `Expected target.${methodName} to be patchable`);
    target[methodName] = replacement;

    try {
        return await action();
    } finally {
        if (!restored) {
            target[methodName] = original;
            restored = true;
        }
    }
}

async function verifyProjectActions(workspaceRoot, bundledCliPath, debugProjectUri) {
    const zrConfig = vscode.workspace.getConfiguration('zr');
    const legacyDebugConfig = vscode.workspace.getConfiguration('zr.debug');
    const previousExecutablePath = zrConfig.get('executablePath');
    const previousLegacyCliPath = legacyDebugConfig.get('cli.path');
    await openDocument(debugProjectUri);
    const debugSourceUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr');
    const debugDocument = await openDocument(debugSourceUri);
    const debugBreakpointPosition = findPositionBySubstring(debugDocument, 'return greetModule.greet()');
    const debugBreakpoint = new vscode.SourceBreakpoint(
        new vscode.Location(debugDocument.uri, debugBreakpointPosition),
    );
    let capturedTask;
    let debugSession;

    await withRetry(
        async () => vscode.commands.executeCommand('zr.__inspectProjectActions'),
        (value) => value?.isVisible === true && String(value?.projectPath ?? '').endsWith('import_basic.zrp'),
        15000,
        'project actions visible on zrp editor',
    );

    await openDocument(vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr'));
    const zrEditorState = await withRetry(
        async () => vscode.commands.executeCommand('zr.__inspectProjectActions'),
        (value) => value?.isVisible === true && String(value?.projectPath ?? '').endsWith('import_basic.zrp'),
        15000,
        'project actions remain visible on zr editor',
    );
    assert(zrEditorState?.isVisible === true,
        'Expected selected-project actions to remain visible when the active editor is a .zr file');
    assert(String(zrEditorState?.projectPath ?? '').endsWith('import_basic.zrp'),
        'Expected selected-project actions to keep targeting the selected .zrp project');

    try {
        await zrConfig.update('executablePath', bundledCliPath, vscode.ConfigurationTarget.Workspace);
        await legacyDebugConfig.update('cli.path', '', vscode.ConfigurationTarget.Workspace);

        const configuredState = await withRetry(
            async () => vscode.commands.executeCommand('zr.__inspectProjectActions'),
            (value) => value?.cliPath === bundledCliPath,
            15000,
            'project actions configured cli path',
        );
        assert(configuredState?.cliPath === bundledCliPath,
            'Expected project actions to resolve the configured zr.executablePath');

        await withPatchedObjectMethod(vscode.tasks, 'executeTask', async (task) => {
            capturedTask = task;
            return {
                dispose() {},
            };
        }, async () => {
            await vscode.commands.executeCommand('zr.runCurrentProject');
        });

        assert(capturedTask, 'Expected zr.runCurrentProject to dispatch a VS Code task');
        assert(capturedTask.execution?.process === bundledCliPath,
            'Expected zr.runCurrentProject to use the configured zr_vm_cli executable');
        assert(Array.isArray(capturedTask.execution?.args) && capturedTask.execution.args[0] === debugProjectUri.fsPath,
            'Expected zr.runCurrentProject to launch the selected .zrp project');

        capturedTask = undefined;
        await withPatchedObjectMethod(vscode.tasks, 'executeTask', async (task) => {
            capturedTask = task;
            return {
                dispose() {},
            };
        }, async () => {
            await vscode.commands.executeCommand('zr.runSelectedProject');
        });

        assert(capturedTask, 'Expected zr.runSelectedProject to dispatch a VS Code task');
        assert(capturedTask.execution?.process === bundledCliPath,
            'Expected zr.runSelectedProject to use the configured zr_vm_cli executable');
        assert(Array.isArray(capturedTask.execution?.args) && capturedTask.execution.args[0] === debugProjectUri.fsPath,
            'Expected zr.runSelectedProject to launch the selected .zrp project');

        vscode.debug.addBreakpoints([debugBreakpoint]);
        const breakpointResolved = waitForDebugEvent('breakpoint', 15000, undefined, 'project-actions:debug-selected-project:breakpoint-resolved');
        const entryStopped = waitForDebugEvent('stopped', 15000, undefined, 'project-actions:debug-selected-project:entry-stop');
        await vscode.commands.executeCommand('zr.debugSelectedProject');
        debugSession = await withRetry(
            async () => vscode.debug.activeDebugSession,
            (value) => value && value.type === 'zr',
            15000,
            'project actions debug session start',
        );
        const resolvedBreakpointEvent = await breakpointResolved;
        const entryStoppedEvent = await entryStopped;
        assert(resolvedBreakpointEvent?.body?.breakpoint?.verified !== false,
            'Expected zr.debugSelectedProject to resolve breakpoints against the selected .zrp project');
        assert(entryStoppedEvent?.body?.reason === 'entry',
            'Expected zr.debugSelectedProject to stop on entry for the selected .zrp project');
        {
            const stackTrace = await debugSession.customRequest('stackTrace', { threadId: 1 });
            const stackFrames = Array.isArray(stackTrace?.stackFrames) ? stackTrace.stackFrames : [];
            assert(stackFrames.length > 0, 'Expected zr.debugSelectedProject to expose a stack frame at the entry stop');
            assert(normalizePath(stackFrames[0]?.source?.path ?? '') === normalizePath(debugDocument.uri.fsPath),
                'Expected zr.debugSelectedProject stack trace source mapping to use the selected .zrp project');
        }
    } finally {
        vscode.debug.removeBreakpoints([debugBreakpoint]);
        if (debugSession) {
            await vscode.debug.stopDebugging(debugSession);
            await withRetry(
                async () => vscode.debug.activeDebugSession,
                (value) => !value || value.id !== debugSession.id,
                15000,
                'project actions stopDebugging',
            );
        }
        await zrConfig.update('executablePath', previousExecutablePath, vscode.ConfigurationTarget.Workspace);
        await legacyDebugConfig.update('cli.path', previousLegacyCliPath, vscode.ConfigurationTarget.Workspace);
    }
}

async function verifyProjectActionIntegration(workspaceRoot) {
    const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');
    const bundledFolder = `${process.platform}-${process.arch}`;
    const bundledCliPath = process.platform === 'win32'
        ? path.join(extension.extensionPath, 'server', 'native', bundledFolder, 'zr_vm_cli.exe')
        : path.join(extension.extensionPath, 'server', 'native', bundledFolder, 'zr_vm_cli');
    const debugProjectUri = vscode.Uri.joinPath(workspaceRoot, 'import_basic.zrp');

    assert(extension, 'Expected Zr extension to be present before project action verification');
    assert((await vscode.commands.getCommands(true)).includes('zr.runCurrentProject'),
        'Expected zr.runCurrentProject to be registered');
    assert((await vscode.commands.getCommands(true)).includes('zr.runSelectedProject'),
        'Expected zr.runSelectedProject to be registered');
    assert((await vscode.commands.getCommands(true)).includes('zr.selectProject'),
        'Expected zr.selectProject to be registered');
    assert(fs.existsSync(bundledCliPath),
        `Expected bundled zr_vm_cli at ${bundledCliPath}`);

    await verifyProjectActions(workspaceRoot, bundledCliPath, debugProjectUri);
}

async function verifyLaunchDebugSession({
    workspaceRoot,
    debugDocument,
    startSession,
    expectedSessionStartLabel,
    expectedFirstStopReason,
    continueFromEntryToBreakpoint = false,
    inspectBreakpointState = true,
    disconnectAfterStop = false,
    expectBreakpointResolved = true,
    onStopped,
}) {
    let session;
    let started;

    const breakpointResolved = expectBreakpointResolved
        ? waitForDebugEvent('breakpoint', 15000, undefined, `${expectedSessionStartLabel}:breakpoint`)
        : undefined;
    const launchStopped = waitForDebugEvent('stopped', 15000, undefined, `${expectedSessionStartLabel}:initial-stop`);
    const launchTerminated = disconnectAfterStop
        ? undefined
        : waitForDebugEvent('terminated', 30000, undefined, `${expectedSessionStartLabel}:terminated`);

    try {
        started = await startSession();
        if (typeof started === 'boolean') {
            assert(started, 'Expected ZR launch debug session to start');
        }
        session = await withRetry(
            async () => vscode.debug.activeDebugSession,
            (value) => value && value.type === 'zr',
            15000,
            expectedSessionStartLabel,
        );
        if (breakpointResolved) {
            const breakpointEvent = await breakpointResolved;
            assert(breakpointEvent?.body?.breakpoint?.verified !== false,
                'Expected launch breakpoint to resolve before continuing');
        }
        let stoppedEvent = await launchStopped;
        assert(stoppedEvent?.body?.reason === expectedFirstStopReason,
            `Expected launch session to stop first because of ${expectedFirstStopReason}`);
        if (continueFromEntryToBreakpoint) {
            const breakpointStopped = waitForDebugEvent(
                'stopped',
                15000,
                undefined,
                `${expectedSessionStartLabel}:post-entry-breakpoint`,
            );
            await session.customRequest('continue', { threadId: 1 });
            stoppedEvent = await breakpointStopped;
            assert(stoppedEvent?.body?.reason === 'breakpoint',
                'Expected launch session to stop on the source breakpoint after the entry stop');
        }
        if (inspectBreakpointState) {
            assert(stoppedEvent?.body?.reason === 'breakpoint',
                'Expected launch session inspection to run from a breakpoint stop');
            await verifyDebugStateInspection(session, debugDocument.uri.fsPath);
        }
        if (onStopped) {
            await onStopped(session, stoppedEvent);
        }
        if (!disconnectAfterStop) {
            await session.customRequest('continue', { threadId: 1 });
            await launchTerminated;
        }
    } finally {
        if (session) {
            await vscode.debug.stopDebugging(session);
            await withRetry(
                async () => vscode.debug.activeDebugSession,
                (value) => !value || value.id !== session.id,
                15000,
                `${expectedSessionStartLabel} stopDebugging`,
            );
        }
    }
}

async function verifyAttachDebugSession({
    expectedSessionStartLabel,
    startSession,
    disconnectAfterStop = false,
}) {
    let session;
    let started;

    const attachStopped = waitForDebugEvent('stopped', 15000, undefined, `${expectedSessionStartLabel}:initial-stop`);
    const attachTerminated = disconnectAfterStop
        ? undefined
        : waitForDebugEvent('terminated', 30000, undefined, `${expectedSessionStartLabel}:terminated`);

    try {
        started = await startSession();
        if (typeof started === 'boolean') {
            assert(started, 'Expected ZR attach debug session to start');
        }
        session = await withRetry(
            async () => vscode.debug.activeDebugSession,
            (value) => value && value.type === 'zr',
            15000,
            expectedSessionStartLabel,
        );
        const attachStoppedEvent = await attachStopped;
        assert(attachStoppedEvent?.body?.reason === 'entry',
            'Expected attach session to observe the runtime entry stop');
        if (!disconnectAfterStop) {
            await session.customRequest('continue', { threadId: 1 });
            await attachTerminated;
        }
    } finally {
        if (session) {
            await vscode.debug.stopDebugging(session);
            await withRetry(
                async () => vscode.debug.activeDebugSession,
                (value) => !value || value.id !== session.id,
                15000,
                `${expectedSessionStartLabel} stopDebugging`,
            );
        }
    }
}

async function verifyInvalidAttachEndpointRejected(workspaceRoot) {
    let errorMessage = '';
    const invalidEndpoint = '192.168.10.8:9000';

    await withPatchedWindowMethod('showErrorMessage', async (message) => {
        errorMessage = String(message ?? '');
        return undefined;
    }, async () => {
        const started = await vscode.debug.startDebugging(workspaceRoot, {
            type: 'zr',
            name: 'ZR Smoke Invalid Attach',
            request: 'attach',
            endpoint: invalidEndpoint,
        });
        assert(!started, 'Expected non-loopback attach configuration to be rejected');
    });

    assert(errorMessage.includes('loopback endpoints'),
        'Expected invalid attach configuration to surface a loopback validation error');
}

async function verifyDebugIntegration(workspaceRoot) {
    const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');
    const contributedDebuggers = extension?.packageJSON?.contributes?.debuggers ?? [];
    const bundledFolder = `${process.platform}-${process.arch}`;
    const bundledCliPath = process.platform === 'win32'
        ? path.join(extension.extensionPath, 'server', 'native', bundledFolder, 'zr_vm_cli.exe')
        : path.join(extension.extensionPath, 'server', 'native', bundledFolder, 'zr_vm_cli');
    const debugMainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr');
    const debugProjectUri = vscode.Uri.joinPath(workspaceRoot, 'import_basic.zrp');
    const networkProjectRootUri = vscode.Uri.file(path.resolve(workspaceRoot.fsPath, '..', 'network_loopback'));
    const richProjectRootUri = vscode.Uri.joinPath(workspaceRoot, '.debug_classes_full_smoke');
    const richProjectSrcUri = vscode.Uri.joinPath(richProjectRootUri, 'src');
    const richProjectUri = vscode.Uri.joinPath(richProjectRootUri, 'debug_classes_full_smoke.zrp');
    const richSourceUri = vscode.Uri.joinPath(richProjectSrcUri, 'main.zr');
    const sourceProjectRootUri = vscode.Uri.joinPath(workspaceRoot, '.debug_source_request_smoke');
    const sourceProjectSrcUri = vscode.Uri.joinPath(sourceProjectRootUri, 'src');
    const sourceProjectUri = vscode.Uri.joinPath(sourceProjectRootUri, 'debug_source_request_smoke.zrp');
    const sourceProjectMainUri = vscode.Uri.joinPath(sourceProjectSrcUri, 'main.zr');
    const sourceProjectLibUri = vscode.Uri.joinPath(sourceProjectSrcUri, 'lib.zr');
    const debugDocument = await openDocument(debugMainUri);
    const debugBreakpointPosition = findPositionBySubstring(debugDocument, 'return greetModule.greet()');
    const debugBreakpoint = new vscode.SourceBreakpoint(
        new vscode.Location(debugDocument.uri, debugBreakpointPosition),
    );
    let attachTarget;
    let commandAttachTarget;

    assert(extension, 'Expected Zr extension to be present before debug verification');
    assert((await vscode.commands.getCommands(true)).includes('zr.debugCurrentProject'),
        'Expected zr.debugCurrentProject to be registered');
    assert((await vscode.commands.getCommands(true)).includes('zr.debugSelectedProject'),
        'Expected zr.debugSelectedProject to be registered');
    assert((await vscode.commands.getCommands(true)).includes('zr.attachDebugEndpoint'),
        'Expected zr.attachDebugEndpoint to be registered');
    assert((await vscode.commands.getCommands(true)).includes('zr.runCurrentProject'),
        'Expected zr.runCurrentProject to be registered');
    assert(Array.isArray(contributedDebuggers) && contributedDebuggers.some((item) => item.type === 'zr'),
        'Expected ZR debugger contributions to be present');
    assert(fs.existsSync(bundledCliPath),
        `Expected bundled zr_vm_cli at ${bundledCliPath}`);

    await verifyProjectActionIntegration(workspaceRoot);

    await vscode.workspace.fs.createDirectory(richProjectSrcUri);
    await vscode.workspace.fs.createDirectory(sourceProjectSrcUri);
    await vscode.workspace.fs.writeFile(
        richProjectUri,
        new TextEncoder().encode(JSON.stringify({
            name: 'debug_classes_full_smoke',
            source: 'src',
            binary: 'bin',
            entry: 'main',
        }, null, 2) + '\n'),
    );
    await vscode.workspace.fs.writeFile(
        richSourceUri,
        new TextEncoder().encode(CLASSES_FULL_DEBUG_SOURCE),
    );
    await vscode.workspace.fs.writeFile(
        sourceProjectUri,
        new TextEncoder().encode(JSON.stringify({
            name: 'debug_source_request_smoke',
            source: 'src',
            binary: 'bin',
            entry: 'main',
        }, null, 2) + '\n'),
    );
    await vscode.workspace.fs.writeFile(
        sourceProjectMainUri,
        new TextEncoder().encode(sanitizeNetworkLoopbackDebugSource(
            fs.readFileSync(path.join(networkProjectRootUri.fsPath, 'src', 'main.zr'), 'utf8'),
        )),
    );
    await vscode.workspace.fs.writeFile(
        sourceProjectLibUri,
        new TextEncoder().encode(
            fs.readFileSync(path.join(networkProjectRootUri.fsPath, 'src', 'lib.zr'), 'utf8'),
        ),
    );
    const classesFullDocument = await openDocument(richSourceUri);
    const sourceRequestDocument = await openDocument(sourceProjectMainUri);
    const richBreakpointPosition = findPositionBySubstring(classesFullDocument, 'this.hp = this.hp + amount;', 0);
    const richBreakpoint = new vscode.SourceBreakpoint(
        new vscode.Location(classesFullDocument.uri, richBreakpointPosition),
    );

    vscode.debug.addBreakpoints([debugBreakpoint]);

    try {
        await verifyLaunchDebugSession({
            workspaceRoot,
            debugDocument,
            expectedSessionStartLabel: 'ZR debug session start',
            expectedFirstStopReason: 'breakpoint',
            startSession: async () => vscode.debug.startDebugging(workspaceRoot, {
                type: 'zr',
                name: 'ZR Smoke Launch',
                request: 'launch',
                project: debugProjectUri.fsPath,
                cwd: workspaceRoot.fsPath,
                executionMode: 'interp',
                stopOnEntry: false,
            }),
        });

        vscode.debug.addBreakpoints([richBreakpoint]);
        try {
            await verifyLaunchDebugSession({
                workspaceRoot,
                debugDocument: classesFullDocument,
                expectedSessionStartLabel: 'ZR rich debug session start',
                expectedFirstStopReason: 'breakpoint',
                inspectBreakpointState: false,
                expectBreakpointResolved: false,
                onStopped: async (session) => verifyRichDebugInspection(session, classesFullDocument.uri.fsPath),
                startSession: async () => vscode.debug.startDebugging(workspaceRoot, {
                    type: 'zr',
                    name: 'ZR Smoke Rich Launch',
                    request: 'launch',
                    project: richProjectUri.fsPath,
                    cwd: richProjectRootUri.fsPath,
                    executionMode: 'interp',
                    stopOnEntry: false,
                }),
            });
        } finally {
            vscode.debug.removeBreakpoints([richBreakpoint]);
        }

        await openDocument(debugProjectUri);
        await verifyLaunchDebugSession({
            workspaceRoot,
            debugDocument,
            expectedSessionStartLabel: 'ZR debug current project command session start',
            expectedFirstStopReason: 'entry',
            inspectBreakpointState: false,
            disconnectAfterStop: true,
            startSession: async () => {
                await vscode.commands.executeCommand('zr.debugCurrentProject');
                return true;
            },
        });

        await verifyLaunchDebugSession({
            workspaceRoot,
            debugDocument: sourceRequestDocument,
            expectedSessionStartLabel: 'ZR debug source request session start',
            expectedFirstStopReason: 'entry',
            inspectBreakpointState: false,
            disconnectAfterStop: true,
            expectBreakpointResolved: false,
            onStopped: async (session) => {
                const sourceResponse = await session.customRequest('source', {
                    source: {
                        path: 'lib',
                        name: 'lib.zr',
                    },
                    sourceReference: 0,
                });
                assert(typeof sourceResponse?.content === 'string' &&
                    sourceResponse.content.includes('%module "lib";'),
                'Expected launch-session DAP source request for lib to return lib.zr content');
            },
            startSession: async () => vscode.debug.startDebugging(workspaceRoot, {
                type: 'zr',
                name: 'ZR Smoke Source Request',
                request: 'launch',
                project: sourceProjectUri.fsPath,
                cwd: sourceProjectRootUri.fsPath,
                executionMode: 'interp',
                stopOnEntry: true,
            }),
        });
    } finally {
        vscode.debug.removeBreakpoints([debugBreakpoint]);
        await vscode.workspace.fs.delete(richProjectRootUri, { recursive: true, useTrash: false });
        await vscode.workspace.fs.delete(sourceProjectRootUri, { recursive: true, useTrash: false });
    }

    await verifyInvalidAttachEndpointRejected(workspaceRoot);

    attachTarget = await startExternalDebugTarget(bundledCliPath, workspaceRoot);
    try {
        await verifyAttachDebugSession({
            expectedSessionStartLabel: 'ZR attach session start',
            startSession: async () => vscode.debug.startDebugging(workspaceRoot, {
                type: 'zr',
                name: 'ZR Smoke Attach',
                request: 'attach',
                endpoint: attachTarget.endpoint,
            }),
        });
    } finally {
        if (attachTarget) {
            attachTarget.child.kill();
        }
    }

    commandAttachTarget = await startExternalDebugTarget(bundledCliPath, workspaceRoot);
    try {
        await verifyAttachDebugSession({
            expectedSessionStartLabel: 'ZR attach debug endpoint command session start',
            disconnectAfterStop: true,
            startSession: async () => withPatchedWindowMethod('showInputBox', async () => commandAttachTarget.endpoint, async () => {
                await vscode.commands.executeCommand('zr.attachDebugEndpoint');
                return true;
            }),
        });
    } finally {
        if (commandAttachTarget) {
            commandAttachTarget.child.kill();
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
        await verifyStructureViews(workspaceFolder.uri);
        await verifyDiagnostics(workspaceFolder.uri);
    } else if (focus === 'structure') {
        await verifyStructureViews(workspaceFolder.uri);
    } else if (focus === 'project-actions') {
        await verifyProjectActionIntegration(workspaceFolder.uri);
    }

    if (focus === 'all' || focus === 'debug') {
        await verifyDebugIntegration(workspaceFolder.uri);
    }
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
