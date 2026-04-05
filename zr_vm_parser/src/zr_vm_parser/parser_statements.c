#include "parser_internal.h"

static SZrAstNode *try_parse_prefixed_function_declaration(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    SZrAstNode *funcDecl;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER || !current_identifier_equals(ps, "func")) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    savedSuppressErrorOutput = ps->suppressErrorOutput;
    ps->suppressErrorOutput = ZR_TRUE;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    funcDecl = parse_function_declaration(ps);
    ps->suppressErrorOutput = savedSuppressErrorOutput;
    if (funcDecl != ZR_NULL && !ps->hasError) {
        ps->hasError = cursor.hasError;
        ps->errorMessage = cursor.errorMessage;
        return funcDecl;
    }

    if (funcDecl != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, funcDecl);
    }

    restore_parser_cursor(ps, &cursor);
    return ZR_NULL;
}

static TZrBool parser_function_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    EZrToken token;
    EZrToken lookahead;
    TZrBool isFunction = ZR_FALSE;

    if (ps == ZR_NULL) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    token = ps->lexer->t.token;
    if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO) {
        parse_access_modifier(ps);
        token = ps->lexer->t.token;
    }

    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
        isFunction = ZR_TRUE;
    } else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
        lookahead = peek_token(ps);
        isFunction = lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN;
    }

    restore_parser_cursor(ps, &cursor);
    return isFunction;
}

static SZrAstNode *try_parse_function_declaration_from_current(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    SZrAstNode *funcDecl;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    savedSuppressErrorOutput = ps->suppressErrorOutput;
    ps->suppressErrorOutput = ZR_TRUE;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    funcDecl = parse_function_declaration(ps);
    ps->suppressErrorOutput = savedSuppressErrorOutput;
    if (funcDecl != ZR_NULL && !ps->hasError) {
        ps->hasError = cursor.hasError;
        ps->errorMessage = cursor.errorMessage;
        return funcDecl;
    }

    if (funcDecl != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, funcDecl);
    }

    restore_parser_cursor(ps, &cursor);
    return ZR_NULL;
}

SZrAstNode *parse_block(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *stmt = parse_statement(ps);
        if (stmt != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, statements, stmt);
        } else {
            break; // 遇到错误
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange blockLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.block.body = statements;
    node->data.block.isStatement = ZR_TRUE;
    return node;
}

// 解析表达式语句

SZrAstNode *parse_expression_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_EXPRESSION_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.expressionStatement.expr = expr;
    return node;
}

// 解析返回语句

SZrAstNode *parse_return_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_RETURN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        expr = parse_expression(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_RETURN_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.returnStatement.expr = expr;
    return node;
}

// 解析 switch 表达式/语句

SZrAstNode *parse_switch_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_SWITCH);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *cases = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    SZrAstNode *defaultCase = ZR_NULL;

    // 解析 switch cases
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        if (consume_token(ps, ZR_TK_LPAREN)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                // 默认 case
                ZrParser_Lexer_Next(ps->lexer);
                SZrAstNode *block = parse_block(ps);
                if (block != ZR_NULL) {
                    SZrFileRange defaultLoc = get_current_location(ps);
                    defaultCase = create_ast_node(ps, ZR_AST_SWITCH_DEFAULT, defaultLoc);
                    if (defaultCase != ZR_NULL) {
                        defaultCase->data.switchDefault.block = block;
                    }
                }
            } else {
                // 普通 case
                SZrAstNode *value = parse_expression(ps);
                expect_token(ps, ZR_TK_RPAREN);
                ZrParser_Lexer_Next(ps->lexer);
                SZrAstNode *block = parse_block(ps);
                if (value != ZR_NULL && block != ZR_NULL) {
                    SZrFileRange caseLoc = get_current_location(ps);
                    SZrAstNode *caseNode = create_ast_node(ps, ZR_AST_SWITCH_CASE, caseLoc);
                    if (caseNode != ZR_NULL) {
                        caseNode->data.switchCase.value = value;
                        caseNode->data.switchCase.block = block;
                        ZrParser_AstNodeArray_Add(ps->state, cases, caseNode);
                    }
                }
            }
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange switchLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *switchNode = create_ast_node(ps, ZR_AST_SWITCH_EXPRESSION, switchLoc);
    if (switchNode != ZR_NULL) {
        switchNode->data.switchExpression.expr = expr;
        switchNode->data.switchExpression.cases = cases;
        switchNode->data.switchExpression.defaultCase = defaultCase;
        switchNode->data.switchExpression.isStatement = ZR_TRUE; // 默认是语句
        return switchNode;
    }
    return ZR_NULL;
}

