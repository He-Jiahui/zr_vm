#include "lsp_editor_features_internal.h"
#include "semantic/lsp_semantic_call_hierarchy.h"
#include "semantic/lsp_semantic_type_hierarchy.h"

TZrBool ZrLanguageServer_Lsp_PrepareCallHierarchy(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticCallHierarchy_Prepare(
            state, context, uri, position, result);
}

TZrBool ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticCallHierarchy_AppendIncoming(
            state, context, item, result);
}

TZrBool ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticCallHierarchy_AppendOutgoing(
            state, context, item, result);
}

TZrBool ZrLanguageServer_Lsp_PrepareTypeHierarchy(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticTypeHierarchy_Prepare(
            state, context, uri, position, result);
}

TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticTypeHierarchy_AppendSupertypes(
            state, context, item, result);
}

TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return ZrLanguageServer_LspSemanticTypeHierarchy_AppendSubtypes(
            state, context, item, result);
}
