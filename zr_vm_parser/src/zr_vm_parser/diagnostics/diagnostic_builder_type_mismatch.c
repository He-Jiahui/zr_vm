#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/diagnostic_messages.h"
#include "zr_vm_parser/diagnostic_registry.h"

#include <stdio.h>

TZrBool ZrParser_DiagnosticBuilder_BuildTypeMismatchDetailed(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *expectedType,
        const TZrChar *actualType,
        const SZrFileRange *expectedTypeLocation,
        const TZrChar *conversionHint) {
    TZrChar cause[ZR_PARSER_TEXT_BUFFER_LENGTH];
    TZrChar fixText[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH * 2];
    TZrChar fixTitle[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH * 2];
    TZrChar message[ZR_PARSER_TEXT_BUFFER_LENGTH];
    TZrChar suggestion[ZR_PARSER_TEXT_BUFFER_LENGTH];
    const SZrDiagnosticDescriptor *descriptor;
    const TZrChar *messageFormat;

    if (state == ZR_NULL || out == ZR_NULL || expectedType == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    descriptor = ZrParser_DiagnosticRegistry_FindByCode("type_mismatch");
    messageFormat = descriptor != ZR_NULL
                            ? ZrParser_DiagnosticMessages_Resolve(
                                      ZR_DIAGNOSTIC_LOCALE_ENGLISH,
                                      descriptor->messageFormatKey)
                            : ZR_NULL;
    snprintf(message,
             sizeof(message),
             messageFormat != ZR_NULL ? messageFormat : "Expected '%s' but found '%s'",
             expectedType,
             actualType);
    snprintf(cause,
             sizeof(cause),
             "Type '%s' cannot be implicitly converted to '%s'.",
             actualType,
             expectedType);
    if (conversionHint != ZR_NULL) {
        snprintf(suggestion,
                 sizeof(suggestion),
                 "Add an explicit cast to '%s' or change the declared type.",
                 conversionHint);
    } else {
        snprintf(suggestion,
                 sizeof(suggestion),
                 "Change the expression or declaration so their types match.");
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "type_mismatch",
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }
    if (expectedTypeLocation != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                out,
                *expectedTypeLocation,
                "Expected type is declared here")) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    if (conversionHint == ZR_NULL) {
        return ZR_TRUE;
    }

    snprintf(fixTitle, sizeof(fixTitle), "Cast value to '%s'", conversionHint);
    snprintf(fixText, sizeof(fixText), "<%s> <expression>", conversionHint);
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                fixTitle,
                location,
                fixText,
                ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
