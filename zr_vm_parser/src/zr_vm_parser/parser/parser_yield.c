#include "parser_internal.h"

SZrAstNode *parse_yield_statement(SZrParserState *ps) {
    SZrFileRange startLocation;
    SZrFileRange endLocation;
    SZrAstNode *expression;
    SZrAstNode *node;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLocation = get_current_token_location(ps);
    expect_token(ps, ZR_TK_YIELD);
    ZrParser_Lexer_Next(ps->lexer);

    expression = parse_expression(ps);
    if (expression == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        report_missing_statement_semicolon(
                ps, "yield", get_current_token_location(ps));
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    endLocation = get_current_location(ps);
    node = create_ast_node(
            ps,
            ZR_AST_YIELD_STATEMENT,
            ZrParser_FileRange_Merge(startLocation, endLocation));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, expression);
        return ZR_NULL;
    }

    node->data.yieldStatement.expr = expression;
    return node;
}
