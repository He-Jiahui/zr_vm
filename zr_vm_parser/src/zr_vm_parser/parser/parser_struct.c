#include "parser_internal.h"

/* struct 成员分派器调用；字段的所有权语义来自类型标注，不能用旧式 using 语法。 */
SZrAstNode *parse_struct_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);

    /* BUG: 带装饰器的字段若在 using/%/var const 或名称解析处失败，
     * 下方早退只释放数组容器，装饰器 AST 仍被遗留。 */
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
    /* BUG: 节点分配失败只释放装饰器容器，名称、类型、初值和装饰器子节点泄漏。 */
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

/* struct 成员分派器调用；readonly struct 的默认接收者随后被提升为 const，
 * 显式 static/const 的语义约束由此 AST 与后续编译检查共同承担。 */
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
        /* TODO: 泛型解析返回 NULL 后仍继续方法解析；核查 hasError 的恢复/拒收契约。 */
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

    /* BUG: malformed where 后只释放 params/decorators 容器，名称、泛型、
     * 参数子节点和返回类型均未被最终 AST 接管。 */
    if (!parse_optional_where_clauses(ps, generic)) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    /* BUG: body 失败时名称、泛型、返回类型和已解析参数未释放。 */
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
    /* BUG: 分配失败时只销毁数组容器，方法签名和 body 子树仍泄漏。 */
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

/* struct 的 @ 元函数解析入口；成功节点负责参数、返回类型和方法体的生命周期。 */
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
        /* BUG: 拒绝已删除的 @decorate 后未释放解析出来的名称 AST。 */
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
    /* BUG: 分配失败时名称、参数节点、返回类型和方法体尚无所有者。 */
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

/* 顶层分派器在前缀可能是 readonly/ref 时使用该试探，必须恢复游标并释放
 * 临时装饰器 AST，正式解析才可取得这些资源。 */
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
        free_ast_node_array_with_elements(ps->state, decorators);
    }
    restore_parser_cursor(ps, &cursor);
    return result;
}

/* 顶层声明入口；统一 property parser 的结果和普通字段/方法进入同一成员树。
 * readonly 的默认接收者在此定型，供后续语义检查区分隐式与显式 const。 */
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
        /* TODO: 与 union 不同，此处未处理泛型解析返回 NULL；
         * 核查上层 hasError 是否保证该声明 AST 不被消费。 */
        generic = parse_generic_declaration(ps, ZR_FALSE);
    }

    /* TODO: struct AST/编译器保留 inherits 支持，但当前语法只建空数组；
     * 核对语言规范及 `struct : Interface` 需求后决定是否属于未实现的接口实现语法。 */
    SZrAstNodeArray *inherits = ZrParser_AstNodeArray_New(ps->state, 0);

    /* BUG: malformed where 返回时名称、泛型和装饰器子节点没有被释放。 */
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
            SZrParserCursor cursor;
            save_parser_cursor(ps, &cursor);
            while (ps->lexer->t.token == ZR_TK_SHARP) {
                SZrAstNode *decorator = parse_decorator_expression(ps);
                if (decorator == ZR_NULL) {
                    break;
                }
                ZrParser_Ast_Free(ps->state, decorator);
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
                restore_parser_cursor(ps, &cursor);
                member = parse_struct_field(ps);
            } else if (nextToken == ZR_TK_AT) {
                // 恢复状态并解析元函数
                restore_parser_cursor(ps, &cursor);
                member = parse_struct_meta_function(ps);
            } else {
                // 恢复状态并解析方法
                restore_parser_cursor(ps, &cursor);
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
    /* BUG: 节点分配失败只释放数组容器，已解析成员及声明头仍泄漏。 */
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
