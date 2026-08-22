//
// Runtime-neutral LSP capability contract registry.
//

#include <string.h>

#include "zr_vm_language_server/lsp_capability_registry.h"

#define ZR_LSP_CAPABILITY_RUNTIME_ALL \
    (ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM)

#define ZR_LSP_CAPABILITY(key, methodName, clientPath, coreEntry, nativeEntry, wasmEntry, testName, resolve) \
    { key, methodName, clientPath, coreEntry, nativeEntry, wasmEntry, testName, \
      ZR_LSP_CAPABILITY_RUNTIME_ALL, 3U, 17U, \
      (resolve) != ZR_LSP_CAPABILITY_RESOLVE_NONE, ZR_FALSE, resolve }

static const SZrLspCapabilityDescriptor g_capabilities[] = {
        ZR_LSP_CAPABILITY("textDocumentSync",
                          "textDocument/didChange",
                          "textDocument.synchronization",
                          "ZrLanguageServer_Lsp_UpdateDocument",
                          "stdio_handle_document_change",
                          "connection.onDidChangeTextDocument",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("positionEncoding",
                          "initialize",
                          "general.positionEncodings",
                          "ZrLanguageServer_LspPositionEncoding_Negotiate",
                          "stdio_initialize",
                          "connection.onInitialize",
                          "language_server_stdio_position_encoding_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("completionProvider",
                          "textDocument/completion",
                          "textDocument.completion",
                          "ZrLanguageServer_Lsp_GetCompletions",
                          "stdio_handle_completion",
                          "connection.onCompletion",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_MATERIAL),
        ZR_LSP_CAPABILITY("hoverProvider",
                          "textDocument/hover",
                          "textDocument.hover",
                          "ZrLanguageServer_Lsp_GetHover",
                          "stdio_handle_hover",
                          "connection.onHover",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("signatureHelpProvider",
                          "textDocument/signatureHelp",
                          "textDocument.signatureHelp",
                          "ZrLanguageServer_Lsp_GetSignatureHelp",
                          "stdio_handle_signature_help",
                          "connection.onSignatureHelp",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("definitionProvider",
                          "textDocument/definition",
                          "textDocument.definition",
                          "ZrLanguageServer_Lsp_GetDefinition",
                          "stdio_handle_definition",
                          "connection.onDefinition",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("referencesProvider",
                          "textDocument/references",
                          "textDocument.references",
                          "ZrLanguageServer_Lsp_GetReferences",
                          "stdio_handle_references",
                          "connection.onReferences",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("renameProvider",
                          "textDocument/rename",
                          "textDocument.rename",
                          "ZrLanguageServer_Lsp_Rename",
                          "stdio_handle_rename",
                          "connection.onRenameRequest",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("documentSymbolProvider",
                          "textDocument/documentSymbol",
                          "textDocument.documentSymbol",
                          "ZrLanguageServer_Lsp_GetDocumentSymbols",
                          "stdio_handle_document_symbol",
                          "connection.onDocumentSymbol",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("workspaceSymbolProvider",
                          "workspace/symbol",
                          "workspace.symbol",
                          "ZrLanguageServer_Lsp_GetWorkspaceSymbols",
                          "stdio_handle_workspace_symbol",
                          "connection.onWorkspaceSymbol",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_IDENTITY),
        ZR_LSP_CAPABILITY("documentHighlightProvider",
                          "textDocument/documentHighlight",
                          "textDocument.documentHighlight",
                          "ZrLanguageServer_Lsp_GetDocumentHighlights",
                          "stdio_handle_document_highlight",
                          "connection.onDocumentHighlight",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("inlayHintProvider",
                          "textDocument/inlayHint",
                          "textDocument.inlayHint",
                          "ZrLanguageServer_Lsp_GetInlayHints",
                          "stdio_handle_inlay_hint",
                          "connection.onRequest('textDocument/inlayHint'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_IDENTITY),
        ZR_LSP_CAPABILITY("semanticTokensProvider",
                          "textDocument/semanticTokens/full",
                          "textDocument.semanticTokens",
                          "ZrLanguageServer_Lsp_GetSemanticTokens",
                          "stdio_handle_semantic_tokens",
                          "connection.onRequest('textDocument/semanticTokens/full'",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("codeActionProvider",
                          "textDocument/codeAction",
                          "textDocument.codeAction",
                          "ZrLanguageServer_Lsp_GetCodeActions",
                          "stdio_handle_code_action",
                          "connection.onRequest('textDocument/codeAction'",
                          "language_server_stdio_diagnostic_fix_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_IDENTITY),
        ZR_LSP_CAPABILITY("documentFormattingProvider",
                          "textDocument/formatting",
                          "textDocument.formatting",
                          "ZrLanguageServer_Lsp_FormatDocument",
                          "stdio_handle_document_formatting",
                          "connection.onRequest('textDocument/formatting'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("documentRangeFormattingProvider",
                          "textDocument/rangeFormatting",
                          "textDocument.rangeFormatting",
                          "ZrLanguageServer_Lsp_FormatRange",
                          "stdio_handle_range_formatting",
                          "connection.onRequest('textDocument/rangeFormatting'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("documentOnTypeFormattingProvider",
                          "textDocument/onTypeFormatting",
                          "textDocument.onTypeFormatting",
                          "ZrLanguageServer_Lsp_FormatOnType",
                          "stdio_handle_on_type_formatting",
                          "connection.onRequest('textDocument/onTypeFormatting'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("foldingRangeProvider",
                          "textDocument/foldingRange",
                          "textDocument.foldingRange",
                          "ZrLanguageServer_Lsp_GetFoldingRanges",
                          "stdio_handle_folding_range",
                          "connection.onRequest('textDocument/foldingRange'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("selectionRangeProvider",
                          "textDocument/selectionRange",
                          "textDocument.selectionRange",
                          "ZrLanguageServer_Lsp_GetSelectionRanges",
                          "stdio_handle_selection_range",
                          "connection.onRequest('textDocument/selectionRange'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("linkedEditingRangeProvider",
                          "textDocument/linkedEditingRange",
                          "textDocument.linkedEditingRange",
                          "ZrLanguageServer_Lsp_GetLinkedEditingRanges",
                          "stdio_handle_linked_editing_range",
                          "connection.onRequest('textDocument/linkedEditingRange'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("monikerProvider",
                          "textDocument/moniker",
                          "textDocument.moniker",
                          "ZrLanguageServer_Lsp_GetMonikers",
                          "stdio_handle_moniker",
                          "connection.onRequest('textDocument/moniker'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("inlineValueProvider",
                          "textDocument/inlineValue",
                          "textDocument.inlineValue",
                          "ZrLanguageServer_Lsp_GetInlineValues",
                          "stdio_handle_inline_value",
                          "connection.onRequest('textDocument/inlineValue'",
                          "language_server_stdio_inline_value_semantic_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("inlineCompletionProvider",
                          "textDocument/inlineCompletion",
                          "textDocument.inlineCompletion",
                          "ZrLanguageServer_Lsp_GetInlineCompletions",
                          "stdio_handle_inline_completion",
                          "connection.onRequest('textDocument/inlineCompletion'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("colorProvider",
                          "textDocument/documentColor",
                          "textDocument.documentColor",
                          "ZrLanguageServer_Lsp_GetDocumentColors",
                          "stdio_handle_document_color",
                          "connection.onRequest('textDocument/documentColor'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("declarationProvider",
                          "textDocument/declaration",
                          "textDocument.declaration",
                          "ZrLanguageServer_Lsp_GetDeclaration",
                          "stdio_handle_declaration",
                          "connection.onRequest('textDocument/declaration'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("typeDefinitionProvider",
                          "textDocument/typeDefinition",
                          "textDocument.typeDefinition",
                          "ZrLanguageServer_Lsp_GetTypeDefinition",
                          "stdio_handle_type_definition",
                          "connection.onRequest('textDocument/typeDefinition'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("implementationProvider",
                          "textDocument/implementation",
                          "textDocument.implementation",
                          "ZrLanguageServer_Lsp_GetImplementation",
                          "stdio_handle_implementation",
                          "connection.onRequest('textDocument/implementation'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("callHierarchyProvider",
                          "textDocument/prepareCallHierarchy",
                          "textDocument.callHierarchy",
                          "ZrLanguageServer_Lsp_PrepareCallHierarchy",
                          "stdio_handle_call_hierarchy",
                          "connection.onRequest('textDocument/prepareCallHierarchy'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("typeHierarchyProvider",
                          "textDocument/prepareTypeHierarchy",
                          "textDocument.typeHierarchy",
                          "ZrLanguageServer_Lsp_PrepareTypeHierarchy",
                          "stdio_handle_type_hierarchy",
                          "connection.onRequest('textDocument/prepareTypeHierarchy'",
                          "language_server_stdio_type_hierarchy_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("documentLinkProvider",
                          "textDocument/documentLink",
                          "textDocument.documentLink",
                          "ZrLanguageServer_Lsp_GetDocumentLinks",
                          "stdio_handle_document_link",
                          "connection.onRequest('textDocument/documentLink'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_IDENTITY),
        ZR_LSP_CAPABILITY("codeLensProvider",
                          "textDocument/codeLens",
                          "textDocument.codeLens",
                          "ZrLanguageServer_Lsp_GetCodeLenses",
                          "stdio_handle_code_lens",
                          "connection.onRequest('textDocument/codeLens'",
                          "language_server_lsp_advanced_editor_features_test",
                          ZR_LSP_CAPABILITY_RESOLVE_IDENTITY),
        ZR_LSP_CAPABILITY("diagnosticProvider",
                          "textDocument/diagnostic",
                          "textDocument.diagnostic",
                          "ZrLanguageServer_Lsp_GetDiagnostics",
                          "stdio_handle_diagnostic",
                          "connection.onRequest('textDocument/diagnostic'",
                          "language_server_stdio_diagnostic_fix_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
        ZR_LSP_CAPABILITY("workspace",
                          "workspace/didChangeWorkspaceFolders",
                          "workspace.workspaceFolders",
                          "ZrLanguageServer_Lsp_UpdateWorkspaceFolders",
                          "stdio_handle_workspace_folder_change",
                          "connection.onDidChangeWorkspaceFolders",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RESOLVE_NONE),
};

#undef ZR_LSP_CAPABILITY

static TZrBool string_is_present(const TZrChar *value) {
    return value != ZR_NULL && value[0] != '\0';
}

TZrSize ZrLanguageServer_LspCapabilityRegistry_Count(void) {
    return sizeof(g_capabilities) / sizeof(g_capabilities[0]);
}

const SZrLspCapabilityDescriptor *ZrLanguageServer_LspCapabilityRegistry_At(TZrSize index) {
    if (index >= ZrLanguageServer_LspCapabilityRegistry_Count()) {
        return ZR_NULL;
    }
    return &g_capabilities[index];
}

const SZrLspCapabilityDescriptor *
ZrLanguageServer_LspCapabilityRegistry_Find(const TZrChar *capabilityKey) {
    TZrSize index;

    if (!string_is_present(capabilityKey)) {
        return ZR_NULL;
    }
    for (index = 0; index < ZrLanguageServer_LspCapabilityRegistry_Count(); index++) {
        if (strcmp(g_capabilities[index].capabilityKey, capabilityKey) == 0) {
            return &g_capabilities[index];
        }
    }
    return ZR_NULL;
}

TZrBool ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(
        const SZrLspCapabilityDescriptor *descriptor) {
    const TZrUInt32 validRuntimeMask = ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM;

    if (descriptor == ZR_NULL ||
        !string_is_present(descriptor->capabilityKey) ||
        !string_is_present(descriptor->method) ||
        !string_is_present(descriptor->clientCapabilityPath) ||
        !string_is_present(descriptor->coreEntryPoint) ||
        !string_is_present(descriptor->nativeAdapter) ||
        !string_is_present(descriptor->testId) ||
        descriptor->runtimeMask == 0U ||
        (descriptor->runtimeMask & ~validRuntimeMask) != 0U ||
        descriptor->minimumMajor == 0U) {
        return ZR_FALSE;
    }
    if ((descriptor->runtimeMask & ZR_LSP_RUNTIME_WASM) != 0U &&
        !string_is_present(descriptor->wasmExport)) {
        return ZR_FALSE;
    }
    if (descriptor->hasResolve) {
        return descriptor->resolveBehavior != ZR_LSP_CAPABILITY_RESOLVE_NONE;
    }
    return descriptor->resolveBehavior == ZR_LSP_CAPABILITY_RESOLVE_NONE;
}

TZrBool ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(
        const SZrLspCapabilityDescriptor *descriptor) {
    if (!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(descriptor)) {
        return ZR_FALSE;
    }
    if (descriptor->resolveBehavior == ZR_LSP_CAPABILITY_RESOLVE_IDENTITY) {
        return ZR_FALSE;
    }
    if ((descriptor->minimumMajor > 3U ||
         (descriptor->minimumMajor == 3U && descriptor->minimumMinor >= 18U)) &&
        !descriptor->isExperimental) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
