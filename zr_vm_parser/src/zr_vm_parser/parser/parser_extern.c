#include "parser_internal.h"

SZrAstNode *parse_enum_member(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);

    // 解析成员名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 可选值（name = value 或 name;）
    SZrAstNode *value = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        value = parse_expression(ps);
        if (value == ZR_NULL) {
            if (decorators != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, decorators);
            }
            return ZR_NULL;
        }
    }

    // 可选分隔符（逗号或分号）
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange memberLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_MEMBER, memberLoc);
    if (node == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.enumMember.name = name;
    node->data.enumMember.value = value;
    node->data.enumMember.decorators = decorators;
    return node;
}

// 解析枚举声明
// 语法：enum Name[: baseType] { members }

SZrAstNode *parse_enum_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    // 期望 enum 关键字
    expect_token(ps, ZR_TK_ENUM);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析枚举名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析基础类型（可选，继承 int, string, float, bool）
    SZrType *baseType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        baseType = parse_type(ps);
        if (baseType == ZR_NULL) {
            return ZR_NULL;
        }
    }

    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (members == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = parse_enum_member(ps);
        if (member != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            if (ps->hasError) {
                break;
            }
            // 跳过当前 token 继续解析
            if (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
                ZrParser_Lexer_Next(ps->lexer);
            }
        }
    }

    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange enumLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_DECLARATION, enumLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, members);
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.enumDeclaration.name = name;
    node->data.enumDeclaration.baseType = baseType;
    node->data.enumDeclaration.members = members;
    node->data.enumDeclaration.decorators = decorators;
    node->data.enumDeclaration.accessModifier = accessModifier;
    return node;
}

SZrAstNode *parse_extern_function_declaration(SZrParserState *ps, SZrAstNodeArray *decorators) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *node;

    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
            SZrAstNode *argsNode = parse_parameter(ps);
            if (argsNode != ZR_NULL) {
                args = &argsNode->data.parameter;
            }
        }
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    node = create_ast_node(ps, ZR_AST_EXTERN_FUNCTION_DECLARATION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.externFunctionDeclaration.name = name;
    node->data.externFunctionDeclaration.params = params;
    node->data.externFunctionDeclaration.args = args;
    node->data.externFunctionDeclaration.returnType = returnType;
    node->data.externFunctionDeclaration.decorators = decorators;
    return node;
}

SZrAstNode *parse_extern_delegate_declaration(SZrParserState *ps, SZrAstNodeArray *decorators) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *node;

    if (!current_identifier_equals(ps, "delegate")) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        report_error(ps, "Expected 'delegate' in extern block");
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
            SZrAstNode *argsNode = parse_parameter(ps);
            if (argsNode != ZR_NULL) {
                args = &argsNode->data.parameter;
            }
        }
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    node = create_ast_node(ps, ZR_AST_EXTERN_DELEGATE_DECLARATION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.externDelegateDeclaration.name = name;
    node->data.externDelegateDeclaration.params = params;
    node->data.externDelegateDeclaration.args = args;
    node->data.externDelegateDeclaration.returnType = returnType;
    node->data.externDelegateDeclaration.decorators = decorators;
    return node;
}

SZrAstNode *parse_extern_member_declaration(SZrParserState *ps) {
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);
    SZrAstNode *node = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_STRUCT) {
        node = parse_struct_declaration(ps);
        if (node != ZR_NULL && node->type == ZR_AST_STRUCT_DECLARATION && decorators != ZR_NULL) {
            node->data.structDeclaration.decorators = decorators;
            decorators = ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_ENUM) {
        node = parse_enum_declaration(ps);
        if (node != ZR_NULL && node->type == ZR_AST_ENUM_DECLARATION && decorators != ZR_NULL) {
            node->data.enumDeclaration.decorators = decorators;
            decorators = ZR_NULL;
        }
    } else if (current_identifier_equals(ps, "delegate")) {
        node = parse_extern_delegate_declaration(ps, decorators);
        decorators = ZR_NULL;
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        node = parse_extern_function_declaration(ps, decorators);
        decorators = ZR_NULL;
    } else {
        report_error(ps, "Unexpected declaration inside extern block");
    }

    if (decorators != ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
    }
    return node;
}

