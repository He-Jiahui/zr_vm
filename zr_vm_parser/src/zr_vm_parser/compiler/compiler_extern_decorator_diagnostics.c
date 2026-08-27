#include "compiler_extern_decorator_diagnostics.h"

TZrBool compiler_extern_report_invalid_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion) {
    SZrStructuredDiagnostic diagnostic;

    if (cs == ZR_NULL || cs->state == ZR_NULL || decoratorNode == ZR_NULL ||
        message == ZR_NULL || cause == ZR_NULL || suggestion == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                decoratorNode->location,
                "invalid_decorator",
                message,
                cause,
                suggestion) ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, message, decoratorNode->location);
        return ZR_FALSE;
    }
    ZrParser_Compiler_StructuredError(cs, &diagnostic);
    return ZR_FALSE;
}
