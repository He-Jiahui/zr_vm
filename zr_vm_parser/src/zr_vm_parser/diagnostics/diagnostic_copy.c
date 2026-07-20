#include "zr_vm_parser/diagnostic_builder.h"

TZrBool ZrParser_StructuredDiagnostic_Copy(
        SZrState *state,
        SZrStructuredDiagnostic *outDiagnostic,
        const SZrStructuredDiagnostic *source) {
    const TZrChar *code;
    const TZrChar *message;
    const TZrChar *cause;
    const TZrChar *suggestion;
    TZrSize index;

    if (outDiagnostic != ZR_NULL) {
        ZrParser_StructuredDiagnostic_Init(outDiagnostic);
    }
    if (state == ZR_NULL || outDiagnostic == ZR_NULL || source == ZR_NULL ||
        source->code == ZR_NULL || source->message == ZR_NULL) {
        return ZR_FALSE;
    }
    code = ZrCore_String_GetNativeString(source->code);
    message = ZrCore_String_GetNativeString(source->message);
    cause = source->cause != ZR_NULL
                    ? ZrCore_String_GetNativeString(source->cause)
                    : ZR_NULL;
    suggestion = source->suggestion != ZR_NULL
                         ? ZrCore_String_GetNativeString(source->suggestion)
                         : ZR_NULL;
    if (code == ZR_NULL || message == ZR_NULL ||
        !ZrParser_DiagnosticBuilder_Build(
                state,
                outDiagnostic,
                source->severity,
                source->location,
                code,
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }
    if (source->descriptorId != 0U) {
        outDiagnostic->descriptorId = source->descriptorId;
    }

    for (index = 0U;
         source->relatedInformation.isValid &&
         index < source->relatedInformation.length;
         index++) {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&source->relatedInformation, index);
        const TZrChar *relatedMessage =
                related != ZR_NULL && related->message != ZR_NULL
                        ? ZrCore_String_GetNativeString(related->message)
                        : ZR_NULL;

        if (related == ZR_NULL || relatedMessage == ZR_NULL ||
            !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                    state,
                    outDiagnostic,
                    related->location,
                    relatedMessage)) {
            ZrParser_StructuredDiagnostic_Free(state, outDiagnostic);
            return ZR_FALSE;
        }
    }
    for (index = 0U;
         source->fixes.isValid && index < source->fixes.length;
         index++) {
        const SZrStructuredDiagnosticFix *fix =
                (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
                        (SZrArray *)&source->fixes, index);
        const TZrChar *title = fix != ZR_NULL && fix->title != ZR_NULL
                                      ? ZrCore_String_GetNativeString(fix->title)
                                      : ZR_NULL;
        const TZrChar *editText = fix != ZR_NULL && fix->editText != ZR_NULL
                                         ? ZrCore_String_GetNativeString(fix->editText)
                                         : ZR_NULL;

        if (fix == ZR_NULL || title == ZR_NULL || editText == ZR_NULL ||
            !ZrParser_StructuredDiagnostic_AddFix(
                    state,
                    outDiagnostic,
                    title,
                    fix->editRange,
                    editText,
                    fix->applicability)) {
            ZrParser_StructuredDiagnostic_Free(state, outDiagnostic);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
