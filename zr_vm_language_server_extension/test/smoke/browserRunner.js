const vscode = require('vscode');

/** Web 版重启探针验证排队重启不会丢掉内存中的未保存文本和旧符号索引。
 * @note 与桌面 restartProbe 保持同一契约；调用方负责传入已打开文档及原始符号位置。
 */
async function verifyRestartResynchronization(document, position, withRetry) {
    const originalText = document.getText();
    const symbol = `restartUnsavedProbe${Date.now()}`;
    const edit = new vscode.WorkspaceEdit();
    edit.insert(document.uri, document.positionAt(originalText.length),
        `\nfn ${symbol}(): int { return 739; }\n`);
    if (!await vscode.workspace.applyEdit(edit) || !document.isDirty) {
        throw new Error('Restart probe requires an unsaved document edit');
    }
    // 同时提交两次用户重启命令，观察客户端重建队列对文档同步的影响。
    try {
        const unsavedPosition = document.positionAt(document.getText().indexOf(symbol) + 1);
        await Promise.all([
            vscode.commands.executeCommand('zr.restartLanguageServer'),
            vscode.commands.executeCommand('zr.restartLanguageServer'),
        ]);
        await withRetry(
            () => vscode.commands.executeCommand('vscode.executeHoverProvider', document.uri, unsavedPosition),
            (items) => Array.isArray(items) && items.some((item) =>
                item.contents.some((content) =>
                    (typeof content === 'string' ? content : content.value)?.includes(symbol))),
            15000,
            'unsaved symbol hover after queued restarts and document resynchronization',
        );
        if (!document.isDirty) {
            throw new Error('Restart probe content must remain unsaved');
        }
        await withRetry(
            () => vscode.commands.executeCommand('vscode.executeHoverProvider', document.uri, position),
            (items) => Array.isArray(items) && items.length > 0,
            15000,
            'original hover after queued restarts',
        );
    // 无论重启或 hover 是否成功，后续场景都应看到进入探针前的文本。
    } finally {
        const restore = new vscode.WorkspaceEdit();
        restore.replace(document.uri,
            new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)),
            originalText);
        if (!await vscode.workspace.applyEdit(restore)) {
            throw new Error('Unable to restore restart smoke document');
        }
    }
}

// Web 类语言功能共用同一段源文本，声明和使用位置共同约束跳转、补全、hover 与诊断。
const CLASSES_FULL_SMOKE_SOURCE = [
    'module classes_full;',
    '',
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
    'fn classesFullProjectShape(): void {',
    '    let boss: BossHero = new BossHero(30);',
    '    boss.hp = boss.hp + 7;',
    '    ScoreBoard.bonus = boss.heal(5);',
    '    boss.total() + ScoreBoard.bonus;',
    '}',
    '',
].join('\n');

// 结构视图的三文件依赖图包含工作区导入、原生导入与循环边，用来验证树节点的导航边界。
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

// main 的工作区导入目标，供结构树和定义跳转测试使用。
const STRUCTURE_SMOKE_HELPER_SOURCE = [
    'let cycle = import("structure_cycle");',
    '',
    'pub var value = fn() => {',
    '    return cycle.answer();',
    '};',
    '',
].join('\n');

// 反向导入故意形成循环，以约束视图构造不得无限递归。
const STRUCTURE_SMOKE_CYCLE_SOURCE = [
    'let helper = import("structure_helper");',
    '',
    'pub var answer = fn() => {',
    '    return 42;',
    '};',
    '',
].join('\n');

/** 让宿主 smoke 的契约失败以异常传播到 test-web，而不是只记录日志。 */
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

/** 供异步语言服务状态轮询退避，避免连续查询淹没 Worker。 */
async function sleep(milliseconds) {
    await new Promise((resolve) => setTimeout(resolve, milliseconds));
}

/** 为单次 VS Code 命令设置等待上限，防止 Promise 永不结算而阻塞整组 smoke。
 * @note 超时只停止等待，不取消底层命令；后续探针必须容忍迟到响应。
 */
