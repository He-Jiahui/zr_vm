#include "parser_internal.h"

// 无关键字函数与调用表达式共享起始形态；仅预读头部和函数体开括号，随后恢复游标。
static TZrBool parser_keywordless_function_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrInt32 parenDepth = 0;
    TZrInt32 genericDepth = 0;
    TZrBool result = ZR_FALSE;

    if (ps == ZR_NULL ||
        (ps->lexer->t.token != ZR_TK_IDENTIFIER && ps->lexer->t.token != ZR_TK_TEST)) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        do {
            if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
                genericDepth++;
            } else if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
                genericDepth--;
            }
            ZrParser_Lexer_Next(ps->lexer);
        } while (genericDepth > 0 && ps->lexer->t.token != ZR_TK_EOS);
    }
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        restore_parser_cursor(ps, &cursor);
        return ZR_FALSE;
    }

    do {
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            parenDepth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            parenDepth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    } while (parenDepth > 0 && ps->lexer->t.token != ZR_TK_EOS);

    if (parenDepth == 0) {
        while (ps->lexer->t.token != ZR_TK_LBRACE &&
               ps->lexer->t.token != ZR_TK_SEMICOLON &&
               ps->lexer->t.token != ZR_TK_EOS) {
            ZrParser_Lexer_Next(ps->lexer);
        }
        result = ps->lexer->t.token == ZR_TK_LBRACE;
    }

    restore_parser_cursor(ps, &cursor);
    return result;
}

// 分派器借此选择声明解析器；包含可见性修饰符与无关键字函数两种入口。
static TZrBool parser_function_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    EZrToken token;
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

    if (token == ZR_TK_FN) {
        isFunction = ZR_TRUE;
    } else if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
        isFunction = ZR_TRUE;
    } else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
        isFunction = parser_keywordless_function_declaration_starts_here(ps);
    }

    restore_parser_cursor(ps, &cursor);
    return isFunction;
}

// 预留 async fn 的专用诊断路径，探测不得消费主解析游标。
static TZrBool parser_async_function_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool isAsync = ZR_FALSE;

    if (ps == ZR_NULL) {
        return ZR_FALSE;
    }
    save_parser_cursor(ps, &cursor);
    if (ps->lexer->t.token == ZR_TK_PUB ||
        ps->lexer->t.token == ZR_TK_PRI ||
        ps->lexer->t.token == ZR_TK_PRO) {
        parse_access_modifier(ps);
    }
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        current_identifier_equals(ps, "async")) {
        ZrParser_Lexer_Next(ps->lexer);
        isAsync = ps->lexer->t.token == ZR_TK_FN;
    }
    restore_parser_cursor(ps, &cursor);
    return isAsync;
}

// native extern 是上下文关键字组合；预读后交给 extern 块解析器。
static TZrBool parser_native_extern_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool isNativeExtern = ZR_FALSE;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "native")) {
        return ZR_FALSE;
    }
    save_parser_cursor(ps, &cursor);
    ZrParser_Lexer_Next(ps->lexer);
    isNativeExtern = ps->lexer->t.token == ZR_TK_IDENTIFIER &&
                     current_identifier_equals(ps, "extern");
    restore_parser_cursor(ps, &cursor);
    return isNativeExtern;
}

// 顶层分派需越过装饰器及修饰符识别 class；试探生成的装饰器 AST 不向外转移。
static TZrBool parser_class_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool isClass = ZR_FALSE;
    SZrAstNodeArray *decorators = ZR_NULL;

    if (ps == ZR_NULL) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);

    if (ps->lexer->t.token == ZR_TK_SHARP) {
        // TODO: 装饰器试探可能已调用诊断回调；需检查回退后的正式解析是否重复报告。
        decorators = parse_leading_decorators(ps);
        if (decorators == ZR_NULL) {
            restore_parser_cursor(ps, &cursor);
            return ZR_FALSE;
        }
        free_ast_node_array_with_elements(ps->state, decorators);
    }

    if (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || ps->lexer->t.token == ZR_TK_PRO) {
        parse_access_modifier(ps);
    }

    parse_declaration_modifier_flags(ps,
                                     ZR_DECLARATION_MODIFIER_ABSTRACT |
                                             ZR_DECLARATION_MODIFIER_FINAL);
    isClass = ps->lexer->t.token == ZR_TK_CLASS;

    restore_parser_cursor(ps, &cursor);
    return isClass;
}

