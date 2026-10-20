#include "parser_internal.h"

/* Argument names borrow VM strings; only their native container is owned. */
static void free_construct_argument_names(SZrParserState *ps, SZrArray *argNames) {
    if (ps == ZR_NULL || argNames == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(ps->state, argNames);
    ZrCore_Memory_RawFreeWithType(ps->state->global,
                                  argNames,
                                  sizeof(SZrArray),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

// 为无参构造或借用表达式建立独立数组，避免共享可变 AST 容器。
SZrAstNodeArray *create_empty_argument_list(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    return parser_ast_array_new(ps, 0);
}

// 构造语法只接受位置实参；逐项查名后释放临时名称数组并报告拒绝原因。
TZrBool reject_named_construct_arguments(SZrParserState *ps, SZrArray *argNames, SZrFileRange location) {
    ZR_UNUSED_PARAMETER(location);

    if (ps == ZR_NULL || argNames == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize i = 0; i < argNames->length; i++) {
        SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            free_construct_argument_names(ps, argNames);
            report_error(ps, "Prototype construction does not support named arguments");
            ps->hasError = ZR_TRUE;
            return ZR_FALSE;
        }
    }

    free_construct_argument_names(ps, argNames);
    return ZR_TRUE;
}

// 把已解析目标移交给 prototype-reference AST，返回节点负责其递归释放。
// TODO: 当前仅见内部声明与此定义，需核对 prototype 构造语法是否仍需该包装入口，再决定保留或清理。
SZrAstNode *create_prototype_reference_node(SZrParserState *ps, SZrAstNode *target, SZrFileRange location) {
    SZrAstNode *node;

    if (ps == ZR_NULL || target == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.prototypeReferenceExpression.target = target;
    return node;
}

// 统一承载 new、own 与 ref 的目标、实参与所有权标志；成功后子树归构造节点。
SZrAstNode *create_construct_expression_node(SZrParserState *ps, SZrAstNode *target, SZrAstNodeArray *args,
                                                    EZrOwnershipQualifier ownershipQualifier, TZrBool isUsing,
                                                    TZrBool isNew, EZrOwnershipBuiltinKind builtinKind,
                                                    SZrFileRange location) {
    SZrAstNode *node;

    if (ps == ZR_NULL || target == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_CONSTRUCT_EXPRESSION, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.constructExpression.target = target;
    node->data.constructExpression.args = args;
    node->data.constructExpression.ownershipQualifier = ownershipQualifier;
    node->data.constructExpression.isUsing = isUsing;
    node->data.constructExpression.isNew = isNew;
    node->data.constructExpression.isResourceSurface = ZR_FALSE;
    node->data.constructExpression.builtinKind = builtinKind;
    return node;
}

// 解析 Type<...> 构造目标并包装为类型 AST；包装失败时释放临时泛型树。
SZrAstNode *parse_generic_construct_target(SZrParserState *ps) {
    SZrAstNode *genericNode;
    SZrAstNode *typeNode;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER || peek_token(ps) != ZR_TK_LESS_THAN) {
        return ZR_NULL;
    }

    genericNode = parse_generic_type(ps);
    if (genericNode == ZR_NULL || ps->hasError) {
        ZrParser_Ast_Free(ps->state, genericNode);
        return ZR_NULL;
    }

    typeNode = create_ast_node(ps, ZR_AST_TYPE, genericNode->data.genericType.wholeRange);
    if (typeNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, genericNode);
        return ZR_NULL;
    }

    typeNode->data.type.name = genericNode;
    return typeNode;
}

/* A prototype path is returned only after every member has been adopted. */
SZrAstNode *parse_prototype_path_expression(SZrParserState *ps) {
    SZrAstNode *base;
    SZrFileRange startLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        report_error(ps, "Expected identifier or member path");
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    base = parse_identifier(ps);
    if (base == ZR_NULL || ps->hasError) {
        ZrParser_Ast_Free(ps->state, base);
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_DOT) {
        SZrFileRange dotLocation = get_current_token_location(ps);
        SZrAstNode *property;
        SZrAstNode *memberNode;

        consume_token(ps, ZR_TK_DOT);
        if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
            report_error(ps, "Expected identifier after '.' in prototype path");
            ZrParser_Ast_Free(ps->state, base);
            return ZR_NULL;
        }

        if (peek_token(ps) == ZR_TK_LESS_THAN) {
            property = parse_generic_construct_target(ps);
        } else {
            property = parse_identifier(ps);
        }
        if (property == ZR_NULL || ps->hasError) {
            ZrParser_Ast_Free(ps->state, property);
            ZrParser_Ast_Free(ps->state, base);
            return ZR_NULL;
        }

        SZrFileRange memberLocation;
        if (!ZrParser_MergeLocations(ps, dotLocation, property->location, &memberLocation) ||
            (memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, memberLocation)) == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, property);
            ZrParser_Ast_Free(ps->state, base);
            return ZR_NULL;
        }
        memberNode->data.memberExpression.property = property;
        memberNode->data.memberExpression.computed = ZR_FALSE;
        memberNode->data.memberExpression.accessMode = ZR_POSTFIX_ACCESS_DIRECT;
        base = append_primary_member(ps, base, memberNode, startLoc);
        if (base == ZR_NULL) {
            return ZR_NULL;
        }
    }

    return base;
}