// 解析 if 表达式/语句

SZrAstNode *parse_if_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_IF);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    SZrAstNode *condition = parse_expression(ps);
    if (condition == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    SZrAstNode *thenExpr = parse_block(ps);
    if (thenExpr == ZR_NULL) {
        return ZR_NULL;
    }

    SZrAstNode *elseExpr = ZR_NULL;
    if (consume_token(ps, ZR_TK_ELSE)) {
        if (ps->lexer->t.token == ZR_TK_IF) {
            elseExpr = parse_if_expression(ps);
        } else {
            elseExpr = parse_block(ps);
        }
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange ifLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_IF_EXPRESSION, ifLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.ifExpression.condition = condition;
    node->data.ifExpression.thenExpr = thenExpr;
    node->data.ifExpression.elseExpr = elseExpr;
    node->data.ifExpression.isStatement = ZR_TRUE; // 默认是语句
    return node;
}

// 解析 while 循环

SZrAstNode *parse_while_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_WHILE);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    SZrAstNode *cond = parse_expression(ps);
    if (cond == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_WHILE_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.whileLoop.cond = cond;
    node->data.whileLoop.block = block;
    node->data.whileLoop.isStatement = ZR_TRUE; // 默认是语句
    return node;
}

// 解析 for 循环

SZrAstNode *parse_for_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_FOR);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    // 解析初始化（可选）
    SZrAstNode *init = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_VAR) {
        init = parse_variable_declaration(ps);
        // 变量声明后面可能有分号，需要跳过
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        // 解析表达式（不是表达式语句，因为后面可能有分号）
        init = parse_expression(ps);
        // 如果后面是分号，跳过它
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    // 解析条件（可选）
    SZrAstNode *cond = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        // 解析表达式（不是表达式语句）
        cond = parse_expression(ps);
        // 如果后面是分号，跳过它
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    // 解析步进（可选）
    SZrAstNode *step = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        step = parse_expression(ps);
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

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
    node->data.forLoop.isStatement = ZR_TRUE; // 默认是语句
    return node;
}

// 解析 foreach 循环

SZrAstNode *parse_foreach_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_FOR);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    expect_token(ps, ZR_TK_VAR);
    consume_token(ps, ZR_TK_VAR);

    // 解析模式（标识符或解构）
    SZrAstNode *pattern = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        pattern = parse_identifier(ps);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        // 对象解构模式 {key1, key2, ...}
        pattern = parse_destructuring_object(ps);
    } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 数组解构模式 [elem1, elem2, ...]
        pattern = parse_destructuring_array(ps);
    } else {
        report_error(ps, "Expected identifier or destructuring pattern");
        return ZR_NULL;
    }

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    expect_token(ps, ZR_TK_IN);
    consume_token(ps, ZR_TK_IN);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

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
    node->data.foreachLoop.isStatement = ZR_TRUE; // 默认是语句
    return node;
}

// 解析 break/continue 语句

SZrAstNode *parse_break_continue_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    TZrBool isBreak = (ps->lexer->t.token == ZR_TK_BREAK);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        expr = parse_expression(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BREAK_CONTINUE_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.breakContinueStatement.isBreak = isBreak;
    node->data.breakContinueStatement.expr = expr;
    return node;
}

// 解析 out 语句（用于生成器表达式）

SZrAstNode *parse_out_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_OUT);
    ZrParser_Lexer_Next(ps->lexer); // 消费 OUT

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange stmtLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_OUT_STATEMENT, stmtLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.outStatement.expr = expr;
    return node;
}

// 解析 throw 语句

SZrAstNode *parse_throw_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_THROW);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_THROW_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.throwStatement.expr = expr;
    return node;
}

// 解析 try-catch-finally 语句

