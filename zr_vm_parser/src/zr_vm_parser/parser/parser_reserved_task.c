#include "parser_internal.h"

/* await 是上下文关键字；操作数按一元表达式优先级解析并交由 await AST 持有。 */
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

/* 由语句分派的 async fn 探测入口调用，复用普通函数解析后补上异步标记。 */
SZrAstNode *parse_reserved_async_function_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *functionNode;
    SZrFunctionDeclaration *declaration;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    if (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || ps->lexer->t.token == ZR_TK_PRO) {
        /* BUG: pub/pro async fn 可达此分支；修饰符先被消费，后续
         * parse_function_declaration 只看到 fn，AST 访问级别回落为 private。 */
        parse_access_modifier(ps);
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
    functionNode->location = ZrParser_FileRange_Merge(startLoc, functionNode->location);
    return functionNode;
}