// 两种 for 共用首 token，分派前仅预读头部中的 in/分号/赋值，不改变解析位置。
static TZrBool parser_for_header_should_parse_foreach(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool parseAsForeach = ZR_FALSE;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_FOR) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        restore_parser_cursor(ps, &cursor);
        return ZR_FALSE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_VAR && ps->lexer->t.token != ZR_TK_LET) {
        restore_parser_cursor(ps, &cursor);
        return ZR_FALSE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    while (ps->lexer->t.token != ZR_TK_EOS &&
           ps->lexer->t.token != ZR_TK_RPAREN &&
           ps->lexer->t.token != ZR_TK_SEMICOLON) {
        if (ps->lexer->t.token == ZR_TK_IN) {
            parseAsForeach = ZR_TRUE;
            break;
        }
        if (ps->lexer->t.token == ZR_TK_EQUALS) {
            parseAsForeach = ZR_FALSE;
            break;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (ps->lexer->t.token == ZR_TK_RPAREN || ps->lexer->t.token == ZR_TK_EOS) {
        parseAsForeach = ZR_TRUE;
    }

    restore_parser_cursor(ps, &cursor);
    return parseAsForeach;
}

// 装饰器之后的函数形态可能与表达式重叠；静默试探失败时还原游标和诊断回调。
// 成功返回的 AST 由分派调用者接管。
static SZrAstNode *try_parse_function_declaration_from_current(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    TZrParserErrorCallback savedErrorCallback;
    TZrParserStructuredErrorCallback savedStructuredErrorCallback;
    TZrPtr savedErrorUserData;
    SZrAstNode *funcDecl;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    savedSuppressErrorOutput = ps->suppressErrorOutput;
    savedErrorCallback = ps->errorCallback;
    savedStructuredErrorCallback = ps->structuredErrorCallback;
    savedErrorUserData = ps->errorUserData;
    ps->suppressErrorOutput = ZR_TRUE;
    ps->errorCallback = ZR_NULL;
    ps->structuredErrorCallback = ZR_NULL;
    ps->errorUserData = ZR_NULL;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    funcDecl = parse_function_declaration(ps);
    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->errorCallback = savedErrorCallback;
    ps->structuredErrorCallback = savedStructuredErrorCallback;
    ps->errorUserData = savedErrorUserData;
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

// 块解析是语句递归入口；declarationKind 仅改变缺失右花括号的诊断上下文。
// 成功返回的块节点持有已加入的语句数组及其所有元素。
static SZrAstNode *parse_block_impl(SZrParserState *ps, const TZrChar *declarationKind) {
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

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        if (ps->lexer->t.token == ZR_TK_EOS) {
            if (declarationKind != ZR_NULL) {
                report_missing_declaration_body_close(ps, declarationKind, startLoc);
            } else {
                report_missing_block_close(ps, startLoc);
            }
        } else {
            expect_token(ps, ZR_TK_RBRACE);
        }
        // BUG: 至少一个语句已加入时，普通数组 Free 不释放元素 AST，缺失 } 会泄漏它们。
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange blockLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (node == ZR_NULL) {
        // BUG: 节点分配失败时只释放数组容器，已解析语句仍悬空。
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.block.body = statements;
    node->data.block.isStatement = ZR_TRUE;
    return node;
}

// 普通语句体、控制流体及函数体共用的块入口，调用前必须位于左花括号。
SZrAstNode *parse_block(SZrParserState *ps) {
    return parse_block_impl(ps, ZR_NULL);
}

// 声明解析器使用此入口，使缺失右花括号的诊断指向具体声明种类。
SZrAstNode *parse_declaration_body_block(SZrParserState *ps, const TZrChar *declarationKind) {
    return parse_block_impl(ps, declarationKind);
}

// 解析表达式语句

// 在声明/控制流预读未接管 token 后兜底解析表达式；分号错误仍交给统一诊断器。
SZrAstNode *parse_expression_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        report_missing_statement_semicolon(ps, "expression", get_current_token_location(ps));
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_EXPRESSION_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        // BUG: 表达式已成功构造但语句节点分配失败时，expr 未释放。
        return ZR_NULL;
    }

    node->data.expressionStatement.expr = expr;
    return node;
}

// 装饰器并非声明前缀时，将其保留为表达式语句供后续阶段处理。
static SZrAstNode *parse_decorator_expression_statement(SZrParserState *ps) {
    SZrAstNode *decorator = parse_decorator_expression(ps);
    SZrAstNode *stmt;

    if (decorator == ZR_NULL) {
        return ZR_NULL;
    }

    stmt = create_ast_node(ps, ZR_AST_EXPRESSION_STATEMENT, decorator->location);
    if (stmt == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, decorator);
        return ZR_NULL;
    }

    stmt->data.expressionStatement.expr = decorator;
    return stmt;
}

// 局部 # 起始语句先试函数声明，失败才进入装饰器表达式路径。
static SZrAstNode *parse_decorated_statement(SZrParserState *ps) {
    SZrAstNode *decoratedFunction = try_parse_function_declaration_from_current(ps);

    if (decoratedFunction != ZR_NULL) {
        return decoratedFunction;
    }

    return parse_decorator_expression_statement(ps);
}

// 解析返回语句

// 返回语句可携带 ref 标记及可选值；此处仅记录语法，引用有效性留给语义阶段。
SZrAstNode *parse_return_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrFileRange referenceLocation;
    TZrBool isReferenceReturn = ZR_FALSE;

    memset(&referenceLocation, 0, sizeof(referenceLocation));
    expect_token(ps, ZR_TK_RETURN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_REF) {
        isReferenceReturn = ZR_TRUE;
        referenceLocation = get_current_token_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    }

    SZrAstNode *expr = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        expr = parse_expression(ps);
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        report_missing_statement_semicolon(ps, "return", get_current_token_location(ps));
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_RETURN_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        // BUG: 已解析返回值未移交节点时直接返回，非空 expr 会泄漏。
        return ZR_NULL;
    }

    node->data.returnStatement.expr = expr;
    node->data.returnStatement.isReferenceReturn = isReferenceReturn;
    node->data.returnStatement.referenceLocation = referenceLocation;
    return node;
}

// 解析 switch 表达式/语句

// 语句/顶层分派构造 switch 表达式形 AST；case 头部先试 move 模式再回退通用表达式。
// expr、各 case 和默认分支只有在 switchNode 成功建立后才转移所有权。
SZrAstNode *parse_switch_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_SWITCH);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_RPAREN) {
        report_missing_condition(ps, "switch", get_current_token_location(ps));
        return ZR_NULL;
    }

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_condition_close(ps, "switch", get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, expr);
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "switch statement", get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, expr);
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *cases = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    // BUG: cases 分配失败仍可继续解析；Add 对空数组静默返回，case AST 会丢失。
    SZrAstNode *defaultCase = ZR_NULL;

    // 解析 switch cases
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        if (consume_token(ps, ZR_TK_LPAREN)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                // 默认 case
                ZrParser_Lexer_Next(ps->lexer);
                if (ps->lexer->t.token != ZR_TK_LBRACE) {
                    report_missing_statement_body_open(ps, "switch default", get_current_token_location(ps));
                    ZrParser_Ast_Free(ps->state, expr);
                    // BUG: 此前已解析的 case 元素未释放；可能还遗失先前 defaultCase。
                    ZrParser_AstNodeArray_Free(ps->state, cases);
                    return ZR_NULL;
                }

                SZrAstNode *block = parse_block(ps);
                if (block != ZR_NULL) {
                    SZrFileRange defaultLoc = get_current_location(ps);
                    // BUG: 第二个默认分支直接覆盖前一个节点，既丢失其块，也让后续阶段看不到重复分支。
                    defaultCase = create_ast_node(ps, ZR_AST_SWITCH_DEFAULT, defaultLoc);
                    if (defaultCase != ZR_NULL) {
                        defaultCase->data.switchDefault.block = block;
                    }
                }
            } else {
                // 普通 case
                SZrAstNode *value = try_parse_switch_move_variant_pattern_case(ps);
                if (value == ZR_NULL) {
                    value = parse_expression(ps);
                    value = try_parse_switch_struct_variant_payload_case(ps, value);
                }
                if (ps->lexer->t.token != ZR_TK_RPAREN) {
                    report_missing_switch_case_header_close(ps, get_current_token_location(ps));
                    if (value != ZR_NULL) {
                        ZrParser_Ast_Free(ps->state, value);
                    }
                    ZrParser_Ast_Free(ps->state, expr);
                    // BUG: 先前已加入的 case 元素不会由数组容器的 Free 递归释放。
                    ZrParser_AstNodeArray_Free(ps->state, cases);
                    return ZR_NULL;
                }

                consume_token(ps, ZR_TK_RPAREN);
                if (ps->lexer->t.token != ZR_TK_LBRACE) {
                    report_missing_statement_body_open(ps, "switch case", get_current_token_location(ps));
                    if (value != ZR_NULL) {
                        ZrParser_Ast_Free(ps->state, value);
                    }
                    ZrParser_Ast_Free(ps->state, expr);
                    // BUG: 此出口仍可能持有已加入的 case 和 defaultCase。
                    ZrParser_AstNodeArray_Free(ps->state, cases);
                    return ZR_NULL;
                }

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
                // BUG: block 或 caseNode 构造失败时，value/已构造的 block 无清理路径。
            }
        } else {
            break;
        }
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        report_missing_switch_body_close(ps, get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, expr);
        // BUG: 普通数组 Free 不释放已加入的 case AST。
        ZrParser_AstNodeArray_Free(ps->state, cases);
        if (defaultCase != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, defaultCase);
        }
        return ZR_NULL;
    }

    consume_token(ps, ZR_TK_RBRACE);

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
    // BUG: 最终节点分配失败时 expr/cases/defaultCase 均未释放。
    return ZR_NULL;
}

