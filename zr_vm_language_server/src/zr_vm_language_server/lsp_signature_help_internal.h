#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SIGNATURE_HELP_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SIGNATURE_HELP_INTERNAL_H

#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_internal.h"

TZrBool ZrLanguageServer_LspSignatureHelp_PopulateFromLabel(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const TZrChar *labelText,
        SZrAstNodeArray *params,
        SZrAstNodeArray *argumentNodes,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_SIGNATURE_HELP_INTERNAL_H */
