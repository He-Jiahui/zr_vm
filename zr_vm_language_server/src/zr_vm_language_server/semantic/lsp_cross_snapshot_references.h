#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CROSS_SNAPSHOT_REFERENCES_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CROSS_SNAPSHOT_REFERENCES_H

#include "semantic/lsp_semantic_query.h"

TZrBool ZrLanguageServer_LspCrossSnapshotReferences_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result);

TZrBool ZrLanguageServer_LspCrossSnapshotReferences_AppendExternal(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result);

TZrBool ZrLanguageServer_LspCrossSnapshotReferences_AppendNativeDeclaration(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        const SZrFileRange *declaration,
        SZrArray *result);

#endif
