#include "parser_internal.h"

static void free_for_loop_parts(SZrParserState *ps,
                                SZrAstNode *init,
                                SZrAstNode *cond,
                                SZrAstNode *step) {
    if (init != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, init);
    }
    if (cond != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, cond);
    }
    if (step != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, step);
    }
}

SZrAstNode *parse_for_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *init = ZR_NULL;
    SZrAstNode *cond = ZR_NULL;
    SZrAstNode *step = ZR_NULL;

    expect_token(ps, ZR_TK_FOR);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    if (ps->lexer->t.token == ZR_TK_VAR) {
        init = parse_variable_declaration_for_header(ps);
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        } else if (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_EOS) {
            report_missing_for_header_separator(ps, get_current_token_location(ps));
            free_for_loop_parts(ps, init, cond, step);
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        init = parse_expression(ps);
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        } else if (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_EOS) {
            report_missing_for_header_separator(ps, get_current_token_location(ps));
            free_for_loop_parts(ps, init, cond, step);
            return ZR_NULL;
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        cond = parse_expression(ps);
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        } else if (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_EOS) {
            report_missing_for_header_separator(ps, get_current_token_location(ps));
            free_for_loop_parts(ps, init, cond, step);
            return ZR_NULL;
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        step = parse_expression(ps);
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_for_header_close(ps, get_current_token_location(ps));
        free_for_loop_parts(ps, init, cond, step);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RPAREN);

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "for statement", get_current_token_location(ps));
        free_for_loop_parts(ps, init, cond, step);
        return ZR_NULL;
    }

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FOR_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.forLoop.init = init;
    node->data.forLoop.cond = cond;
    node->data.forLoop.step = step;
    node->data.forLoop.block = block;
    node->data.forLoop.isStatement = ZR_TRUE;
    return node;
}

SZrAstNode *parse_foreach_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *pattern = ZR_NULL;
    SZrType *typeInfo = ZR_NULL;

    expect_token(ps, ZR_TK_FOR);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    expect_token(ps, ZR_TK_VAR);
    consume_token(ps, ZR_TK_VAR);

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        pattern = parse_identifier(ps);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        pattern = parse_destructuring_object(ps);
    } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        pattern = parse_destructuring_array(ps);
    } else {
        report_error(ps, "Expected identifier or destructuring pattern");
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    if (ps->lexer->t.token != ZR_TK_IN) {
        report_missing_foreach_in_keyword(ps, get_current_token_location(ps));
        if (pattern != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, pattern);
        }
        free_owned_type(ps->state, typeInfo);
        return ZR_NULL;
    }

    consume_token(ps, ZR_TK_IN);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_foreach_header_close(ps, get_current_token_location(ps));
        if (pattern != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, pattern);
        }
        free_owned_type(ps->state, typeInfo);
        ZrParser_Ast_Free(ps->state, expr);
        return ZR_NULL;
    }

    consume_token(ps, ZR_TK_RPAREN);

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "foreach statement", get_current_token_location(ps));
        if (pattern != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, pattern);
        }
        free_owned_type(ps->state, typeInfo);
        ZrParser_Ast_Free(ps->state, expr);
        return ZR_NULL;
    }

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FOREACH_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.foreachLoop.pattern = pattern;
    node->data.foreachLoop.typeInfo = typeInfo;
    node->data.foreachLoop.expr = expr;
    node->data.foreachLoop.block = block;
    node->data.foreachLoop.isStatement = ZR_TRUE;
    return node;
}
