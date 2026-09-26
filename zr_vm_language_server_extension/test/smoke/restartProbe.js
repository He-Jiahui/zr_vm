const vscode = require('vscode');

async function verifyRestartResynchronization(document, position, withRetry) {
    const originalText = document.getText();
    const symbol = `restartUnsavedProbe${Date.now()}`;
    const edit = new vscode.WorkspaceEdit();
    edit.insert(document.uri, document.positionAt(originalText.length),
        `\nfn ${symbol}(): int { return 739; }\n`);
    if (!await vscode.workspace.applyEdit(edit) || !document.isDirty) {
        throw new Error('Restart probe requires an unsaved document edit');
    }
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
