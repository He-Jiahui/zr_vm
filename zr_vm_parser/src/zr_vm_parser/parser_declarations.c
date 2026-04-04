#include "parser_internal.h"

SZrAstNode *parse_module_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *name;
    SZrAstNode *node;

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_MODULE) {
            ZrParser_Lexer_Next(ps->lexer);
        } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "module")) {
            ZrParser_Lexer_Next(ps->lexer);
        } else {
            report_error(ps, "Expected 'module' after '%'");
            return ZR_NULL;
        }
    } else {
        expect_token(ps, ZR_TK_MODULE);
        ZrParser_Lexer_Next(ps->lexer);
    }

    name = parse_normalized_module_path(ps, "module");
    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    node = create_ast_node(ps, ZR_AST_MODULE_DECLARATION, ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, name);
        return ZR_NULL;
    }

    node->data.moduleDeclaration.name = name;
    return node;
}

// 解析变量声明

SZrAstNode *parse_variable_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    expect_token(ps, ZR_TK_VAR);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析 const 关键字（可选）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

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

    if (pattern == ZR_NULL) {
        return ZR_NULL;
    }

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
        if (typeInfo == ZR_NULL) {
            report_error(ps, "Failed to parse type annotation");
            return ZR_NULL;
        }
    }

    // 可选初始值
    SZrAstNode *value = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        value = parse_expression(ps);
        if (value == ZR_NULL) {
            // 如果解析表达式失败，尝试错误恢复
            if (ps->hasError) {
                return ZR_NULL;
            }
            // 如果没有错误但返回 NULL，可能是遇到了不支持的语法
            const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
            snprintf(errorMsg, sizeof(errorMsg), "无法解析表达式（遇到 '%s'）", tokenStr);
            report_error_with_token(ps, errorMsg, ps->lexer->t.token);
            return ZR_NULL;
        }
    }

    // 分号是可选的（在某些情况下）
    // 注意：在检查分号之前，表达式应该已经完全解析（包括成员访问和函数调用）
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token != ZR_TK_EOS && ps->lexer->t.token != ZR_TK_VAR &&
               ps->lexer->t.token != ZR_TK_USING && ps->lexer->t.token != ZR_TK_STRUCT &&
               ps->lexer->t.token != ZR_TK_CLASS && ps->lexer->t.token != ZR_TK_INTERFACE &&
               ps->lexer->t.token != ZR_TK_ENUM && ps->lexer->t.token != ZR_TK_TEST &&
               ps->lexer->t.token != ZR_TK_INTERMEDIATE && ps->lexer->t.token != ZR_TK_MODULE &&
               ps->lexer->t.token != ZR_TK_DOT && // 允许成员访问继续
               ps->lexer->t.token != ZR_TK_LBRACKET && // 允许计算属性访问
               ps->lexer->t.token != ZR_TK_LPAREN) { // 允许函数调用
        // 如果下一个 token 不是语句开始关键字或表达式继续符号，期望分号
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
        TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(errorMsg, sizeof(errorMsg), "期望 ';'，但遇到 '%s'", tokenStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
        // 不立即返回，允许错误恢复
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_VARIABLE_DECLARATION, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.variableDeclaration.pattern = pattern;
    node->data.variableDeclaration.value = value;
    node->data.variableDeclaration.typeInfo = typeInfo;
    node->data.variableDeclaration.accessModifier = accessModifier;
    node->data.variableDeclaration.isConst = isConst;
    return node;
}

// 解析函数声明

SZrAstNode *parse_function_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);

    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
        } else {
            break;
        }
    }

    if (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || ps->lexer->t.token == ZR_TK_PRO) {
        parse_access_modifier(ps);
    }

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析函数名（不需要function关键字，直接是标识符）
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_FALSE);
    }

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL; // 可变参数

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        // 只有可变参数的情况 (...name: type)
        // parse_parameter 会处理 ZR_TK_PARAMS
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        } else {
            // 如果解析失败，清理并返回
            // parse_parameter 已经报告了错误（如果 token 不是标识符）
            ZrParser_AstNodeArray_Free(ps->state, decorators);
            return ZR_NULL;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                // 普通参数后跟可变参数 (param1, param2, ...name: type)
                // parse_parameter 会处理 ZR_TK_PARAMS
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                } else {
                    // 如果解析失败，清理并返回
                    // parse_parameter 已经报告了错误（如果 token 不是标识符）
                    if (params != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, params);
                    }
                    ZrParser_AstNodeArray_Free(ps->state, decorators);
                    return ZR_NULL;
                }
            } else {
                // 逗号后不是可变参数，回退
                // 这里应该报告错误，但为了兼容性，我们继续解析
            }
        }
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    // 解析返回类型（可选）
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    if (!parse_optional_where_clauses(ps, generic)) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    // 解析函数体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    SZrFileRange endLoc = body->location;
    SZrFileRange funcLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FUNCTION_DECLARATION, funcLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.functionDeclaration.name = name;
    node->data.functionDeclaration.nameLocation = nameLoc;
    node->data.functionDeclaration.generic = generic;
    node->data.functionDeclaration.params = params;
    node->data.functionDeclaration.args = args;
    node->data.functionDeclaration.returnType = returnType;
    node->data.functionDeclaration.body = body;
    node->data.functionDeclaration.decorators = decorators;
    return node;
}

// ==================== 语句解析 ====================

// 解析块
