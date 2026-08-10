#include "parser_internal.h"

SZrAstNode *parse_struct_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
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

    if (ps->lexer->t.token == ZR_TK_USING) {
        report_error(ps, "Field-scoped 'using' is invalid; write `var field: Unique<T>` or `var field: Shared<T>` so ownership lives in the field type");
        skip_to_semicolon_or_eos(ps);
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        report_removed_percent_syntax(ps);
        skip_to_semicolon_or_eos(ps);
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    // 解析 const 关键字（可选，可以在 var 之前或之后）
    TZrBool isConst = ps->lexer->t.token == ZR_TK_LET;
    if (isConst) {
        ZrParser_Lexer_Next(ps->lexer);
    }
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // var 关键字是可选的（如果已经有 const，可以省略 var）
    if (!isConst && ps->lexer->t.token == ZR_TK_VAR) {
        ZrParser_Lexer_Next(ps->lexer);

        // `let` is the sole immutable binding spelling after the cutover.
        if (ps->lexer->t.token == ZR_TK_CONST) {
            report_removed_legacy_syntax(
                    ps,
                    "var const struct field",
                    "Use `let field: Type = value;` for an immutable struct field.");
            ZrParser_AstNodeArray_Free(ps->state, decorators);
            return ZR_NULL;
        }
    } else if (!isConst) {
        // 如果没有 const 也没有 var，期望 var 关键字
        expect_token(ps, ZR_TK_VAR);
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析字段名
    SZrAstNode *nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

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
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange fieldLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_FIELD, fieldLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.structField.decorators = decorators;
    node->data.structField.access = access;
    node->data.structField.isStatic = isStatic;
    node->data.structField.reservedRemovedUsingManaged = ZR_FALSE;
    node->data.structField.isConst = isConst;
    node->data.structField.name = name;
    node->data.structField.typeInfo = typeInfo;
    node->data.structField.init = init;
    return node;
}

// 解析结构体方法

SZrAstNode *parse_struct_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    EZrOwnershipQualifier receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    EZrMethodReceiverModifier receiverModifier = ZR_METHOD_RECEIVER_DEFAULT;
    TZrBool isAsync = ZR_FALSE;

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        report_removed_percent_syntax(ps);
        return ZR_NULL;
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

    if (ps->lexer->t.token == ZR_TK_CONST) {
        receiverModifier = ZR_METHOD_RECEIVER_CONST;
        ZrParser_Lexer_Next(ps->lexer);
        if (isStatic) {
            report_error(ps, "static const fn is invalid because static functions have no receiver");
        }
    }

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "async") &&
        peek_token(ps) == ZR_TK_FN) {
        isAsync = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (ps->lexer->t.token != ZR_TK_FN) {
        report_removed_legacy_syntax(ps,
                                     "keywordless struct method",
                                     "Prefix the method declaration with `fn`.");
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    // 解析方法名
    SZrAstNode *nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
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

    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange methodLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_METHOD, methodLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.structMethod.decorators = decorators;
    node->data.structMethod.access = access;
    node->data.structMethod.isStatic = isStatic;
    node->data.structMethod.receiverModifier = receiverModifier;
    node->data.structMethod.isImplicitReadonlyReceiver = ZR_FALSE;
    node->data.structMethod.isAsync = isAsync;
    node->data.structMethod.receiverQualifier = receiverQualifier;
    node->data.structMethod.name = name;
    node->data.structMethod.generic = generic;
    node->data.structMethod.params = params;
    node->data.structMethod.args = args;
    node->data.structMethod.returnType = returnType;
    node->data.structMethod.body = body;
    return node;
}

// 解析结构体元函数

SZrAstNode *parse_struct_meta_function(SZrParserState *ps) {
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

    // 解析元标识符（@ 后面跟小写蛇形标识符）
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    if (meta->name != ZR_NULL &&
        strcmp(ZrCore_String_GetNativeString(meta->name), "decorate") == 0) {
        report_error(
                ps,
                "@decorate was removed; use a declarationTransform comptime function");
        return ZR_NULL;
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

    // 解析返回类型（可选）
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    // 解析函数体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_META_FUNCTION, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }

    node->data.structMetaFunction.access = access;
    node->data.structMetaFunction.isStatic = isStatic;
    node->data.structMetaFunction.meta = meta;
    node->data.structMetaFunction.params = params;
    node->data.structMetaFunction.args = args;
    node->data.structMetaFunction.returnType = returnType;
    node->data.structMetaFunction.body = body;
    return node;
}

// 解析结构体声明

TZrBool parser_struct_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    SZrAstNodeArray *decorators;
    TZrBool result;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_FALSE;
    }
    save_parser_cursor(ps, &cursor);
    decorators = parse_leading_decorators(ps);
    (void)parse_access_modifier(ps);
    if (current_identifier_equals(ps, "readonly")) {
        ZrParser_Lexer_Next(ps->lexer);
    }
    if (ps->lexer->t.token == ZR_TK_REF) {
        ZrParser_Lexer_Next(ps->lexer);
    }
    result = ps->lexer->t.token == ZR_TK_STRUCT;
    if (decorators != ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
    }
    restore_parser_cursor(ps, &cursor);
    return result;
}