// 解析 if 表达式/语句

// if 与 else if 共享 AST 形态；块为必需，条件/两个分支在成功时移交给 if 节点。
SZrAstNode *parse_if_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_IF);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    if (ps->lexer->t.token == ZR_TK_RPAREN) {
        report_missing_condition(ps, "if", get_current_token_location(ps));
        return ZR_NULL;
    }

    SZrAstNode *condition = parse_expression(ps);
    if (condition == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_condition_close(ps, "if", get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, condition);
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "if statement", get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, condition);
        return ZR_NULL;
    }

    SZrAstNode *thenExpr = parse_block(ps);
    if (thenExpr == ZR_NULL) {
        // BUG: 条件 AST 已分配，then 块失败后未释放。
        return ZR_NULL;
    }

    SZrAstNode *elseExpr = ZR_NULL;
    if (consume_token(ps, ZR_TK_ELSE)) {
        if (ps->lexer->t.token == ZR_TK_IF) {
            elseExpr = parse_if_expression(ps);
        } else {
            if (ps->lexer->t.token != ZR_TK_LBRACE) {
                report_missing_statement_body_open(ps, "else statement", get_current_token_location(ps));
                ZrParser_Ast_Free(ps->state, condition);
                ZrParser_Ast_Free(ps->state, thenExpr);
                return ZR_NULL;
            }
            elseExpr = parse_block(ps);
            // TODO: else 块失败仍可返回无 else 的 AST；核对脚本错误恢复是否禁止编译此残缺节点。
        }
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange ifLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_IF_EXPRESSION, ifLoc);
    if (node == ZR_NULL) {
        // BUG: 最终节点分配失败时条件与已解析分支均未释放。
        return ZR_NULL;
    }

    node->data.ifExpression.condition = condition;
    node->data.ifExpression.thenExpr = thenExpr;
    node->data.ifExpression.elseExpr = elseExpr;
    node->data.ifExpression.isStatement = ZR_TRUE; // 默认是语句
    return node;
}

// 解析 while 循环

// while 从语句分派进入；条件与块均由成功返回的循环节点负责释放。
SZrAstNode *parse_while_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_WHILE);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    if (ps->lexer->t.token == ZR_TK_RPAREN) {
        report_missing_condition(ps, "while", get_current_token_location(ps));
        return ZR_NULL;
    }

    SZrAstNode *cond = parse_expression(ps);
    if (cond == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_condition_close(ps, "while", get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, cond);
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "while statement", get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, cond);
        return ZR_NULL;
    }

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        // BUG: 块失败时 cond 仍由本函数持有，却直接返回。
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_WHILE_LOOP, loopLoc);
    if (node == ZR_NULL) {
        // BUG: 最终节点分配失败时 cond 和 block 均未释放。
        return ZR_NULL;
    }

    node->data.whileLoop.cond = cond;
    node->data.whileLoop.block = block;
    node->data.whileLoop.isStatement = ZR_TRUE; // 默认是语句
    return node;
}

// 解析 break/continue 语句

// break/continue 共用 AST；同一行的可选表达式先保留给后续语义阶段判断合法性。
SZrAstNode *parse_break_continue_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    TZrBool isBreak = (ps->lexer->t.token == ZR_TK_BREAK);
    const TZrChar *statementKind = isBreak ? "break" : "continue";
    TZrBool reportedMissingSemicolon = ZR_FALSE;
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        SZrFileRange nextTokenLocation = get_current_token_location(ps);
        if (nextTokenLocation.start.line > startLoc.start.line) {
            report_missing_statement_semicolon(ps, statementKind, nextTokenLocation);
            reportedMissingSemicolon = ZR_TRUE;
        } else {
            expr = parse_expression(ps);
        }
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        if (!reportedMissingSemicolon) {
            report_missing_statement_semicolon(ps, statementKind, get_current_token_location(ps));
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BREAK_CONTINUE_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        // BUG: 可选表达式已分配时，节点分配失败会遗失 expr。
        return ZR_NULL;
    }

    node->data.breakContinueStatement.isBreak = isBreak;
    node->data.breakContinueStatement.expr = expr;
    return node;
}

// 解析 out 语句（用于生成器表达式）

// TODO: 当前两级语句分派均拒绝旧 out 语法，此入口无生产调用；需确认迁移路径后删除或限定用途。
SZrAstNode *parse_out_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_OUT);
    ZrParser_Lexer_Next(ps->lexer); // 消费 OUT

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        report_missing_statement_semicolon(ps, "out", get_current_token_location(ps));
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

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

// throw 保留异常表达式供后续 CFG/异常处理构建；成功节点取得其所有权。
SZrAstNode *parse_throw_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_THROW);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        report_missing_statement_semicolon(ps, "throw", get_current_token_location(ps));
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_THROW_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        // BUG: 异常表达式已构造，节点分配失败后没有释放。
        return ZR_NULL;
    }

    node->data.throwStatement.expr = expr;
    return node;
}

// 解析 try-catch-finally 语句

// 将 try、多个 catch 与可选 finally 合并为一个 AST，交给后续异常控制流构建。
// 成功节点拥有各分支；每个失败出口须处理已解析的先前分支。
SZrAstNode *parse_try_catch_finally_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_TRY);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_statement_body_open(ps, "try statement", get_current_token_location(ps));
        return ZR_NULL;
    }

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

        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            report_missing_catch_pattern_close(ps, get_current_token_location(ps));
            if (catchPattern != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchPattern);
            }
            if (catchClauses != ZR_NULL) {
                // BUG: 之前已加入的 catch 节点不会随数组容器释放；其他同类错误出口亦然。
                ZrParser_AstNodeArray_Free(ps->state, catchClauses);
            }
            ZrParser_Ast_Free(ps->state, block);
            return ZR_NULL;
        }

        consume_token(ps, ZR_TK_RPAREN);

        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            report_missing_statement_body_open(ps, "catch statement", get_current_token_location(ps));
            if (catchPattern != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchPattern);
            }
            if (catchClauses != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchClauses);
            }
            ZrParser_Ast_Free(ps->state, block);
            return ZR_NULL;
        }

        catchBlock = parse_block(ps);
        if (catchBlock == ZR_NULL) {
            if (catchPattern != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchPattern);
            }
            if (catchClauses != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchClauses);
            }
            // BUG: 当前出口遗失先前成功解析的 try block。
            return ZR_NULL;
        }

        if (catchClauses == ZR_NULL) {
            catchClauses = ZrParser_AstNodeArray_New(ps->state, 2);
            if (catchClauses == ZR_NULL) {
                if (catchPattern != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, catchPattern);
                }
                ZrParser_Ast_Free(ps->state, catchBlock);
                // BUG: try block 在 catch 数组分配失败时未释放。
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
            // BUG: try block 和此前的 catch 数组在此出口仍由本函数持有。
            return ZR_NULL;
        }

        catchClauseNode->data.catchClause.pattern = catchPattern;
        catchClauseNode->data.catchClause.block = catchBlock;
        ZrParser_AstNodeArray_Add(ps->state, catchClauses, catchClauseNode);
    }

    // 解析 finally（可选）
    SZrAstNode *finallyBlock = ZR_NULL;
    if (consume_token(ps, ZR_TK_FINALLY)) {
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            report_missing_statement_body_open(ps, "finally statement", get_current_token_location(ps));
            if (catchClauses != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, catchClauses);
            }
            ZrParser_Ast_Free(ps->state, block);
            return ZR_NULL;
        }

        finallyBlock = parse_block(ps);
        // TODO: finally 块失败仍可返回无 finally 的 AST；核对脚本错误恢复是否禁止编译此残缺节点。
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange tryLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryLoc);
    if (node == ZR_NULL) {
        if (catchClauses != ZR_NULL) {
            // BUG: 已加入的 catch 节点不随数组容器释放。
            ZrParser_AstNodeArray_Free(ps->state, catchClauses);
        }
        // BUG: 分配失败时 try/finally 块和 catch 元素没有释放。
        return ZR_NULL;
    }

    node->data.tryCatchFinallyStatement.block = block;
    node->data.tryCatchFinallyStatement.catchClauses = catchClauses;
    node->data.tryCatchFinallyStatement.finallyBlock = finallyBlock;
    return node;
}

