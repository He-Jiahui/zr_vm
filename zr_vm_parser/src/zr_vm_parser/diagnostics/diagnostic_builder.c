#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/diagnostic_registry.h"

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
    ZrCore_Array_Construct(&diagnostic->relatedInformation);
    ZrCore_Array_Construct(&diagnostic->fixes);
}

void ZrParser_StructuredDiagnostic_Free(SZrState *state, SZrStructuredDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    if (state != ZR_NULL && diagnostic->relatedInformation.isValid) {
        ZrCore_Array_Free(state, &diagnostic->relatedInformation);
    }
    if (state != ZR_NULL && diagnostic->fixes.isValid) {
        ZrCore_Array_Free(state, &diagnostic->fixes);
    }
    diagnostic->code = ZR_NULL;
    diagnostic->message = ZR_NULL;
    diagnostic->cause = ZR_NULL;
    diagnostic->suggestion = ZR_NULL;
}

TZrBool ZrParser_StructuredDiagnostic_AddRelatedInformation(SZrState *state,
                                                            SZrStructuredDiagnostic *diagnostic,
                                                            SZrFileRange location,
                                                            const TZrChar *message) {
    SZrStructuredDiagnosticRelatedInformation related;

    if (state == ZR_NULL || diagnostic == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!diagnostic->relatedInformation.isValid) {
        ZrCore_Array_Init(state,
                          &diagnostic->relatedInformation,
                          sizeof(SZrStructuredDiagnosticRelatedInformation),
                          ZR_PARSER_INITIAL_CAPACITY_TINY);
    }

    memset(&related, 0, sizeof(related));
    related.location = location;
    related.message = structured_diagnostic_create_string(state, message);
    if (related.message == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &diagnostic->relatedInformation, &related);
    return ZR_TRUE;
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
    out->descriptorId = ZrParser_DiagnosticRegistry_DescriptorIdForCode(code);
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

TZrBool ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location,
                                                                   SZrFileRange fixLocation,
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

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_declaration_body_open",
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing declaration body",
                fixLocation,
                "{}",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyClose(SZrState *state,
                                                                    SZrStructuredDiagnostic *out,
                                                                    SZrFileRange location,
                                                                    SZrFileRange fixLocation,
                                                                    const TZrChar *declarationKind) {
    const TZrChar *kind = declarationKind != ZR_NULL ? declarationKind : "declaration";
    TZrChar message[160];
    TZrChar cause[256];
    TZrChar suggestion[192];

    snprintf(message,
             sizeof(message),
             "Missing closing '}' for %s body",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The %s body started with '{', but the parser reached the end of input before a closing '}' appeared.",
             kind);
    snprintf(suggestion,
             sizeof(suggestion),
             "Insert '}' to close the %s body before continuing.",
             kind);

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_declaration_body_close",
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing '}'",
                fixLocation,
                "}",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location,
                                                                  SZrFileRange fixLocation,
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

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_statement_body_open",
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing statement body",
                fixLocation,
                "{}",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingBlockClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location,
                                                          SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_block_close",
                "Missing closing '}' for block",
                "The block started with '{', but the parser reached the end of input before a closing '}' appeared.",
                "Insert '}' to close the block before continuing.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing '}'",
                fixLocation,
                "}",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose(SZrState *state,
                                                                 SZrStructuredDiagnostic *out,
                                                                 SZrFileRange location,
                                                                 SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_catch_pattern_close",
                "Missing closing ')' in catch pattern",
                "The catch clause started a pattern with '(', but the parser reached the catch body before a closing ')' appeared.",
                "Insert ')' after the catch pattern before the catch body.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ')'",
                fixLocation,
                ")",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location,
                                                                  SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_using_resource_close",
                "Missing closing ')' in using resource",
                "The using statement started a resource expression with '(', but the parser reached the using body before a closing ')' appeared.",
                "Insert ')' after the using resource before the using body.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ')'",
                fixLocation,
                ")",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool build_missing_header_fix(SZrState *state,
                                        SZrStructuredDiagnostic *out,
                                        SZrFileRange location,
                                        SZrFileRange fixLocation,
                                        const TZrChar *code,
                                        const TZrChar *message,
                                        const TZrChar *cause,
                                        const TZrChar *suggestion,
                                        const TZrChar *title,
                                        const TZrChar *editText) {
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

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                title,
                fixLocation,
                editText,
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose(SZrState *state,
                                                              SZrStructuredDiagnostic *out,
                                                              SZrFileRange location,
                                                              SZrFileRange fixLocation) {
    return build_missing_header_fix(
            state,
            out,
            location,
            fixLocation,
            "missing_for_header_close",
            "Missing closing ')' in for header",
            "The for statement header started with '(', but the parser reached the loop body before a closing ')' appeared.",
            "Insert ')' after the for header before the loop body.",
            "Insert missing ')'",
            ")");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location,
                                                                  SZrFileRange fixLocation) {
    return build_missing_header_fix(
            state,
            out,
            location,
            fixLocation,
            "missing_for_header_separator",
            "Missing ';' between for header clauses",
            "A traditional for header requires ';' between initializer, condition, and step clauses, but another clause started first.",
            "Insert ';' between the for header clauses.",
            "Insert missing ';'",
            ";");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location,
                                                                  SZrFileRange fixLocation) {
    return build_missing_header_fix(
            state,
            out,
            location,
            fixLocation,
            "missing_foreach_header_close",
            "Missing closing ')' in foreach header",
            "The foreach header started with '(', but the parser reached the loop body before a closing ')' appeared.",
            "Insert ')' after the foreach iterable before the loop body.",
            "Insert missing ')'",
            ")");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location,
                                                               SZrFileRange fixLocation) {
    return build_missing_header_fix(
            state,
            out,
            location,
            fixLocation,
            "missing_foreach_in_keyword",
            "Missing 'in' in foreach header",
            "The foreach header has a pattern, but the parser did not find 'in' before the iterable expression.",
            "Insert 'in' between the foreach pattern and iterable expression.",
            "Insert missing 'in'",
            "in ");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose(SZrState *state,
                                                                     SZrStructuredDiagnostic *out,
                                                                     SZrFileRange location) {
    return build_missing_header_fix(
            state,
            out,
            location,
            location,
            "missing_switch_case_header_close",
            "Missing closing ')' in switch case header",
            "The switch case header started with '(', but the parser reached the case body before a closing ')' appeared.",
            "Insert ')' after the switch case expression before the case body.",
            "Insert missing ')'",
            ")");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return build_missing_header_fix(
            state,
            out,
            location,
            location,
            "missing_switch_body_close",
            "Missing closing '}' for switch body",
            "The switch body started with '{', but the parser reached the end of input before a closing '}' appeared.",
            "Insert '}' to close the switch body before continuing.",
            "Insert missing '}'",
            "}");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingExternSpecClose(SZrState *state,
                                                               SZrStructuredDiagnostic *out,
                                                               SZrFileRange location) {
    return build_missing_header_fix(
            state,
            out,
            location,
            location,
            "missing_extern_spec_close",
            "Missing closing ')' in extern block spec",
            "The extern block started a library spec with '(', but the parser reached the extern block body before a closing ')' appeared.",
            "Insert ')' after the extern block spec before the extern block body.",
            "Insert missing ')'",
            ")");
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingConditionClose(SZrState *state,
                                                              SZrStructuredDiagnostic *out,
                                                              SZrFileRange location,
                                                              const TZrChar *statementKind) {
    const TZrChar *kind = statementKind != ZR_NULL ? statementKind : "control";
    TZrChar message[128];
    TZrChar cause[224];
    SZrFileRange fixLocation = location;

    snprintf(message,
             sizeof(message),
             "Missing ')' after '%s' condition",
             kind);
    snprintf(cause,
             sizeof(cause),
             "The '%s' condition started with '(', but the parser reached the block before a closing ')' appeared.",
             kind);

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_condition_close",
                message,
                cause,
                "Insert ')' after the condition expression before the block.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ')'",
                fixLocation,
                ")",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingIndexClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location,
                                                          SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_index_close",
                "Missing closing ']' in index access",
                "The computed member access started with '[', but the parser reached another token before a closing ']' appeared.",
                "Insert ']' after the index expression before continuing the member access.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ']'",
                fixLocation,
                "]",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingCallClose(SZrState *state,
                                                         SZrStructuredDiagnostic *out,
                                                         SZrFileRange location,
                                                         SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_call_close",
                "Missing closing ')' in function call",
                "The function call started an argument list with '(', but the parser reached another token before a closing ')' appeared.",
                "Insert ')' after the last argument before continuing.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ')'",
                fixLocation,
                ")",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingParameterListClose(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location) {
    SZrFileRange fixLocation;

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_parameter_list_close",
                "Missing closing ')' in function declaration parameters",
                "The function declaration started a parameter list with '(', but the parser reached another token before a closing ')' appeared.",
                "Insert ')' after the parameter list before continuing.")) {
        return ZR_FALSE;
    }

    fixLocation = location;
    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ')'",
                fixLocation,
                ")",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingGroupClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location,
                                                          SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_group_close",
                "Missing closing ')' in grouped expression",
                "The grouped expression started with '(', but the parser reached another token before a closing ')' appeared.",
                "Insert ')' after the grouped expression before continuing.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ')'",
                fixLocation,
                ")",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayClose(SZrState *state,
                                                          SZrStructuredDiagnostic *out,
                                                          SZrFileRange location,
                                                          SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_array_close",
                "Missing closing ']' in array literal",
                "The array literal started with '[', but the parser reached another token before a closing ']' appeared.",
                "Insert ']' after the last array element before continuing.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ']'",
                fixLocation,
                "]",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator(SZrState *state,
                                                                     SZrStructuredDiagnostic *out,
                                                                     SZrFileRange location) {
    SZrFileRange fixLocation = location;

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_array_element_separator",
                "Missing separator between array elements",
                "The array literal has another element expression immediately after the previous element.",
                "Insert ',' or ';' between array elements.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ','",
                fixLocation,
                ",",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectClose(SZrState *state,
                                                           SZrStructuredDiagnostic *out,
                                                           SZrFileRange location,
                                                           SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_object_close",
                "Missing closing '}' in object literal",
                "The object literal started with '{', but the parser reached another token before a closing '}' appeared.",
                "Insert '}' after the last object property before continuing.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing '}'",
                fixLocation,
                "}",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose(SZrState *state,
                                                                      SZrStructuredDiagnostic *out,
                                                                      SZrFileRange location,
                                                                      SZrFileRange fixLocation) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_object_computed_key_close",
                "Missing closing ']' in computed object key",
                "The computed object key started with '[', but the parser reached another token before the key expression closed.",
                "Insert ']' after the computed key expression before ':'.")) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ']'",
                fixLocation,
                "]",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectPropertyColon(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange location) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_object_property_colon",
                "Missing ':' after object property key",
                "Object literal properties require ':' between the key and the value expression.",
                "Insert ':' between the property key and value expression.")) {
        return ZR_FALSE;
    }

    location.end = location.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ':'",
                location,
                ":",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingObjectPropertySeparator(SZrState *state,
                                                                       SZrStructuredDiagnostic *out,
                                                                       SZrFileRange location) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_object_property_separator",
                "Missing separator between object properties",
                "The object literal has another property key immediately after the previous property's value.",
                "Insert ',' or ';' between object properties.")) {
        return ZR_FALSE;
    }

    location.end = location.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing ','",
                location,
                ",",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange location,
                                                                  SZrFileRange fixLocation,
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

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "missing_statement_semicolon",
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }

    fixLocation.end = fixLocation.start;
    if (!ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Insert missing semicolon",
                fixLocation,
                ";",
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildLegacyPropertySyntax(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange declarationLocation,
        SZrFileRange nameLocation,
        const SZrFileRange *typeLocation,
        const SZrFileRange *bodyLocation,
        const TZrChar *replacementText) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                declarationLocation,
                "legacy_property_syntax",
                "Legacy getter/setter property syntax is not a semantic declaration",
                "Properties now use one canonical property declaration with structured accessor roles.",
                replacementText != ZR_NULL
                        ? "Apply the parser-owned unified property replacement."
                        : "Rewrite the declaration manually because no unambiguous replacement is available.") ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                out,
                nameLocation,
                "Legacy property name")) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    if (typeLocation != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                out,
                *typeLocation,
                "Legacy property type")) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    if (bodyLocation != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                out,
                *bodyLocation,
                "Legacy accessor body")) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    if (replacementText != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Migrate legacy property syntax",
                declarationLocation,
                replacementText,
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DiagnosticBuilder_BuildRemovedOwnershipMemberSyntax(
        SZrState *state,
        SZrStructuredDiagnostic *out,
        SZrFileRange location,
        const TZrChar *replacementText) {
    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                out,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "removed_ownership_member_syntax",
                "Ownership operations use reserved intrinsic calls",
                "The receiver has a canonical ownership type, but the requested target member does not exist.",
                "Replace the removed ownership member call with its reserved intrinsic form.")) {
        return ZR_FALSE;
    }
    if (replacementText != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddFix(
                state,
                out,
                "Migrate ownership operation",
                location,
                replacementText,
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE)) {
        ZrParser_StructuredDiagnostic_Free(state, out);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