SZrAstNode *parse_extern_block(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *libraryName = ZR_NULL;
    SZrAstNodeArray *declarations = ZR_NULL;
    SZrAstNode *node;

    expect_token(ps, ZR_TK_PERCENT);
    ZrParser_Lexer_Next(ps->lexer);

    if (!current_identifier_equals(ps, "extern")) {
        report_error(ps, "Expected 'extern' after '%'");
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token != ZR_TK_STRING) {
        report_error(ps, "Expected string literal library spec for extern block");
        return ZR_NULL;
    }
    libraryName = parse_literal(ps);

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    declarations = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (declarations == ZR_NULL) {
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_LBRACE)) {
        while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
            SZrAstNode *declaration = parse_extern_member_declaration(ps);
            if (declaration == ZR_NULL) {
                break;
            }
            ZrParser_AstNodeArray_Add(ps->state, declarations, declaration);
        }
        expect_token(ps, ZR_TK_RBRACE);
        consume_token(ps, ZR_TK_RBRACE);
    } else {
        SZrAstNode *declaration = parse_extern_member_declaration(ps);
        if (declaration == ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, declarations);
            return ZR_NULL;
        }
        ZrParser_AstNodeArray_Add(ps->state, declarations, declaration);
    }

    node = create_ast_node(ps, ZR_AST_EXTERN_BLOCK, ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, declarations);
        return ZR_NULL;
    }

    node->data.externBlock.libraryName = libraryName;
    node->data.externBlock.declarations = declarations;
    return node;
}

// 解析测试声明
// 语法：%test("test_name") { ... }

SZrAstNode *parse_test_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;

    // 解析 %test
    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        // 保存 % token 的位置信息（在调用 ZrParser_Lexer_Next 之前）
        startLoc = get_current_location(ps);
        TZrInt32 percentLine = startLoc.start.line;
        TZrInt32 percentColumn = startLoc.start.column;
        ZrParser_Lexer_Next(ps->lexer);
        // 期望 "test" 标识符或关键字
        // 注意：test 可能是关键字 ZR_TK_TEST，也可能是标识符 ZR_TK_IDENTIFIER
        if (ps->lexer->t.token == ZR_TK_TEST) {
            // test 是关键字，直接接受
            ZrParser_Lexer_Next(ps->lexer);
        } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            // test 是标识符，需要检查名称
            SZrString *identName = ps->lexer->t.seminfo.stringValue;
            if (identName == ZR_NULL) {
                report_error(ps, "Expected 'test' after '%'");
                return ZR_NULL;
            }
            TZrNativeString nameStr = ZrCore_String_GetNativeString(identName);
            if (nameStr == ZR_NULL || strcmp(nameStr, "test") != 0) {
                TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
                snprintf(errorMsg, sizeof(errorMsg), "Expected 'test' after '%%', but got identifier '%s'",
                         nameStr ? nameStr : "<null>");
                report_error(ps, errorMsg);
                return ZR_NULL;
            }
            ZrParser_Lexer_Next(ps->lexer);
        } else {
            // 使用保存的位置信息报告错误
            const TZrChar *fileName = "<unknown>";
            if (startLoc.source != ZR_NULL) {
                TZrNativeString nameStr = ZrCore_String_GetNativeString(startLoc.source);
                if (nameStr != ZR_NULL) {
                    fileName = nameStr;
                }
            }
            const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            if (!ps->suppressErrorOutput) {
                ZrCore_Log_Diagnosticf(ps->state,
                                       ZR_LOG_LEVEL_ERROR,
                                       ZR_OUTPUT_CHANNEL_STDERR,
                                       "  [%s:%d:%d] Expected 'test' after '%%' (遇到 token: '%s')\n",
                                       fileName,
                                       percentLine,
                                       percentColumn,
                                       tokenStr);
            }
            report_error(ps, "Expected 'test' after '%'");
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_TEST) {
        // 兼容旧的语法：test() { ... }
        startLoc = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected '%test' or 'test'");
        return ZR_NULL;
    }

    // 解析测试名称参数：("test_name")
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    // 期望字符串字面量作为测试名
    SZrIdentifier *name = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_STRING) {
        SZrString *testNameStr = ps->lexer->t.seminfo.stringValue;
        ZrParser_Lexer_Next(ps->lexer);

        // 创建标识符节点来存储测试名
        SZrAstNode *nameNode = create_identifier_node(ps, testNameStr);
        if (nameNode != ZR_NULL) {
            name = &nameNode->data.identifier;
        }
    } else {
        report_error(ps, "Expected string literal as test name");
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    // 解析测试体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange testLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TEST_DECLARATION, testLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.testDeclaration.name = name;
    node->data.testDeclaration.params = ZrParser_AstNodeArray_New(ps->state, 0); // 测试没有参数列表
    node->data.testDeclaration.args = ZR_NULL;
    node->data.testDeclaration.body = body;
    return node;
}