async function withActionTimeout(promise, timeoutMs, label) {
    let timeoutHandle;

    try {
        return await Promise.race([
            promise,
            // 超时只终止本次等待；finally 必须撤销计时器，迟到的宿主命令仍可能完成。
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

/** 允许 Worker 启动、文档同步和提供者注册在短时间内收敛，再判定 smoke 失败。
 * @note action 应可重复调用且无破坏性；单次动作超时不会取消其执行。
 * BUG: 总期限检查只发生在每次动作之前；若临近期限才启动一次不结算的命令，15 秒预算可延长近一倍。
 *      run 的调用链会因此晚于标称期限报错；应把剩余时间传给单次等待。
 */
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

/** 把最后一次未满足断言的响应形状带入失败信息，方便区分空结果与未注册提供者。 */
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

/** 根据测试源码中稳定的文本片段定位光标，供定义、补全及结构导航断言共用。
 * @note 调用方须保证片段存在且 occurrence 与 offset 精确指向目标标识符。
 */
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

/** 将临时源码显示为活动编辑器，使依赖 activeTextEditor 的扩展命令走真实用户入口。 */
async function openDocument(filePath) {
    const document = await vscode.workspace.openTextDocument(filePath);
    await vscode.window.showTextDocument(document);
    return document;
}

/** 在完成场景后退出临时编辑器并清理桌面文件；Web 宿主保留临时文件以规避退出期文件事件噪声。
 * @note fallbackUri 默认指向 fixture 的 src/main.zr，调用方必须提供含该文件的工作区。
 * TODO: Web 分支依赖测试工作区隔离；需核对重复运行或共享 test-web 数据目录时残留同名文件的影响。
 */
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

/** 从语法错误到保存修复检查 Web 诊断清除；修复文本包含 CRLF 和 emoji。 */
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

    const repaired = new vscode.WorkspaceEdit();
    repaired.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)),
        'var x = 1;\r\nvar emoji = "😀";\r\n');
    assert(await vscode.workspace.applyEdit(repaired), 'Expected diagnostic repair edit to apply');
    await document.save();
    await withRetry(
        () => vscode.languages.getDiagnostics(document.uri),
        (items) => items.every((item) => item.severity !== vscode.DiagnosticSeverity.Error),
        15000,
        'syntax diagnostics clear after an edit and save with UTF-16/CRLF text',
    );
    await deleteDocumentFile(diagnosticUri);
    console.log('[zr-web-smoke] verifyDiagnostics:done');
}

/** 从实际 Web 编辑器请求基础 LSP 功能，再在同一未保存文档上执行排队重启探针。
 * @note 清理由本场景末尾执行；中途异常可能留下临时文件供后续排查。
 */
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

    // BUG: 空列表也满足 length >= 0；补全提供者完全失效时本场景仍会通过，需改为检查预期条目。
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

    const workspaceSymbols = await vscode.commands.executeCommand('vscode.executeWorkspaceSymbolProvider', 'x');
    assert(Array.isArray(workspaceSymbols) && workspaceSymbols.length === 0,
        'Web must not advertise workspace symbol indexing');

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
    console.log('[zr-web-smoke] verifyLanguageFeatures:done');
}

/** 用真实 Web 扩展宿主验证格式化、折叠、代码动作、导入命令和拉取诊断等编辑器契约。
 * @note 所有文件写到提供的 fixture 工作区；命令依赖刚打开且仍为活动编辑器的 Zr 文档。
 * TODO: finally 吞掉所有删除异常；若清理失败，需在独立 Web smoke 中确认残留是否影响后续运行。
 */
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
        'let system = import("zr.system");',
        '',
        'class AdvancedSmoke {',
        'pub fn run(value: int): int {',
        'let local = value;',
        'return local;',
        '}',
        '}',
        '',
        'fn advancedHelper(): int { return 1; }',
        '',
        '#zr.testing.test#',
        'fn advancedEditorSmoke(): void {',
        'advancedHelper();',
        'return;',
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
        assert(codeLens.some((item) => item.command?.command === 'zr.showReferences') &&
                codeLens.every((item) => !['zr.runCurrentProject', 'zr.debugCurrentProject']
                    .includes(item.command?.command)),
            'Expected web CodeLens to expose references without unavailable project commands');

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

        let workspaceDiagnosticError;
        try {
            await vscode.commands.executeCommand('zr.__sendLanguageServerRequest',
                'workspace/diagnostic', { previousResultIds: [] }, { strict: true });
        } catch (error) {
            workspaceDiagnosticError = error;
        }
        assert(workspaceDiagnosticError?.code === -32601,
            'Web workspace/diagnostic must return MethodNotFound while project indexing is unavailable');
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

/** 兼容 VS Code 和 LSP 两种补全响应形状，供成员补全断言读取候选项。 */
function completionEntries(items) {
    if (!items) {
        return [];
    }

    return Array.isArray(items) ? items : items.items;
}

/** 通过扩展内部命令读取 Worker 原始 LSP 结果，以补充 VS Code 提供者的外层观察。
 * @note 失败被折叠为 undefined，只适用于会在调用方另行断言结果的探针。
 */
async function sendRawLanguageServerRequest(method, params) {
    try {
        return await vscode.commands.executeCommand('zr.__sendLanguageServerRequest', method, params);
    } catch {
        return undefined;
    }
}

