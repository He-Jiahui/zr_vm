#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_COMPLETION_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_COMPLETION_H

#include "zr_vm_language_server/semantic_analyzer.h"

TZrBool ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        SZrFileRange position,
        SZrArray *result);

#endif
