#include "parser_internal.h"
static TZrUInt32 class_member_allowed_modifier_flags(void) {
    return ZR_DECLARATION_MODIFIER_ABSTRACT |
           ZR_DECLARATION_MODIFIER_VIRTUAL |
           ZR_DECLARATION_MODIFIER_OVERRIDE |
           ZR_DECLARATION_MODIFIER_FINAL |
           ZR_DECLARATION_MODIFIER_SHADOW;
}

static EZrAstNodeType classify_class_member_from_current(SZrParserState *ps) {
    SZrParserCursor cursor;
    EZrAstNodeType kind = ZR_AST_CLASS_METHOD;
    TZrBool sawFieldUsingPrefix = ZR_FALSE;

    if (ps == ZR_NULL) {
        return ZR_AST_CLASS_METHOD;
    }

    save_parser_cursor(ps, &cursor);

    if (consume_percent_keyword_token(ps, ZR_TK_USING)) {
        sawFieldUsingPrefix = ZR_TRUE;
    } else if (ps->lexer->t.token == ZR_TK_PERCENT) {
        parse_optional_method_receiver_qualifier(ps);
    }

    while (ps->lexer->t.token == ZR_TK_SHARP) {
        parse_decorator_expression(ps);
    }

    parse_access_modifier(ps);
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

    if (consume_percent_keyword_token(ps, ZR_TK_USING)) {
        sawFieldUsingPrefix = ZR_TRUE;
    }

    if (sawFieldUsingPrefix || ps->lexer->t.token == ZR_TK_VAR || ps->lexer->t.token == ZR_TK_CONST) {
        kind = ZR_AST_CLASS_FIELD;
    } else if (ps->lexer->t.token == ZR_TK_GET || ps->lexer->t.token == ZR_TK_SET) {
        kind = ZR_AST_CLASS_PROPERTY;
    } else if (ps->lexer->t.token == ZR_TK_AT) {
        kind = ZR_AST_CLASS_META_FUNCTION;
    } else {
        kind = ZR_AST_CLASS_METHOD;
    }

    restore_parser_cursor(ps, &cursor);
    return kind;
}