static TZrBool using_resource_is_import_expression(SZrAstNode *resource);
static TZrBool using_pattern_is_plain_identifier(SZrAstNode *pattern);
static TZrBool using_pattern_has_guard_shape(SZrAstNode *pattern);
static TZrBool using_no_block_pattern_starts_here(SZrParserState *ps);
static SZrAstNode *parse_using_binding_pattern(SZrParserState *ps);
static SZrAstNode *parse_using_binding_identifier(SZrParserState *ps);
static SZrAstNode *parse_using_array_destructuring_pattern(SZrParserState *ps);
static SZrAstNode *parse_using_object_destructuring_pattern(SZrParserState *ps);

// using 共享块作用域资源管理、无块模式绑定和无块资源释放三种语法；普通资源走 drop 守卫，
// 带模式的绑定保留 guardKind/else 分支供编译器建立条件释放与失败分支。
// resource、pattern、guardTypeInfo、body 在节点建立之前都由本函数负责。
static SZrAstNode *parse_using_statement_body(SZrParserState *ps, SZrFileRange startLoc) {
    SZrAstNode *resource = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    SZrAstNode *pattern = ZR_NULL;
    SZrType *guardTypeInfo = ZR_NULL;
    SZrAstNode *elseBody = ZR_NULL;
    TZrBool isBlockScoped = ZR_FALSE;
    EZrUsingGuardKind guardKind = ZR_USING_GUARD_DROP;
    SZrAstNode *node;

    if (consume_token(ps, ZR_TK_LPAREN)) {
        if (consume_token(ps, ZR_TK_LET) || consume_token(ps, ZR_TK_VAR)) {
            guardKind = ZR_USING_GUARD_PATTERN;
            pattern = parse_using_binding_pattern(ps);
            if (pattern == ZR_NULL) {
                report_using_binder_invalid(ps, get_current_token_location(ps));
                return ZR_NULL;
            }
            if (!using_pattern_has_guard_shape(pattern)) {
                report_using_binder_invalid(ps, pattern->location);
                ZrParser_Ast_Free(ps->state, pattern);
                return ZR_NULL;
            }
            if (consume_token(ps, ZR_TK_COLON)) {
                guardTypeInfo = parse_type(ps);
                if (guardTypeInfo == ZR_NULL) {
                    report_error(ps, "Expected union variant annotation after ':' in using pattern");
                    ZrParser_Ast_Free(ps->state, pattern);
                    return ZR_NULL;
                }
            }
            if (ps->lexer->t.token != ZR_TK_EQUALS) {
                report_missing_using_resource_close(ps, get_current_token_location(ps));
                ZrParser_Ast_Free(ps->state, pattern);
                if (guardTypeInfo != ZR_NULL) {
                    free_owned_type(ps->state, guardTypeInfo);
                }
                return ZR_NULL;
            }
            consume_token(ps, ZR_TK_EQUALS);
        }

        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            if (pattern != ZR_NULL) {
                ZrParser_Ast_Free(ps->state, pattern);
            }
            if (guardTypeInfo != ZR_NULL) {
                free_owned_type(ps->state, guardTypeInfo);
            }
            return ZR_NULL;
        }

        if (guardKind == ZR_USING_GUARD_PATTERN &&
            guardTypeInfo == ZR_NULL &&
            using_pattern_is_plain_identifier(pattern) &&
            using_resource_is_import_expression(resource)) {
            guardKind = ZR_USING_GUARD_PLUGIN;
        }

        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            report_missing_using_resource_close(ps, get_current_token_location(ps));
            ZrParser_Ast_Free(ps->state, resource);
            if (pattern != ZR_NULL) {
                ZrParser_Ast_Free(ps->state, pattern);
            }
            if (guardTypeInfo != ZR_NULL) {
                free_owned_type(ps->state, guardTypeInfo);
            }
            return ZR_NULL;
        }

        consume_token(ps, ZR_TK_RPAREN);

        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            report_missing_statement_body_open(ps, "using statement", get_current_token_location(ps));
            ZrParser_Ast_Free(ps->state, resource);
            if (pattern != ZR_NULL) {
                ZrParser_Ast_Free(ps->state, pattern);
            }
            if (guardTypeInfo != ZR_NULL) {
                free_owned_type(ps->state, guardTypeInfo);
            }
            return ZR_NULL;
        }

        body = parse_block(ps);
        if (body == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, resource);
            if (pattern != ZR_NULL) {
                ZrParser_Ast_Free(ps->state, pattern);
            }
            if (guardTypeInfo != ZR_NULL) {
                free_owned_type(ps->state, guardTypeInfo);
            }
            return ZR_NULL;
        }

        if ((guardKind == ZR_USING_GUARD_PATTERN || guardKind == ZR_USING_GUARD_PLUGIN) &&
            consume_token(ps, ZR_TK_ELSE)) {
            if (ps->lexer->t.token != ZR_TK_LBRACE) {
                report_missing_statement_body_open(ps, "using else", get_current_token_location(ps));
                ZrParser_Ast_Free(ps->state, resource);
                ZrParser_Ast_Free(ps->state, pattern);
                if (guardTypeInfo != ZR_NULL) {
                    free_owned_type(ps->state, guardTypeInfo);
                }
                ZrParser_Ast_Free(ps->state, body);
                return ZR_NULL;
            }
            elseBody = parse_block(ps);
            if (elseBody == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, resource);
                ZrParser_Ast_Free(ps->state, pattern);
                if (guardTypeInfo != ZR_NULL) {
                    free_owned_type(ps->state, guardTypeInfo);
                }
                ZrParser_Ast_Free(ps->state, body);
                return ZR_NULL;
            }
        }

        if (guardKind == ZR_USING_GUARD_DROP && consume_token(ps, ZR_TK_ELSE)) {
            SZrAstNode *invalidElseBody = ZR_NULL;
            report_error(ps,
                         "using_else_without_guard: using else requires a guard binder; use `using (let name = import(...))` or a union variant pattern");
            if (ps->lexer->t.token != ZR_TK_LBRACE) {
                report_missing_statement_body_open(ps, "using else", get_current_token_location(ps));
                ZrParser_Ast_Free(ps->state, resource);
                ZrParser_Ast_Free(ps->state, body);
                return ZR_NULL;
            }
            invalidElseBody = parse_block(ps);
            if (invalidElseBody == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, resource);
                ZrParser_Ast_Free(ps->state, body);
                return ZR_NULL;
            }
            ZrParser_Ast_Free(ps->state, invalidElseBody);
        }

        isBlockScoped = ZR_TRUE;
    } else if (using_no_block_pattern_starts_here(ps)) {
        guardKind = ZR_USING_GUARD_PATTERN;
        if (!consume_token(ps, ZR_TK_LET)) {
            consume_token(ps, ZR_TK_VAR);
        }
        pattern = parse_using_binding_pattern(ps);
        if (pattern == ZR_NULL) {
            report_using_binder_invalid(ps, get_current_token_location(ps));
            return ZR_NULL;
        }
        if (!using_pattern_has_guard_shape(pattern)) {
            report_using_binder_invalid(ps, pattern->location);
            ZrParser_Ast_Free(ps->state, pattern);
            return ZR_NULL;
        }
        if (consume_token(ps, ZR_TK_COLON)) {
            guardTypeInfo = parse_type(ps);
            if (guardTypeInfo == ZR_NULL) {
                report_error(ps, "Expected union variant annotation after ':' in using pattern");
                ZrParser_Ast_Free(ps->state, pattern);
                return ZR_NULL;
            }
        }
        if (ps->lexer->t.token != ZR_TK_EQUALS) {
            report_missing_using_resource_close(ps, get_current_token_location(ps));
            ZrParser_Ast_Free(ps->state, pattern);
            if (guardTypeInfo != ZR_NULL) {
                free_owned_type(ps->state, guardTypeInfo);
            }
            return ZR_NULL;
        }
        consume_token(ps, ZR_TK_EQUALS);

        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, pattern);
            if (guardTypeInfo != ZR_NULL) {
                free_owned_type(ps->state, guardTypeInfo);
            }
            return ZR_NULL;
        }

        if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
            report_missing_statement_semicolon(ps, "using", get_current_token_location(ps));
        } else {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else {
        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            return ZR_NULL;
        }

        if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
            report_missing_statement_semicolon(ps, "using", get_current_token_location(ps));
        } else {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    }

    node = create_ast_node(ps, ZR_AST_USING_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, resource);
        ZrParser_Ast_Free(ps->state, body);
        if (pattern != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, pattern);
        }
        if (guardTypeInfo != ZR_NULL) {
            free_owned_type(ps->state, guardTypeInfo);
        }
        if (elseBody != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, elseBody);
        }
        return ZR_NULL;
    }

    node->data.usingStatement.resource = resource;
    node->data.usingStatement.body = body;
    node->data.usingStatement.isBlockScoped = isBlockScoped;
    node->data.usingStatement.guardKind = guardKind;
    node->data.usingStatement.pattern = pattern;
    node->data.usingStatement.guardTypeInfo = guardTypeInfo;
    node->data.usingStatement.elseBody = elseBody;
    return node;
}

