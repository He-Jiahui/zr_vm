#include "zr_vm_parser/diagnostic_builder.h"

#include <stdio.h>

static TZrBool diagnostic_builder_build_without_fix(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *code,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion,
        EZrDiagnosticNoFixReason reason) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                code,
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(out, reason)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingExpressionAfterAssignment(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_expression_after_assignment",
            "Missing expression after '='",
            "The assignment operator starts an initializer, but the statement ends before a value expression appears.",
            "Add an expression before ';' or remove the '=' initializer.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingRightOperand(
        SZrState *state,
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

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_right_operand",
            message,
            cause,
            suggestion,
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingCondition(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *statementKind) {
    const TZrChar *kind = statementKind != ZR_NULL ? statementKind : "control";
    TZrChar message[128];
    TZrChar cause[224];

    snprintf(message,
             sizeof(message),
             "Missing condition inside '%s'",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The '%s' statement opened condition parentheses but closed them before a condition expression appeared.",
             kind);

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_condition",
            message,
            cause,
            "Add a boolean expression between '(' and ')' or remove the control statement.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingTestNameClose(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_test_name_close",
            "Missing closing ')' in test declaration name",
            "The test declaration started a name with '(', but the parser reached the test body before a closing ')' appeared.",
            "Insert ')' after the test declaration name before the test body.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingMemberName(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_member_name",
            "Missing member name after '.'",
            "The member-access operator was written, but no property, method, or field name follows it.",
            "Add a member name after '.' or remove the member access.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "array_element_assignment",
            "Array element cannot be an assignment expression",
            "Array literals collect value expressions. An assignment inside the element list would mutate state while the parser is still reading the literal.",
            "Move the assignment into a statement before the array literal, then reference the assigned value from the array.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalConsequent(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_conditional_consequent",
            "Missing expression after '?' in conditional expression",
            "The conditional operator selected a consequent branch with '?', but no expression appears before ':'.",
            "Add the consequent expression between '?' and ':'.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalColon(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        SZrFileRange fixLocation,
        TZrBool hasAlternateExpression) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_conditional_colon",
                "Missing ':' in conditional expression",
                "The conditional expression has a condition and consequent branch, but the alternate branch separator is missing.",
                "Insert ':' between the consequent and alternate expressions.")) {
        return ZR_FALSE;
    }

    if (!hasAlternateExpression) {
        if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                    out, ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT)) {
            ZrParser_StructuredDiagnostic_Free(state, out);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ':'",
                fixLocation,
                ":",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "missing_conditional_alternate",
            "Missing expression after ':' in conditional expression",
            "The conditional operator opened an alternate branch with ':', but no expression follows it.",
            "Add the alternate expression after ':'.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}
