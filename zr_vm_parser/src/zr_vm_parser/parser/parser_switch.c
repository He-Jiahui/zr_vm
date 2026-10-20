#include "parser_internal.h"

SZrAstNode *parse_switch_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *expr = ZR_NULL;
    SZrAstNodeArray *cases = ZR_NULL;
    SZrAstNode *defaultCase = ZR_NULL;
    SZrAstNode *value = ZR_NULL;
    SZrAstNode *block = ZR_NULL;
    SZrAstNode *caseNode = ZR_NULL;
    SZrAstNode *switchNode;

    expect_token(ps, ZR_TK_SWITCH);
    ZrParser_Lexer_Next(ps->lexer);
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_RPAREN) {
        report_missing_condition(ps, "switch", get_current_token_location(ps));
        goto fail;
    }
    expr = parse_expression(ps);
    if (expr == ZR_NULL || ps->hasError) {
        goto fail;
    }
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_condition_close(ps, "switch", get_current_token_location(ps));
        goto fail;
    }
    consume_token(ps, ZR_TK_RPAREN);
    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "switch statement", get_current_token_location(ps));
        goto fail;
    }
    consume_token(ps, ZR_TK_LBRACE);
    cases = parser_ast_array_new(ps, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (cases == ZR_NULL) {
        goto fail;
    }

    /* Pending branch inputs stay local until their wrapper owns them. */
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        if (!consume_token(ps, ZR_TK_LPAREN)) {
            break;
        }
        if (ps->lexer->t.token == ZR_TK_RPAREN) {
            if (defaultCase != ZR_NULL) {
                report_error(ps, "switch may have only one default case");
                goto fail;
            }
            consume_token(ps, ZR_TK_RPAREN);
            if (ps->lexer->t.token != ZR_TK_LBRACE) {
                report_missing_statement_body_open(ps, "switch default", get_current_token_location(ps));
                goto fail;
            }
            block = parse_block(ps);
            if (block == ZR_NULL || ps->hasError) {
                goto fail;
            }
            defaultCase = create_ast_node(ps, ZR_AST_SWITCH_DEFAULT, get_current_location(ps));
            if (defaultCase == ZR_NULL) {
                goto fail;
            }
            defaultCase->data.switchDefault.block = block;
            block = ZR_NULL;
            continue;
        }

        value = try_parse_switch_move_variant_pattern_case(ps);
        if (value == ZR_NULL && !ps->hasFatalError) {
            value = parse_expression(ps);
            value = try_parse_switch_struct_variant_payload_case(ps, value);
        }
        if (value == ZR_NULL || ps->hasError || ps->hasFatalError) {
            goto fail;
        }
        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            report_missing_switch_case_header_close(ps, get_current_token_location(ps));
            goto fail;
        }
        consume_token(ps, ZR_TK_RPAREN);
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            report_missing_statement_body_open(ps, "switch case", get_current_token_location(ps));
            goto fail;
        }
        block = parse_block(ps);
        if (block == ZR_NULL || ps->hasError) {
            goto fail;
        }
        caseNode = create_ast_node(ps, ZR_AST_SWITCH_CASE, get_current_location(ps));
        if (caseNode == ZR_NULL) {
            goto fail;
        }
        caseNode->data.switchCase.value = value;
        caseNode->data.switchCase.block = block;
        value = ZR_NULL;
        block = ZR_NULL;
        if (!parser_ast_array_add(ps, cases, caseNode)) {
            goto fail;
        }
        caseNode = ZR_NULL;
    }
    if (ps->hasError || ps->hasFatalError) {
        goto fail;
    }
    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        report_missing_switch_body_close(ps, get_current_token_location(ps));
        goto fail;
    }
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange mergedLocation;
    if (!ZrParser_MergeLocations(ps, startLoc, get_current_location(ps), &mergedLocation) ||
        (switchNode = create_ast_node(ps, ZR_AST_SWITCH_EXPRESSION,
            mergedLocation)) == ZR_NULL) {
        goto fail;
    }
    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases = cases;
    switchNode->data.switchExpression.defaultCase = defaultCase;
    switchNode->data.switchExpression.isStatement = ZR_TRUE;
    return switchNode;

fail:
    ZrParser_Ast_Free(ps->state, value);
    ZrParser_Ast_Free(ps->state, block);
    ZrParser_Ast_Free(ps->state, caseNode);
    ZrParser_Ast_Free(ps->state, expr);
    free_ast_node_array_with_elements(ps->state, cases);
    ZrParser_Ast_Free(ps->state, defaultCase);
    return ZR_NULL;
}
