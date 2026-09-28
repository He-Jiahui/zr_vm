const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const vscode = require('vscode');
const { verifyRestartResynchronization } = require('./restartProbe');

// 由调试集成场景生成独立项目，固定入口和参数值以核对 DAP 栈帧、作用域与求值。
const RICH_DEBUG_SOURCE = [
    'fn total(delta: int): int {',
    '    return delta + 31;',
    '}',
    'return total(7);',
].join('\n');

// 同时覆盖继承、实例/静态属性、文档注释和方法引用；类名会在调用方附加时间戳以避开缓存。
const CLASSES_FULL_SMOKE_SOURCE = [
    'class BaseHero {',
    '    pri var _hp: int = 0;',
    '',
    '    pub @constructor(seed: int) {',
    '        this._hp = seed;',
    '    }',
    '',
    '    // Current hero hit points.',
    '    pub property hp: int {',
    '        get { return this._hp; }',
    '        set { this._hp = value; }',
    '    }',
    '',
    '    pub fn heal(amount: int): int {',
    '        this.hp = this.hp + amount;',
    '        return this.hp;',
    '    }',
    '}',
    '',
    'class ScoreBoard {',
    '    pri static var _bonus: int = 5;',
    '',
    '    // Shared bonus exposed through get/set.',
    '    pub static property bonus: int {',
    '        get { return ScoreBoard._bonus; }',
    '        set { ScoreBoard._bonus = value; }',
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
    '    pub fn total(): int {',
    '        return this.hp + ScoreBoard.bonus + BossHero.created;',
    '    }',
    '}',
    '',
    '#zr.testing.test#',
    'fn classesFullProjectShape(): int {',
    '    let boss: BossHero = new BossHero(30);',
    '    boss.hp = boss.hp + 7;',
    '    ScoreBoard.bonus = boss.heal(5);',
    '    return boss.total() + ScoreBoard.bonus;',
    '}',
    '',
].join('\n');

// 用项目导入及所有权类型检验 Hover 来源说明和语义 token 分类。
const PROJECT_INFERENCE_SMOKE_SOURCE = [
    'let greetModule = import("greet");',
    '',
    'resource class Hero {',
    '    pub fn total(): int {',
    '        return 1;',
    '    }',
    '}',
    '',
    'fn take(): Unique<Hero> {',
    '    return own Hero();',
    '}',
    '',
    'var hero = take();',
    'return greetModule.greet();',
    '',
].join('\n');

// 结构视图场景的入口模块；与下面两个模块构成工作区导入和循环导入。
const STRUCTURE_SMOKE_MAIN_SOURCE = [
    'let helper = import("structure_helper");',
    'let system = import("zr.system");',
    '',
    'class StructureHero {',
    '    pub fn total(): int {',
    '        return helper.value();',
    '    }',
    '}',
    '',
    '#zr.testing.test#',
    'fn structureViewSmoke(): void {',
    '    helper.value();',
    '}',
    '',
    'return helper.value();',
    '',
].join('\n');

// 作为结构树中可导航的工作区导入，同时指向 cycle 以检验循环关系的展示。
const STRUCTURE_SMOKE_HELPER_SOURCE = [
    'let cycle = import("structure_cycle");',
    '',
    'pub var value = fn() => {',
    '    return cycle.answer();',
    '};',
    '',
].join('\n');

// 回指 helper，要求结构树遍历在循环导入下仍能稳定提供节点。
const STRUCTURE_SMOKE_CYCLE_SOURCE = [
    'let helper = import("structure_helper");',
    '',
    'pub var answer = fn() => {',
    '    return 42;',
    '};',
    '',
].join('\n');

// smoke 断言统一抛出错误，让宿主 runner 将失败传播到进程退出状态。
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// 仅供轮询间隔使用；测试必须等待异步扩展激活、LSP 发布及 DAP 事件。
async function sleep(milliseconds) {
    await new Promise((resolve) => setTimeout(resolve, milliseconds));
}

// 对最终一致的 VS Code/LSP 状态重复读取；action 应可重复执行，predicate 只判定可观察结果。
// 超时后保留最后的请求错误，便于区分宿主调用失败和结果尚未就绪。
// TODO: deadline 只在 action() 前检查；VS Code 命令若不结算，会超过 timeoutMs 一直等待。
// 需在独立宿主探针中注入不结算请求，确认是否应对单次动作设置可取消的等待上限。
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

// 只输出失败等待时的结果形状，避免把大型服务端响应塞进测试错误。
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

// 让测试基于样例文本定位符号；调用方须保证目标片段及偏移落在预期 token 内。
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

// 显示文档以触发依赖活动编辑器的项目选择、结构视图和语言服务入口。
async function openDocument(filePath) {
    const document = await vscode.workspace.openTextDocument(filePath);
    await vscode.window.showTextDocument(document);
    return document;
}

// 收尾统一适配本地磁盘和 VS Code 虚拟文件系统，并容忍 Windows 文件句柄的短暂占用。
// BUG: ignoreBusy=true 时持续 EBUSY/EPERM/ENOTEMPTY 会被当成清理成功；调试场景可能留下固定名称的项目目录。
async function deleteWorkspaceEntry(uri, options = {}) {
    const recursive = Boolean(options.recursive);
    const ignoreBusy = Boolean(options.ignoreBusy);

    if (uri.scheme === 'file') {
        try {
            await withRetry(
                async () => {
                    await fs.promises.rm(uri.fsPath, {
                        force: true,
                        maxRetries: 3,
                        recursive,
                        retryDelay: 250,
                    });
                    return true;
                },
                (value) => value === true,
                recursive ? 20000 : 5000,
                `delete ${uri.fsPath}`,
            );
        } catch (error) {
            if (!ignoreBusy || !['EBUSY', 'EPERM', 'ENOTEMPTY'].includes(error?.code)) {
                throw error;
            }
        }
        return;
    }

    await withRetry(
        async () => {
            await vscode.workspace.fs.delete(uri, { recursive, useTrash: false });
            return true;
        },
        (value) => value === true,
        3000,
        `delete ${uri.toString()}`,
    );
}

