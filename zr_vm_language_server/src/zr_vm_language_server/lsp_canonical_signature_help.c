#include "lsp_canonical_signature_help.h"

#include "zr_vm_parser/semantic_query.h"

TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result) {
    SZrParserSemanticCallQuery query;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(analyzer->semanticContext, position, ZR_NULL, &query) ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext, &query, label, sizeof(label))) {
        return ZR_FALSE;
    }
    return ZrLanguageServer_LspSignatureHelp_PopulateFromLabel(state,
                                                               analyzer,
                                                               label,
                                                               ZR_NULL,
                                                               argumentNodes,
                                                               ZR_NULL,
                                                               activeParameter,
                                                               result);
}