TZrBool is_compile_time_function_declaration(SZrParserState *ps) {
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
    TZrInt32 parenDepth = 0;
    TZrBool isFunctionDeclaration = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        return ZR_FALSE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        goto restore;
    }

    do {
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            parenDepth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            parenDepth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    } while (parenDepth > 0 && ps->lexer->t.token != ZR_TK_EOS);

    if (parenDepth == 0 && (ps->lexer->t.token == ZR_TK_LBRACE || ps->lexer->t.token == ZR_TK_COLON)) {
        isFunctionDeclaration = ZR_TRUE;
    }

restore:
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
    return isFunctionDeclaration;
}

// 语法：%compileTime function/variable/class/struct/statement/expression

SZrAstNode *parse_compile_time_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;

    // 解析 %compileTime
    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        startLoc = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);

        // 期望 "compileTime" 标识符
        if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
            report_error(ps, "Expected 'compileTime' after '%'");
            return ZR_NULL;
        }

        SZrString *identName = ps->lexer->t.seminfo.stringValue;
        if (identName == ZR_NULL) {
            report_error(ps, "Expected 'compileTime' after '%'");
            return ZR_NULL;
        }

        TZrNativeString nameStr = ZrCore_String_GetNativeString(identName);
        if (nameStr == ZR_NULL || strcmp(nameStr, "compileTime") != 0) {
            TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
            snprintf(errorMsg, sizeof(errorMsg), "Expected 'compileTime' after '%%', but got identifier '%s'",
                     nameStr ? nameStr : "<null>");
            report_error(ps, errorMsg);
            return ZR_NULL;
        }
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected '%compileTime'");
        return ZR_NULL;
    }

    // 根据后续内容判断声明类型
    EZrCompileTimeDeclarationType declType;
    SZrAstNode *declaration = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_VAR) {
        // 编译期变量声明：%compileTime var name = value;
        declType = ZR_COMPILE_TIME_VARIABLE;
        declaration = parse_variable_declaration(ps);
    } else if (ps->lexer->t.token == ZR_TK_CLASS) {
        declType = ZR_COMPILE_TIME_CLASS;
        declaration = parse_class_declaration(ps);
    } else if (ps->lexer->t.token == ZR_TK_STRUCT) {
        declType = ZR_COMPILE_TIME_STRUCT;
        declaration = parse_struct_declaration(ps);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        if (is_compile_time_function_declaration(ps)) {
            // 函数声明：%compileTime functionName(...) { ... }
            declType = ZR_COMPILE_TIME_FUNCTION;
            declaration = parse_function_declaration(ps);
        } else {
            // 函数调用表达式或其他编译期表达式
            declType = ZR_COMPILE_TIME_EXPRESSION;
            declaration = parse_expression(ps);
        }
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        // 编译期语句块：%compileTime { ... }
        declType = ZR_COMPILE_TIME_STATEMENT;
        declaration = parse_block(ps);
    } else {
        // 尝试解析为表达式
        declType = ZR_COMPILE_TIME_EXPRESSION;
        declaration = parse_expression(ps);
    }

    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange compileTimeLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_COMPILE_TIME_DECLARATION, compileTimeLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.compileTimeDeclaration.declarationType = declType;
    node->data.compileTimeDeclaration.declaration = declaration;
    return node;
}