SZrAstNode *parse_try_catch_finally_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_TRY);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析 catch 子句（可重复）
    SZrAstNodeArray *catchClauses = ZR_NULL;
    while (consume_token(ps, ZR_TK_CATCH)) {
        SZrFileRange catchStartLoc = get_current_location(ps);
        SZrAstNodeArray *catchPattern = ZR_NULL;
        SZrAstNode *catchBlock = ZR_NULL;
        SZrAstNode *catchClauseNode;

        expect_token(ps, ZR_TK_LPAREN);
        consume_token(ps, ZR_TK_LPAREN);

        catchPattern = parse_parameter_list(ps);

        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);

        catchBlock = parse_block(ps);
        if (catchBlock == ZR_NULL) {
            if (catchPattern != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchPattern);
            }
            if (catchClauses != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchClauses);
            }
            return ZR_NULL;
        }

        if (catchClauses == ZR_NULL) {
            catchClauses = ZrParser_AstNodeArray_New(ps->state, 2);
            if (catchClauses == ZR_NULL) {
                if (catchPattern != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, catchPattern);
                }
                ZrParser_Ast_Free(ps->state, catchBlock);
                return ZR_NULL;
            }
        }

        catchClauseNode = create_ast_node(ps,
                                          ZR_AST_CATCH_CLAUSE,
                                          ZrParser_FileRange_Merge(catchStartLoc, get_current_location(ps)));
        if (catchClauseNode == ZR_NULL) {
            if (catchPattern != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchPattern);
            }
            ZrParser_Ast_Free(ps->state, catchBlock);
            return ZR_NULL;
        }

        catchClauseNode->data.catchClause.pattern = catchPattern;
        catchClauseNode->data.catchClause.block = catchBlock;
        ZrParser_AstNodeArray_Add(ps->state, catchClauses, catchClauseNode);
    }

    // 解析 finally（可选）
    SZrAstNode *finallyBlock = ZR_NULL;
    if (consume_token(ps, ZR_TK_FINALLY)) {
        finallyBlock = parse_block(ps);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange tryLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryLoc);
    if (node == ZR_NULL) {
        if (catchClauses != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, catchClauses);
        }
        return ZR_NULL;
    }

    node->data.tryCatchFinallyStatement.block = block;
    node->data.tryCatchFinallyStatement.catchClauses = catchClauses;
    node->data.tryCatchFinallyStatement.finallyBlock = finallyBlock;
    return node;
}

SZrAstNode *parse_using_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *resource = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    TZrBool isBlockScoped = ZR_FALSE;
    SZrAstNode *node;

    expect_token(ps, ZR_TK_USING);
    ZrParser_Lexer_Next(ps->lexer);

    if (consume_token(ps, ZR_TK_LPAREN)) {
        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            return ZR_NULL;
        }

        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);

        body = parse_block(ps);
        if (body == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, resource);
            return ZR_NULL;
        }

        isBlockScoped = ZR_TRUE;
    } else {
        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            return ZR_NULL;
        }

        expect_token(ps, ZR_TK_SEMICOLON);
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    node = create_ast_node(ps, ZR_AST_USING_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, resource);
        ZrParser_Ast_Free(ps->state, body);
        return ZR_NULL;
    }

    node->data.usingStatement.resource = resource;
    node->data.usingStatement.body = body;
    node->data.usingStatement.isBlockScoped = isBlockScoped;
    return node;
}

// 解析语句（入口函数）

