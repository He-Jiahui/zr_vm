#include "parser_internal.h"

SZrAstNode *parse_interface_field_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 const 关键字（可选，可以在 var 之前或之后）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (ps->lexer->t.token == ZR_TK_VAR) {
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_CONST) {
            isConst = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    } else if (!isConst) {
        expect_token(ps, ZR_TK_VAR);
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析字段名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange fieldLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_FIELD_DECLARATION, fieldLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.interfaceFieldDeclaration.access = access;
    node->data.interfaceFieldDeclaration.isConst = isConst;
    node->data.interfaceFieldDeclaration.name = name;
    node->data.interfaceFieldDeclaration.typeInfo = typeInfo;
    return node;
}

// 解析接口方法签名

SZrAstNode *parse_interface_method_signature(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析方法名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_FALSE);
    }

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

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

    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    if (!parse_optional_where_clauses(ps, generic)) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange methodLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_METHOD_SIGNATURE, methodLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    node->data.interfaceMethodSignature.access = access;
    node->data.interfaceMethodSignature.name = name;
    node->data.interfaceMethodSignature.generic = generic;
    node->data.interfaceMethodSignature.params = params;
    node->data.interfaceMethodSignature.args = args;
    node->data.interfaceMethodSignature.returnType = returnType;
    return node;
}

// 解析接口属性签名

SZrAstNode *parse_interface_property_signature(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 get/set 修饰符
    TZrBool hasGet = ZR_FALSE;
    TZrBool hasSet = ZR_FALSE;

    if (ps->lexer->t.token == ZR_TK_GET) {
        hasGet = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_SET) {
            hasSet = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        hasSet = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_GET) {
            hasGet = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    }

    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange propertyLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_PROPERTY_SIGNATURE, propertyLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.interfacePropertySignature.access = access;
    node->data.interfacePropertySignature.hasGet = hasGet;
    node->data.interfacePropertySignature.hasSet = hasSet;
    node->data.interfacePropertySignature.name = name;
    node->data.interfacePropertySignature.typeInfo = typeInfo;
    return node;
}

// 解析接口元函数签名

SZrAstNode *parse_interface_meta_signature(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 期望 @ 符号
    expect_token(ps, ZR_TK_AT);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析元标识符
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

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

    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_META_SIGNATURE, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    node->data.interfaceMetaSignature.access = access;
    node->data.interfaceMetaSignature.meta = meta;
    node->data.interfaceMetaSignature.params = params;
    node->data.interfaceMetaSignature.args = args;
    node->data.interfaceMetaSignature.returnType = returnType;
    return node;
}

// 解析接口声明

SZrAstNode *parse_interface_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    // 期望 interface 关键字
    expect_token(ps, ZR_TK_INTERFACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析接口名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_TRUE);
    }

    // 解析继承列表（可选）
    SZrAstNodeArray *inherits = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_COLON)) {
        // 解析类型列表
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            SZrType *firstType = parse_type(ps);
            if (firstType != ZR_NULL) {
                SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                if (typeNode != ZR_NULL) {
                    typeNode->data.type = *firstType;
                    ZrCore_Memory_RawFreeWithType(ps->state->global, firstType, sizeof(SZrType),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    ZrParser_AstNodeArray_Add(ps->state, inherits, typeNode);
                }
            }

            while (consume_token(ps, ZR_TK_COMMA)) {
                if (ps->lexer->t.token == ZR_TK_LBRACE) {
                    break;
                }
                SZrType *type = parse_type(ps);
                if (type != ZR_NULL) {
                    SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                    if (typeNode != ZR_NULL) {
                        typeNode->data.type = *type;
                        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType),
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrParser_AstNodeArray_Add(ps->state, inherits, typeNode);
                    }
                } else {
                    break;
                }
            }
        }
    }

    if (!parse_optional_where_clauses(ps, generic)) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }

    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, 8);
    if (members == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;

        // 检查成员类型
        EZrToken token = ps->lexer->t.token;
        if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO ||
            token == ZR_TK_VAR || token == ZR_TK_CONST) {
            // 向前看以确定成员类型
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

            if (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI ||
                ps->lexer->t.token == ZR_TK_PRO) {
                ZrParser_Lexer_Next(ps->lexer);
            }

            EZrToken nextToken = ps->lexer->t.token;

            if (nextToken == ZR_TK_VAR || nextToken == ZR_TK_CONST) {
                // 字段声明
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
                member = parse_interface_field_declaration(ps);
            } else if (nextToken == ZR_TK_GET || nextToken == ZR_TK_SET) {
                // 属性签名
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
                member = parse_interface_property_signature(ps);
            } else if (nextToken == ZR_TK_AT) {
                // 元函数签名
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
                member = parse_interface_meta_signature(ps);
            } else {
                // 方法签名
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
                member = parse_interface_method_signature(ps);
            }
        } else if (token == ZR_TK_AT) {
            // 元函数签名
            member = parse_interface_meta_signature(ps);
        } else if (token == ZR_TK_IDENTIFIER) {
            // 方法签名
            member = parse_interface_method_signature(ps);
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in interface declaration");
            break;
        }

        if (member != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }

    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange interfaceLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_DECLARATION, interfaceLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }

    node->data.interfaceDeclaration.name = name;
    node->data.interfaceDeclaration.generic = generic;
    node->data.interfaceDeclaration.inherits = inherits;
    node->data.interfaceDeclaration.members = members;
    node->data.interfaceDeclaration.accessModifier = accessModifier;
    return node;
}

// 存根实现：枚举声明解析
// 解析枚举成员
