#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_SIGNATURE_HELP_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_SIGNATURE_HELP_H

#include "lsp_signature_help_internal.h"

typedef enum EZrLspExternalCallableSignatureStatus {
    ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_NOT_EXTERNAL = 0,
    ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_RESOLVED = 1,
    ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_UNAVAILABLE = 2
} EZrLspExternalCallableSignatureStatus;

EZrLspExternalCallableSignatureStatus
ZrLanguageServer_LspExternalCallableSignatureHelp_Resolve(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange calleeRange,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result);

#endif