// 插件 import 仅在无显式类型的简单绑定上升级为插件守卫。
static TZrBool using_resource_is_import_expression(SZrAstNode *resource) {
    return resource != ZR_NULL && resource->type == ZR_AST_IMPORT_EXPRESSION;
}

// 与解构/变体模式区分，以免错误地把复合绑定判为插件别名。
static TZrBool using_pattern_is_plain_identifier(SZrAstNode *pattern) {
    return pattern != ZR_NULL &&
           pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
           pattern->data.identifier.name != ZR_NULL;
}

// 限定 using 守卫可交给后续变体/解构语义处理的 AST 种类。
static TZrBool using_pattern_has_guard_shape(SZrAstNode *pattern) {
    if (using_pattern_is_plain_identifier(pattern)) {
        return ZR_TRUE;
    }

    return pattern != ZR_NULL &&
           (pattern->type == ZR_AST_PRIMARY_EXPRESSION ||
            pattern->type == ZR_AST_DESTRUCTURING_ARRAY ||
            pattern->type == ZR_AST_OBJECT_LITERAL);
}

// 无块 using 与普通表达式共用前缀；静默试探解构及可选类型后恢复全部游标/诊断状态。
static TZrBool using_no_block_pattern_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    TZrParserErrorCallback savedErrorCallback;
    TZrParserStructuredErrorCallback savedStructuredErrorCallback;
    TZrPtr savedErrorUserData;
    SZrAstNode *pattern = ZR_NULL;
    SZrType *guardTypeInfo = ZR_NULL;
    TZrBool result = ZR_FALSE;

    if (ps == ZR_NULL) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    savedSuppressErrorOutput = ps->suppressErrorOutput;
    savedErrorCallback = ps->errorCallback;
    savedStructuredErrorCallback = ps->structuredErrorCallback;
    savedErrorUserData = ps->errorUserData;
    ps->suppressErrorOutput = ZR_TRUE;
    ps->errorCallback = ZR_NULL;
    ps->structuredErrorCallback = ZR_NULL;
    ps->errorUserData = ZR_NULL;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    if (!consume_token(ps, ZR_TK_LET)) {
        consume_token(ps, ZR_TK_VAR);
    }
    if (ps->lexer->t.token == ZR_TK_LBRACKET || ps->lexer->t.token == ZR_TK_LBRACE) {
        pattern = parse_using_binding_pattern(ps);
        if (pattern != ZR_NULL && using_pattern_has_guard_shape(pattern)) {
            if (consume_token(ps, ZR_TK_COLON)) {
                guardTypeInfo = parse_type(ps);
            }
            result = (TZrBool)((guardTypeInfo != ZR_NULL || ps->lexer->t.token != ZR_TK_COLON) &&
                               ps->lexer->t.token == ZR_TK_EQUALS);
        }
    }

    if (guardTypeInfo != ZR_NULL) {
        free_owned_type(ps->state, guardTypeInfo);
    }
    if (pattern != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, pattern);
    }
    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->errorCallback = savedErrorCallback;
    ps->structuredErrorCallback = savedStructuredErrorCallback;
    ps->errorUserData = savedErrorUserData;
    restore_parser_cursor(ps, &cursor);
    return result;
}

// 守卫绑定统一生成可供编译器识别的标识符、变体、数组或对象模式。
static SZrAstNode *parse_using_binding_pattern(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    switch (ps->lexer->t.token) {
        case ZR_TK_IDENTIFIER:
        case ZR_TK_TEST:
            if (peek_token(ps) == ZR_TK_LPAREN || peek_token(ps) == ZR_TK_DOT) {
                return parse_primary_expression(ps);
            }
            return parse_identifier(ps);
        case ZR_TK_LBRACKET:
            return parse_using_array_destructuring_pattern(ps);
        case ZR_TK_LBRACE:
            return parse_using_object_destructuring_pattern(ps);
        default:
            return ZR_NULL;
    }
}