// 删除活动文档前先切换编辑器，避免宿主继续持有文件句柄；需要已打开的工作区及可用回退文档。
async function deleteDocumentFile(uri, fallbackUri) {
    const workspaceFolder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
    const fallbackTarget = fallbackUri ?? vscode.Uri.joinPath(workspaceFolder.uri, 'src', 'main.zr');

    if (vscode.window.activeTextEditor?.document?.uri?.toString() === uri.toString()) {
        const fallbackDocument = await vscode.workspace.openTextDocument(fallbackTarget);
        await vscode.window.showTextDocument(fallbackDocument, { preview: false });
    }

    await deleteWorkspaceEntry(uri);
}

// 用确定的语法错误验证扩展激活后诊断会发布到新建 .zr 文档。
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

// 从 VS Code 命令入口检查基础 LSP 能力，并通过重启探针覆盖未保存文档的重新同步。
// 该场景假设 import_basic 项目及 src/main.zr 已由 smoke 工作区准备好。
async function verifyLanguageFeatures(workspaceRoot) {
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'lsp_smoke.zr');
    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode('var x = 10; var y = x;'),
    );
    await selectProjectByLabel('import_basic');

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
        async () => executeCompletionItems(mainDocument.uri, new vscode.Position(0, 0)),
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
    // BUG: 这里只验证返回形态；即使补全候选项为空，也会报告基础补全 smoke 成功。
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
        async () => executeDocumentSymbols(mainDocument.uri),
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

    await verifyRestartResynchronization(mainDocument, definitionPosition, withRetry);
    await deleteDocumentFile(smokeUri);
}

