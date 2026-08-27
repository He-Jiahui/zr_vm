#include "parser_internal.h"

TZrBool is_ownership_intrinsic_token(EZrToken token) {
    return token == ZR_TK_SHARE || token == ZR_TK_DEGRADE ||
           token == ZR_TK_WAKE || token == ZR_TK_INTO_GC ||
           token == ZR_TK_DROP;
}

static EZrOwnershipIntrinsicOperation intrinsic_operation(EZrToken token) {
    switch (token) {
        case ZR_TK_SHARE:
            return ZR_OWNERSHIP_INTRINSIC_SHARE;
        case ZR_TK_DEGRADE:
            return ZR_OWNERSHIP_INTRINSIC_DEGRADE;
        case ZR_TK_WAKE:
            return ZR_OWNERSHIP_INTRINSIC_WAKE;
        case ZR_TK_INTO_GC:
            return ZR_OWNERSHIP_INTRINSIC_INTO_GC;
        case ZR_TK_DROP:
            return ZR_OWNERSHIP_INTRINSIC_DROP;
        default:
            return ZR_OWNERSHIP_INTRINSIC_SHARE;
    }
}

static void report_ownership_intrinsic_syntax_error(
        SZrParserState *ps,
        SZrFileRange location,
        EZrToken token,
        const TZrChar *code,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }
    if (!ZrParser_DiagnosticBuilder_Build(
                ps->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                code,
                message,
                cause,
                suggestion)) {
        report_error_with_token(ps, message, token);
        return;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
        report_error_with_token(ps, message, token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

SZrAstNode *parse_member_identifier(SZrParserState *ps) {
    SZrFileRange location;
    const TZrChar *name;
    SZrString *value;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_NULL;
    }
    if (!is_ownership_intrinsic_token(ps->lexer->t.token)) {
        return parse_identifier(ps);
    }

    location = get_current_token_location(ps);
    name = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
    if (name == ZR_NULL) {
        report_error(ps, "Expected member identifier");
        return ZR_NULL;
    }
    value = ZrCore_String_Create(ps->state, (TZrNativeString)name, strlen(name));
    ZrParser_Lexer_Next(ps->lexer);
    return create_identifier_node_with_location(ps, value, location);
}

SZrAstNode *parse_ownership_intrinsic_expression(SZrParserState *ps) {
    EZrToken token;
    EZrOwnershipIntrinsicOperation operation;
    SZrFileRange nameRange;
    SZrFileRange callOpenRange;
    SZrFileRange callCloseRange;
    SZrAstNode *argument;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL ||
        !is_ownership_intrinsic_token(ps->lexer->t.token)) {
        return ZR_NULL;
    }

    token = ps->lexer->t.token;
    operation = intrinsic_operation(token);
    nameRange = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        report_ownership_intrinsic_syntax_error(
                ps,
                nameRange,
                token,
                "ownership_intrinsic_call_required",
                "Ownership intrinsic must be called with exactly one positional argument",
                "Reserved ownership intrinsic names are operations and cannot be referenced as values.",
                "Call the intrinsic with exactly one positional owner expression.");
        return ZR_NULL;
    }

    callOpenRange = get_current_token_location(ps);
    consume_token(ps, ZR_TK_LPAREN);
    if (ps->lexer->t.token == ZR_TK_RPAREN) {
        report_ownership_intrinsic_syntax_error(
                ps,
                get_current_token_location(ps),
                ps->lexer->t.token,
                "ownership_intrinsic_arity_mismatch",
                "Ownership intrinsic requires exactly one positional argument",
                "The ownership operation has no owner operand.",
                "Pass exactly one positional owner expression.");
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_NULL;
    }
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        peek_token(ps) == ZR_TK_COLON) {
        report_ownership_intrinsic_syntax_error(
                ps,
                get_current_token_location(ps),
                ps->lexer->t.token,
                "ownership_intrinsic_arity_mismatch",
                "Ownership intrinsic accepts exactly one positional argument",
                "Ownership operations do not accept named arguments.",
                "Pass exactly one positional owner expression.");
        do {
            ZrParser_Lexer_Next(ps->lexer);
        } while (ps->lexer->t.token != ZR_TK_RPAREN &&
                 ps->lexer->t.token != ZR_TK_EOS);
        if (ps->lexer->t.token == ZR_TK_RPAREN) {
            ZrParser_Lexer_Next(ps->lexer);
        }
        return ZR_NULL;
    }

    argument = parse_expression(ps);
    if (argument == ZR_NULL) {
        return ZR_NULL;
    }
    if (ps->lexer->t.token == ZR_TK_COMMA) {
        report_ownership_intrinsic_syntax_error(
                ps,
                get_current_token_location(ps),
                ps->lexer->t.token,
                "ownership_intrinsic_arity_mismatch",
                "Ownership intrinsic accepts exactly one positional argument",
                "The ownership operation has more than one operand.",
                "Pass exactly one positional owner expression.");
        do {
            ZrParser_Lexer_Next(ps->lexer);
        } while (ps->lexer->t.token != ZR_TK_RPAREN &&
                 ps->lexer->t.token != ZR_TK_EOS);
    }
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_call_close(ps, callOpenRange);
        ZrParser_Ast_Free(ps->state, argument);
        return ZR_NULL;
    }

    callCloseRange = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RPAREN);
    node = create_ast_node(
            ps,
            ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION,
            ZrParser_FileRange_Merge(nameRange, callCloseRange));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, argument);
        return ZR_NULL;
    }
    node->data.ownershipIntrinsicExpression.operation = operation;
    node->data.ownershipIntrinsicExpression.argument = argument;
    node->data.ownershipIntrinsicExpression.nameRange = nameRange;
    node->data.ownershipIntrinsicExpression.callRange =
            ZrParser_FileRange_Merge(callOpenRange, callCloseRange);
    return node;
}
