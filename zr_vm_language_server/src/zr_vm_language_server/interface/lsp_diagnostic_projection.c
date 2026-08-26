#include "zr_vm_language_server/lsp_interface.h"

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
