#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_REFERENCE_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_REFERENCE_QUERY_H

#include "semantic/lsp_semantic_query.h"

TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        TZrBool includeDeclaration,
        SZrArray *result);
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendHighlights(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result);

#endif
