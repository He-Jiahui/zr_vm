#include "zr_vm_parser/diagnostic_builder.h"

#include <stdio.h>
#include <string.h>

static SZrString *structured_diagnostic_create_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

void ZrParser_StructuredDiagnostic_Init(SZrStructuredDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->severity = ZR_STRUCTURED_DIAGNOSTIC_ERROR;
}

void ZrParser_StructuredDiagnostic_Free(SZrState *state, SZrStructuredDiagnostic *diagnostic) {
    ZR_UNUSED_PARAMETER(state);

    if (diagnostic == ZR_NULL) {
        return;
    }

    diagnostic->code = ZR_NULL;
    diagnostic->message = ZR_NULL;
    diagnostic->cause = ZR_NULL;
    diagnostic->suggestion = ZR_NULL;
}

TZrBool ZrParser_DiagnosticBuilder_Build(SZrState *state,
                                         SZrStructuredDiagnostic *out,
                                         EZrStructuredDiagnosticSeverity severity,
                                         SZrFileRange location,
                                         const TZrChar *code,
                                         const TZrChar *message,
                                         const TZrChar *cause,
                                         const TZrChar *suggestion) {
    if (state == ZR_NULL || out == ZR_NULL || code == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_StructuredDiagnostic_Init(out);
    out->severity = severity;
    out->location = location;
    out->code = structured_diagnostic_create_string(state, code);
    out->message = structured_diagnostic_create_string(state, message);
    out->cause = cause != ZR_NULL ? structured_diagnostic_create_string(state, cause) : ZR_NULL;
    out->suggestion = suggestion != ZR_NULL ? structured_diagnostic_create_string(state, suggestion) : ZR_NULL;

    if (out->code == ZR_NULL || out->message == ZR_NULL ||
        (cause != ZR_NULL && out->cause == ZR_NULL) ||
        (suggestion != ZR_NULL && out->suggestion == ZR_NULL)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingExpressionAfterAssignment(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_expression_after_assignment",
            "Missing expression after '='",
            "The assignment operator starts an initializer, but the statement ends before a value expression appears.",
            "Add an expression before ';' or remove the '=' initializer.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingRightOperand(SZrState *state,
                                                            SZrStructuredDiagnostic *out,
                                                            SZrFileRange location,
                                                            const TZrChar *operatorText) {
    TZrChar message[128];
    TZrChar cause[192];
    TZrChar suggestion[192];

    snprintf(message,
             sizeof(message),
             "Missing expression after '%s'",
             operatorText != ZR_NULL ? operatorText : "<operator>");
    snprintf(cause,
             sizeof(cause),
             "The operator '%s' requires a right-hand expression, but the expression ended first.",
             operatorText != ZR_NULL ? operatorText : "<operator>");
    snprintf(suggestion,
             sizeof(suggestion),
             "Add the right-hand expression after '%s' or remove the operator.",
             operatorText != ZR_NULL ? operatorText : "<operator>");

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_right_operand",
            message,
            cause,
            suggestion);
}

TZrBool ZrParser_DiagnosticBuilder_BuildWeakUpgrade(SZrState *state,
                                                    SZrStructuredDiagnostic *out,
                                                    SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "weak_value_requires_upgrade",
            "Weak value must be upgraded before it can be borrowed",
            "A %weak value does not keep its owner alive, so it cannot satisfy a %borrowed use directly.",
            "Use %upgrade(...) and handle the nullable upgraded owner before borrowing it.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildBorrowEscape(SZrState *state,
                                                     SZrStructuredDiagnostic *out,
                                                     SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "borrow_escape",
            "Borrowed value cannot escape its owner",
            "The expression uses %borrow(...), which creates a temporary borrow tied to the source owner.",
            "Return the owner or keep the borrow inside the current scope.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildLoanEscape(SZrState *state,
                                                   SZrStructuredDiagnostic *out,
                                                   SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "loan_escape",
            "Loaned value cannot escape its owner",
            "The expression uses %loan(...), which creates a temporary loan tied to the source owner.",
            "Return the owner or keep the loan inside the current scope.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildOwnerToPlainEscape(SZrState *state,
                                                           SZrStructuredDiagnostic *out,
                                                           SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "owner_to_plain_escape",
            "Owned value cannot flow into a plain GC value implicitly",
            "A %unique or %shared value owns deterministic cleanup; assigning it to a plain value would drop ownership semantics.",
            "Use %detach(...) when intentionally converting an owned value to a plain value.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildOwnershipMismatch(SZrState *state,
                                                         SZrStructuredDiagnostic *out,
                                                         SZrFileRange location,
                                                         const TZrChar *expectedType,
                                                         const TZrChar *actualType) {
    TZrChar cause[256];
    TZrChar suggestion[256];

    snprintf(cause,
             sizeof(cause),
             "Actual value has type %s, but the target requires %s.",
             actualType != ZR_NULL ? actualType : "unknown",
             expectedType != ZR_NULL ? expectedType : "unknown");
    snprintf(suggestion,
             sizeof(suggestion),
             "Provide a %s value, use an ownership builtin, or change the target annotation to match.",
             expectedType != ZR_NULL ? expectedType : "matching ownership");

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "ownership_mismatch",
            "Ownership qualifier mismatch",
            cause,
            suggestion);
}
