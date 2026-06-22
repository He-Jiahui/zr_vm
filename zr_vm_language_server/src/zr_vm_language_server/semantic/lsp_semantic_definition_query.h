#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_DEFINITION_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_DEFINITION_QUERY_H

#include "semantic/lsp_semantic_query.h"

TZrBool ZrLanguageServer_LspSemanticDefinitionQuery_AppendReachingDefinition(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result);

#endif