SZrAstNode *parse_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    switch (token) {
        case ZR_TK_LBRACE:
            return parse_block(ps);

        case ZR_TK_RETURN:
            return parse_return_statement(ps);

        case ZR_TK_VAR:
            return parse_variable_declaration(ps);

        case ZR_TK_USING:
            if (is_expression_level_using_new(ps)) {
                return parse_expression_statement(ps);
            }
            return parse_using_statement(ps);

        case ZR_TK_IF:
            return parse_if_expression(ps);

        case ZR_TK_SWITCH: {
            SZrAstNode *switchNode = parse_switch_expression(ps);
            if (switchNode != ZR_NULL) {
                switchNode->data.switchExpression.isStatement = ZR_TRUE;
            }
            return switchNode;
        }

        case ZR_TK_WHILE: {
            SZrAstNode *whileNode = parse_while_loop(ps);
            if (whileNode != ZR_NULL) {
                whileNode->data.whileLoop.isStatement = ZR_TRUE;
            }
            return whileNode;
        }

        case ZR_TK_FOR: {
            // 判断是 for 还是 foreach
            // FOR ( VAR ... IN ... ) 是 foreach
            // FOR ( ... ; ... ; ... ) 是 for
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

            // 跳过 FOR 和 LPAREN
            ZrParser_Lexer_Next(ps->lexer); // 消费 FOR
            if (ps->lexer->t.token == ZR_TK_LPAREN) {
                ZrParser_Lexer_Next(ps->lexer); // 消费 LPAREN
                if (ps->lexer->t.token == ZR_TK_VAR) {
                    // 可能是 foreach，继续检查
                    ZrParser_Lexer_Next(ps->lexer); // 消费 VAR
                    // 跳过模式（标识符、类型注解等）
                    while (ps->lexer->t.token != ZR_TK_IN && ps->lexer->t.token != ZR_TK_COLON &&
                           ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_EOS) {
                        ZrParser_Lexer_Next(ps->lexer);
                    }
                    if (ps->lexer->t.token == ZR_TK_IN) {
                        // 是 foreach，恢复状态并解析
                        ps->lexer->currentPos = savedPos;
                        ps->lexer->currentChar = savedChar;
                        ps->lexer->lineNumber = savedLine;
                        ps->lexer->lastLine = savedLastLine;
                        ps->lexer->t = savedToken;
                        ps->lexer->lookahead = savedLookahead;
                        ps->lexer->lookaheadPos = savedLookaheadPos;
                        ps->lexer->lookaheadChar = savedLookaheadChar;
                        ps->lexer->lookaheadLine = savedLookaheadLine;
                        ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                        SZrAstNode *loop = parse_foreach_loop(ps);
                        if (loop != ZR_NULL) {
                            loop->data.foreachLoop.isStatement = ZR_TRUE;
                        }
                        return loop;
                    }
                }
            }
            // 恢复状态并解析 for
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
            SZrAstNode *loop = parse_for_loop(ps);
            if (loop != ZR_NULL) {
                loop->data.forLoop.isStatement = ZR_TRUE;
            }
            return loop;
        }

        case ZR_TK_BREAK:
        case ZR_TK_CONTINUE:
            return parse_break_continue_statement(ps);

        case ZR_TK_OUT:
            return parse_out_statement(ps);

        case ZR_TK_THROW:
            return parse_throw_statement(ps);

        case ZR_TK_TRY:
            return parse_try_catch_finally_statement(ps);

        case ZR_TK_PERCENT:
            if (current_percent_directive_equals(ps, "module")) {
                return parse_module_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "async")) {
                return parse_reserved_async_function_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "compileTime")) {
                return parse_compile_time_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "extern")) {
                return parse_extern_block(ps);
            }
            if (current_percent_directive_equals(ps, "test")) {
                return parse_test_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "owned")) {
                return parse_owned_class_declaration(ps);
            }
            return parse_expression_statement(ps);

        default:
            if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
                SZrAstNode *funcDecl = try_parse_prefixed_function_declaration(ps);
                if (funcDecl != ZR_NULL) {
                    return funcDecl;
                }
            }
            // 检查是否是函数声明（identifier(params) { statements} 风格）
            if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
                // 查看下一个 token 判断是否是函数声明
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                    // 可能是函数声明，需要进一步检查后面是否有函数体 { }
                    // 保存状态以便向前看
                    TZrSize savedPos = ps->lexer->currentPos;
                    TZrInt32 savedChar = ps->lexer->currentChar;
                    TZrInt32 savedLine = ps->lexer->lineNumber;
                    TZrInt32 savedLastLine = ps->lexer->lastLine;
                    SZrToken savedToken = ps->lexer->t;
                    SZrToken savedLookahead = ps->lexer->lookahead;
                    TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
                    TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
                    TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
                    TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

                    // 跳过标识符和左括号（或泛型）
                    ZrParser_Lexer_Next(ps->lexer); // 消费标识符
                    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
                        // 跳过泛型参数
                        while (ps->lexer->t.token != ZR_TK_GREATER_THAN && ps->lexer->t.token != ZR_TK_EOS) {
                            ZrParser_Lexer_Next(ps->lexer);
                        }
                        if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
                            ZrParser_Lexer_Next(ps->lexer); // 消费 >
                        }
                    }
                    if (ps->lexer->t.token == ZR_TK_LPAREN) {
                        ZrParser_Lexer_Next(ps->lexer); // 消费 (
                        // 跳过参数列表（直到遇到 )）
                        while (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_EOS) {
                            ZrParser_Lexer_Next(ps->lexer);
                        }
                        if (ps->lexer->t.token == ZR_TK_RPAREN) {
                            ZrParser_Lexer_Next(ps->lexer); // 消费 )
                            // 跳过可选的返回类型注解
                            if (ps->lexer->t.token == ZR_TK_COLON) {
                                ZrParser_Lexer_Next(ps->lexer); // 消费 :
                                // 跳过类型
                                while (ps->lexer->t.token != ZR_TK_LBRACE && ps->lexer->t.token != ZR_TK_EOS &&
                                       ps->lexer->t.token != ZR_TK_SEMICOLON) {
                                    ZrParser_Lexer_Next(ps->lexer);
                                }
                            }
                            // 检查下一个 token 是否是函数体 { }
                            if (ps->lexer->t.token == ZR_TK_LBRACE) {
                                // 是函数声明，恢复状态并解析
                                ps->lexer->currentPos = savedPos;
                                ps->lexer->currentChar = savedChar;
                                ps->lexer->lineNumber = savedLine;
                                ps->lexer->lastLine = savedLastLine;
                                ps->lexer->t = savedToken;
                                ps->lexer->lookahead = savedLookahead;
                                ps->lexer->lookaheadPos = savedLookaheadPos;
                                ps->lexer->lookaheadChar = savedLookaheadChar;
                                ps->lexer->lookaheadLine = savedLookaheadLine;
                                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                                SZrAstNode *funcDecl = parse_function_declaration(ps);
                                // 如果解析失败，直接返回 NULL（无论是否有错误）
                                // 因为如果标识符后跟括号和函数体，它应该是函数声明，不应该回退到表达式解析
                                if (funcDecl == ZR_NULL) {
                                    return ZR_NULL;
                                }
                                return funcDecl;
                            }
                        }
                    }
                    // 恢复状态
                    ps->lexer->currentPos = savedPos;
                    ps->lexer->currentChar = savedChar;
                    ps->lexer->lineNumber = savedLine;
                    ps->lexer->lastLine = savedLastLine;
                    ps->lexer->t = savedToken;
                    ps->lexer->lookahead = savedLookahead;
                    ps->lexer->lookaheadPos = savedLookaheadPos;
                    ps->lexer->lookaheadChar = savedLookaheadChar;
                    ps->lexer->lookaheadLine = savedLookaheadLine;
                    ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                }
            }
            // 尝试解析为表达式语句
            return parse_expression_statement(ps);
    }
}

