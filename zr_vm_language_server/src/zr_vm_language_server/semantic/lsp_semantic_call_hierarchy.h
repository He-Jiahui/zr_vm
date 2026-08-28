#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CALL_HIERARCHY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CALL_HIERARCHY_H

#include "semantic/lsp_semantic_query.h"

TZrBool ZrLanguageServer_LspSemanticCallHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result);
TZrBool ZrLanguageServer_LspSemanticCallHierarchy_AppendIncoming(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);
TZrBool ZrLanguageServer_LspSemanticCallHierarchy_AppendOutgoing(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);

#endif
