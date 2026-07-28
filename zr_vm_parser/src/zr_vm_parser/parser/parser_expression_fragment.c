#include "parser_internal.h"

SZrAstNode *ZrParser_ParseExpressionWithState(SZrParserState *ps) {
    SZrAstNode *expression;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL ||
        ps->hasError) {
        return ZR_NULL;
    }

    expression = parse_expression(ps);
    if (expression == ZR_NULL || ps->hasError) {
        if (expression != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, expression);
        }
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_EOS) {
        report_error(ps, "Unexpected token after expression");
        ZrParser_Ast_Free(ps->state, expression);
        return ZR_NULL;
    }

    return expression;
}