// 存根实现：中间代码声明解析
// 解析 Intermediate 指令参数

SZrAstNode *parse_intermediate_instruction_parameter(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrString *value = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        value = ps->lexer->t.seminfo.stringValue;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_INTEGER) {
        // 整数（包括十进制、十六进制和八进制）都使用 ZR_TK_INTEGER
        value = ps->lexer->t.seminfo.stringValue; // 获取原始字符串
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected identifier or number in intermediate instruction parameter");
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange paramLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_INSTRUCTION_PARAMETER, paramLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.intermediateInstructionParameter.value = value;
    return node;
}

// 解析 Intermediate 指令

SZrAstNode *parse_intermediate_instruction(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析指令名
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        report_error(ps, "Expected identifier for intermediate instruction name");
        return ZR_NULL;
    }

    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析参数列表（参数之间用空格分隔，lexer 已经处理了空白）
    // 语法：name value1 value2 ... ;
    SZrAstNodeArray *values = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (values == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析指令参数，直到遇到分号
    // lexer 已经跳过了空白，所以直接解析参数即可
    while (ps->lexer->t.token != ZR_TK_SEMICOLON && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *param = parse_intermediate_instruction_parameter(ps);
        if (param != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, values, param);
        } else {
            // 如果解析失败，可能是遇到了分号或其他结束符
            // 检查是否是分号（可能前面有注释）
            if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
                break;
            }
            // 否则报告错误并继续尝试
            break;
        }
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange instLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_INSTRUCTION, instLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, values);
        return ZR_NULL;
    }

    node->data.intermediateInstruction.name = name;
    node->data.intermediateInstruction.values = values;
    return node;
}

// 解析 Intermediate 常量

SZrAstNode *parse_intermediate_constant(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析常量名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    expect_token(ps, ZR_TK_EQUALS);
    consume_token(ps, ZR_TK_EQUALS);

    // 解析字面量值
    SZrAstNode *value = parse_literal(ps);
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange constLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_CONSTANT, constLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.intermediateConstant.name = name;
    node->data.intermediateConstant.value = value;
    return node;
}

// 解析 Intermediate 声明

