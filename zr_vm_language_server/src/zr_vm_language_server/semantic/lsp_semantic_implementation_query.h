#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPLEMENTATION_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPLEMENTATION_QUERY_H

#include "semantic/lsp_semantic_query.h"

TZrBool ZrLanguageServer_LspSemanticImplementationQuery_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result);

#endif
