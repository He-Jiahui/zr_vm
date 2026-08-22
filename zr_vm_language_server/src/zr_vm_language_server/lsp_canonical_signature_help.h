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

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_TryGetResolvedCallReferenceRange(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrFileRange *result);

/* Detects a local direct call whose canonical callable payload is unavailable. */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_HasUnavailableLocalCall(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position);

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveReceiverHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result);

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveExternalCallableHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SIGNATURE_HELP_H */