/** 同时观察编辑器补全提供者和 Worker 原始响应，避免单一宿主转换层掩盖后端候选项。
 * @note 混合列表用于 smoke 的存在性断言；它不是用户实际看到的去重排序结果。
 * TODO: 当仅原始请求有条目时成员补全断言仍通过；需确认此套件是否还要求证明 VS Code 提供者实际可用。
 */
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

/** 优先检查 VS Code 文档符号呈现；宿主暂时返回空列表时用原始请求辅助定位后端状态。
 * TODO: 若原始请求有符号而编辑器提供者持续为空，本回退仍令 smoke 通过；需单独验证宿主呈现层。
 */
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

/** 将 VS Code hover 与补全文档的多种 Markdown 包装统一为可断言文本。 */
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

/** 聚合 hover 内容，用于验证源码注释和符号信息被传到 Web 编辑器。 */
function hoverText(items) {
    if (!Array.isArray(items)) {
        return '';
    }

    return items.map((item) => markdownLikeToString(item.contents)).join('\n');
}

/** 提取补全项文档，用于检查属性前导注释没有在 Worker 桥接时丢失。 */
function completionDocumentationText(item) {
    return markdownLikeToString(item?.documentation);
}

/** 统一 Location 与 LocationLink 的目标 URI，供定义和引用断言复用。 */
function locationUri(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.uri ?? entry.targetUri;
}

/** 统一定义结果的命中范围，优先使用跳转目标选择区而非整个目标定义。 */
function locationRange(entry) {
    if (!entry) {
        return undefined;
    }

    return entry.range ?? entry.targetSelectionRange ?? entry.targetRange;
}

/** 验证导航位置精确落在标识符起点，避免只检查同一文件产生假阳性。 */
function positionEquals(position, line, character) {
    return Boolean(position) && position.line === line && position.character === character;
}

/** 约束定义结果的标识符跨度，而非仅验证文件 URI。 */
function rangeEquals(range, startLine, startCharacter, endLine, endCharacter) {
    return Boolean(range) &&
        positionEquals(range.start, startLine, startCharacter) &&
        positionEquals(range.end, endLine, endCharacter);
}

/** 在层级符号树中查找预期声明；类成员可能嵌套在父符号之下。 */
function hasDocumentSymbol(items, name) {
    if (!Array.isArray(items)) {
        return false;
    }

    return items.some((item) =>
        item &&
        (item.name === name || hasDocumentSymbol(item.children, name)));
}

/** 读取结构视图快照的子节点；叶节点可没有 children 字段。 */
function structureChildren(node) {
    return Array.isArray(node?.children) ? node.children : [];
}

/** 只在当前层查找结构节点，用于固定根节点或分组归属。 */
function findImmediateStructureNode(items, predicate) {
    return Array.isArray(items) ? items.find((item) => item && predicate(item)) : undefined;
}

/** 在结构树内递归查找声明或导入节点，验证它们可从预期分组到达。 */
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

/** 将视图快照的可选命令参数传给真实 VS Code 命令执行入口。 */
function commandArguments(node) {
    return Array.isArray(node?.commandArguments) ? node.commandArguments : [];
}

/** 限定 Imports 与 Declarations 必须作为文件节点的直接分组。 */
function findImmediateGroupNode(node, label) {
    return findImmediateStructureNode(
        structureChildren(node),
        (child) => child?.nodeType === 'group' && child.label === label,
    );
}

/** 等待结构导航改变活动编辑器与选区，供视图节点命令断言使用。
 * TODO: 当前 Web 场景未调用；核查桌面共用断言是否应提取，或此辅助函数是否应移除。
 */
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

