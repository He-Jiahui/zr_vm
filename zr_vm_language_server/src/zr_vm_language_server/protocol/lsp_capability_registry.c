//
// Runtime-neutral LSP capability contract registry.
//

#include <string.h>

#include "zr_vm_language_server/lsp_capability_registry.h"

#define ZR_LSP_CAPABILITY_RUNTIME_ALL \
    (ZR_LSP_RUNTIME_NATIVE | ZR_LSP_RUNTIME_WASM)

#define ZR_LSP_CORE_CAPABILITY_WITH_RESOLVE_RUNTIMES(key, methodName, clientPath, coreEntry, nativeEntry, wasmEntry, testName, runtimes, resolve, resolveRuntimes) \
    { key, methodName, clientPath, coreEntry, nativeEntry, wasmEntry, testName, \
      runtimes, 3U, 17U, (resolve) != ZR_LSP_CAPABILITY_RESOLVE_NONE, ZR_FALSE, \
      resolve, resolveRuntimes, ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE }

#define ZR_LSP_CORE_CAPABILITY(key, methodName, clientPath, coreEntry, nativeEntry, wasmEntry, testName, runtimes) \
    ZR_LSP_CORE_CAPABILITY_WITH_RESOLVE_RUNTIMES(key, methodName, clientPath, coreEntry, nativeEntry, \
                                              wasmEntry, testName, runtimes, ZR_LSP_CAPABILITY_RESOLVE_NONE, 0U)

#define ZR_LSP_NATIVE_ADAPTER_CAPABILITY(key, methodName, clientPath, nativeEntry, testName, minor, experimental) \
    { key, methodName, clientPath, ZR_NULL, nativeEntry, ZR_NULL, testName, \
      ZR_LSP_RUNTIME_NATIVE, 3U, minor, ZR_FALSE, experimental, \
      ZR_LSP_CAPABILITY_RESOLVE_NONE, 0U, ZR_LSP_CAPABILITY_IMPLEMENTATION_NATIVE_ADAPTER }

