function expectedCapabilities(inlineCompletion, rangesFormatting) {
    const fileOperation = { filters: [{ pattern: { glob: '**/*.{zr,zrp,zro,dll,so,dylib}' } }] };
    const capabilities = {
        textDocumentSync: {
            openClose: true, change: 2, willSaveWaitUntil: true,
            save: { includeText: false },
        },
        positionEncoding: 'utf-16',
        completionProvider: {
            resolveProvider: true, triggerCharacters: ['.', ':'], allCommitCharacters: [';', ',', '.', '('],
        },
        hoverProvider: true,
        signatureHelpProvider: { triggerCharacters: ['(', ','] },
        definitionProvider: true,
        referencesProvider: true,
        renameProvider: { prepareProvider: true },
        documentSymbolProvider: true,
        workspaceSymbolProvider: { resolveProvider: false },
        documentHighlightProvider: true,
        inlayHintProvider: { resolveProvider: false },
        semanticTokensProvider: {
            legend: {
                tokenTypes: ['namespace', 'class', 'struct', 'interface', 'enum', 'function',
                    'method', 'property', 'variable', 'parameter', 'keyword', 'decorator', 'metaMethod'],
                tokenModifiers: ['declaration'],
            },
            full: { delta: true }, range: true,
        },
        codeActionProvider: {
            codeActionKinds: ['quickfix', 'source.organizeImports', 'source.removeUnused'], resolveProvider: true,
        },
        documentFormattingProvider: true,
        documentRangeFormattingProvider: rangesFormatting ? { rangesSupport: true } : true,
        documentOnTypeFormattingProvider: { firstTriggerCharacter: '}', moreTriggerCharacter: [';'] },
        foldingRangeProvider: true,
        selectionRangeProvider: true,
        linkedEditingRangeProvider: true,
        monikerProvider: true,
        inlineValueProvider: true,
        implementationProvider: true,
        callHierarchyProvider: true,
        typeHierarchyProvider: true,
        documentLinkProvider: { resolveProvider: false },
        codeLensProvider: { resolveProvider: false },
        diagnosticProvider: { interFileDependencies: true, workspaceDiagnostics: true },
        workspace: {
            workspaceFolders: { supported: true, changeNotifications: true },
            fileOperations: {
                didCreate: fileOperation, willRename: fileOperation,
                didRename: fileOperation, didDelete: fileOperation,
            },
        },
    };
    if (inlineCompletion) capabilities.inlineCompletionProvider = true;
    return capabilities;
}

module.exports = { expectedCapabilities };