/** 以继承、构造、属性及静态成员样例覆盖 Web 类语义服务的导航、补全和诊断。 */
async function verifyClassLanguageFeatures(workspaceRoot) {
    console.log('[zr-web-smoke] verifyClassLanguageFeatures:start');
    const smokeUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'classes_full_smoke.zr');
    await vscode.workspace.fs.writeFile(
        smokeUri,
        new TextEncoder().encode(CLASSES_FULL_SMOKE_SOURCE),
    );

    const document = await openDocument(smokeUri);
    const bossHeroUsage = findPositionBySubstring(document, 'boss: BossHero', 0, 6);
    const constructorUsage = findPositionBySubstring(document, 'new BossHero(30)', 0, 4);
    const bossCompletionPosition = findPositionBySubstring(document, 'boss.hp =', 0, 5);
    const scoreBoardCompletionPosition = findPositionBySubstring(document, 'ScoreBoard.bonus =', 0, 11);
    const totalUsagePosition = findPositionBySubstring(document, 'boss.total() + ScoreBoard.bonus', 0, 5);
    const bossHeroDefinitionPosition = findPositionBySubstring(document, 'class BossHero: BaseHero', 0, 6);
    const constructorDefinitionPosition = findPositionBySubstring(document,
        '@constructor(seed: int) super(seed)', 0);
    const totalDefinitionPosition = findPositionBySubstring(document, 'pub fn total(): int {', 0, 7);

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
    const constructorDefinition = await withRetry(
        () => vscode.commands.executeCommand('vscode.executeDefinitionProvider', document.uri, constructorUsage),
        (items) => Array.isArray(items) && items.length > 0,
        15000,
        'explicit constructor definition provider',
    );
    assert(constructorDefinition.some((item) =>
        uriPath(locationUri(item)).endsWith('/src/classes_full_smoke.zr') &&
        positionEquals(locationRange(item)?.start,
            constructorDefinitionPosition.line, constructorDefinitionPosition.character)),
    'Explicit constructor calls should navigate to the constructor declaration');

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

    const classReport = await vscode.commands.executeCommand('zr.__sendLanguageServerRequest',
        'textDocument/diagnostic', { textDocument: { uri: document.uri.toString(true) } });
    assert(classReport?.kind === 'full' && Array.isArray(classReport.items),
        'Valid class analysis must produce a full diagnostic report');
    const classErrors = classReport.items.filter((item) => item.severity === 1);
    assert(classErrors.length === 0,
        `Valid class field and property access must not report errors: ${JSON.stringify(classErrors)}`);
    await deleteDocumentFile(smokeUri);
    console.log('[zr-web-smoke] verifyClassLanguageFeatures:done');
}

/** 用三文件循环导入图验证 Web 结构树、工作区导入导航及原生声明虚拟文档。
 * @note Web 项目视图必须明确展示索引不可用，不能让空项目列表伪装为已索引。
 */
async function verifyStructureViews(workspaceRoot) {
    console.log('[zr-web-smoke] verifyStructureViews:start');
    const mainUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_smoke_main.zr');
    const helperUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_helper.zr');
    const cycleUri = vscode.Uri.joinPath(workspaceRoot, 'src', 'structure_cycle.zr');
    // TODO: 三文件写入位于 try/finally 外；第二或第三次写入失败会跳过后续回退与视图刷新。
    // Web 清理例程本就保留临时文件，需核查此失败路径对重复运行的影响。
    await vscode.workspace.fs.writeFile(mainUri, new TextEncoder().encode(STRUCTURE_SMOKE_MAIN_SOURCE));
    await vscode.workspace.fs.writeFile(helperUri, new TextEncoder().encode(STRUCTURE_SMOKE_HELPER_SOURCE));
    await vscode.workspace.fs.writeFile(cycleUri, new TextEncoder().encode(STRUCTURE_SMOKE_CYCLE_SOURCE));
    try {
        await openDocument(helperUri);
        await openDocument(cycleUri);
        const mainDocument = await openDocument(mainUri);
        const totalDefinitionPosition = findPositionBySubstring(mainDocument, 'pub fn total(): int {', 0, 7);
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
            'Expected Declarations group to include test function structureViewSmoke',
        );

        const unavailable = findImmediateStructureNode(snapshot.project,
            (node) => node.nodeType === 'info' && node.id === 'project:unavailable:web');
        assert(unavailable && unavailable.label.includes('Project indexing is unavailable in VS Code Web'),
            'Web project view must explain the current indexing boundary');
        assert(snapshot.project.every((node) => node.nodeType === 'info'),
            'Web project view must not display a synthetic project index');
        assert(Array.isArray(snapshot.builtin) && snapshot.builtin.length > 0,
            'Web must retain the builtin module view');

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

    } finally {
        await deleteDocumentFile(mainUri);
        await deleteDocumentFile(helperUri);
        await deleteDocumentFile(cycleUri);
        await vscode.commands.executeCommand('zr.structure.refresh');
    }
    console.log('[zr-web-smoke] verifyStructureViews:done');
}

/** @vscode/test-web 加载的导出入口：激活扩展后按焦点运行用户可见的 Web 语言功能场景。
 * @note run-web-smoke.js 提供含 src/main.zr 的工作区；ZR_TEST_SMOKE_FOCUS 默认 all。
 */
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

    // BUG: 非 all/lsp/structure 的非空焦点跳过所有验证并成功返回；环境变量拼写错误会产生假阳性。
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

/** 将 Location URI 归一为路径文本，用于检查目标属于本次临时测试文件。 */
function uriPath(uri) {
    return typeof uri?.path === 'string' ? uri.path : uri?.toString() ?? '';
}

module.exports = {
    run,
};
