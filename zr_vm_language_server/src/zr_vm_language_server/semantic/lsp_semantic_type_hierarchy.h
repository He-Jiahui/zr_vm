#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TYPE_HIERARCHY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TYPE_HIERARCHY_H

#include "semantic/lsp_semantic_query.h"

TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result);
TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_AppendSupertypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);
TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_AppendSubtypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);

#endif
