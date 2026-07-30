#include "parser_internal.h"

static SZrAstNode *create_expression_body_block(SZrParserState *ps, SZrAstNode *expression) {
    SZrAstNode *returnNode;
    SZrAstNode *blockNode;
    SZrAstNodeArray *body;

    if (ps == ZR_NULL || expression == ZR_NULL) {
        return ZR_NULL;
    }
    returnNode = create_ast_node(ps, ZR_AST_RETURN_STATEMENT, expression->location);
    if (returnNode == ZR_NULL) {
        return ZR_NULL;
    }
    returnNode->data.returnStatement.expr = expression;
    body = ZrParser_AstNodeArray_New(ps->state, 1u);
    if (body == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, returnNode);
        return ZR_NULL;
    }
    ZrParser_AstNodeArray_Add(ps->state, body, returnNode);
    blockNode = create_ast_node(ps, ZR_AST_BLOCK, expression->location);
    if (blockNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, body);
        return ZR_NULL;
    }
    blockNode->data.block.body = body;
    blockNode->data.block.isStatement = ZR_FALSE;
    return blockNode;
}

SZrAstNode *parse_fn_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange returnDelimiterLoc;
    SZrFileRange bodyDelimiterLoc;
    SZrAstNodeArray *params;
    SZrParameter *args = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *body;
    SZrAstNode *lambdaNode;
    TZrBool isExpressionBody = ZR_FALSE;

    memset(&returnDelimiterLoc, 0, sizeof(returnDelimiterLoc));
    memset(&bodyDelimiterLoc, 0, sizeof(bodyDelimiterLoc));
    ZrParser_Lexer_Next(ps->lexer);
    if (!consume_token(ps, ZR_TK_LPAREN)) {
        report_error(ps, "Expected '(' after 'fn' in anonymous function");
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        args = argsNode != ZR_NULL ? &argsNode->data.parameter : ZR_NULL;
        params = ZrParser_AstNodeArray_New(ps->state, 0u);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
            SZrAstNode *argsNode = parse_parameter(ps);
            args = argsNode != ZR_NULL ? &argsNode->data.parameter : ZR_NULL;
        }
    }
    if (params == ZR_NULL || !consume_token(ps, ZR_TK_RPAREN)) {
        report_missing_parameter_list_close(ps, get_current_token_location(ps));
        free_ast_node_array_with_elements(ps->state, params);
        free_parameter_node_from_ptr(ps->state, args);
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_COLON) {
        returnDelimiterLoc = get_current_token_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
        returnType = parse_type(ps);
        if (returnType == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, params);
            free_parameter_node_from_ptr(ps->state, args);
            return ZR_NULL;
        }
    }

    if (ps->lexer->t.token == ZR_TK_FAT_ARROW) {
        SZrAstNode *expression;
        bodyDelimiterLoc = get_current_token_location(ps);
        isExpressionBody = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_LBRACE) {
            body = parse_block(ps);
        } else {
            expression = parse_expression(ps);
            body = create_expression_body_block(ps, expression);
        }
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        bodyDelimiterLoc = get_current_token_location(ps);
        body = parse_block(ps);
    } else {
        report_error(ps, ps->lexer->t.token == ZR_TK_THIN_ARROW
                                 ? "Anonymous function expressions use '=>' before an expression body"
                                 : "Expected block or '=>' expression body after anonymous function signature");
        free_ast_node_array_with_elements(ps->state, params);
        free_parameter_node_from_ptr(ps->state, args);
        free_owned_type(ps->state, returnType);
        return ZR_NULL;
    }
    if (body == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, params);
        free_parameter_node_from_ptr(ps->state, args);
        free_owned_type(ps->state, returnType);
        return ZR_NULL;
    }

    lambdaNode = create_ast_node(
            ps, ZR_AST_LAMBDA_EXPRESSION, ZrParser_FileRange_Merge(startLoc, body->location));
    if (lambdaNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, params);
        free_parameter_node_from_ptr(ps->state, args);
        free_owned_type(ps->state, returnType);
        ZrParser_Ast_Free(ps->state, body);
        return ZR_NULL;
    }
    lambdaNode->data.lambdaExpression.params = params;
    lambdaNode->data.lambdaExpression.args = args;
    lambdaNode->data.lambdaExpression.block = body;
    lambdaNode->data.lambdaExpression.returnType = returnType;
    lambdaNode->data.lambdaExpression.returnDelimiterLocation = returnDelimiterLoc;
    lambdaNode->data.lambdaExpression.bodyDelimiterLocation = bodyDelimiterLoc;
    lambdaNode->data.lambdaExpression.isExpressionBody = isExpressionBody;
    lambdaNode->data.lambdaExpression.isAsync = ZR_FALSE;
    return lambdaNode;
}
