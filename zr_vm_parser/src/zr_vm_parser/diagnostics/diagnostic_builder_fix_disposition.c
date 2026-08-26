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

TZrBool ZrParser_DiagnosticBuilder_BuildUsingBinderInvalid(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "using_binder_invalid",
            "using_binder_invalid: invalid using guard binder",
            "A using guard binder must be an import binding name or a union destructuring pattern.",
            "Use `using (let name = import(...))` for plugin guards, `using (let [value]: Union.Variant = resource)` for tuple variants, or `using (let {local: field}: Union.Variant = resource)` for struct variants.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildImportPathNotConstant(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *directiveName) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrChar cause[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrChar suggestion[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *name = directiveName != ZR_NULL ? directiveName : "import";

    snprintf(message,
             sizeof(message),
             "%s(...) requires a string literal module path",
             name);
    snprintf(cause,
             sizeof(cause),
             "The module path inside %s(...) must be known at parse time; variables and expressions cannot participate in module signature binding.",
             name);
    snprintf(suggestion,
             sizeof(suggestion),
             "Use `%s(\"zr.module\")`; static import paths must be string literals in parentheses.",
             name);

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "import_path_not_constant",
            message,
            cause,
            suggestion,
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternShapeMismatch(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "pattern_shape_mismatch",
            message != ZR_NULL ? message : "Union pattern destructuring shape does not match variant payload shape",
            cause != ZR_NULL
                ? cause
                : "The pattern uses a destructuring shape that does not match the selected union variant payload.",
            suggestion != ZR_NULL
                ? suggestion
                : "Use tuple destructuring for positional variants and object destructuring for named-field variants.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternUnknownField(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *fieldName,
        const TZrChar *availableFields) {
    TZrChar message[256];
    TZrChar cause[512];
    TZrChar suggestion[512];

    snprintf(message,
             sizeof(message),
             "Unknown union pattern field '%s'",
             fieldName != ZR_NULL ? fieldName : "<unknown>");
    snprintf(cause,
             sizeof(cause),
             "The selected union variant does not declare a payload field named '%s'.",
             fieldName != ZR_NULL ? fieldName : "<unknown>");
    snprintf(suggestion,
             sizeof(suggestion),
             "Use one of the declared payload fields: %s.",
             availableFields != ZR_NULL && availableFields[0] != '\0' ? availableFields : "<none>");

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "pattern_unknown_field",
            message,
            cause,
            suggestion,
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternArityMismatch(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        TZrSize expectedCount,
        TZrSize actualCount,
        const TZrChar *availableFields) {
    TZrChar message[256];
    TZrChar cause[256];
    TZrChar suggestion[512];

    snprintf(message,
             sizeof(message),
             "Union pattern arity mismatch: expects %u binding(s)",
             (unsigned)expectedCount);
    snprintf(cause,
             sizeof(cause),
             "The selected union variant expects %u payload binding(s), but got %u.",
             (unsigned)expectedCount,
             (unsigned)actualCount);
    if (availableFields != ZR_NULL && availableFields[0] != '\0') {
        snprintf(suggestion,
                 sizeof(suggestion),
                 "Use object destructuring and bind exactly the named payload fields: %s.",
                 availableFields);
    } else {
        snprintf(suggestion,
                 sizeof(suggestion),
                 "Use tuple destructuring with exactly %u binding(s).",
                 (unsigned)expectedCount);
    }

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "pattern_arity_mismatch",
            message,
            cause,
            suggestion,
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternVariantMismatch(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *annotationUnionName,
        const TZrChar *variantName,
        const TZrChar *resourceUnionName) {
    TZrChar selectedVariant[256];
    TZrChar message[384];
    TZrChar cause[512];
    TZrChar suggestion[512];

    snprintf(selectedVariant,
             sizeof(selectedVariant),
             "%s.%s",
             annotationUnionName != ZR_NULL ? annotationUnionName : "<unknown>",
             variantName != ZR_NULL ? variantName : "<unknown>");
    snprintf(message,
             sizeof(message),
             "Union pattern variant '%s' does not match the resource union type",
             selectedVariant);
    snprintf(cause,
             sizeof(cause),
             "The using resource has union type '%s', but the pattern annotation selects '%s'.",
             resourceUnionName != ZR_NULL ? resourceUnionName : "<unknown>",
             selectedVariant);
    snprintf(suggestion,
             sizeof(suggestion),
             "Use a variant declared on '%s' or change the resource expression to match '%s'.",
             resourceUnionName != ZR_NULL ? resourceUnionName : "<unknown>",
             selectedVariant);

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "pattern_variant_mismatch",
            message,
            cause,
            suggestion,
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

TZrBool ZrParser_DiagnosticBuilder_BuildWeakWake(SZrState *state,
                                                 SZrStructuredDiagnostic *out,
                                                 SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "weak_value_requires_wake",
            "Weak value must be woken before it can be borrowed",
            "A Weak<T> value does not keep its owner alive, so it cannot satisfy a ref readonly T use directly.",
            "Call wake(weak) and handle the nullable shared owner before borrowing it.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildLegacyOwnershipTypeSyntaxWarning(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *legacyQualifier,
        const TZrChar *wrapperName) {
    TZrChar message[192];
    TZrChar suggestion[192];

    snprintf(message,
             sizeof(message),
             "Legacy ownership type syntax '%s T' is deprecated",
             legacyQualifier != ZR_NULL ? legacyQualifier : "%ownership");
    snprintf(suggestion,
             sizeof(suggestion),
             "Write %s<T> instead.",
             wrapperName != ZR_NULL ? wrapperName : "Owner");

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_WARNING,
                location,
                "legacy_ownership_type_syntax",
                message,
                "Ownership qualifiers are now intrinsic generic types; the legacy percent-prefixed type form is kept only as migration syntax.",
                suggestion)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                out, ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildBorrowEscape(SZrState *state,
                                                     SZrStructuredDiagnostic *out,
                                                     SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "borrow_escape",
            "Borrowed value cannot escape its owner",
            "The expression uses ref, which creates a temporary borrow tied to the source owner.",
            "Return the owner or keep the borrow inside the current scope.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildLoanEscape(SZrState *state,
                                                   SZrStructuredDiagnostic *out,
                                                   SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "loan_escape",
            "Loaned value cannot escape its owner",
            "The expression uses ref, which creates a temporary loan tied to the source owner.",
            "Return the owner or keep the loan inside the current scope.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildOwnerToPlainEscape(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location) {
    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "owner_to_plain_escape",
            "Owned value cannot flow into a plain GC value implicitly",
            "A Unique<T> or Shared<T> value owns deterministic cleanup; assigning it to a plain value would drop ownership semantics.",
            "Keep the ownership wrapper in the target type or revise the ownership transfer explicitly.",
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}

TZrBool ZrParser_DiagnosticBuilder_BuildOwnershipMismatch(
        SZrState *state,
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

    return diagnostic_builder_build_without_fix(
            state,
            out,
            location,
            "ownership_mismatch",
            "Ownership qualifier mismatch",
            cause,
            suggestion,
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION);
}
