#include "parser_internal.h"

SZrAstNode *parse_reserved_await_expression(SZrParserState *ps) {
    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_NULL;
    }

    report_error(ps, "Legacy '%await' syntax is not supported; use 'await'");
    return ZR_NULL;
}

SZrAstNode *parse_await_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *operand;
    SZrAstNode *awaitNode;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "await")) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    operand = parse_unary_expression(ps);
    if (operand == ZR_NULL) {
        return ZR_NULL;
    }

    awaitNode = create_ast_node(ps,
                                ZR_AST_AWAIT_EXPRESSION,
                                ZrParser_FileRange_Merge(startLoc, operand->location));
    if (awaitNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, operand);
        return ZR_NULL;
    }

    awaitNode->data.awaitExpression.operand = operand;
    return awaitNode;
}

SZrAstNode *parse_reserved_async_function_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *functionNode;
    SZrFunctionDeclaration *declaration;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    if (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || ps->lexer->t.token == ZR_TK_PRO) {
        parse_access_modifier(ps);
    }

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        report_error(ps, "Legacy '%async' syntax is not supported; use 'async fn ...: zr.task.Task<T>'");
        return ZR_NULL;
    }
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "async")) {
        report_error(ps, "Expected 'async' before function declaration");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    functionNode = parse_function_declaration(ps);
    if (functionNode == ZR_NULL || functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return functionNode;
    }

    declaration = &functionNode->data.functionDeclaration;
    declaration->isAsync = ZR_TRUE;
    declaration->isLegacyAsyncSyntax = ZR_FALSE;
    functionNode->location = ZrParser_FileRange_Merge(startLoc, functionNode->location);
    return functionNode;
}