// 消费 new 目标与可选实参，拒绝命名参数后把子树交给构造节点。
SZrAstNode *parse_construct_expression(SZrParserState *ps,
                                              SZrFileRange startLoc,
                                              EZrOwnershipQualifier ownershipQualifier,
                                              TZrBool isUsing,
                                              EZrOwnershipBuiltinKind builtinKind) {
    SZrAstNode *target = ZR_NULL;
    SZrAstNodeArray *args = ZR_NULL;
    SZrArray *argNames = ZR_NULL;
    SZrAstNode *constructNode;
    SZrFileRange fullLoc;
    SZrFileRange endLoc = startLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_NEW);
    if (ps->lexer->t.token != ZR_TK_NEW) {
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        consume_token(ps, ZR_TK_LPAREN);
        target = parse_expression(ps);
        if (target == ZR_NULL || ps->hasError) {
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
        expect_token(ps, ZR_TK_RPAREN);
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_RPAREN);
    } else {
        if (ps->lexer->t.token == ZR_TK_IDENTIFIER && peek_token(ps) == ZR_TK_LESS_THAN) {
            target = parse_generic_construct_target(ps);
        } else {
            target = parse_prototype_path_expression(ps);
        }
        if (target != ZR_NULL) {
            endLoc = target->location;
        }
    }

    if (target == ZR_NULL || ps->hasError) {
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_LPAREN)) {
        args = parse_argument_list(ps, &argNames, ZR_NULL);
        if (args == ZR_NULL || ps->hasError) {
            free_ast_node_array_with_elements(ps->state, args);
            free_construct_argument_names(ps, argNames);
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
        expect_token(ps, ZR_TK_RPAREN);
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_RPAREN);
        if (ps->hasError) {
            free_ast_node_array_with_elements(ps->state, args);
            free_construct_argument_names(ps, argNames);
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
        if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
            free_ast_node_array_with_elements(ps->state, args);
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
    } else {
        args = create_empty_argument_list(ps);
        if (args == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
    }

    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &fullLoc)) {
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    constructNode = create_construct_expression_node(ps,
                                                     target,
                                                     args,
                                                     ownershipQualifier,
                                                     isUsing,
                                                     ZR_TRUE,
                                                     builtinKind,
                                                     fullLoc);
    if (constructNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    return constructNode;
}

// 解析 own Type(...) 资源表面语法，并标识唯一所有权构造供后续阶段区分。
SZrAstNode *parse_resource_own_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNodeArray *args;
    SZrArray *argNames = ZR_NULL;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "own")) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && peek_token(ps) == ZR_TK_LESS_THAN) {
        target = parse_generic_construct_target(ps);
    } else {
        target = parse_prototype_path_expression(ps);
    }
    if (target == ZR_NULL || ps->hasError) {
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    if (!consume_token(ps, ZR_TK_LPAREN)) {
        report_error(ps, "Expected '(' after resource construction target");
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    args = parse_argument_list(ps, &argNames, ZR_NULL);
    if (args == ZR_NULL || ps->hasError) {
        free_ast_node_array_with_elements(ps->state, args);
        free_construct_argument_names(ps, argNames);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    expect_token(ps, ZR_TK_RPAREN);
    SZrFileRange endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RPAREN);
    if (ps->hasError) {
        free_ast_node_array_with_elements(ps->state, args);
        free_construct_argument_names(ps, argNames);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    SZrFileRange constructLocation;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &constructLocation)) {
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    node = create_construct_expression_node(
            ps,
            target,
            args,
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_FALSE,
            ZR_TRUE,
            ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE,
            constructLocation);
    if (node == ZR_NULL) {
        /* The constructor adopts target and arguments only after allocation. */
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    node->data.constructExpression.isResourceSurface = ZR_TRUE;
    return node;
}

// 把 ref operand 降为无参借用构造节点；拒绝 ref ref 并在创建失败时回收目标。
SZrAstNode *parse_reference_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNodeArray *args;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->lexer->t.token != ZR_TK_REF) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_REF) {
        report_error(ps, "Nested 'ref ref' is not valid; use 'ref existingRef' to reborrow");
        return ZR_NULL;
    }

    target = parse_unary_expression(ps);
    if (target == ZR_NULL || ps->hasError) {
        if (!ps->hasError) {
            report_error(ps, "Expected an expression after 'ref'");
        }
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    args = create_empty_argument_list(ps);
    if (args == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    SZrFileRange referenceLocation;
    if (!ZrParser_MergeLocations(ps, startLoc, target->location, &referenceLocation)) {
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    node = create_construct_expression_node(
            ps,
            target,
            args,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            ZR_FALSE,
            ZR_OWNERSHIP_BUILTIN_KIND_BORROW,
            referenceLocation);
    if (node == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
    }
    return node;
}