static const SZrLspCapabilityDescriptor g_capabilities[] = {
        ZR_LSP_CORE_CAPABILITY("textDocumentSync",
                          "textDocument/didChange",
                          "textDocument.synchronization",
                          "ZrLanguageServer_Lsp_UpdateDocument",
                          "handle_did_change",
                          "wasm_ZrLspUpdateDocument",
                          "language_server_stdio_document_sync_conformance",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("positionEncoding",
                          "initialize",
                          "general.positionEncodings",
                          "handle_initialize_request",
                          "language_server_stdio_position_encoding_smoke",
                          17U, ZR_FALSE),
        ZR_LSP_CORE_CAPABILITY_WITH_RESOLVE_RUNTIMES("completionProvider",
                          "textDocument/completion",
                          "textDocument.completion",
                          "ZrLanguageServer_Lsp_GetCompletion",
                          "handle_completion_request",
                          "wasm_ZrLspGetCompletion",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL,
                          ZR_LSP_CAPABILITY_RESOLVE_MATERIAL,
                          ZR_LSP_RUNTIME_NATIVE),
        ZR_LSP_CORE_CAPABILITY("hoverProvider",
                          "textDocument/hover",
                          "textDocument.hover",
                          "ZrLanguageServer_Lsp_GetHover",
                          "handle_hover_request",
                          "wasm_ZrLspGetHover",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("signatureHelpProvider",
                          "textDocument/signatureHelp",
                          "textDocument.signatureHelp",
                          "ZrLanguageServer_Lsp_GetSignatureHelp",
                          "handle_signature_help_request",
                          ZR_NULL,
                          "language_server_stdio_smoke",
                          ZR_LSP_RUNTIME_NATIVE),
        ZR_LSP_CORE_CAPABILITY("definitionProvider",
                          "textDocument/definition",
                          "textDocument.definition",
                          "ZrLanguageServer_Lsp_GetDefinition",
                          "handle_definition_request",
                          "wasm_ZrLspGetDefinition",
                          "language_server_stdio_navigation_capabilities_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("referencesProvider",
                          "textDocument/references",
                          "textDocument.references",
                          "ZrLanguageServer_Lsp_FindReferences",
                          "handle_references_request",
                          "wasm_ZrLspFindReferences",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("renameProvider",
                          "textDocument/rename",
                          "textDocument.rename",
                          "ZrLanguageServer_Lsp_Rename",
                          "handle_rename_request",
                          "wasm_ZrLspRename",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("documentSymbolProvider",
                          "textDocument/documentSymbol",
                          "textDocument.documentSymbol",
                          "ZrLanguageServer_Lsp_GetDocumentSymbols",
                          "handle_document_symbols_request",
                          "wasm_ZrLspGetDocumentSymbols",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("workspaceSymbolProvider",
                          "workspace/symbol",
                          "workspace.symbol",
                          "ZrLanguageServer_Lsp_GetWorkspaceSymbols",
                          "handle_workspace_symbols_request",
                          "wasm_ZrLspGetWorkspaceSymbols",
                          "language_server_stdio_resolve_capabilities_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("documentHighlightProvider",
                          "textDocument/documentHighlight",
                          "textDocument.documentHighlight",
                          "ZrLanguageServer_Lsp_GetDocumentHighlights",
                          "handle_document_highlights_request",
                          "wasm_ZrLspGetDocumentHighlights",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("inlayHintProvider",
                          "textDocument/inlayHint",
                          "textDocument.inlayHint",
                          "ZrLanguageServer_Lsp_GetInlayHints",
                          "handle_inlay_hint_request",
                          "wasm_ZrLspGetInlayHints",
                          "language_server_stdio_resolve_capabilities_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("semanticTokensProvider",
                          "textDocument/semanticTokens/full",
                          "textDocument.semanticTokens",
                          "ZrLanguageServer_Lsp_GetSemanticTokens",
                          "handle_semantic_tokens_full_request",
                          "wasm_ZrLspGetSemanticTokens",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY_WITH_RESOLVE_RUNTIMES("codeActionProvider",
                          "textDocument/codeAction",
                          "textDocument.codeAction",
                          "ZrLanguageServer_Lsp_GetCodeActions",
                          "handle_code_action_request",
                          "wasm_ZrLspGetCodeActions",
                          "language_server_stdio_resolve_capabilities_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL,
                          ZR_LSP_CAPABILITY_RESOLVE_MATERIAL,
                          ZR_LSP_RUNTIME_NATIVE),
        ZR_LSP_CORE_CAPABILITY("documentFormattingProvider",
                          "textDocument/formatting",
                          "textDocument.formatting",
                          "ZrLanguageServer_Lsp_GetFormatting",
                          "handle_formatting_request",
                          "wasm_ZrLspGetFormatting",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("documentRangeFormattingProvider",
                          "textDocument/rangeFormatting",
                          "textDocument.rangeFormatting",
                          "ZrLanguageServer_Lsp_GetRangeFormatting",
                          "handle_range_formatting_request",
                          "wasm_ZrLspGetRangeFormatting",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("documentOnTypeFormattingProvider",
                          "textDocument/onTypeFormatting",
                          "textDocument.onTypeFormatting",
                          "handle_on_type_formatting_request",
                          "language_server_stdio_smoke",
                          17U, ZR_FALSE),
        ZR_LSP_CORE_CAPABILITY("foldingRangeProvider",
                          "textDocument/foldingRange",
                          "textDocument.foldingRange",
                          "ZrLanguageServer_Lsp_GetFoldingRanges",
                          "handle_folding_range_request",
                          "wasm_ZrLspGetFoldingRanges",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("selectionRangeProvider",
                          "textDocument/selectionRange",
                          "textDocument.selectionRange",
                          "ZrLanguageServer_Lsp_GetSelectionRanges",
                          "handle_selection_range_request",
                          "wasm_ZrLspGetSelectionRange",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("linkedEditingRangeProvider",
                          "textDocument/linkedEditingRange",
                          "textDocument.linkedEditingRange",
                          "handle_linked_editing_range_request",
                          "language_server_stdio_smoke",
                          17U, ZR_FALSE),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("monikerProvider",
                          "textDocument/moniker",
                          "textDocument.moniker",
                          "handle_moniker_request",
                          "language_server_stdio_smoke",
                          17U, ZR_FALSE),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("inlineValueProvider",
                          "textDocument/inlineValue",
                          "textDocument.inlineValue",
                          "handle_inline_value_request",
                          "language_server_stdio_inline_value_semantic_smoke",
                          17U, ZR_FALSE),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("inlineCompletionProvider",
                          "textDocument/inlineCompletion",
                          "textDocument.inlineCompletion",
                          "handle_inline_completion_request",
                          "language_server_stdio_smoke",
                          18U, ZR_TRUE),
        ZR_LSP_CORE_CAPABILITY("implementationProvider",
                          "textDocument/implementation",
                          "textDocument.implementation",
                          "ZrLanguageServer_Lsp_GetImplementation",
                          "handle_implementation_request",
                          ZR_NULL,
                          "language_server_stdio_navigation_capabilities_smoke",
                          ZR_LSP_RUNTIME_NATIVE),
        ZR_LSP_CORE_CAPABILITY("callHierarchyProvider",
                          "textDocument/prepareCallHierarchy",
                          "textDocument.callHierarchy",
                          "ZrLanguageServer_Lsp_PrepareCallHierarchy",
                          "handle_prepare_call_hierarchy_request",
                          ZR_NULL,
                          "language_server_stdio_smoke",
                          ZR_LSP_RUNTIME_NATIVE),
        ZR_LSP_CORE_CAPABILITY("typeHierarchyProvider",
                          "textDocument/prepareTypeHierarchy",
                          "textDocument.typeHierarchy",
                          "ZrLanguageServer_Lsp_PrepareTypeHierarchy",
                          "handle_prepare_type_hierarchy_request",
                          ZR_NULL,
                          "language_server_stdio_type_hierarchy_smoke",
                          ZR_LSP_RUNTIME_NATIVE),
        ZR_LSP_CORE_CAPABILITY("documentLinkProvider",
                          "textDocument/documentLink",
                          "textDocument.documentLink",
                          "ZrLanguageServer_Lsp_GetDocumentLinks",
                          "handle_document_link_request",
                          "wasm_ZrLspGetDocumentLinks",
                          "language_server_stdio_resolve_capabilities_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("codeLensProvider",
                          "textDocument/codeLens",
                          "textDocument.codeLens",
                          "ZrLanguageServer_Lsp_GetCodeLens",
                          "handle_code_lens_request",
                          "wasm_ZrLspGetCodeLens",
                          "language_server_stdio_resolve_capabilities_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_CORE_CAPABILITY("diagnosticProvider",
                          "textDocument/diagnostic",
                          "textDocument.diagnostic",
                          "ZrLanguageServer_Lsp_GetDiagnostics",
                          "handle_text_document_diagnostic_request",
                          "wasm_ZrLspGetDiagnosticReport",
                          "language_server_stdio_smoke",
                          ZR_LSP_CAPABILITY_RUNTIME_ALL),
        ZR_LSP_NATIVE_ADAPTER_CAPABILITY("workspace",
                          "workspace/didChangeWorkspaceFolders",
                          "workspace.workspaceFolders",
                          "handle_did_change_workspace_folders",
                          "language_server_stdio_workspace_folders_smoke",
                          17U, ZR_FALSE),
};

#undef ZR_LSP_CORE_CAPABILITY
#undef ZR_LSP_CORE_CAPABILITY_WITH_RESOLVE_RUNTIMES
#undef ZR_LSP_NATIVE_ADAPTER_CAPABILITY

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
        !string_is_present(descriptor->testId) ||
        descriptor->runtimeMask == 0U ||
        (descriptor->runtimeMask & ~validRuntimeMask) != 0U ||
        (descriptor->resolveRuntimeMask & ~descriptor->runtimeMask) != 0U ||
        descriptor->minimumMajor == 0U) {
        return ZR_FALSE;
    }
    if (descriptor->implementationLayer == ZR_LSP_CAPABILITY_IMPLEMENTATION_CORE) {
        if (!string_is_present(descriptor->coreEntryPoint)) {
            return ZR_FALSE;
        }
    } else if (descriptor->implementationLayer == ZR_LSP_CAPABILITY_IMPLEMENTATION_NATIVE_ADAPTER) {
        if (descriptor->runtimeMask != ZR_LSP_RUNTIME_NATIVE || descriptor->coreEntryPoint != ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }
    if ((descriptor->runtimeMask & ZR_LSP_RUNTIME_NATIVE) != 0U &&
        !string_is_present(descriptor->nativeAdapter)) {
        return ZR_FALSE;
    }
    if ((descriptor->runtimeMask & ZR_LSP_RUNTIME_NATIVE) == 0U &&
        descriptor->nativeAdapter != ZR_NULL) {
        return ZR_FALSE;
    }
    if ((descriptor->runtimeMask & ZR_LSP_RUNTIME_WASM) != 0U &&
        !string_is_present(descriptor->wasmExport)) {
        return ZR_FALSE;
    }
    if ((descriptor->runtimeMask & ZR_LSP_RUNTIME_WASM) == 0U &&
        descriptor->wasmExport != ZR_NULL) {
        return ZR_FALSE;
    }
    if (descriptor->hasResolve) {
        return descriptor->resolveRuntimeMask != 0U &&
               descriptor->resolveBehavior != ZR_LSP_CAPABILITY_RESOLVE_NONE;
    }
    return descriptor->resolveRuntimeMask == 0U &&
           descriptor->resolveBehavior == ZR_LSP_CAPABILITY_RESOLVE_NONE;
}

TZrBool ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(
        const SZrLspCapabilityDescriptor *descriptor) {
    if (!ZrLanguageServer_LspCapabilityRegistry_HasRequiredMetadata(descriptor)) {
        return ZR_FALSE;
    }
    if (descriptor->hasResolve &&
        descriptor->resolveBehavior != ZR_LSP_CAPABILITY_RESOLVE_MATERIAL) {
        return ZR_FALSE;
    }
    if ((descriptor->minimumMajor > 3U ||
         (descriptor->minimumMajor == 3U && descriptor->minimumMinor >= 18U)) &&
        !descriptor->isExperimental) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
        const TZrChar *capabilityKey, EZrLspRuntimeMask runtime) {
    const SZrLspCapabilityDescriptor *descriptor;

    if (runtime != ZR_LSP_RUNTIME_NATIVE && runtime != ZR_LSP_RUNTIME_WASM) {
        return ZR_FALSE;
    }
    descriptor = ZrLanguageServer_LspCapabilityRegistry_Find(capabilityKey);
    return ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(descriptor) &&
           descriptor->hasResolve && (descriptor->resolveRuntimeMask & (TZrUInt32)runtime) != 0U;
}
