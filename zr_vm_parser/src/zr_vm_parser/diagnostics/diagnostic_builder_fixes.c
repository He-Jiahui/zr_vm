#include "zr_vm_parser/diagnostic_builder.h"

#include <string.h>

TZrBool ZrParser_StructuredDiagnostic_SetNoFixReason(
        SZrStructuredDiagnostic *diagnostic,
        EZrDiagnosticNoFixReason reason) {
    if (diagnostic == ZR_NULL ||
        reason <= ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED ||
        reason > ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT ||
        (diagnostic->fixes.isValid && diagnostic->fixes.length > 0U) ||
        (diagnostic->noFixReason != ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED &&
         diagnostic->noFixReason != reason)) {
        return ZR_FALSE;
    }

    diagnostic->noFixReason = reason;
    return ZR_TRUE;
}

TZrBool ZrParser_StructuredDiagnostic_AddFix(
        SZrState *state,
        SZrStructuredDiagnostic *diagnostic,
        const TZrChar *title,
        SZrFileRange editRange,
        const TZrChar *editText,
        EZrDiagnosticFixApplicability applicability) {
    SZrStructuredDiagnosticFix fix;

    if (state == ZR_NULL || diagnostic == ZR_NULL || title == ZR_NULL || editText == ZR_NULL ||
        diagnostic->noFixReason != ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED) {
        return ZR_FALSE;
    }

    if (!diagnostic->fixes.isValid) {
        ZrCore_Array_Init(state,
                          &diagnostic->fixes,
                          sizeof(SZrStructuredDiagnosticFix),
                          ZR_PARSER_INITIAL_CAPACITY_TINY);
    }

    memset(&fix, 0, sizeof(fix));
    fix.title = ZrCore_String_Create(state, (TZrNativeString)title, strlen(title));
    fix.editRange = editRange;
    fix.editText = ZrCore_String_Create(state, (TZrNativeString)editText, strlen(editText));
    fix.applicability = applicability;
    if (fix.title == ZR_NULL || fix.editText == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &diagnostic->fixes, &fix);
    return ZR_TRUE;
}