SZrAstNode *parse_intermediate_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析名称
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
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

    // 解析闭包（可选）< ... >
    // 注意：在 intermediate 声明中，返回类型和闭包参数列表之间使用 % 分隔符
    // 格式：:int % < ... > 或 :int %（如果没有闭包参数）
    SZrAstNodeArray *closures = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_PERCENT)) {
        // 如果存在 % 分隔符，则解析闭包参数列表
        if (consume_token(ps, ZR_TK_LESS_THAN)) {
            // 解析闭包参数列表（格式与普通参数列表相同，但结束符是 >）
            // 闭包参数列表可以为空，所以先检查是否是 >
            // 使用 peek_token 检查，不要消费 token
            if (peek_token(ps) != ZR_TK_GREATER_THAN) {
                // 解析第一个参数
                SZrAstNode *first = parse_parameter(ps);
                if (first != ZR_NULL) {
                    ZrParser_AstNodeArray_Add(ps->state, closures, first);
                } else {
                    // 第一个参数解析失败，可能是语法错误
                    // 检查是否是空列表（直接是 >）
                    if (ps->lexer->t.token != ZR_TK_GREATER_THAN) {
                        // 不是空列表，但参数解析失败，报告错误
                        report_error(ps, "Failed to parse closure parameter");
                    }
                }

                // 解析后续参数（用逗号分隔）
                while (ps->lexer->t.token != ZR_TK_GREATER_THAN && ps->lexer->t.token != ZR_TK_EOS) {
                    if (!consume_token(ps, ZR_TK_COMMA)) {
                        // 没有逗号，可能是结束或错误
                        break;
                    }
                    if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
                        break;
                    }
                    SZrAstNode *param = parse_parameter(ps);
                    if (param != ZR_NULL) {
                        ZrParser_AstNodeArray_Add(ps->state, closures, param);
                    } else {
                        // 参数解析失败，报告错误但继续尝试恢复
                        break;
                    }
                }
            }
            expect_token(ps, ZR_TK_GREATER_THAN);
            consume_token(ps, ZR_TK_GREATER_THAN);
        }
        // 如果 % 后面没有 <，说明没有闭包参数列表，这是允许的
    }

    // 解析常量（可选）[ ... ]
    SZrAstNodeArray *constants = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_LBRACKET)) {
        while (ps->lexer->t.token != ZR_TK_RBRACKET && ps->lexer->t.token != ZR_TK_EOS) {
            SZrAstNode *constant = parse_intermediate_constant(ps);
            if (constant != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, constants, constant);
            } else {
                break;
            }
        }
        expect_token(ps, ZR_TK_RBRACKET);
        consume_token(ps, ZR_TK_RBRACKET);
    }

    // 解析局部变量（可选）( ... )
    SZrAstNodeArray *locals = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_LPAREN)) {
        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNodeArray *localParams = parse_parameter_list(ps);
            if (localParams != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, locals);
                locals = localParams;
            }
        }
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange declLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_DECLARATION, declLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL)
            ZrParser_AstNodeArray_Free(ps->state, params);
        if (closures != ZR_NULL)
            ZrParser_AstNodeArray_Free(ps->state, closures);
        if (constants != ZR_NULL)
            ZrParser_AstNodeArray_Free(ps->state, constants);
        if (locals != ZR_NULL)
            ZrParser_AstNodeArray_Free(ps->state, locals);
        return ZR_NULL;
    }

    node->data.intermediateDeclaration.name = name;
    node->data.intermediateDeclaration.params = params;
    node->data.intermediateDeclaration.args = args;
    node->data.intermediateDeclaration.returnType = returnType;
    node->data.intermediateDeclaration.closures = closures;
    node->data.intermediateDeclaration.constants = constants;
    node->data.intermediateDeclaration.locals = locals;
    return node;
}

// 解析 Intermediate 语句

SZrAstNode *parse_intermediate_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    expect_token(ps, ZR_TK_INTERMEDIATE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析声明
    SZrAstNode *declaration = parse_intermediate_declaration(ps);
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析指令块
    expect_token(ps, ZR_TK_LBRACE);
    consume_token(ps, ZR_TK_LBRACE);

    SZrAstNodeArray *instructions = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (instructions == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *instruction = parse_intermediate_instruction(ps);
        if (instruction != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, instructions, instruction);
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange stmtLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_STATEMENT, stmtLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, instructions);
        return ZR_NULL;
    }

    node->data.intermediateStatement.declaration = declaration;
    node->data.intermediateStatement.instructions = instructions;
    return node;
}

// 解析生成器表达式（{{}}）

SZrAstNode *parse_generator_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 期望第一个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 期望第二个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析块内容（语句列表），不期望 { 和 }
    SZrFileRange blockStartLoc = get_current_location(ps);
    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    // 解析语句直到遇到第一个 }
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *stmt = parse_statement(ps);
        if (stmt != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, statements, stmt);
        } else {
            break; // 遇到错误
        }
    }

    // 期望第一个 }
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    // 期望第二个 }
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    // 创建块节点
    SZrFileRange blockEndLoc = get_current_location(ps);
    SZrFileRange blockLoc = ZrParser_FileRange_Merge(blockStartLoc, blockEndLoc);
    SZrAstNode *block = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (block == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }
    block->data.block.body = statements;
    block->data.block.isStatement = ZR_FALSE; // 生成器表达式中的块是表达式

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange generatorLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERATOR_EXPRESSION, generatorLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.generatorExpression.block = block;
    return node;
}

// 解析类字段
