#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H

#include "lsp_signature_help_internal.h"

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H */
