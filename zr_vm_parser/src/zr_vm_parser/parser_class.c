#include "parser_internal.h"

SZrAstNode *parse_class_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

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

    // 期望 class 关键字
    expect_token(ps, ZR_TK_CLASS);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析类名
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
        generic = parse_generic_declaration(ps);
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

    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, 8);
    if (members == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;

        // 检查成员类型
        EZrToken token = ps->lexer->t.token;

        // 检查是否是装饰器或访问修饰符（可能是字段、方法或属性）
        if (token == ZR_TK_PERCENT || token == ZR_TK_SHARP || token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO ||
            token == ZR_TK_STATIC || token == ZR_TK_CONST || token == ZR_TK_USING ||
            token == ZR_TK_VAR) {
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

            // 跳过装饰器
            if (ps->lexer->t.token == ZR_TK_PERCENT) {
                parse_optional_method_receiver_qualifier(ps);
            }
            while (ps->lexer->t.token == ZR_TK_SHARP) {
                parse_decorator_expression(ps);
            }

            // 跳过访问修饰符和 static（用于 lookahead）
            // 注意：不跳过 const，因为 const 可能是字段的一部分
            while (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI ||
                   ps->lexer->t.token == ZR_TK_PRO || ps->lexer->t.token == ZR_TK_STATIC) {
                ZrParser_Lexer_Next(ps->lexer);
            }

            // 检查是字段、属性还是方法
            // 如果下一个 token 是 const 或 var，则是字段
            EZrToken nextToken = ps->lexer->t.token;

            // 恢复状态，让成员解析函数处理
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

            if (nextToken == ZR_TK_VAR || nextToken == ZR_TK_CONST || nextToken == ZR_TK_USING) {
                // 字段（var 或 const 都可以表示字段）
                member = parse_class_field(ps);
            } else if (nextToken == ZR_TK_GET || nextToken == ZR_TK_SET) {
                // 属性
                member = parse_class_property(ps);
            } else if (nextToken == ZR_TK_AT) {
                // 元函数
                member = parse_class_meta_function(ps);
            } else {
                // 方法
                member = parse_class_method(ps);
            }
        } else if (token == ZR_TK_AT) {
            // 元函数
            member = parse_class_meta_function(ps);
        } else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_SHARP || token == ZR_TK_PERCENT) {
            // 方法（可能有装饰器）
            member = parse_class_method(ps);
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in class declaration");
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
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange classLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_DECLARATION, classLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }

    node->data.classDeclaration.name = name;
    node->data.classDeclaration.nameLocation = nameLoc;
    node->data.classDeclaration.generic = generic;
    node->data.classDeclaration.inherits = inherits;
    node->data.classDeclaration.members = members;
    node->data.classDeclaration.decorators = decorators;
    node->data.classDeclaration.accessModifier = accessModifier;
    node->data.classDeclaration.isOwned = ZR_FALSE;
    return node;
}

// 解析接口字段声明

SZrAstNode *parse_class_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;

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

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析 using 关键字（可选，field-scoped 生命周期管理）
    TZrBool fieldIsUsingManaged = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_USING) {
        fieldIsUsingManaged = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析 const 关键字（可选，可以在 var 之前或之后）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // var 关键字是可选的（如果已经有 const，可以省略 var）
    if (ps->lexer->t.token == ZR_TK_VAR) {
        ZrParser_Lexer_Next(ps->lexer);

        // 如果 var 后面还有 const，也解析它（支持 var const 语法）
        if (ps->lexer->t.token == ZR_TK_CONST) {
            isConst = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    } else if (!isConst) {
        // 如果没有 const 也没有 var，期望 var 关键字
        expect_token(ps, ZR_TK_VAR);
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析字段名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    // 可选初始值
    SZrAstNode *init = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        init = parse_expression(ps);
    }

    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_SEMICOLON);
    SZrFileRange fieldLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_FIELD, fieldLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.classField.decorators = decorators;
    node->data.classField.access = access;
    node->data.classField.isStatic = isStatic;
    node->data.classField.isUsingManaged = fieldIsUsingManaged;
    node->data.classField.isConst = isConst;
    node->data.classField.name = name;
    node->data.classField.nameLocation = nameLoc;
    node->data.classField.typeInfo = typeInfo;
    node->data.classField.init = init;
    return node;
}

