const vscode = require('vscode');

/** 桌面 smoke 在连续重启期间保留未保存的编辑，并确认新旧符号都能重新被 hover 解析。
 * @param {Function} withRetry 调用方的重试器；重启和文档同步是异步的，不能把首次空响应当失败。
 * @note 调用方应传入已打开的 Zr 文档及仍有效的原始符号位置；finally 只恢复调用前文本，不负责保存。
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
    // 两次命令并发提交是为了覆盖扩展的重启队列，而不是模拟两个彼此独立的客户端。
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
    // 即使 hover 或重启失败也恢复编辑器文本，避免影响后续 smoke 场景。
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

module.exports = { verifyRestartResynchronization };
