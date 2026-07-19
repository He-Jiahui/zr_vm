#include "semantic/semantic_analyzer_duplicate_diagnostics.h"

#include <stdio.h>

static const TZrChar *semantic_duplicate_name_text(SZrString *name) {
    if (name == ZR_NULL) {
        return ZR_NULL;
    }
    return name->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(name)
                   : ZrCore_String_GetNativeString(name);
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ReportDuplicateType(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrString *name,
        SZrFileRange location,
        const SZrFileRange *previousLocation) {
    const TZrChar *nameText;
    SZrStructuredDiagnostic structured;
    SZrDiagnostic *diagnostic;
    TZrChar message[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = semantic_duplicate_name_text(name);
    if (nameText != ZR_NULL) {
        snprintf(message,
                 sizeof(message),
                 "Type name '%s' is already declared in this context",
                 nameText);
    } else {
        snprintf(message, sizeof(message), "Type name is already declared in this context");
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                &structured,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "duplicate_type",
                message,
                "Another type binding with the same name is already visible in this context.",
                "Rename this type or remove the duplicate declaration.")) {
        return ZR_FALSE;
    }
    if (previousLocation != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                &structured,
                *previousLocation,
                "Type was first declared here")) {
        ZrParser_StructuredDiagnostic_Free(state, &structured);
        return ZR_FALSE;
    }

    diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, &structured);
    ZrParser_StructuredDiagnostic_Free(state, &structured);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    return ZR_TRUE;
}