// 解析类方法

SZrAstNode *parse_class_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    EZrOwnershipQualifier receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        receiverQualifier = parse_optional_method_receiver_qualifier(ps);
    }

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

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析方法名
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
        generic = parse_generic_declaration(ps);
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

    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    SZrFileRange endLoc = body->location;
    SZrFileRange methodLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_METHOD, methodLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    node->data.classMethod.decorators = decorators;
    node->data.classMethod.access = access;
    node->data.classMethod.isStatic = isStatic;
    node->data.classMethod.receiverQualifier = receiverQualifier;
    node->data.classMethod.name = name;
    node->data.classMethod.nameLocation = nameLoc;
    node->data.classMethod.generic = generic;
    node->data.classMethod.params = params;
    node->data.classMethod.args = args;
    node->data.classMethod.returnType = returnType;
    node->data.classMethod.body = body;
    return node;
}

// 解析属性 Getter

SZrAstNode *parse_property_get(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);

    // 期望 get 关键字
    expect_token(ps, ZR_TK_GET);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 可选返回类型
    SZrType *targetType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        targetType = parse_type(ps);
    }

    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = body->location;
    SZrFileRange getLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_GET, getLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.propertyGet.name = name;
    node->data.propertyGet.nameLocation = nameLoc;
    node->data.propertyGet.targetType = targetType;
    node->data.propertyGet.body = body;
    return node;
}

// 解析属性 Setter

SZrAstNode *parse_property_set(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);

    // 期望 set 关键字
    expect_token(ps, ZR_TK_SET);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析参数名
    SZrAstNode *paramNode = parse_identifier(ps);
    if (paramNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *param = &paramNode->data.identifier;

    // 可选参数类型
    SZrType *targetType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        targetType = parse_type(ps);
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = body->location;
    SZrFileRange setLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_SET, setLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.propertySet.name = name;
    node->data.propertySet.nameLocation = nameLoc;
    node->data.propertySet.param = param;
    node->data.propertySet.targetType = targetType;
    node->data.propertySet.body = body;
    return node;
}

// 解析类属性

SZrAstNode *parse_class_property(SZrParserState *ps) {
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

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析 get 或 set
    SZrAstNode *modifier = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_GET) {
        modifier = parse_property_get(ps);
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        modifier = parse_property_set(ps);
    } else {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        report_error(ps, "Expected 'get' or 'set' for property");
        return ZR_NULL;
    }

    if (modifier == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    SZrFileRange endLoc = modifier->location;
    SZrFileRange propertyLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_PROPERTY, propertyLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.classProperty.decorators = decorators;
    node->data.classProperty.access = access;
    node->data.classProperty.isStatic = isStatic;
    node->data.classProperty.modifier = modifier;
    return node;
}

// 解析类元函数

SZrAstNode *parse_class_meta_function(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

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

    // 解析 super 调用参数（可选）
    TZrBool hasSuperCall = ZR_FALSE;
    SZrAstNodeArray *superArgs = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_SUPER)) {
        hasSuperCall = ZR_TRUE;
        expect_token(ps, ZR_TK_LPAREN);
        ZrParser_Lexer_Next(ps->lexer);

        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNode *firstArg = parse_expression(ps);
            if (firstArg != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, superArgs, firstArg);
            }

            while (consume_token(ps, ZR_TK_COMMA)) {
                if (ps->lexer->t.token == ZR_TK_RPAREN) {
                    break;
                }
                SZrAstNode *arg = parse_expression(ps);
                if (arg != ZR_NULL) {
                    ZrParser_AstNodeArray_Add(ps->state, superArgs, arg);
                } else {
                    break;
                }
            }
        }

        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    }

    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, superArgs);
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_META_FUNCTION, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, superArgs);
        return ZR_NULL;
    }

    node->data.classMetaFunction.access = access;
    node->data.classMetaFunction.isStatic = isStatic;
    node->data.classMetaFunction.meta = meta;
    node->data.classMetaFunction.params = params;
    node->data.classMetaFunction.args = args;
    node->data.classMetaFunction.hasSuperCall = hasSuperCall;
    node->data.classMetaFunction.superArgs = superArgs;
    node->data.classMetaFunction.returnType = returnType;
    node->data.classMetaFunction.body = body;
    return node;
}
