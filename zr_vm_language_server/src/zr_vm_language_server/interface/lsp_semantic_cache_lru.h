#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CACHE_LRU_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CACHE_LRU_H

#include "zr_vm_language_server/lsp_interface.h"

typedef struct SZrLspSemanticCacheLru SZrLspSemanticCacheLru;

SZrLspSemanticCacheLru *ZrLanguageServer_LspSemanticCacheLru_New(
        SZrState *state);
void ZrLanguageServer_LspSemanticCacheLru_Free(
        SZrState *state,
        SZrLspContext *context);
void ZrLanguageServer_LspSemanticCacheLru_Touch(
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer);
void ZrLanguageServer_LspSemanticCacheLru_Enforce(
        SZrState *state,
        SZrLspContext *context);
TZrBool ZrLanguageServer_LspSemanticCacheLru_SetLimit(
        SZrState *state,
        SZrLspContext *context,
        TZrSize limitBytes);
TZrBool ZrLanguageServer_LspSemanticCacheLru_GetInfo(
        const SZrLspContext *context,
        SZrLspSemanticCacheStorageInfo *outInfo);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CACHE_LRU_H
