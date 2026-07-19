#include "zr_vm_parser/diagnostic_builder.h"

#include <string.h>

TZrBool ZrParser_StructuredDiagnostic_AddFix(
        SZrState *state,
        SZrStructuredDiagnostic *diagnostic,
        const TZrChar *title,
        SZrFileRange editRange,
        const TZrChar *editText,
        EZrDiagnosticFixApplicability applicability) {
    SZrStructuredDiagnosticFix fix;

    if (state == ZR_NULL || diagnostic == ZR_NULL || title == ZR_NULL || editText == ZR_NULL) {
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
