#include "zr_vm_language_server/semantic_analyzer.h"

#include <string.h>

TZrBool ZrLanguageServer_Diagnostic_AddFix(
        SZrState *state,
        SZrDiagnostic *diagnostic,
        const SZrStructuredDiagnosticFix *structuredFix) {
    SZrDiagnosticFix fix;
    const TZrChar *title;
    const TZrChar *editText;

    if (state == ZR_NULL || diagnostic == ZR_NULL || structuredFix == ZR_NULL ||
        structuredFix->title == ZR_NULL || structuredFix->editText == ZR_NULL) {
        return ZR_FALSE;
    }

    title = ZrCore_String_GetNativeString(structuredFix->title);
    editText = ZrCore_String_GetNativeString(structuredFix->editText);
    if (title == ZR_NULL || editText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!diagnostic->fixes.isValid) {
        ZrCore_Array_Init(state,
                          &diagnostic->fixes,
                          sizeof(SZrDiagnosticFix),
                          ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    memset(&fix, 0, sizeof(fix));
    fix.title = ZrCore_String_Create(state, (TZrNativeString)title, strlen(title));
    fix.editRange = structuredFix->editRange;
    fix.editText = ZrCore_String_Create(state, (TZrNativeString)editText, strlen(editText));
    fix.applicability = structuredFix->applicability;
    if (fix.title == ZR_NULL || fix.editText == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &diagnostic->fixes, &fix);
    return ZR_TRUE;
}