// ==================== 顶层解析 ====================

// 解析顶层语句

SZrAstNode *parse_top_level_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    // 检查是否是可见性修饰符（pub/pri/pro），后面应该跟 var/struct/class/interface/enum
    if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO) {
        // 使用 peek_token 查看下一个 token，不消费当前 token
        EZrToken nextToken = peek_token(ps);

        if (nextToken == ZR_TK_PERCENT) {
            SZrAstNode *ownedClass = ZR_NULL;
            EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

            ZrParser_Lexer_Next(ps->lexer);
            ZrParser_Lexer_Next(ps->lexer);
            if (current_identifier_equals(ps, "owned")) {
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;

                accessModifier = parse_access_modifier(ps);
                ownedClass = parse_owned_class_declaration(ps);
                if (ownedClass != ZR_NULL && ownedClass->type == ZR_AST_CLASS_DECLARATION) {
                    ownedClass->data.classDeclaration.accessModifier = accessModifier;
                }
                return ownedClass;
            }
            if (current_identifier_equals(ps, "async")) {
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                return parse_reserved_async_function_declaration(ps);
            }

            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
        }

        if (parser_function_declaration_starts_here(ps)) {
            return parse_function_declaration(ps);
        }

        // 根据下一个 token 调用相应的解析函数（它们会自己解析可见性修饰符）
        switch (nextToken) {
            case ZR_TK_VAR:
                return parse_variable_declaration(ps);
            case ZR_TK_STRUCT:
                return parse_struct_declaration(ps);
            case ZR_TK_CLASS:
                return parse_class_declaration(ps);
            case ZR_TK_INTERFACE:
                return parse_interface_declaration(ps);
            case ZR_TK_ENUM:
                return parse_enum_declaration(ps);
            default:
                // 如果后面不是声明类型，报告错误
                report_error(ps, "Expected declaration after access modifier");
                return ZR_NULL;
        }
    }

    switch (token) {
        case ZR_TK_MODULE:
            report_error(ps, "Legacy module syntax is not supported; use %module");
            skip_to_semicolon_or_eos(ps);
            return ZR_NULL;

        case ZR_TK_VAR:
            return parse_variable_declaration(ps);

        case ZR_TK_USING:
            return parse_using_statement(ps);

        case ZR_TK_STRUCT:
            return parse_struct_declaration(ps);

        case ZR_TK_CLASS:
            return parse_class_declaration(ps);

        case ZR_TK_INTERFACE:
            return parse_interface_declaration(ps);

        case ZR_TK_ENUM:
            return parse_enum_declaration(ps);

        case ZR_TK_PERCENT:
            if (current_percent_directive_equals(ps, "module")) {
                return parse_module_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "async")) {
                return parse_reserved_async_function_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "compileTime")) {
                return parse_compile_time_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "extern")) {
                return parse_extern_block(ps);
            }
            if (current_percent_directive_equals(ps, "test")) {
                return parse_test_declaration(ps);
            }
            if (current_percent_directive_equals(ps, "owned")) {
                return parse_owned_class_declaration(ps);
            }
            return parse_expression_statement(ps);

        case ZR_TK_INTERMEDIATE:
            return parse_intermediate_statement(ps);

        case ZR_TK_IF: {
            SZrAstNode *ifNode = parse_if_expression(ps);
            if (ifNode != ZR_NULL) {
                ifNode->data.ifExpression.isStatement = ZR_TRUE;
            }
            return ifNode;
        }

        case ZR_TK_WHILE: {
            SZrAstNode *whileNode = parse_while_loop(ps);
            if (whileNode != ZR_NULL) {
                whileNode->data.whileLoop.isStatement = ZR_TRUE;
            }
            return whileNode;
        }

        case ZR_TK_FOR: {
            // 判断是 for 还是 foreach
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

            // 跳过 for 和 (
            ZrParser_Lexer_Next(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_LPAREN) {
                ZrParser_Lexer_Next(ps->lexer);
            }

            // 检查是否是 foreach (var x in ...)
            if (ps->lexer->t.token == ZR_TK_VAR) {
                // 恢复状态并解析 foreach
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                SZrAstNode *foreachNode = parse_foreach_loop(ps);
                if (foreachNode != ZR_NULL) {
                    foreachNode->data.foreachLoop.isStatement = ZR_TRUE;
                }
                return foreachNode;
            }

            // 恢复状态并解析 for
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
            SZrAstNode *forNode = parse_for_loop(ps);
            if (forNode != ZR_NULL) {
                forNode->data.forLoop.isStatement = ZR_TRUE;
            }
            return forNode;
        }

        case ZR_TK_SWITCH: {
            SZrAstNode *switchNode = parse_switch_expression(ps);
            if (switchNode != ZR_NULL) {
                switchNode->data.switchExpression.isStatement = ZR_TRUE;
            }
            return switchNode;
        }

        case ZR_TK_RETURN:
            return parse_return_statement(ps);

        case ZR_TK_BREAK:
        case ZR_TK_CONTINUE:
            return parse_break_continue_statement(ps);

        case ZR_TK_OUT:
            return parse_out_statement(ps);

        case ZR_TK_THROW:
            return parse_throw_statement(ps);

        case ZR_TK_TRY:
            return parse_try_catch_finally_statement(ps);

        default:
            if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
                SZrAstNode *funcDecl = try_parse_prefixed_function_declaration(ps);
                if (funcDecl != ZR_NULL) {
                    return funcDecl;
                }
            }
            if (token == ZR_TK_PERCENT) {
                if (current_percent_directive_equals(ps, "async")) {
                    return parse_reserved_async_function_declaration(ps);
                }
                if (current_percent_directive_equals(ps, "compileTime")) {
                    return parse_compile_time_declaration(ps);
                }
                if (current_percent_directive_equals(ps, "extern")) {
                    return parse_extern_block(ps);
                }
                if (current_percent_directive_equals(ps, "test")) {
                    return parse_test_declaration(ps);
                }
                if (current_percent_directive_equals(ps, "owned")) {
                    return parse_owned_class_declaration(ps);
                }
            }
            // 检查是否是装饰器（# ... #），后面应该跟 class/struct/function 等
            if (token == ZR_TK_SHARP) {
                SZrParserCursor cursor;
                SZrAstNode *decorator;
                SZrAstNodeArray *decorators;
                EZrToken nextToken;
                EZrToken declarationToken = ZR_TK_EOS;
                TZrBool decoratorStartsFunction = ZR_FALSE;

                // 先向前看整串 leading decorators 后面的声明类型，再回到起点让声明解析函数处理装饰器本身。
                save_parser_cursor(ps, &cursor);
                decorators = parse_leading_decorators(ps);
                if (decorators == ZR_NULL || decorators->count == 0) {
                    if (decorators != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, decorators);
                    }
                    restore_parser_cursor(ps, &cursor);
                    return ZR_NULL;
                }
                nextToken = ps->lexer->t.token;
                decoratorStartsFunction = parser_function_declaration_starts_here(ps);
                if (nextToken == ZR_TK_PUB || nextToken == ZR_TK_PRI || nextToken == ZR_TK_PRO) {
                    declarationToken = peek_token(ps);
                }
                ZrParser_AstNodeArray_Free(ps->state, decorators);
                restore_parser_cursor(ps, &cursor);

                if (nextToken == ZR_TK_CLASS) {
                    return parse_class_declaration(ps);
                } else if (nextToken == ZR_TK_STRUCT) {
                    return parse_struct_declaration(ps);
                } else if (nextToken == ZR_TK_PUB || nextToken == ZR_TK_PRI || nextToken == ZR_TK_PRO) {
                    if (declarationToken == ZR_TK_CLASS) {
                        return parse_class_declaration(ps);
                    } else if (declarationToken == ZR_TK_STRUCT) {
                        return parse_struct_declaration(ps);
                    }
                }

                if (decoratorStartsFunction) {
                    return parse_function_declaration(ps);
                }

                {
                    SZrAstNode *decoratedFunction = try_parse_function_declaration_from_current(ps);
                    if (decoratedFunction != ZR_NULL) {
                        return decoratedFunction;
                    }
                }
                // 如果后面不是声明，则作为表达式语句处理
                // 但装饰器表达式通常不应该单独出现，这里可能需要错误处理
                // TODO: 暂时先返回装饰器作为表达式语句
                decorator = parse_decorator_expression(ps);
                if (decorator == ZR_NULL) {
                    return ZR_NULL;
                }
                SZrAstNode *stmt = create_ast_node(ps, ZR_AST_EXPRESSION_STATEMENT, decorator->location);
                if (stmt != ZR_NULL) {
                    stmt->data.expressionStatement.expr = decorator;
                }
                return stmt;
            }
            // 检查是否是函数声明（标识符后跟括号或泛型）
            if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
                // 查看下一个 token 判断是否是函数声明
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                    SZrParserCursor cursor;
                    TZrBool savedSuppressErrorOutput = ps->suppressErrorOutput;
                    SZrAstNode *funcDecl;

                    save_parser_cursor(ps, &cursor);
                    ps->suppressErrorOutput = ZR_TRUE;
                    ps->hasError = ZR_FALSE;
                    ps->errorMessage = ZR_NULL;

                    funcDecl = parse_function_declaration(ps);
                    ps->suppressErrorOutput = savedSuppressErrorOutput;
                    if (funcDecl != ZR_NULL && !ps->hasError) {
                        ps->hasError = cursor.hasError;
                        ps->errorMessage = cursor.errorMessage;
                        return funcDecl;
                    }

                    if (funcDecl != ZR_NULL) {
                        ZrParser_Ast_Free(ps->state, funcDecl);
                    }

                    restore_parser_cursor(ps, &cursor);
                }
            }
            // 尝试解析为表达式语句
            return parse_expression_statement(ps);
    }
}

// 解析脚本
