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

TZrBool ZrParser_DiagnosticBuilder_BuildMissingCondition(SZrState *state,
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

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_condition",
            message,
            cause,
            "Add a boolean expression between '(' and ')' or remove the control statement.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location,
                                                                   const TZrChar *declarationKind) {
    const TZrChar *kind = declarationKind != ZR_NULL ? declarationKind : "declaration";
    TZrChar message[160];
    TZrChar cause[256];
    TZrChar suggestion[192];

    snprintf(message,
             sizeof(message),
             "Missing '{' to start %s body",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The %s header was parsed, but the parser reached another token before the body-opening '{'.",
             kind);
    snprintf(suggestion,
             sizeof(suggestion),
             "Insert '{' after the %s header or finish the declaration body.",
             kind);

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_declaration_body_open",
            message,
            cause,
            suggestion);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen(SZrState *state,
                                                                 SZrStructuredDiagnostic *out,
                                                                 SZrFileRange location,
                                                                 const TZrChar *statementKind) {
    const TZrChar *kind = statementKind != ZR_NULL ? statementKind : "statement";
    TZrChar message[160];
    TZrChar cause[256];
    TZrChar suggestion[192];

    snprintf(message,
             sizeof(message),
             "Missing '{' to start %s body",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The %s header was parsed, but the parser reached another token before the body-opening '{'.",
             kind);
    snprintf(suggestion,
             sizeof(suggestion),
             "Insert '{' after the %s header or wrap the statement body in braces.",
             kind);

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_statement_body_open",
            message,
            cause,
            suggestion);
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingBlockClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_block_close",
            "Missing closing '}' for block",
            "The block started with '{', but the parser reached the end of input before a closing '}' appeared.",
            "Insert '}' to close the block before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose(SZrState *state,
                                                                 SZrStructuredDiagnostic *out,
                                                                 SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_catch_pattern_close",
            "Missing closing ')' in catch pattern",
            "The catch clause started a pattern with '(', but the parser reached the catch body before a closing ')' appeared.",
            "Insert ')' after the catch pattern before the catch body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_using_resource_close",
            "Missing closing ')' in using resource",
            "The using statement started a resource expression with '(', but the parser reached the using body before a closing ')' appeared.",
            "Insert ')' after the using resource before the using body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose(SZrState *state,
                                                              SZrStructuredDiagnostic *out,
                                                              SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_for_header_close",
            "Missing closing ')' in for header",
            "The for statement header started with '(', but the parser reached the loop body before a closing ')' appeared.",
            "Insert ')' after the for header before the loop body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_for_header_separator",
            "Missing ';' between for header clauses",
            "A traditional for header requires ';' between initializer, condition, and step clauses, but another clause started first.",
            "Insert ';' between the for header clauses.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_foreach_header_close",
            "Missing closing ')' in foreach header",
            "The foreach header started with '(', but the parser reached the loop body before a closing ')' appeared.",
            "Insert ')' after the foreach iterable before the loop body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_foreach_in_keyword",
            "Missing 'in' in foreach header",
            "The foreach header has a pattern, but the parser did not find 'in' before the iterable expression.",
            "Insert 'in' between the foreach pattern and iterable expression.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose(SZrState *state,
                                                                     SZrStructuredDiagnostic *out,
                                                                     SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_switch_case_header_close",
            "Missing closing ')' in switch case header",
            "The switch case header started with '(', but the parser reached the case body before a closing ')' appeared.",
            "Insert ')' after the switch case expression before the case body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_switch_body_close",
            "Missing closing '}' for switch body",
            "The switch body started with '{', but the parser reached the end of input before a closing '}' appeared.",
            "Insert '}' to close the switch body before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingExternSpecClose(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_extern_spec_close",
            "Missing closing ')' in extern block spec",
            "The extern block started a library spec with '(', but the parser reached the extern block body before a closing ')' appeared.",
            "Insert ')' after the extern block spec before the extern block body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingTestNameClose(SZrState *state,
                                                             SZrStructuredDiagnostic *out,
                                                             SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_test_name_close",
            "Missing closing ')' in test declaration name",
            "The test declaration started a name with '(', but the parser reached the test body before a closing ')' appeared.",
            "Insert ')' after the test declaration name before the test body.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionClose(SZrState *state,
                                                              SZrStructuredDiagnostic *out,
                                                              SZrFileRange location,
                                                              const TZrChar *statementKind) {
    const TZrChar *kind = statementKind != ZR_NULL ? statementKind : "control";
    TZrChar message[128];
    TZrChar cause[224];

    snprintf(message,
             sizeof(message),
             "Missing ')' after '%s' condition",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The '%s' condition started with '(', but the parser reached the block before a closing ')' appeared.",
             kind);

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_condition_close",
            message,
            cause,
            "Insert ')' after the condition expression before the block.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingMemberName(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_member_name",
            "Missing member name after '.'",
            "The member-access operator was written, but no property, method, or field name follows it.",
            "Add a member name after '.' or remove the member access.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingIndexClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_index_close",
            "Missing closing ']' in index access",
            "The computed member access started with '[', but the parser reached another token before a closing ']' appeared.",
            "Insert ']' after the index expression before continuing the member access.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingCallClose(SZrState *state,
                                                         SZrStructuredDiagnostic *out,
                                                         SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_call_close",
            "Missing closing ')' in function call",
            "The function call started an argument list with '(', but the parser reached another token before a closing ')' appeared.",
            "Insert ')' after the last argument before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingParameterListClose(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_parameter_list_close",
            "Missing closing ')' in function declaration parameters",
            "The function declaration started a parameter list with '(', but the parser reached another token before a closing ')' appeared.",
            "Insert ')' after the parameter list before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingGroupClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_group_close",
            "Missing closing ')' in grouped expression",
            "The grouped expression started with '(', but the parser reached another token before a closing ')' appeared.",
            "Insert ')' after the grouped expression before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "array_element_assignment",
            "Array element cannot be an assignment expression",
            "Array literals collect value expressions. An assignment inside the element list would mutate state while the parser is still reading the literal.",
            "Move the assignment into a statement before the array literal, then reference the assigned value from the array.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_array_close",
            "Missing closing ']' in array literal",
            "The array literal started with '[', but the parser reached another token before a closing ']' appeared.",
            "Insert ']' after the last array element before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator(SZrState *state,
                                                                     SZrStructuredDiagnostic *out,
                                                                     SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_array_element_separator",
            "Missing separator between array elements",
            "The array literal has another element expression immediately after the previous element.",
            "Insert ',' or ';' between array elements.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectClose(SZrState *state,
                                                           SZrStructuredDiagnostic *out,
                                                           SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_object_close",
            "Missing closing '}' in object literal",
            "The object literal started with '{', but the parser reached another token before a closing '}' appeared.",
            "Insert '}' after the last object property before continuing.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose(SZrState *state,
                                                                      SZrStructuredDiagnostic *out,
                                                                      SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_object_computed_key_close",
            "Missing closing ']' in computed object key",
            "The computed object key started with '[', but the parser reached another token before the key expression closed.",
            "Insert ']' after the computed key expression before ':'.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectPropertyColon(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_object_property_colon",
            "Missing ':' after object property key",
            "Object literal properties require ':' between the key and the value expression.",
            "Insert ':' between the property key and value expression.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectPropertySeparator(SZrState *state,
                                                                       SZrStructuredDiagnostic *out,
                                                                       SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_object_property_separator",
            "Missing separator between object properties",
            "The object literal has another property key immediately after the previous property's value.",
            "Insert ',' or ';' between object properties.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalConsequent(SZrState *state,
                                                                     SZrStructuredDiagnostic *out,
                                                                     SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_conditional_consequent",
            "Missing expression after '?' in conditional expression",
            "The conditional operator selected a consequent branch with '?', but no expression appears before ':'.",
            "Add the consequent expression between '?' and ':'.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalColon(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_conditional_colon",
            "Missing ':' in conditional expression",
            "The conditional expression has a condition and consequent branch, but the alternate branch separator is missing.",
            "Insert ':' between the consequent and alternate expressions.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_conditional_alternate",
            "Missing expression after ':' in conditional expression",
            "The conditional operator opened an alternate branch with ':', but no expression follows it.",
            "Add the alternate expression after ':'.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location,
                                                                  const TZrChar *statementKind) {
    const TZrChar *kind = statementKind != ZR_NULL ? statementKind : "statement";
    TZrChar message[160];
    TZrChar cause[256];
    TZrChar suggestion[192];

    snprintf(message,
             sizeof(message),
             "Missing ';' after %s statement",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The %s statement ended before a ';' terminator, so the next token is being read as part of the same statement.",
             kind);
    snprintf(suggestion,
             sizeof(suggestion),
             "Insert ';' after the %s statement before the next statement.",
             kind);

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "missing_statement_semicolon",
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
