#include "parser_internal.h"

static void report_conditional_diagnostic(
        SZrParserState *ps,
        SZrFileRange location,
        TZrBool (*build)(SZrState *, SZrStructuredDiagnostic *, SZrFileRange),
        const TZrChar *fallbackMessage) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL || build == ZR_NULL) {
        return;
    }

    if (!build(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, fallbackMessage, ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_conditional_consequent(SZrParserState *ps, SZrFileRange location) {
    report_conditional_diagnostic(ps,
                                  location,
                                  ZrParser_DiagnosticBuilder_BuildMissingConditionalConsequent,
                                  "Missing expression after '?' in conditional expression");
}

void report_missing_conditional_colon(SZrParserState *ps, SZrFileRange location) {
    report_conditional_diagnostic(ps,
                                  location,
                                  ZrParser_DiagnosticBuilder_BuildMissingConditionalColon,
                                  "Missing ':' in conditional expression");
}

void report_missing_conditional_alternate(SZrParserState *ps, SZrFileRange location) {
    report_conditional_diagnostic(ps,
                                  location,
                                  ZrParser_DiagnosticBuilder_BuildMissingConditionalAlternate,
                                  "Missing expression after ':' in conditional expression");
}

void report_missing_statement_semicolon(SZrParserState *ps, const TZrChar *statementKind, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon(ps->state,
                                                                   &diagnostic,
                                                                   location,
                                                                   statementKind)) {
        report_error_with_token(ps, "Missing ';' after statement", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_declaration_body_open(SZrParserState *ps,
                                          const TZrChar *declarationKind,
                                          SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyOpen(ps->state,
                                                                    &diagnostic,
                                                                    location,
                                                                    declarationKind)) {
        report_error_with_token(ps, "Missing '{' to start declaration body", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_declaration_body_close(SZrParserState *ps,
                                           const TZrChar *declarationKind,
                                           SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyClose(ps->state,
                                                                     &diagnostic,
                                                                     location,
                                                                     declarationKind)) {
        report_error_with_token(ps, "Missing closing '}' for declaration body", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_statement_body_open(SZrParserState *ps,
                                        const TZrChar *statementKind,
                                        SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen(ps->state,
                                                                  &diagnostic,
                                                                  location,
                                                                  statementKind)) {
        report_error_with_token(ps, "Missing '{' to start statement body", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_block_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingBlockClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing '}' for block", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_catch_pattern_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in catch pattern", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_using_resource_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingUsingResourceClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in using resource", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_using_binder_invalid(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildUsingBinderInvalid(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "using_binder_invalid: invalid using guard binder", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_import_path_not_constant(SZrParserState *ps,
                                     SZrFileRange location,
                                     const TZrChar *directiveName) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildImportPathNotConstant(ps->state,
                                                               &diagnostic,
                                                               location,
                                                               directiveName)) {
        report_error_with_token(ps, "%import(...) requires a string literal module path", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_for_header_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in for header", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_for_header_separator(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing ';' between for header clauses", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_foreach_header_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in foreach header", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_foreach_in_keyword(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing 'in' in foreach header", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_switch_case_header_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingSwitchCaseHeaderClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in switch case header", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_switch_body_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingSwitchBodyClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing '}' for switch body", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_extern_spec_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingExternSpecClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in extern block spec", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_test_name_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingTestNameClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in test declaration name", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}