// move 仅在后接绑定名时改变所有权语义，其他位置仍按普通标识符处理。
static TZrBool using_current_token_is_move_binding_marker(SZrParserState *ps) {
    EZrToken lookahead;

    if (ps == ZR_NULL ||
        ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "move")) {
        return ZR_FALSE;
    }

    lookahead = peek_token(ps);
    return (TZrBool)(lookahead == ZR_TK_IDENTIFIER || lookahead == ZR_TK_TEST);
}

// 在解构叶子上保留 move 标记，供编译器判断绑定是否消费资源。
static SZrAstNode *parse_using_binding_identifier(SZrParserState *ps) {
    TZrBool isMoveBinding = ZR_FALSE;
    SZrFileRange moveLocation;
    SZrAstNode *identifier;

    if (using_current_token_is_move_binding_marker(ps)) {
        isMoveBinding = ZR_TRUE;
        moveLocation = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        moveLocation = get_current_location(ps);
    }

    identifier = parse_identifier(ps);
    if (identifier != ZR_NULL && isMoveBinding) {
        identifier->data.identifier.isMoveBinding = ZR_TRUE;
        identifier->location = ZrParser_FileRange_Merge(moveLocation, identifier->location);
    }

    return identifier;
}

// using 数组模式复用解构 AST 约定；成功节点接管所有绑定叶子。
static SZrAstNode *parse_using_array_destructuring_pattern(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *keys;
    SZrAstNode *node;
    SZrFileRange endLoc;
    SZrFileRange destructuringLoc;

    expect_token(ps, ZR_TK_LBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    keys = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        SZrAstNode *first = parse_using_binding_identifier(ps);
        if (first == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, keys);
            return ZR_NULL;
        }
        ZrParser_AstNodeArray_Add(ps->state, keys, first);

        while (consume_token(ps, ZR_TK_COMMA)) {
            SZrAstNode *key;

            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }

            key = parse_using_binding_identifier(ps);
            if (key == ZR_NULL) {
                free_ast_node_array_with_elements(ps->state, keys);
                return ZR_NULL;
            }
            ZrParser_AstNodeArray_Add(ps->state, keys, key);
        }
    }

    expect_token(ps, ZR_TK_RBRACKET);
    if (!consume_token(ps, ZR_TK_RBRACKET)) {
        free_ast_node_array_with_elements(ps->state, keys);
        return ZR_NULL;
    }

    endLoc = get_current_location(ps);
    destructuringLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    node = create_ast_node(ps, ZR_AST_DESTRUCTURING_ARRAY, destructuringLoc);
    if (node == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringArray.keys = keys;
    return node;
}

// using 对象模式以键值对 AST 保留源属性与绑定名，交给后续变体守卫检查。
static SZrAstNode *parse_using_object_destructuring_pattern(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNodeArray *properties;
    SZrAstNode *node;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    properties = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (properties == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *first = parse_using_binding_identifier(ps);
        SZrAstNode *key;
        SZrAstNode *value;
        SZrAstNode *kvNode;
        SZrFileRange kvLoc;

        if (first == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }

        if (consume_token(ps, ZR_TK_COLON)) {
            key = parse_identifier(ps);
            value = first;
            if (key == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, value);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
        } else {
            key = create_identifier_node_with_location(ps,
                                                       first->data.identifier.name,
                                                       first->location);
            value = first;
            if (key == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, value);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
        }

        kvLoc = ZrParser_FileRange_Merge(key->location, value->location);
        kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
        if (kvNode == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, key);
            ZrParser_Ast_Free(ps->state, value);
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }
        kvNode->data.keyValuePair.key = key;
        kvNode->data.keyValuePair.value = value;
        kvNode->data.keyValuePair.keyIsComputed = ZR_FALSE;
        ZrParser_AstNodeArray_Add(ps->state, properties, kvNode);

        if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            continue;
        }
        if (ps->lexer->t.token != ZR_TK_RBRACE) {
            report_error(ps, "Expected ',' or '}' in using object destructuring pattern");
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        report_missing_object_close(ps, startLoc);
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RBRACE);

    node = create_ast_node(ps, ZR_AST_OBJECT_LITERAL, ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }
    node->data.objectLiteral.properties = properties;
    return node;
}

// 两级语句分派的 using 入口；消费关键字后由共用体解析决定块或无块形态。
SZrAstNode *parse_using_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_USING);
    if (!consume_token(ps, ZR_TK_USING)) {
        return ZR_NULL;
    }
    return parse_using_statement_body(ps, startLoc);
}

// 解析语句（入口函数）

// 左花括号既可开始块也可开始对象表达式；预读首属性键及冒号后恢复游标。
static TZrBool parser_brace_starts_object_literal_statement(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool result = ZR_FALSE;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_LBRACE) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER || ps->lexer->t.token == ZR_TK_STRING) {
        ZrParser_Lexer_Next(ps->lexer);
        result = ps->lexer->t.token == ZR_TK_COLON;
    } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        TZrInt32 depth = 1;

        ZrParser_Lexer_Next(ps->lexer);
        while (depth > 0 && ps->lexer->t.token != ZR_TK_EOS) {
            if (ps->lexer->t.token == ZR_TK_LBRACKET) {
                depth++;
            } else if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                depth--;
                if (depth == 0) {
                    ZrParser_Lexer_Next(ps->lexer);
                    break;
                }
            }

            if (depth > 0) {
                ZrParser_Lexer_Next(ps->lexer);
            }
        }

        result = depth == 0 && ps->lexer->t.token == ZR_TK_COLON;
    }

    restore_parser_cursor(ps, &cursor);
    return result;
}