// 核对编辑器提供器与导入整理命令的用户可见效果，而非仅检查命令是否已注册。
async function verifyAdvancedEditorProviders(workspaceRoot) {
    const advancedUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_smoke_${Date.now()}.zr`);
    const actionUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_action_${Date.now()}.zr`);
    const organizeUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_imports_${Date.now()}.zr`);
    const cleanupUri = vscode.Uri.joinPath(workspaceRoot, 'src', `advanced_editor_cleanup_${Date.now()}.zr`);
    const commands = await vscode.commands.getCommands(true);
    assert(commands.includes('zr.organizeImports'), 'Expected zr.organizeImports command to be registered');
    assert(commands.includes('zr.removeUnusedImports'), 'Expected zr.removeUnusedImports command to be registered');
    const advancedSource = [
        'let system = import("zr.system");',
        'let tcp = import("zr.network.tcp");',
        '',
        'class AdvancedSmoke {',
        'pub fn run(value: int): int {',
        'let local = value;',
        'return local;',
        '}',
        '}',
        '',
        '#zr.testing.test#',
        'fn advancedEditorSmoke(): void {',
        'return;',
        '}',
        '',
    ].join('\n');

    try {
        await selectProjectByLabel('import_basic');
        await vscode.workspace.fs.writeFile(
            advancedUri,
            new TextEncoder().encode(advancedSource),
        );

        const document = await openDocument(advancedUri);
        assert(document.languageId === 'zr', 'Expected advanced editor smoke document to use the ZR language mode');
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
        const formattingTexts = formattingEdits.map((edit) => edit.newText);
        assert(formattingTexts.includes('    ') && formattingTexts.includes('        '),
            'Expected format provider to produce nested indentation edits');

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
        const rangeFormattingTexts = rangeFormattingEdits.map((edit) => edit.newText);
        assert(rangeFormattingTexts.includes('        ') ||
                rangeFormattingTexts.some((text) => text.includes('        return local;')),
            'Expected range format provider to indent method body');

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
            'Expected folding ranges for class/function regions');

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
            'Expected semantic selection ranges with parent expansion');

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
            'Expected ZR quick fix code action');

        await vscode.workspace.fs.writeFile(
            organizeUri,
            new TextEncoder().encode([
                'let system = import("zr.system");',
                'let math = import("zr.math");',
                '',
                'return 1;',
                '',
            ].join('\n')),
        );
        const organizeDocument = await openDocument(organizeUri);
        await vscode.commands.executeCommand('zr.organizeImports');
        await withRetry(
            async () => organizeDocument.getText(),
            (text) => text.indexOf('let math = import("zr.math");\nlet system = import("zr.system");') >= 0,
            15000,
            'zr.organizeImports command',
        );

        await vscode.workspace.fs.writeFile(
            cleanupUri,
            new TextEncoder().encode([
                'let math = import("zr.math");',
                'let system = import("zr.system");',
                '',
                'return math.PI;',
                '',
            ].join('\n')),
        );
        const cleanupDocument = await openDocument(cleanupUri);
        await vscode.commands.executeCommand('zr.removeUnusedImports');
        await withRetry(
            async () => cleanupDocument.getText(),
            (text) => text.includes('let math = import("zr.math");') &&
                !text.includes('let system = import("zr.system");'),
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
            'Expected document links with targets');

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
            'Expected code lens to expose the Zr test run command');
    } finally {
        // BUG: 删除失败被空 catch 吞掉，所有断言通过时 smoke 仍可能成功退出并遗留测试文档。
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
}

// 兼容 VS Code CompletionList 与裸数组，供补全场景使用同一套断言。
function completionEntries(items) {
    if (!items) {
        return [];
    }

    return Array.isArray(items) ? items : items.items;
}

// 绕过 VS Code provider 直接询问扩展客户端，用于诊断协议能力与宿主适配的差异。
// 请求失败被折叠为 undefined；调用方不能据此区分不支持与传输错误。
async function sendRawLanguageServerRequest(method, params) {
    try {
        return await vscode.commands.executeCommand('zr.__sendLanguageServerRequest', method, params);
    } catch {
        return undefined;
    }
}

// 同时收集宿主 provider 和原始 LSP 响应，以便类成员场景看到两种形态的候选项。
// BUG: 原始响应可单独满足补全断言；若 VS Code provider 未注册或返回空，相关 smoke 仍可能通过。
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

// 文档符号场景优先看 VS Code 结果，再以原始 LSP 响应补足宿主序列化差异。
// BUG: 宿主 provider 返回空时，原始响应可使文档符号断言通过，未覆盖实际编辑器入口。
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

// 类定义场景汇合 VS Code 与原始 LSP Location，容纳两条请求路径的不同响应形态。
// BUG: 仅原始响应含定义时仍判场景通过，无法发现 VS Code 转接路径缺失。
async function executeDefinitions(uri, position) {
    const primary = await vscode.commands.executeCommand(
        'vscode.executeDefinitionProvider',
        uri,
        position,
    );
    const direct = await sendRawLanguageServerRequest('textDocument/definition', {
        textDocument: { uri: uri.toString(true) },
        position: { line: position.line, character: position.character },
    });
    const primaryEntries = Array.isArray(primary) ? primary : [];
    const directEntries = Array.isArray(direct) ? direct : [];
    if (primaryEntries.length > 0 && directEntries.length > 0) {
        return [...directEntries, ...primaryEntries];
    }
    if (directEntries.length > 0) {
        return directEntries;
    }
    return primaryEntries.length > 0 ? primaryEntries : primary;
}

// 为方法调用中的不同 token 偏移寻找可解析点，避免测试样例的标点位置主导断言。
async function executeDefinitionsAtAnyPosition(uri, positions) {
    for (const position of positions) {
        const definitions = await executeDefinitions(uri, position);
        if (Array.isArray(definitions) && definitions.length > 0) {
            return definitions;
        }
    }
    return [];
}

// 把 VS Code WorkspaceEdit 与 LSP WorkspaceEdit 归一为 URI/编辑列表，供重命名断言共用。
function workspaceEditEntries(edit) {
    if (!edit) {
        return [];
    }

    if (typeof edit.entries === 'function') {
        return edit.entries();
    }

    const entries = [];
    for (const [uriText, edits] of Object.entries(edit.changes ?? {})) {
        entries.push([uriText, edits]);
    }
    for (const documentChange of edit.documentChanges ?? []) {
        const uriText = documentChange.textDocument?.uri;
        if (uriText) {
            entries.push([uriText, documentChange.edits ?? []]);
        }
    }
    return entries;
}

// 要求声明和使用处都被重命名，防止只收到一个局部编辑仍被视为完整重命名。
function workspaceEditHasRenameEdits(edit, pathSuffix, newText) {
    return workspaceEditEntries(edit).some(([uri, edits]) =>
        uriPath(uri).endsWith(pathSuffix) &&
        Array.isArray(edits) &&
        edits.filter((item) => item?.newText === newText).length >= 2);
}

// 优先检查编辑器重命名结果；不足时查询原始协议响应以定位服务端是否已有编辑。
// BUG: 原始响应可使断言通过，即便 VS Code 的重命名 provider 没有交付完整编辑。
async function executeRenameEdit(uri, position, newName, pathSuffix) {
    const primary = await vscode.commands.executeCommand(
        'vscode.executeDocumentRenameProvider',
        uri,
        position,
        newName,
    );
    if (workspaceEditHasRenameEdits(primary, pathSuffix, newName)) {
        return primary;
    }

    const direct = await sendRawLanguageServerRequest('textDocument/rename', {
        textDocument: { uri: uri.toString(true) },
        position: { line: position.line, character: position.character },
        newName,
    });
    return direct ?? primary;
}

// 将 Hover/补全文档的多种 VS Code 与 LSP 表示转为可断言文本。
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

// 聚合多个 Hover 区块，测试只关心用户最终可见的语义文本。
function hoverText(items) {
    if (!Array.isArray(items)) {
        return '';
    }

    return items.map((item) => markdownLikeToString(item.contents)).join('\n');
}

// 兼容 MarkdownString 与普通字符串，用于验证源码注释进入补全文档。
function completionDocumentationText(item) {
    return markdownLikeToString(item?.documentation);
}

// 检查特定样例注释是否经补全链路保留。
function completionHasDocumentation(item, expectedText) {
    return completionDocumentationText(item).includes(expectedText);
}

// TODO: 此辅助函数当前没有调用方；核查是否曾用于补全详情断言，再决定恢复断言或删除。
// 从两种补全标签布局读取详情，预留给不同宿主版本的断言。
function detailText(item) {
    return item?.detail ?? item?.label?.detail ?? '';
}

// 兼容 Location、LocationLink 与嵌套位置，保留定义/引用目标的 URI。
function locationUri(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.uri ?? entry.targetUri ?? entry.location?.uri;
}

// 定义目标优先选择精确选中范围，以核对标识符跨度而非整段声明。
function locationRange(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.range ?? entry.targetSelectionRange ?? entry.targetRange ?? entry.location?.range;
}

// LSP 坐标为零基；供目标范围断言比较源码中的预期位置。
function positionEquals(position, line, character) {
    return Boolean(position) && position.line === line && position.character === character;
}

// 检查定义位置的完整跨度，避免仅落在相同行上造成假阳性。
function rangeEquals(range, startLine, startCharacter, endLine, endCharacter) {
    return Boolean(range) &&
        positionEquals(range.start, startLine, startCharacter) &&
        positionEquals(range.end, endLine, endCharacter);
}

// 在混合 Location 结果中找到本次临时样例的精确标识符范围。
function hasLocationRange(items, pathSuffix, startLine, startCharacter, endLine, endCharacter) {
    return Array.isArray(items) && items.some((item) =>
        uriPath(locationUri(item)).endsWith(pathSuffix) &&
        rangeEquals(locationRange(item), startLine, startCharacter, endLine, endCharacter));
}

// 文档符号允许层级结构；递归查询可覆盖类成员的嵌套呈现。
function hasDocumentSymbol(items, name) {
    if (!Array.isArray(items)) {
        return false;
    }

    return items.some((item) =>
        item &&
        (item.name === name || hasDocumentSymbol(item.children, name)));
}

// 结构视图快照中的缺失 children 代表叶节点，供树遍历统一处理。
function structureChildren(node) {
    return Array.isArray(node?.children) ? node.children : [];
}

// 限定直接子级查询，避免错误层级中的同名节点满足结构视图断言。
function findImmediateStructureNode(items, predicate) {
    return Array.isArray(items) ? items.find((item) => item && predicate(item)) : undefined;
}

// 递归查询结构视图快照中的声明或模块；仅用于需要允许嵌套的断言。
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

// 结构节点导航命令允许没有参数，供命令调用保持统一形态。
function commandArguments(node) {
    return Array.isArray(node?.commandArguments) ? node.commandArguments : [];
}

// 要求分组直接属于目标项目/文件节点，防止跨层级误匹配。
function findImmediateGroupNode(node, label) {
    return findImmediateStructureNode(
        structureChildren(node),
        (child) => child?.nodeType === 'group' && child.label === label,
    );
}

// TODO: 当前结构视图场景直接内嵌同类等待逻辑，未调用本函数；复审时确认是否要合并。
// 等待导航命令更新活动编辑器与光标位置，原拟用于结构视图的用户操作验收。
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

// VS Code 可能返回数组或类数组 token 数据；统一为可解码的数字序列。
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

// 按 LSP 增量坐标恢复 token 和源码文本，测试检查语义分类而非仅检查非空结果。
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

// 同时匹配语义类型与文本，防止同名类别中的其他 token 掩盖错误分类。
function hasSemanticToken(decodedTokens, expectedType, expectedText) {
    return decodedTokens.some((token) =>
        token.type === expectedType && token.text === expectedText);
}

// 调试 source 请求只需多模块源码；替换网络样例中的运行期 Record 调用以稳定暂停点。
function sanitizeNetworkLoopbackDebugSource(text) {
    return text.replace(
        'var r = new lib.Record(3, 4);\nvar sum = r();',
        'var r = 0;\nvar sum = r;',
    );
}

// 分别从已有项目和临时所有权样例验证导入来源 Hover 与语义 token 的跨层结果。
async function verifyProjectInferenceAndSemanticTokens(workspaceRoot) {
    const projectMainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'main.zr');
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'project_inference_smoke.zr');
    const featureTimeoutMs = 30000;
    await selectProjectByLabel('import_basic');
    const projectDocument = await openDocument(projectMainUri);
    const aliasUsagePosition = findPositionBySubstring(
        projectDocument,
        'greetModule.greet()',
        0,
        'greetModule'.length,
    );
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
            return lastProjectImportHoverText.includes('project source');
        },
        featureTimeoutMs,
        'project import hover provider',
    );
    if (!hoverText(hover).includes('project source')) {
        throw new Error(`Import alias hover mismatch: ${lastProjectImportHoverText}`);
    }
    assert(hoverText(hover).includes('project source'),
        'Import alias hover should render project source provenance');

    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode(PROJECT_INFERENCE_SMOKE_SOURCE),
    );

    const document = await openDocument(smokeUri);
    const takeUsagePosition = findPositionBySubstring(document, 'take();', 0, 1);

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
    assert(hoverText(takeHover).length > 0,
        'Hover should render function information');

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
    assert(hasSemanticToken(decodedTokens, 'keyword', 'import'),
        'Semantic tokens should mark import as a keyword');
    assert(hasSemanticToken(decodedTokens, 'class', 'Unique'),
        'Semantic tokens should mark ownership wrapper types as classes');
    assert(hasSemanticToken(decodedTokens, 'namespace', 'greetModule'),
        'Semantic tokens should mark imported module aliases as namespaces');

    await deleteDocumentFile(smokeUri);
}

// 检查活动文件树、选中项目树及节点导航；通过临时项目验证显式选择跨刷新保留。
// 该场景依赖 import_basic 初始选择及两个调试用内部检查命令。
async function verifyStructureViews(workspaceRoot) {
    const mainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_smoke_main.zr');
    const helperUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_helper.zr');
    const cycleUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_cycle.zr');
    const alternateProjectRootUri = vscode.Uri.joinPath(workspaceRoot, '.structure_selected_project_smoke');
    const alternateProjectUri = vscode.Uri.joinPath(alternateProjectRootUri, 'structure_selected_project_smoke.zrp');
    const alternateProjectSrcUri = vscode.Uri.joinPath(alternateProjectRootUri, 'src');
    const alternateProjectMainUri = vscode.Uri.joinPath(alternateProjectSrcUri, 'main.zr');

    await vscode.commands.executeCommand('workbench.action.closeAllEditors');
    await vscode.workspace.fs.writeFile(mainUri, new TextEncoder().encode(STRUCTURE_SMOKE_MAIN_SOURCE));
    await vscode.workspace.fs.writeFile(helperUri, new TextEncoder().encode(STRUCTURE_SMOKE_HELPER_SOURCE));
    await vscode.workspace.fs.writeFile(cycleUri, new TextEncoder().encode(STRUCTURE_SMOKE_CYCLE_SOURCE));
    try {
        const mainDocument = await openDocument(mainUri);
        const totalDefinitionPosition = findPositionBySubstring(mainDocument, 'pub fn total(): int {', 0, 7);
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
            (value) => {
                if (!Array.isArray(value?.files) || !Array.isArray(value?.project)) {
                    return false;
                }

                const mainFile = findImmediateStructureNode(
                    value.files,
                    (node) => node.nodeType === 'file' && node.label === 'structure_smoke_main',
                );
                const declarations = mainFile ? findImmediateGroupNode(mainFile, 'Declarations') : undefined;
                return Boolean(
                    declarations &&
                    findStructureNode(
                        structureChildren(declarations),
                        (node) => node.nodeType === 'declaration' && node.label === 'StructureHero',
                    ) &&
                    findStructureNode(
                        structureChildren(declarations),
                        (node) => node.nodeType === 'declaration' && node.label === 'total',
                    ),
                );
            },
            30000,
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
            'Expected Declarations group to include test function structureViewSmoke',
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
                document.getText().includes('native extern("zr.system")'),
            15000,
            'native declaration virtual document load',
        );
        assert(nativeDocument.getText().includes('native extern("zr.system")'),
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
        await deleteWorkspaceEntry(alternateProjectRootUri, { recursive: true });
        await deleteDocumentFile(mainUri);
        await deleteDocumentFile(helperUri);
        await deleteDocumentFile(cycleUri);
        await vscode.commands.executeCommand('zr.structure.refresh');
    }
}

// 用独立类样例覆盖诊断、继承成员补全、文档、定义跨度、引用及重命名。
// 时间戳类名避开前次会话缓存，但要求所有查询针对同一临时文档版本。
async function verifyClassLanguageFeatures(workspaceRoot) {
    const uniqueId = Date.now();
    const baseHeroName = `BaseHeroSmoke${uniqueId}`;
    const bossHeroName = `BossHeroSmoke${uniqueId}`;
    const scoreBoardName = `ScoreBoardSmoke${uniqueId}`;
    const smokeFileName = `classes_full_smoke_${uniqueId}.zr`;
    const smokePathSuffix = `/src/${smokeFileName}`;
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', smokeFileName);
    const classSmokeSource = CLASSES_FULL_SMOKE_SOURCE
        .replaceAll('BaseHero', baseHeroName)
        .replaceAll('BossHero', bossHeroName)
        .replaceAll('ScoreBoard', scoreBoardName)
        .replaceAll('classesFullProjectShape', `classesFullProjectShape${uniqueId}`);
    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode(classSmokeSource),
    );

    const document = await openDocument(smokeUri);
    const bossHeroUsage = findPositionBySubstring(document, `boss: ${bossHeroName}`, 0, 6);
    const bossCompletionPosition = findPositionBySubstring(document, 'boss.hp =', 0, 5);
    const scoreBoardCompletionPosition = findPositionBySubstring(document, `${scoreBoardName}.bonus =`, 0, scoreBoardName.length);
    const scoreBoardCompletionAfterDotPosition =
        findPositionBySubstring(document, `${scoreBoardName}.bonus =`, 0, scoreBoardName.length + 1);
    const totalUsagePosition = findPositionBySubstring(document, `boss.total() + ${scoreBoardName}.bonus`, 0, 7);
    const totalUsagePositions = [5, 6, 7, 8, 9]
        .map((offset) => findPositionBySubstring(document, `boss.total() + ${scoreBoardName}.bonus`, 0, offset));
    const bossHeroDefinitionPosition = findPositionBySubstring(document, `class ${bossHeroName}: ${baseHeroName}`, 0, 6);
    const totalDefinitionPosition = findPositionBySubstring(document, 'pub fn total(): int {', 0, 7);

    const classReport = await vscode.commands.executeCommand('zr.__sendLanguageServerRequest',
        'textDocument/diagnostic', { textDocument: { uri: document.uri.toString(true) } });
    assert(classReport?.kind === 'full' && Array.isArray(classReport.items),
        'Class analysis must produce a full diagnostic report');
    const classErrors = classReport.items.filter((item) => item.severity === 1);
    assert(classErrors.length === 0,
        `Class fixture must analyze without errors: ${JSON.stringify(classErrors)}`);

    const bossHeroDefinition = await withRetry(
        async () => executeDefinitions(document.uri, bossHeroUsage),
        (items) => hasLocationRange(
            items,
            smokePathSuffix,
            bossHeroDefinitionPosition.line,
            bossHeroDefinitionPosition.character,
            bossHeroDefinitionPosition.line,
            bossHeroDefinitionPosition.character + bossHeroName.length,
        ),
        15000,
        `${bossHeroName} definition provider`,
    );
    assert(
        bossHeroDefinition.some((item) =>
            uriPath(locationUri(item)).endsWith(smokePathSuffix) &&
            rangeEquals(
                locationRange(item),
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character,
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character + bossHeroName.length,
            )),
        `${bossHeroName} definition should resolve to the class identifier span`,
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
            uriPath(locationUri(item)).endsWith(smokePathSuffix) &&
            rangeEquals(
                locationRange(item),
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character,
                bossHeroDefinitionPosition.line,
                bossHeroDefinitionPosition.character + bossHeroName.length,
            )),
        `${bossHeroName} references should include the class declaration`,
    );

    const bossCompletions = await withRetry(
        async () => executeCompletionItems(document.uri, bossCompletionPosition, '.'),
        (items) => completionEntries(items)
            .some((item) => (item.label?.label ?? item.label) === 'hp'),
        15000,
        'boss member completion',
    );
    const bossCompletionLabels = completionEntries(bossCompletions).map((item) => item.label?.label ?? item.label);
    assert(bossCompletionLabels.includes('hp'), `boss. completion should include property hp: ${bossCompletionLabels.join(', ')}`);
    assert(bossCompletionLabels.includes('heal'), `boss. completion should include method heal: ${bossCompletionLabels.join(', ')}`);
    assert(bossCompletionLabels.includes('total'), `boss. completion should include method total: ${bossCompletionLabels.join(', ')}`);

    // 同一静态属性在点号前后两个光标位置都可能被宿主接收；汇总后验证属性候选。
    const scoreBoardCompletions = await withRetry(
        async () => {
            const primary = await executeCompletionItems(document.uri, scoreBoardCompletionPosition, '.');
            const afterDot = await executeCompletionItems(document.uri, scoreBoardCompletionAfterDotPosition, '.');
            return {
                items: [
                    ...completionEntries(primary),
                    ...completionEntries(afterDot),
                ],
            };
        },
        (items) => completionEntries(items)
            .some((item) => (item.label?.label ?? item.label) === 'bonus'),
        15000,
        'ScoreBoard member completion',
    );
    const scoreBoardCompletionLabels = completionEntries(scoreBoardCompletions)
        .map((item) => item.label?.label ?? item.label);
    assert(scoreBoardCompletionLabels.includes('bonus'),
        'ScoreBoard. completion should include static property bonus');
    const bonusCompletion = completionEntries(scoreBoardCompletions)
        .find((item) => (item.label?.label ?? item.label) === 'bonus' &&
            completionHasDocumentation(item, 'Shared bonus exposed through get/set.'));
    assert(Boolean(bonusCompletion),
        'ScoreBoard. completion should surface leading property comments in documentation');

    const totalDefinition = await withRetry(
        async () => executeDefinitionsAtAnyPosition(document.uri, totalUsagePositions),
        (items) => hasLocationRange(
            items,
            smokePathSuffix,
            totalDefinitionPosition.line,
            totalDefinitionPosition.character,
            totalDefinitionPosition.line,
            totalDefinitionPosition.character + 'total'.length,
        ),
        15000,
        'total definition provider',
    );
    assert(
        totalDefinition.some((item) =>
            uriPath(locationUri(item)).endsWith(smokePathSuffix) &&
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
    assert(totalHoverText.length > 0,
        'Hover should render method information');
    assert(!totalHoverText.includes('[object Object]'),
        'Hover should render markdown instead of object placeholders');

    const renameEdit = await withRetry(
        async () => executeRenameEdit(document.uri, totalUsagePosition, 'renamedTotal', smokePathSuffix),
        (value) => workspaceEditHasRenameEdits(value, smokePathSuffix, 'renamedTotal'),
        15000,
        'total rename provider',
    );
    assert(workspaceEditHasRenameEdits(renameEdit, smokePathSuffix, 'renamedTotal'),
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
}

// 在启动会话前注册 DAP tracker，避免丢失刚启动时的 breakpoint/stopped 事件。
// BUG: 启动调用抛错时，已创建的 Promise 无取消路径，超时拒绝可能成为未处理的异步错误。
async function waitForDebugEvent(eventName, timeoutMs, expectedSessionId, label, predicate) {
    return new Promise((resolve, reject) => {
        const trackerDisposable = vscode.debug.registerDebugAdapterTrackerFactory('zr', {
            // 每次调试会话建立时由 VS Code 调用；提供 ID 时仅订阅目标会话。
            createDebugAdapterTracker(debugSession) {
                // BUG: 当前所有调用方均传 undefined；并行 ZR 会话的同名事件也可提前解除等待。
                if (expectedSessionId && debugSession.id !== expectedSessionId) {
                    return undefined;
                }

                return {
                    // DAP 事件到达后才判定测试已进入预期暂停或断点解析阶段。
                    onDidSendMessage(message) {
                        if (message && message.type === 'event' && message.event === eventName) {
                            if (predicate && !predicate(message)) {
                                return;
                            }
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

// 仅对当前活动的目标会话订阅终止；已结束的会话无需等待历史事件。
async function waitForDebugSessionEnd(session, timeoutMs, label) {
    if (!session) {
        return undefined;
    }

    const activeSession = vscode.debug.activeDebugSession;
    if (!activeSession || activeSession.id !== session.id) {
        return undefined;
    }

    return new Promise((resolve, reject) => {
        // 仅目标会话的结束可解除等待，其他并行会话不会提前通过。
        const disposable = vscode.debug.onDidTerminateDebugSession((terminatedSession) => {
            if (terminatedSession.id !== session.id) {
                return;
            }
            clearTimeout(timeoutHandle);
            disposable.dispose();
            resolve(terminatedSession);
        });
        // 到期时解除事件订阅；调用方只应把目标会话的终止视作成功。
        const timeoutHandle = setTimeout(() => {
            disposable.dispose();
            reject(new Error(`Timed out waiting for debug session to end${label ? ` (${label})` : ''}`));
        }, timeoutMs);
    });
}

// 为 attach 测试启动独立 CLI，使用回环动态端口并从标准输出获取可连接端点。
// 成功获得端点后，调用方必须终止仍在等待调试器的子进程。
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
        // 动态端点未发布时杀掉等待调试器的 CLI，避免失败测试留下外部目标。
        const timeoutHandle = setTimeout(() => {
            if (!resolved) {
                child.kill();
                reject(new Error(`Timed out waiting for external debug endpoint.\nstdout:\n${stdoutBuffer}\nstderr:\n${stderrBuffer}`));
            }
        }, 15000);

        // 将端点、提前退出和启动错误汇合成一次建立连接的结果。
        function finish(error, value) {
            clearTimeout(timeoutHandle);
            if (error) {
                reject(error);
                return;
            }
            resolve(value);
        }

        // CLI 输出端点即交付子进程所有权，后续关闭由 attach 场景负责。
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
        // 把失败前的服务端日志保留在异常上下文，供 smoke 失败定位。
        child.stderr.on('data', (chunk) => {
            stderrBuffer += chunk.toString();
        });
        // 若 CLI 尚未发布端点就退出，attach 测试没有可连接目标，应立即失败。
        child.on('exit', (code) => {
            if (!resolved) {
                finish(new Error(`External debug target exited before endpoint became available (code=${code}).\nstdout:\n${stdoutBuffer}\nstderr:\n${stderrBuffer}`));
            }
        });
        // spawn 错误属于目标创建失败；与提前退出共用同一拒绝路径。
        child.on('error', (error) => {
            if (!resolved) {
                finish(error);
            }
        });
    });
}

// 在断点暂停后核对 DAP 栈、源映射与基础作用域，证明调试适配器可供编辑器检查状态。
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

// 对 DAP variables 响应按名称取值，供富调试场景断言稳定字段。
function debugVariableByName(variables, name) {
    return Array.isArray(variables) ? variables.find((item) => item?.name === name) : undefined;
}

// 展开 DAP 变量引用；没有变量列表视为空集合，使断言承担错误报告。
async function readDebugVariables(session, variablesReference) {
    const variablesResult = await session.customRequest('variables', {
        variablesReference,
    });
    return Array.isArray(variablesResult?.variables) ? variablesResult.variables : [];
}

// 要求指定栈帧中的作用域可展开，再读取其变量供语义级断言使用。
async function readDebugScopeVariables(session, frameId, scopeName) {
    const scopesResult = await session.customRequest('scopes', { frameId });
    const scopes = Array.isArray(scopesResult?.scopes) ? scopesResult.scopes : [];
    const scope = scopes.find((item) => item?.name === scopeName);

    assert(scope && typeof scope.variablesReference === 'number' && scope.variablesReference > 0,
        `Expected ${scopeName} scope to expose variables`);
    return readDebugVariables(session, scope.variablesReference);
}

// 验证富调试项目中参数、全局对象、模块展开和暂停态求值在 DAP 上均可见。
async function verifyRichDebugInspection(session, expectedSourcePath) {
    const stackTrace = await session.customRequest('stackTrace', { threadId: 1 });
    const stackFrames = Array.isArray(stackTrace?.stackFrames) ? stackTrace.stackFrames : [];
    const topFrame = stackFrames[0];

    assert(topFrame, 'Expected rich inspection stack to expose a top frame');
    assert(normalizePath(topFrame?.source?.path ?? '') === normalizePath(expectedSourcePath),
        'Expected rich inspection frame source path to match the rich debug source');
    assert(String(topFrame?.name ?? '').includes('total'),
        'Expected rich inspection frame name to expose the function name');
    assert(String(topFrame?.name ?? '').includes('@'),
        'Expected rich inspection frame name to include module information');
    assert(String(topFrame?.name ?? '').includes('depth=0'),
        'Expected rich inspection frame name to include frame depth');

    const argumentsVariables = await readDebugScopeVariables(session, topFrame.id, 'Arguments');
    const globalsVariables = await readDebugScopeVariables(session, topFrame.id, 'Globals');

    assert(debugVariableByName(argumentsVariables, 'delta')?.value === '7',
        'Expected function arguments scope to expose delta=7');
    assert(debugVariableByName(globalsVariables, 'zrState')?.variablesReference > 0,
        'Expected globals scope to expose zrState');
    assert(debugVariableByName(globalsVariables, 'loadedModules')?.variablesReference > 0,
        'Expected globals scope to expose loadedModules');
    assert(debugVariableByName(globalsVariables, 'zr')?.variablesReference > 0,
        'Expected globals scope to expose zr runtime helpers');

    const evaluateValue = await session.customRequest('evaluate', {
        expression: 'delta + 1',
        frameId: topFrame.id,
        context: 'watch',
    });
    assert(String(evaluateValue?.result ?? '') === '8',
        'Expected paused readonly evaluate to compute delta + 1');

    const stateEntry = debugVariableByName(globalsVariables, 'zrState');
    const loadedModulesEntry = debugVariableByName(globalsVariables, 'loadedModules');

    const zrStateVariables = await readDebugVariables(session, stateEntry.variablesReference);
    const loadedModuleVariables = await readDebugVariables(session, loadedModulesEntry.variablesReference);

    assert(zrStateVariables.length > 0,
        'Expected zrState expansion to expose runtime state metadata');
    assert(loadedModuleVariables.length > 0,
        'Expected loadedModules expansion to expose at least one loaded module');
}

// 测试中替换 VS Code 交互入口，确定性地选择项目或输入端点；操作后必须恢复原方法。
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

// 将交互式项目选择固定到指定标签，供各 LSP/视图场景使用同一已知项目上下文。
async function selectProjectByLabel(label) {
    await withPatchedWindowMethod('showQuickPick', async (items) =>
        items.find((item) => item.label === label), async () => {
        await vscode.commands.executeCommand('zr.selectProject');
    });
}

// 临时拦截任务执行以检验构造出的命令参数，同时避免 smoke 真正运行项目任务。
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

// 在 .zrp 与 .zr 活动编辑器之间核对项目动作的目标、CLI 设置和调试入口。
// 工作区设置、断点和会话属于宿主共享状态，必须在场景完成后恢复。
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

    // BUG: 此时活动编辑器已被上面的 openDocument(debugSourceUri) 切到 .zr，
    // 名为「zrp editor」的检查实际没有覆盖 .zrp 为活动编辑器的情况。
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

        // 截获当前项目任务，核对 CLI 与 .zrp 参数，同时避免在 smoke 中真正执行程序。
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
        // 另走显式选中项目命令，确认它仍把同一项目交给 VS Code 任务系统。
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

// 先确认打包 CLI 和命令贡献可用，再运行项目动作的用户路径测试。
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

// 复用 launch 场景的事件顺序检查：先订阅、再启动、暂停检查、继续或主动断开。
// onInitialStopped/onStopped 回调供富调试与 source 请求注入特定 DAP 断言。
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
    onInitialStopped,
    onStopped,
}) {
    let session;
    let started;

    const breakpointResolved = expectBreakpointResolved
        ? waitForDebugEvent('breakpoint', 15000, undefined, `${expectedSessionStartLabel}:breakpoint`)
        : undefined;
    const launchStopped = waitForDebugEvent('stopped', 15000, undefined, `${expectedSessionStartLabel}:initial-stop`);
    let stoppedEventCount = 0;
    // 入口暂停与源码断点都发 stopped；过滤首个事件后再核对第二次暂停的原因。
    const postEntryStopped = continueFromEntryToBreakpoint
        ? waitForDebugEvent(
            'stopped',
            15000,
            undefined,
            `${expectedSessionStartLabel}:post-entry-breakpoint`,
            () => {
                stoppedEventCount += 1;
                return stoppedEventCount > 1;
            },
        )
        : undefined;

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
        if (onInitialStopped) {
            await onInitialStopped(session, stoppedEvent);
        }
        if (continueFromEntryToBreakpoint) {
            await session.customRequest('continue', { threadId: 1 });
            stoppedEvent = await postEntryStopped;
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
            const launchTerminated = waitForDebugSessionEnd(session, 30000, `${expectedSessionStartLabel}:terminated`);
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

// 从外部 CLI 端点 attach，核对入口暂停以及继续/断开两种生命周期。
async function verifyAttachDebugSession({
    expectedSessionStartLabel,
    startSession,
    disconnectAfterStop = false,
}) {
    let session;
    let started;

    const attachStopped = waitForDebugEvent('stopped', 15000, undefined, `${expectedSessionStartLabel}:initial-stop`);

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
            const attachTerminated = waitForDebugSessionEnd(session, 30000, `${expectedSessionStartLabel}:terminated`);
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

// 检查非回环端点在启动前被扩展拒绝，并向用户显示明确配置错误。
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

// 打通打包 CLI、launch、DAP 检查、source 请求、attach 和命令入口的桌面宿主链路。
// 场景从 smoke 工作区的 import_basic 与相邻 network_loopback 样例构造临时项目。
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
    const richProjectMainUri = vscode.Uri.joinPath(richProjectSrcUri, 'main.zr');
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
        richProjectMainUri,
        new TextEncoder().encode(RICH_DEBUG_SOURCE),
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
    const classesFullDocument = await openDocument(richProjectMainUri);
    const sourceRequestDocument = await openDocument(sourceProjectMainUri);
    const richBreakpointPosition = findPositionBySubstring(classesFullDocument, 'return delta + 31;', 0);
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
                expectedFirstStopReason: 'entry',
                continueFromEntryToBreakpoint: true,
                inspectBreakpointState: false,
                disconnectAfterStop: true,
                expectBreakpointResolved: false,
                // 入口暂停后用运行期源路径设置断点，再让通用流程继续到方法体检查。
                onInitialStopped: async (session) => {
                    const stackTrace = await session.customRequest('stackTrace', { threadId: 1 });
                    const stackFrames = Array.isArray(stackTrace?.stackFrames) ? stackTrace.stackFrames : [];
                    const runtimeSourcePath = stackFrames[0]?.source?.path ?? classesFullDocument.uri.fsPath;
                    const sourceBreakpointResult = await session.customRequest('setBreakpoints', {
                        source: {
                            path: runtimeSourcePath,
                            name: path.basename(runtimeSourcePath),
                        },
                        breakpoints: [
                            { line: richBreakpointPosition.line + 1 },
                        ],
                        sourceModified: false,
                    });
                    const sourceBreakpoint = Array.isArray(sourceBreakpointResult?.breakpoints)
                        ? sourceBreakpointResult.breakpoints[0]
                        : undefined;
                    assert(sourceBreakpoint && sourceBreakpoint.verified === true,
                        `Expected rich debug source breakpoint to bind: ${JSON.stringify(sourceBreakpointResult)}`);
                },
                onStopped: async (session) => verifyRichDebugInspection(session, classesFullDocument.uri.fsPath),
                startSession: async () => vscode.debug.startDebugging(workspaceRoot, {
                    type: 'zr',
                    name: 'ZR Smoke Rich Launch',
                    request: 'launch',
                    project: richProjectUri.fsPath,
                    cwd: richProjectRootUri.fsPath,
                    executionMode: 'interp',
                    stopOnEntry: true,
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
            // 命令入口返回 void；通用 launch 探针另行等待真正的活动会话。
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
            // 在真正暂停的会话上请求依赖模块源码，验证 DAP source 与项目模块映射。
            onStopped: async (session) => {
                const sourceResponse = await session.customRequest('source', {
                    source: {
                        path: 'lib',
                        name: 'lib.zr',
                    },
                    sourceReference: 0,
                });
                assert(typeof sourceResponse?.content === 'string' &&
                    sourceResponse.content.includes('module lib;'),
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
        await vscode.commands.executeCommand('workbench.action.closeAllEditors');
        await deleteWorkspaceEntry(richProjectRootUri, { recursive: true, ignoreBusy: true });
        await deleteWorkspaceEntry(sourceProjectRootUri, { recursive: true, ignoreBusy: true });
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
            // 模拟用户输入已启动 CLI 的回环端点，之后由 attach 探针核对实际会话。
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

/**
 * 桌面扩展宿主的 smoke 总入口；electronRunner 从环境变量传入期望模式和测试范围。
 * 需要已准备的工作区、打包扩展与原生 CLI；各场景会临时写入工作区并改动宿主状态。
 */
async function runSmokeSuite({ expectedMode, focus = 'all' }) {
    const workspaceFolder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
    assert(workspaceFolder, 'Expected a workspace folder for smoke test');
    const extension = vscode.extensions.all.find((item) => item.packageJSON?.name === 'zr-vm-language-server');
    assert(extension, 'Expected Zr extension to be present in extension host');

    await extension.activate();
    assert(extension.isActive, 'Expected Zr extension to remain active after activation');
    // BUG: 此处仅检查参数属于枚举，未核对已激活扩展的实际服务端模式；错误模式仍可能通过。
    assert(expectedMode === 'native' || expectedMode === 'web',
        `Unexpected smoke mode: ${expectedMode}`);

    // BUG: 未知 focus 不匹配任何分支时，所有场景被跳过且 runSmokeSuite 正常返回。
    if (focus === 'all' || focus === 'lsp') {
        await verifyLanguageFeatures(workspaceFolder.uri);
        await verifyAdvancedEditorProviders(workspaceFolder.uri);
        await verifyProjectInferenceAndSemanticTokens(workspaceFolder.uri);
        await verifyClassLanguageFeatures(workspaceFolder.uri);
        await verifyStructureViews(workspaceFolder.uri);
        await verifyDiagnostics(workspaceFolder.uri);
    } else if (focus === 'structure') {
        await verifyStructureViews(workspaceFolder.uri);
    } else if (focus === 'class') {
        await verifyClassLanguageFeatures(workspaceFolder.uri);
    } else if (focus === 'project-actions') {
        await verifyProjectActionIntegration(workspaceFolder.uri);
    }

    if (focus === 'all' || focus === 'debug') {
        await verifyDebugIntegration(workspaceFolder.uri);
    }
}

// 统一 URI 与字符串形式以比较临时文件后缀；调用方应传入有效位置对象。
function uriPath(uri) {
    return typeof uri.path === 'string' ? uri.path : uri.toString();
}

// 消除平台路径分隔符差异以核对 DAP 源映射。
// BUG: 无条件转小写会在区分大小写的文件系统上把错误大小写路径误判为相同。
function normalizePath(value) {
    return typeof value === 'string' ? value.replace(/[\\/]+/g, '/').toLowerCase() : '';
}

module.exports = {
    runSmokeSuite,
};
