#include "parser_internal.h"

// 片段入口要求整个 token 流恰好是一条表达式；成功 AST 归调用者，失败或多余 token 时由本层释放。
SZrAstNode *ZrParser_ParseExpressionWithState(SZrParserState *ps) {
    SZrAstNode *expression;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_NULL;
    }
    if (ps->hasError) {
        parser_report_first_token_lex_error(ps);
        return ZR_NULL;
    }

    expression = parse_expression(ps);
    parser_report_nesting_error(ps);
    // 未闭合块注释只在 EOS 上留下词法错误：不论表达式本身是否已经出错都要报告，片段不能被接受。
    if (!ps->lexer->hasSourceEvidenceError &&
        ps->lexer->t.token == ZR_TK_EOS && ps->lexer->t.hasLexError) {
        report_error(ps, ps->lexer->t.lexErrorMessage != ZR_NULL
                                 ? ps->lexer->t.lexErrorMessage
                                 : "Lexical error at end of input");
    }
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