// 块内语句分派的递归入口；优先识别声明与控制流，再交给表达式兜底。
// 返回节点由 parse_block_impl 接管；NULL 使块停止并交由外层错误恢复。
SZrAstNode *parse_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "iterator") &&
        peek_token(ps) == ZR_TK_FN) {
        report_error(ps,
                     "Iterator function modifiers are not supported; declare an explicit Iterator<T> return carrier and use yield");
        return ZR_NULL;
    }

    if (parser_async_function_declaration_starts_here(ps)) {
        return parse_reserved_async_function_declaration(ps);
    }
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "comptime")) {
        return parse_compile_time_declaration(ps);
    }

    if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO) {
        EZrToken nextToken = peek_token(ps);
        if (nextToken == ZR_TK_IDENTIFIER) {
            SZrParserCursor cursor;
            save_parser_cursor(ps, &cursor);
            (void)parse_access_modifier(ps);
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "comptime")) {
                return parse_compile_time_declaration(ps);
            }
            restore_parser_cursor(ps, &cursor);
        }
        if (parser_function_declaration_starts_here(ps)) {
            return parse_function_declaration(ps);
        }
        switch (nextToken) {
            case ZR_TK_VAR:
            case ZR_TK_LET:
            case ZR_TK_CONST:
                return parse_variable_declaration(ps);
            case ZR_TK_STRUCT:
                return parse_struct_declaration(ps);
            case ZR_TK_CLASS:
                return parse_class_declaration(ps);
            case ZR_TK_INTERFACE:
                return parse_interface_declaration(ps);
            case ZR_TK_ENUM:
                return parse_enum_declaration(ps);
            case ZR_TK_UNION:
                return parse_union_declaration(ps);
            default:
                report_error(ps, "Expected declaration after access modifier");
                return ZR_NULL;
        }
    }

    switch (token) {
        case ZR_TK_MODULE:
            return parse_module_declaration(ps);

        case ZR_TK_LBRACE:
            return parser_brace_starts_object_literal_statement(ps)
                       ? parse_expression_statement(ps)
                       : parse_block(ps);

        case ZR_TK_RETURN:
            return parse_return_statement(ps);

        case ZR_TK_VAR:
        case ZR_TK_LET:
        case ZR_TK_CONST:
            return parse_variable_declaration(ps);

        case ZR_TK_USING:
            return parse_using_statement(ps);

        case ZR_TK_SHARP:
            return parse_decorated_statement(ps);

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
            if (parser_for_header_should_parse_foreach(ps)) {
                SZrAstNode *foreachNode = parse_foreach_loop(ps);
                if (foreachNode != ZR_NULL) {
                    foreachNode->data.foreachLoop.isStatement = ZR_TRUE;
                }
                return foreachNode;
            }

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
            report_removed_legacy_syntax(
                    ps,
                    "out generator statement",
                    "Use `yield expression;` inside an iterator function.");
            return ZR_NULL;

        case ZR_TK_YIELD:
            return parse_yield_statement(ps);

        case ZR_TK_THROW:
            return parse_throw_statement(ps);

        case ZR_TK_TRY:
            return parse_try_catch_finally_statement(ps);

        case ZR_TK_PERCENT:
            if (report_removed_percent_syntax(ps)) {
                return ZR_NULL;
            }
            return parse_expression_statement(ps);

        default:
            if (parser_native_extern_starts_here(ps)) {
                return parse_extern_block(ps);
            }
            if (token == ZR_TK_FN) {
                return parse_function_declaration(ps);
            }
            if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
                return parse_function_declaration(ps);
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

// 顶层 resource class 是 class AST 的所有权修饰，而非另一种独立声明节点。
static SZrAstNode *parse_resource_class_declaration(SZrParserState *ps,
                                                     EZrAccessModifier accessModifier) {
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "resource")) {
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_CLASS) {
        report_error(ps, "Expected 'class' after 'resource'");
        return ZR_NULL;
    }
    node = parse_class_declaration(ps);
    if (node != ZR_NULL && node->type == ZR_AST_CLASS_DECLARATION) {
        node->data.classDeclaration.isOwned = ZR_TRUE;
        node->data.classDeclaration.modifierFlags |= ZR_DECLARATION_MODIFIER_RESOURCE;
        node->data.classDeclaration.accessModifier = accessModifier;
    }
    return node;
}

// 顶层装饰器先试 comptime 函数；handled 区分已认定但解析失败与应回退的情况。
// 非 comptime 时释放试探装饰器并复原游标，成功时装饰器转给函数声明节点。
static SZrAstNode *try_parse_top_level_decorated_comptime_declaration(
        SZrParserState *ps,
        TZrBool *handled) {
    SZrParserCursor cursor;
    SZrAstNodeArray *decorators;
    SZrAstNode *node;
    EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;

    if (handled != ZR_NULL) {
        *handled = ZR_FALSE;
    }
    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_SHARP) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    decorators = parse_leading_decorators(ps);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }
    if (ps->lexer->t.token == ZR_TK_PUB ||
        ps->lexer->t.token == ZR_TK_PRI ||
        ps->lexer->t.token == ZR_TK_PRO) {
        accessModifier = parse_access_modifier(ps);
    }
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER || !current_identifier_equals(ps, "comptime")) {
        free_ast_node_array_with_elements(ps->state, decorators);
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }

    if (handled != ZR_NULL) {
        *handled = ZR_TRUE;
    }
    node = parse_compile_time_declaration(ps);
    if (node == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }
    if (node->type == ZR_AST_COMPILE_TIME_DECLARATION &&
        node->data.compileTimeDeclaration.declarationType == ZR_COMPILE_TIME_FUNCTION &&
        node->data.compileTimeDeclaration.declaration != ZR_NULL &&
        node->data.compileTimeDeclaration.declaration->type == ZR_AST_FUNCTION_DECLARATION) {
        SZrAstNodeArray *existing =
                node->data.compileTimeDeclaration.declaration->data.functionDeclaration.decorators;
        if (existing != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, existing);
        }
        node->data.compileTimeDeclaration.declaration->data.functionDeclaration.decorators = decorators;
        node->data.compileTimeDeclaration.declaration->data.functionDeclaration.accessModifier =
                accessModifier;
        return node;
    }

    free_ast_node_array_with_elements(ps->state, decorators);
    return node;
}

