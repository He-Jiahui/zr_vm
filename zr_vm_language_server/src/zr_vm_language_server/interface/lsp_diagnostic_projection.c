#include "zr_vm_language_server/lsp_interface.h"

#include "semantic/lsp_optimization_remarks.h"

const TZrChar *ZrLanguageServer_Lsp_DiagnosticNoFixReasonName(
        EZrDiagnosticNoFixReason reason) {
    switch (reason) {
        case ZR_DIAGNOSTIC_NO_FIX_REASON_NOT_APPLICABLE:
            return "not_applicable";
        case ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT:
            return "insufficient_context";
        case ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION:
            return "requires_user_decision";
        case ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT:
            return "unsafe_edit";
        case ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED:
        default:
            return ZR_NULL;
    }
}

TZrBool ZrLanguageServer_Lsp_ProjectOptimizationRemarkRange(
        const struct SZrOptimizationRemark *remark,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspRange *outRange) {
    SZrLspOptimizationRemark projected;
    SZrOptimizationRemarkDiagnostic diagnostic;
    EZrLspOptimizationRemarkResult result;

    if (remark == ZR_NULL || outRange == ZR_NULL) return ZR_FALSE;
    result = ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
            remark, ZR_FALSE, 0u, content, contentLength, &projected,
            &diagnostic);
    if (result != ZR_LSP_OPTIMIZATION_REMARK_OK) return ZR_FALSE;
    *outRange = projected.range;
    return ZR_TRUE;
}
