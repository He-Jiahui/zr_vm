#include "semantic_analyzer_internal.h"

#include <string.h>

#include "zr_vm_parser/diagnostic_registry.h"

static EZrDiagnosticSeverity semantic_diagnostic_severity_from_structured(
        EZrStructuredDiagnosticSeverity severity) {
    switch (severity) {
        case ZR_STRUCTURED_DIAGNOSTIC_WARNING:
            return ZR_DIAGNOSTIC_WARNING;
        case ZR_STRUCTURED_DIAGNOSTIC_INFO:
            return ZR_DIAGNOSTIC_INFO;
        case ZR_STRUCTURED_DIAGNOSTIC_HINT:
            return ZR_DIAGNOSTIC_HINT;
        case ZR_STRUCTURED_DIAGNOSTIC_ERROR:
        default:
            return ZR_DIAGNOSTIC_ERROR;
    }
}

static SZrString *semantic_diagnostic_clone_string(SZrState *state, SZrString *value) {
    TZrNativeString text;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, text, ZrCore_String_GetByteLength(value));
}

static TZrBool semantic_diagnostic_copy_descriptor(
        SZrState *state,
        SZrDiagnostic *diagnostic,
        const SZrStructuredDiagnostic *structured) {
    const SZrDiagnosticDescriptor *descriptor;

    diagnostic->descriptorId = structured->descriptorId;
    diagnostic->noFixReason = structured->noFixReason;
    descriptor = ZrParser_DiagnosticRegistry_FindById(structured->descriptorId);
    if (descriptor == ZR_NULL || descriptor->helpUri == ZR_NULL) {
        return ZR_TRUE;
    }

    diagnostic->codeDescriptionHref = ZrCore_String_Create(
            state,
            (TZrNativeString)descriptor->helpUri,
            strlen(descriptor->helpUri));
    return diagnostic->codeDescriptionHref != ZR_NULL;
}

SZrDiagnostic *ZrLanguageServer_Diagnostic_FromStructured(
        SZrState *state,
        const SZrStructuredDiagnostic *structured) {
    const TZrChar *messageText;
    const TZrChar *codeText = ZR_NULL;
    SZrDiagnostic *diagnostic;
    TZrSize index;

    if (state == ZR_NULL || structured == ZR_NULL || structured->message == ZR_NULL) {
        return ZR_NULL;
    }

    messageText = ZrCore_String_GetNativeString(structured->message);
    if (messageText == ZR_NULL) {
        return ZR_NULL;
    }
    if (structured->code != ZR_NULL) {
        codeText = ZrCore_String_GetNativeString(structured->code);
    }

    diagnostic = ZrLanguageServer_Diagnostic_New(
            state,
            semantic_diagnostic_severity_from_structured(structured->severity),
            structured->location,
            messageText,
            codeText);
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }

    if (structured->cause != ZR_NULL) {
        diagnostic->cause = semantic_diagnostic_clone_string(state, structured->cause);
        if (diagnostic->cause == ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, diagnostic);
            return ZR_NULL;
        }
    }
    if (structured->suggestion != ZR_NULL) {
        diagnostic->suggestion = semantic_diagnostic_clone_string(state, structured->suggestion);
        if (diagnostic->suggestion == ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, diagnostic);
            return ZR_NULL;
        }
    }
    if (!semantic_diagnostic_copy_descriptor(state, diagnostic, structured)) {
        ZrLanguageServer_Diagnostic_Free(state, diagnostic);
        return ZR_NULL;
    }
    for (index = 0; structured->fixes.isValid && index < structured->fixes.length; index++) {
        const SZrStructuredDiagnosticFix *fix =
            (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get((SZrArray *)&structured->fixes, index);
        if (fix == ZR_NULL || !ZrLanguageServer_Diagnostic_AddFix(state, diagnostic, fix)) {
            ZrLanguageServer_Diagnostic_Free(state, diagnostic);
            return ZR_NULL;
        }
    }
    for (index = 0;
         structured->relatedInformation.isValid && index < structured->relatedInformation.length;
         index++) {
        const SZrStructuredDiagnosticRelatedInformation *related =
            (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                    (SZrArray *)&structured->relatedInformation,
                    index);
        const TZrChar *relatedMessage =
            related != ZR_NULL && related->message != ZR_NULL
                ? ZrCore_String_GetNativeString(related->message)
                : ZR_NULL;
        if (relatedMessage == ZR_NULL ||
            !ZrLanguageServer_Diagnostic_AddRelatedInformation(
                    state,
                    diagnostic,
                    related->location,
                    relatedMessage)) {
            ZrLanguageServer_Diagnostic_Free(state, diagnostic);
            return ZR_NULL;
        }
    }

    return diagnostic;
}