SZrAstNode *parse_struct_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);
    TZrBool isReadonly = ZR_FALSE;
    TZrBool isRefLike = ZR_FALSE;

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    if (current_identifier_equals(ps, "readonly")) {
        isReadonly = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    if (ps->lexer->t.token == ZR_TK_REF) {
        isRefLike = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 期望 struct 关键字
    expect_token(ps, ZR_TK_STRUCT);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析结构体名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_FALSE);
    }

    // TODO: 解析继承列表（可选，但注释说 struct 不允许继承，所以这里暂时不支持）
    SZrAstNodeArray *inherits = ZrParser_AstNodeArray_New(ps->state, 0);

    if (!parse_optional_where_clauses(ps, generic)) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
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
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;

        if (parser_property_declaration_starts_here(ps)) {
            member = parse_property_declaration(
                    ps, ZR_PROPERTY_CONTAINER_STRUCT);
        }

        // 检查是否是字段（以 var 开头，可能前面有访问修饰符、static 或 const）
        EZrToken token = ps->lexer->t.token;
        if (member != ZR_NULL) {
            // 已由共享 property parser 消费。
        } else if (token == ZR_TK_PERCENT) {
            report_removed_percent_syntax(ps);
            break;
        } else if (token == ZR_TK_SHARP ||
            token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO ||
            token == ZR_TK_STATIC || token == ZR_TK_CONST || token == ZR_TK_USING ||
            token == ZR_TK_VAR || token == ZR_TK_LET) {
            // 可能是字段，尝试解析
            // 需要向前看一个 token 来确定
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
            while (ps->lexer->t.token == ZR_TK_SHARP) {
                parse_decorator_expression(ps);
            }

            while (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI ||
                   ps->lexer->t.token == ZR_TK_PRO || ps->lexer->t.token == ZR_TK_STATIC) {
                ZrParser_Lexer_Next(ps->lexer);
            }

            EZrToken nextToken = ps->lexer->t.token;

            if (nextToken == ZR_TK_CONST) {
                ZrParser_Lexer_Next(ps->lexer);
                nextToken = ps->lexer->t.token == ZR_TK_FN
                                    ? ZR_TK_FN
                                    : ZR_TK_CONST;
            }

            if (nextToken == ZR_TK_VAR || nextToken == ZR_TK_LET ||
                nextToken == ZR_TK_CONST || nextToken == ZR_TK_USING) {
                // 恢复状态并解析字段
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
                member = parse_struct_field(ps);
            } else if (nextToken == ZR_TK_AT) {
                // 恢复状态并解析元函数
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
                member = parse_struct_meta_function(ps);
            } else {
                // 恢复状态并解析方法
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
                member = parse_struct_method(ps);
            }
        } else if (token == ZR_TK_AT) {
            // 元函数
            member = parse_struct_meta_function(ps);
        } else if (token == ZR_TK_GET || token == ZR_TK_SET) {
            report_removed_legacy_syntax(
                    ps,
                    "split get/set struct property",
                    "Use one `property name: Type { get ...; set ...; }` declaration.");
        } else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_SHARP ||
                   token == ZR_TK_FN) {
            // 方法（可能有装饰器）
            member = parse_struct_method(ps);
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in struct declaration");
            break;
        }

        if (member != ZR_NULL && isReadonly &&
            member->type == ZR_AST_STRUCT_METHOD &&
            !member->data.structMethod.isStatic &&
            member->data.structMethod.receiverModifier == ZR_METHOD_RECEIVER_DEFAULT) {
            member->data.structMethod.receiverModifier = ZR_METHOD_RECEIVER_CONST;
            member->data.structMethod.isImplicitReadonlyReceiver = ZR_TRUE;
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
    SZrFileRange structLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_DECLARATION, structLoc);
    if (node == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }

    node->data.structDeclaration.name = name;
    node->data.structDeclaration.generic = generic;
    node->data.structDeclaration.inherits = inherits;
    node->data.structDeclaration.members = members;
    node->data.structDeclaration.decorators = decorators;
    node->data.structDeclaration.isReadonly = isReadonly;
    node->data.structDeclaration.isRefLike = isRefLike;
    node->data.structDeclaration.accessModifier = accessModifier;
    return node;
}

// 解析类声明
