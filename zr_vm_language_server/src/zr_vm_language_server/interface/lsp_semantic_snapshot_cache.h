#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_CACHE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_SNAPSHOT_CACHE_H

#include "zr_vm_language_server/lsp_interface.h"

typedef struct SZrLspSemanticSnapshotCache SZrLspSemanticSnapshotCache;

SZrLspSemanticSnapshotCache *ZrLanguageServer_LspSemanticSnapshotCache_New(
        SZrState *state);
void ZrLanguageServer_LspSemanticSnapshotCache_Free(
        SZrState *state,
        SZrLspContext *context);
void ZrLanguageServer_LspSemanticSnapshotCache_RemoveUri(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
TZrBool ZrLanguageServer_LspSemanticSnapshotCache_Capture(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrFileVersion *fileVersion,
        SZrSemanticAnalyzer *currentAnalyzer,
        SZrAstNode *retainedAst,
        TZrBool preserveScopedQueryAnalyzer);

#endif