// 脚本和增量解析的顶层入口；在普通语句之外识别类型、模块与装饰器声明。
// 返回 AST 交给脚本语句数组或增量 API，失败由各自调用者处理诊断/恢复。
SZrAstNode *parse_top_level_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    if (token == ZR_TK_SHARP) {
        TZrBool handled = ZR_FALSE;
        SZrAstNode *decoratedComptime =
                try_parse_top_level_decorated_comptime_declaration(ps, &handled);
        if (handled) {
            return decoratedComptime;
        }
    }

    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "iterator") &&
        peek_token(ps) == ZR_TK_FN) {
        report_error(ps,
                     "Iterator function modifiers are not supported; declare an explicit Iterator<T> return carrier and use yield");
        return ZR_NULL;
    }

    if (parser_struct_declaration_starts_here(ps)) {
        return parse_struct_declaration(ps);
    }

    if (parser_async_function_declaration_starts_here(ps)) {
        return parse_reserved_async_function_declaration(ps);
    }
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "comptime")) {
        return parse_compile_time_declaration(ps);
    }

    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "resource") &&
        peek_token(ps) == ZR_TK_CLASS) {
        return parse_resource_class_declaration(ps, ZR_ACCESS_PRIVATE);
    }

    // 检查是否是可见性修饰符（pub/pri/pro），后面应该跟 var/struct/class/interface/enum/union
    if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO) {
        // 使用 peek_token 查看下一个 token，不消费当前 token
        EZrToken nextToken = peek_token(ps);

        if (nextToken == ZR_TK_IDENTIFIER) {
            SZrParserCursor cursor;
            EZrAccessModifier accessModifier;

            save_parser_cursor(ps, &cursor);
            accessModifier = parse_access_modifier(ps);
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "comptime")) {
                SZrAstNode *compileTimeDeclaration =
                        parse_compile_time_declaration(ps);
                if (compileTimeDeclaration != ZR_NULL &&
                    compileTimeDeclaration->type ==
                            ZR_AST_COMPILE_TIME_DECLARATION &&
                    compileTimeDeclaration->data.compileTimeDeclaration
                                    .declarationType ==
                            ZR_COMPILE_TIME_FUNCTION &&
                    compileTimeDeclaration->data.compileTimeDeclaration
                                    .declaration != ZR_NULL &&
                    compileTimeDeclaration->data.compileTimeDeclaration
                                    .declaration->type ==
                            ZR_AST_FUNCTION_DECLARATION) {
                    compileTimeDeclaration->data.compileTimeDeclaration
                            .declaration->data.functionDeclaration
                            .accessModifier = accessModifier;
                }
                return compileTimeDeclaration;
            }
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
                current_identifier_equals(ps, "resource") &&
                peek_token(ps) == ZR_TK_CLASS) {
                return parse_resource_class_declaration(ps, accessModifier);
            }
            restore_parser_cursor(ps, &cursor);
        }

        if (nextToken == ZR_TK_PERCENT) {
            ZrParser_Lexer_Next(ps->lexer);
            if (report_removed_percent_syntax(ps)) {
                return ZR_NULL;
            }
            report_error(ps, "Expected declaration after access modifier");
            return ZR_NULL;
        }

        if (parser_function_declaration_starts_here(ps)) {
            return parse_function_declaration(ps);
        }

        if (parser_class_declaration_starts_here(ps)) {
            return parse_class_declaration(ps);
        }

        // 根据下一个 token 调用相应的解析函数（它们会自己解析可见性修饰符）
        switch (nextToken) {
            case ZR_TK_VAR:
            case ZR_TK_LET:
            case ZR_TK_CONST:
                return parse_variable_declaration(ps);
            case ZR_TK_STRUCT:
                return parse_struct_declaration(ps);
            case ZR_TK_CLASS:
                return parse_class_declaration(ps);
            case ZR_TK_INTERFACE:
                return parse_interface_declaration(ps);
            case ZR_TK_ENUM:
                return parse_enum_declaration(ps);
            case ZR_TK_UNION:
                return parse_union_declaration(ps);
            default:
                // 如果后面不是声明类型，报告错误
                report_error(ps, "Expected declaration after access modifier");
                return ZR_NULL;
        }
    }

    switch (token) {
        case ZR_TK_MODULE:
            return parse_module_declaration(ps);

        case ZR_TK_VAR:
        case ZR_TK_LET:
        case ZR_TK_CONST:
            return parse_variable_declaration(ps);

        case ZR_TK_USING:
            return parse_using_statement(ps);

        case ZR_TK_STRUCT:
            return parse_struct_declaration(ps);

        case ZR_TK_ABSTRACT:
        case ZR_TK_FINAL:
            if (parser_class_declaration_starts_here(ps)) {
                return parse_class_declaration(ps);
            }
            return parse_expression_statement(ps);

        case ZR_TK_CLASS:
            return parse_class_declaration(ps);

        case ZR_TK_INTERFACE:
            return parse_interface_declaration(ps);

        case ZR_TK_ENUM:
            return parse_enum_declaration(ps);

        case ZR_TK_UNION:
            return parse_union_declaration(ps);

        case ZR_TK_PERCENT:
            if (report_removed_percent_syntax(ps)) {
                return ZR_NULL;
            }
            return parse_expression_statement(ps);

        case ZR_TK_INTERMEDIATE:
            report_removed_legacy_syntax(
                    ps,
                    "intermediate source declaration",
                    "Use compiler-generated SemIR/.zri output; user source cannot declare intermediate instructions.");
            return ZR_NULL;

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
            if (parser_for_header_should_parse_foreach(ps)) {
                SZrAstNode *foreachNode = parse_foreach_loop(ps);
                if (foreachNode != ZR_NULL) {
                    foreachNode->data.foreachLoop.isStatement = ZR_TRUE;
                }
                return foreachNode;
            }

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
            report_removed_legacy_syntax(
                    ps,
                    "out generator statement",
                    "Use `yield expression;` inside an iterator function.");
            return ZR_NULL;

        case ZR_TK_YIELD:
            return parse_yield_statement(ps);

        case ZR_TK_THROW:
            return parse_throw_statement(ps);

        case ZR_TK_TRY:
            return parse_try_catch_finally_statement(ps);

        default:
            if (parser_native_extern_starts_here(ps)) {
                return parse_extern_block(ps);
            }
            if (token == ZR_TK_FN) {
                return parse_function_declaration(ps);
            }
            if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
                return parse_function_declaration(ps);
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
                        free_ast_node_array_with_elements(ps->state, decorators);
                    }
                    restore_parser_cursor(ps, &cursor);
                    return ZR_NULL;
                }
                nextToken = ps->lexer->t.token;
                decoratorStartsFunction = parser_function_declaration_starts_here(ps);
                if (nextToken == ZR_TK_PUB || nextToken == ZR_TK_PRI || nextToken == ZR_TK_PRO) {
                    declarationToken = peek_token(ps);
                }
                free_ast_node_array_with_elements(ps->state, decorators);
                restore_parser_cursor(ps, &cursor);

                if (nextToken == ZR_TK_CLASS || nextToken == ZR_TK_ABSTRACT || nextToken == ZR_TK_FINAL) {
                    return parse_class_declaration(ps);
                } else if (nextToken == ZR_TK_STRUCT) {
                    return parse_struct_declaration(ps);
                } else if (nextToken == ZR_TK_UNION) {
                    return parse_union_declaration(ps);
                } else if (nextToken == ZR_TK_ENUM) {
                    return parse_enum_declaration(ps);
                } else if (nextToken == ZR_TK_PUB || nextToken == ZR_TK_PRI || nextToken == ZR_TK_PRO) {
                    if (declarationToken == ZR_TK_CLASS || declarationToken == ZR_TK_ABSTRACT ||
                        declarationToken == ZR_TK_FINAL) {
                        return parse_class_declaration(ps);
                    } else if (declarationToken == ZR_TK_STRUCT) {
                        return parse_struct_declaration(ps);
                    } else if (declarationToken == ZR_TK_UNION) {
                        return parse_union_declaration(ps);
                    } else if (declarationToken == ZR_TK_ENUM) {
                        return parse_enum_declaration(ps);
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
                // TODO: 顶层非声明装饰器目前退化为表达式语句；需核对语言规则及编译器消费路径。
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
            if ((token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) &&
                parser_keywordless_function_declaration_starts_here(ps)) {
                return parse_function_declaration(ps);
            }
            if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
                // 查看下一个 token 判断是否是函数声明
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                    SZrParserCursor cursor;
                    TZrBool savedSuppressErrorOutput = ps->suppressErrorOutput;
                    TZrParserErrorCallback savedErrorCallback = ps->errorCallback;
                    TZrParserStructuredErrorCallback savedStructuredErrorCallback = ps->structuredErrorCallback;
                    TZrPtr savedErrorUserData = ps->errorUserData;
                    SZrAstNode *funcDecl;

                    save_parser_cursor(ps, &cursor);
                    ps->suppressErrorOutput = ZR_TRUE;
                    ps->errorCallback = ZR_NULL;
                    ps->structuredErrorCallback = ZR_NULL;
                    ps->errorUserData = ZR_NULL;
                    ps->hasError = ZR_FALSE;
                    ps->errorMessage = ZR_NULL;

                    funcDecl = parse_function_declaration(ps);
                    ps->suppressErrorOutput = savedSuppressErrorOutput;
                    ps->errorCallback = savedErrorCallback;
                    ps->structuredErrorCallback = savedStructuredErrorCallback;
                    ps->errorUserData = savedErrorUserData;
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