SZrAstNode *parse_class_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    SZrAstNodeArray *decorators;
    TZrUInt32 modifierFlags;

    // 允许 top-level decorators 出现在访问修饰符之前。
    decorators = parse_leading_decorators(ps);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    modifierFlags = parse_declaration_modifier_flags(ps,
                                                     ZR_DECLARATION_MODIFIER_ABSTRACT |
                                                             ZR_DECLARATION_MODIFIER_FINAL);

    // 同时继续兼容访问修饰符之后的装饰器写法。
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
        generic = parse_generic_declaration(ps, ZR_FALSE);
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
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }

    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (members == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        EZrToken token = ps->lexer->t.token;

        if (token == ZR_TK_PERCENT || token == ZR_TK_SHARP || token == ZR_TK_PUB || token == ZR_TK_PRI ||
            token == ZR_TK_PRO || token == ZR_TK_STATIC || token == ZR_TK_CONST || token == ZR_TK_USING ||
            token == ZR_TK_VAR || token == ZR_TK_ABSTRACT || token == ZR_TK_VIRTUAL ||
            token == ZR_TK_OVERRIDE || token == ZR_TK_FINAL || token == ZR_TK_SHADOW || token == ZR_TK_AT ||
            token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
            switch (classify_class_member_from_current(ps)) {
                case ZR_AST_CLASS_FIELD:
                    member = parse_class_field(ps);
                    break;
                case ZR_AST_CLASS_PROPERTY:
                    member = parse_class_property(ps);
                    break;
                case ZR_AST_CLASS_META_FUNCTION:
                    member = parse_class_meta_function(ps);
                    break;
                case ZR_AST_CLASS_METHOD:
                default:
                    member = parse_class_method(ps);
                    break;
            }
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
    node->data.classDeclaration.modifierFlags = modifierFlags;
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
    TZrUInt32 invalidModifierFlags = ZR_DECLARATION_MODIFIER_NONE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    invalidModifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());
    if (invalidModifierFlags != ZR_DECLARATION_MODIFIER_NONE) {
        report_error(ps, "Field declarations do not support abstract/virtual/override/final/shadow modifiers");
    }

    if (ps->lexer->t.token == ZR_TK_USING) {
        report_error(ps, "Field-scoped '%using' is no longer supported; express ownership on the field type directly");
        skip_to_semicolon_or_eos(ps);
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    TZrBool fieldIsUsingManaged = ZR_FALSE;
    if (consume_percent_keyword_token(ps, ZR_TK_USING)) {
        report_error(ps, "Field-scoped '%using' is removed; write '%unique field: T' or '%shared field: T' directly in the type");
        skip_to_semicolon_or_eos(ps);
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
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
    SZrFileRange endLoc;
    EZrOwnershipQualifier receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    TZrUInt32 modifierFlags = ZR_DECLARATION_MODIFIER_NONE;

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

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

    if (receiverQualifier == ZR_OWNERSHIP_QUALIFIER_NONE && ps->lexer->t.token == ZR_TK_PERCENT) {
        receiverQualifier = parse_optional_method_receiver_qualifier(ps);
    }

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "func")) {
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
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    // 解析方法体；允许 bodyless 成员声明，由语义阶段约束 abstract/非法组合。
    SZrAstNode *body = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
            if (params != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, params);
            }
            return ZR_NULL;
        }
        endLoc = body->location;
    }

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
    node->data.classMethod.modifierFlags = modifierFlags;
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
    TZrUInt32 modifierFlags;

    // 期望 get 关键字
    expect_token(ps, ZR_TK_GET);
    ZrParser_Lexer_Next(ps->lexer);

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

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

    // 解析方法体；抽象访问器等场景允许使用 ';'。
    SZrAstNode *body = ZR_NULL;
    SZrFileRange endLoc;
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            return ZR_NULL;
        }
        endLoc = body->location;
    }

    SZrFileRange getLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_GET, getLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.propertyGet.modifierFlags = modifierFlags;
    node->data.propertyGet.name = name;
    node->data.propertyGet.nameLocation = nameLoc;
    node->data.propertyGet.targetType = targetType;
    node->data.propertyGet.body = body;
    return node;
}

// 解析属性 Setter

SZrAstNode *parse_property_set(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    TZrUInt32 modifierFlags;

    // 期望 set 关键字
    expect_token(ps, ZR_TK_SET);
    ZrParser_Lexer_Next(ps->lexer);

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

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

    // 解析方法体；抽象访问器等场景允许使用 ';'。
    SZrAstNode *body = ZR_NULL;
    SZrFileRange endLoc;
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            return ZR_NULL;
        }
        endLoc = body->location;
    }

    SZrFileRange setLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_SET, setLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.propertySet.modifierFlags = modifierFlags;
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
    TZrUInt32 modifierFlags = ZR_DECLARATION_MODIFIER_NONE;

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

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

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
    node->data.classProperty.modifierFlags = modifierFlags;
    node->data.classProperty.modifier = modifier;
    return node;
}

// 解析类元函数

SZrAstNode *parse_class_meta_function(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    TZrUInt32 modifierFlags = ZR_DECLARATION_MODIFIER_NONE;

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

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

    // 解析方法体；抽象成员等场景允许使用 ';'。
    SZrAstNode *body = ZR_NULL;
    SZrFileRange endLoc;
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            if (params != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, params);
            }
            ZrParser_AstNodeArray_Free(ps->state, superArgs);
            return ZR_NULL;
        }
        endLoc = get_current_location(ps);
    }

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
    node->data.classMetaFunction.modifierFlags = modifierFlags;
    node->data.classMetaFunction.meta = meta;
    node->data.classMetaFunction.params = params;
    node->data.classMetaFunction.args = args;
    node->data.classMetaFunction.hasSuperCall = hasSuperCall;
    node->data.classMetaFunction.superArgs = superArgs;
    node->data.classMetaFunction.returnType = returnType;
    node->data.classMetaFunction.body = body;
    return node;
}
