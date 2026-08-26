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

TZrBool ZrParser_DiagnosticBuilder_BuildUsingBinderInvalid(SZrState *state,
                                                           SZrStructuredDiagnostic *out,
                                                           SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "using_binder_invalid",
            "using_binder_invalid: invalid using guard binder",
            "A using guard binder must be an import binding name or a union destructuring pattern.",
            "Use `using (let name = import(...))` for plugin guards, `using (let [value]: Union.Variant = resource)` for tuple variants, or `using (let {local: field}: Union.Variant = resource)` for struct variants.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildImportPathNotConstant(SZrState *state,
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

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "import_path_not_constant",
            message,
            cause,
            suggestion);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternShapeMismatch(SZrState *state,
                                                             SZrStructuredDiagnostic *out,
                                                             SZrFileRange location,
                                                             const TZrChar *message,
                                                             const TZrChar *cause,
                                                             const TZrChar *suggestion) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "pattern_shape_mismatch",
            message != ZR_NULL ? message : "Union pattern destructuring shape does not match variant payload shape",
            cause != ZR_NULL
                ? cause
                : "The pattern uses a destructuring shape that does not match the selected union variant payload.",
            suggestion != ZR_NULL
                ? suggestion
                : "Use tuple destructuring for positional variants and object destructuring for named-field variants.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternUnknownField(SZrState *state,
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

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "pattern_unknown_field",
            message,
            cause,
            suggestion);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternArityMismatch(SZrState *state,
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

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "pattern_arity_mismatch",
            message,
            cause,
            suggestion);
}

TZrBool ZrParser_DiagnosticBuilder_BuildPatternVariantMismatch(SZrState *state,
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

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "pattern_variant_mismatch",
            message,
            cause,
            suggestion);
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

TZrBool ZrParser_DiagnosticBuilder_BuildWeakWake(SZrState *state,
                                                 SZrStructuredDiagnostic *out,
                                                 SZrFileRange location) {
    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            location,
            "weak_value_requires_wake",
            "Weak value must be woken before it can be borrowed",
            "A Weak<T> value does not keep its owner alive, so it cannot satisfy a ref readonly T use directly.",
            "Call wake(weak) and handle the nullable shared owner before borrowing it.");
}

TZrBool ZrParser_DiagnosticBuilder_BuildLegacyOwnershipTypeSyntaxWarning(SZrState *state,
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

    return ZrParser_DiagnosticBuilder_Build(
            state,
            out,
            ZR_STRUCTURED_DIAGNOSTIC_WARNING,
            location,
            "legacy_ownership_type_syntax",
            message,
            "Ownership qualifiers are now intrinsic generic types; the legacy percent-prefixed type form is kept only as migration syntax.",
            suggestion);
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
            "The expression uses ref, which creates a temporary borrow tied to the source owner.",
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
            "The expression uses ref, which creates a temporary loan tied to the source owner.",
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
            "A Unique<T> or Shared<T> value owns deterministic cleanup; assigning it to a plain value would drop ownership semantics.",
            "Keep the ownership wrapper in the target type or revise the ownership transfer explicitly.");
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
