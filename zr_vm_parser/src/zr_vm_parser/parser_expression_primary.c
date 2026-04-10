#include "parser_internal.h"

TZrBool try_get_ownership_qualifier(SZrString *name, EZrOwnershipQualifier *qualifier) {
    if (qualifier == ZR_NULL) {
        return ZR_FALSE;
    }

    *qualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_string_equals_literal(name, "unique")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "shared")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "weak")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "borrowed")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "loaned")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static EZrOwnershipBuiltinKind ownership_builtin_kind_from_flags(EZrOwnershipQualifier ownershipQualifier,
                                                                 TZrBool isUsing) {
    ZR_UNUSED_PARAMETER(isUsing);

    switch (ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE;
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return ZR_OWNERSHIP_BUILTIN_KIND_SHARED;
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return ZR_OWNERSHIP_BUILTIN_KIND_WEAK;
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
        default:
            return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
}

SZrAstNodeArray *parse_argument_list(SZrParserState *ps, SZrArray **argNames) {
    SZrAstNodeArray *args = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (args == ZR_NULL) {
        if (argNames != ZR_NULL) {
            *argNames = ZR_NULL;
        }
        return ZR_NULL;
    }

    // 初始化参数名数组
    SZrArray *names = ZR_NULL;
    if (argNames != ZR_NULL) {
        *argNames = ZR_NULL;
    }
    TZrBool hasNamedArgs = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        // 检查第一个参数是否为命名参数（identifier: expression）
        TZrBool isNamed = ZR_FALSE;
        SZrString *paramName = ZR_NULL;

        if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            // 保存标识符名称
            paramName = ps->lexer->t.seminfo.stringValue;
            EZrToken lookahead = peek_token(ps);
            if (lookahead == ZR_TK_COLON) {
                // 这是命名参数：identifier: expression
                isNamed = ZR_TRUE;
                ZrParser_Lexer_Next(ps->lexer); // 跳过 identifier
                consume_token(ps, ZR_TK_COLON); // 跳过 :
            }
        }

        if (isNamed) {
            hasNamedArgs = ZR_TRUE;
            // 创建参数名数组
            if (names == ZR_NULL) {
                names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray),
                                                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (names != ZR_NULL) {
                    ZrCore_Array_Init(ps->state, names, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
                }
            }
            if (names != ZR_NULL) {
                ZrCore_Array_Push(ps->state, names, &paramName);
            }
        } else {
            // 位置参数，参数名为 ZR_NULL
            if (names == ZR_NULL) {
                names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray),
                                                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (names != ZR_NULL) {
                    ZrCore_Array_Init(ps->state, names, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
                }
            }
            if (names != ZR_NULL) {
                SZrString *nullName = ZR_NULL;
                ZrCore_Array_Push(ps->state, names, &nullName);
            }
        }

        // 解析参数值表达式
        SZrAstNode *first = parse_expression(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, args, first);
        } else {
            // 表达式解析失败，清理并返回
            if (names != ZR_NULL) {
                ZrCore_Array_Free(ps->state, names);
                ZrCore_Memory_RawFreeWithType(ps->state->global, names, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (argNames != ZR_NULL) {
                *argNames = ZR_NULL;
            }
            return args; // 返回部分解析的结果
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }

            // 检查是否为命名参数
            isNamed = ZR_FALSE;
            paramName = ZR_NULL;

            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                paramName = ps->lexer->t.seminfo.stringValue;
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_COLON) {
                    isNamed = ZR_TRUE;
                    ZrParser_Lexer_Next(ps->lexer); // 跳过 identifier
                    consume_token(ps, ZR_TK_COLON); // 跳过 :
                }
            }

            if (isNamed) {
                hasNamedArgs = ZR_TRUE;
                if (names == ZR_NULL) {
                    names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray),
                                                            ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    if (names != ZR_NULL) {
                        ZrCore_Array_Init(ps->state, names, sizeof(SZrString *), args->count + 1);
                        // 为之前的位置参数填充 ZR_NULL
                        for (TZrSize i = 0; i < args->count; i++) {
                            SZrString *nullName = ZR_NULL;
                            ZrCore_Array_Push(ps->state, names, &nullName);
                        }
                    }
                }
                if (names != ZR_NULL) {
                    ZrCore_Array_Push(ps->state, names, &paramName);
                }
            } else {
                if (hasNamedArgs) {
                    // 命名参数后不能再有位置参数
                    report_error(ps, "Positional arguments cannot come after named arguments");
                    break;
                }
                if (names == ZR_NULL) {
                    names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray),
                                                            ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    if (names != ZR_NULL) {
                        ZrCore_Array_Init(ps->state, names, sizeof(SZrString *), args->count + 1);
                        // 为之前的位置参数填充 ZR_NULL
                        for (TZrSize i = 0; i < args->count; i++) {
                            SZrString *nullName = ZR_NULL;
                            ZrCore_Array_Push(ps->state, names, &nullName);
                        }
                    }
                }
                if (names != ZR_NULL) {
                    SZrString *nullName = ZR_NULL;
                    ZrCore_Array_Push(ps->state, names, &nullName);
                }
            }

            SZrAstNode *arg = parse_expression(ps);
            if (arg != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, args, arg);
            } else {
                break;
            }
        }
    }

    if (argNames != ZR_NULL) {
        *argNames = names;
    }

    return args;
}

SZrAstNode *append_primary_member(SZrParserState *ps, SZrAstNode *base, SZrAstNode *memberNode,
                                         SZrFileRange startLoc) {
    if (ps == ZR_NULL || base == ZR_NULL || memberNode == ZR_NULL) {
        return base;
    }

    if (base->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (base->data.primaryExpression.members == ZR_NULL) {
            base->data.primaryExpression.members = ZrParser_AstNodeArray_New(ps->state, 1);
            if (base->data.primaryExpression.members == ZR_NULL) {
                return base;
            }
        }
        ZrParser_AstNodeArray_Add(ps->state, base->data.primaryExpression.members, memberNode);
        return base;
    }

    SZrAstNode *primaryNode = create_ast_node(ps, ZR_AST_PRIMARY_EXPRESSION, startLoc);
    if (primaryNode == ZR_NULL) {
        return base;
    }
    primaryNode->data.primaryExpression.property = base;
    primaryNode->data.primaryExpression.members = ZrParser_AstNodeArray_New(ps->state, 1);
    if (primaryNode->data.primaryExpression.members == ZR_NULL) {
        return base;
    }
    ZrParser_AstNodeArray_Add(ps->state, primaryNode->data.primaryExpression.members, memberNode);
    return primaryNode;
}

TZrBool is_lambda_expression_after_lparen(SZrParserState *ps) {
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
    TZrInt32 depth = 0;
    TZrBool isLambda = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        return ZR_FALSE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    depth = 1;
    while (depth > 0 && ps->lexer->t.token != ZR_TK_EOS) {
        /* 形如 ((params) -> { }) 或多层括号包裹：参数列表闭合后 depth>=1，下一 token 为 -> */
        if (ps->lexer->t.token == ZR_TK_RIGHT_ARROW && depth >= 1 && depth <= 32) {
            isLambda = ZR_TRUE;
            break;
        }
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            depth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            depth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (!isLambda && depth == 0 && ps->lexer->t.token == ZR_TK_RIGHT_ARROW) {
        isLambda = ZR_TRUE;
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
    return isLambda;
}

TZrBool is_expression_level_using_new(SZrParserState *ps) {
    TZrSize savedPos;
    TZrInt32 savedChar;
    TZrInt32 savedLine;
    TZrInt32 savedLastLine;
    SZrToken savedToken;
    SZrToken savedLookahead;
    TZrSize savedLookaheadPos;
    TZrInt32 savedLookaheadChar;
    TZrInt32 savedLookaheadLine;
    TZrInt32 savedLookaheadLastLine;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_USING) {
        return ZR_FALSE;
    }

    savedPos = ps->lexer->currentPos;
    savedChar = ps->lexer->currentChar;
    savedLine = ps->lexer->lineNumber;
    savedLastLine = ps->lexer->lastLine;
    savedToken = ps->lexer->t;
    savedLookahead = ps->lexer->lookahead;
    savedLookaheadPos = ps->lexer->lookaheadPos;
    savedLookaheadChar = ps->lexer->lookaheadChar;
    savedLookaheadLine = ps->lexer->lookaheadLine;
    savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        SZrString *name = ps->lexer->t.seminfo.stringValue;
        if (zr_string_equals_literal(name, "unique") || zr_string_equals_literal(name, "shared") ||
            zr_string_equals_literal(name, "share") || zr_string_equals_literal(name, "weak")) {
            ZrParser_Lexer_Next(ps->lexer);
        }
    }

    {
        TZrBool result = ps->lexer->t.token == ZR_TK_NEW;
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
        return result;
    }
}

SZrAstNodeArray *create_empty_argument_list(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_AstNodeArray_New(ps->state, 0);
}

TZrBool reject_named_construct_arguments(SZrParserState *ps, SZrArray *argNames, SZrFileRange location) {
    if (ps == ZR_NULL || argNames == ZR_NULL || argNames->length == 0) {
        return ZR_TRUE;
    }

    for (TZrSize i = 0; i < argNames->length; i++) {
        SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            ZrCore_Array_Free(ps->state, argNames);
            ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            report_error(ps, "Prototype construction does not support named arguments");
            ps->hasError = ZR_TRUE;
            ZR_UNUSED_PARAMETER(location);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(ps->state, argNames);
    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return ZR_TRUE;
}

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
    node->data.constructExpression.builtinKind = builtinKind;
    return node;
}

static SZrAstNode *parse_generic_construct_target(SZrParserState *ps) {
    SZrType *parsedType;
    SZrAstNode *typeNode;
    SZrFileRange typeLoc;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER || peek_token(ps) != ZR_TK_LESS_THAN) {
        return ZR_NULL;
    }

    parsedType = parse_type(ps);
    if (parsedType == ZR_NULL) {
        return ZR_NULL;
    }

    typeLoc = get_current_location(ps);
    typeNode = create_ast_node(ps, ZR_AST_TYPE, typeLoc);
    if (typeNode == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(ps->state->global,
                                      parsedType,
                                      sizeof(SZrType),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    typeNode->data.type = *parsedType;
    ZrCore_Memory_RawFreeWithType(ps->state->global, parsedType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return typeNode;
}

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
    if (base == ZR_NULL) {
        return ZR_NULL;
    }

    while (consume_token(ps, ZR_TK_DOT)) {
        SZrAstNode *property;
        SZrAstNode *memberNode;

        if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
            report_error(ps, "Expected identifier after '.' in prototype path");
            return base;
        }

        if (peek_token(ps) == ZR_TK_LESS_THAN) {
            property = parse_generic_construct_target(ps);
        } else {
            property = parse_identifier(ps);
        }
        if (property == ZR_NULL) {
            return base;
        }

        memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
        if (memberNode == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, property);
            return base;
        }
        memberNode->data.memberExpression.property = property;
        memberNode->data.memberExpression.computed = ZR_FALSE;
        base = append_primary_member(ps, base, memberNode, startLoc);
    }

    return base;
}

SZrAstNode *parse_prototype_reference_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNode *prototypeNode;
    SZrFileRange fullLoc;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_DOLLAR) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        consume_token(ps, ZR_TK_LPAREN);
        target = parse_expression(ps);
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER && peek_token(ps) == ZR_TK_LESS_THAN) {
        target = parse_generic_construct_target(ps);
    } else {
        target = parse_prototype_path_expression(ps);
    }

    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    prototypeNode = create_prototype_reference_node(ps, target, fullLoc);
    if (prototypeNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    return prototypeNode;
}

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
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER && peek_token(ps) == ZR_TK_LESS_THAN) {
        target = parse_generic_construct_target(ps);
    } else {
        target = parse_prototype_path_expression(ps);
    }

    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_LPAREN)) {
        args = parse_argument_list(ps, &argNames);
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
        if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
            if (args != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, args);
            }
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
    } else {
        args = create_empty_argument_list(ps);
    }

    fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    constructNode = create_construct_expression_node(ps,
                                                     target,
                                                     args,
                                                     ownershipQualifier,
                                                     isUsing,
                                                     ZR_TRUE,
                                                     builtinKind,
                                                     fullLoc);
    if (constructNode == ZR_NULL) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    return constructNode;
}

EZrOwnershipQualifier parse_optional_method_receiver_qualifier(SZrParserState *ps) {
    EZrOwnershipQualifier qualifier = ZR_OWNERSHIP_QUALIFIER_NONE;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &qualifier)) {
        report_error(ps, "Expected ownership qualifier after '%' in method declaration");
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    if (qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
        report_error(ps, "'%weak' is not a valid method receiver qualifier");
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    return qualifier;
}

SZrAstNode *parse_percent_ownership_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    EZrOwnershipBuiltinKind builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    TZrBool isUsing = ZR_FALSE;
    SZrAstNode *target = ZR_NULL;
    SZrAstNodeArray *args = ZR_NULL;
    SZrAstNode *node;
    SZrFileRange fullLoc;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_USING) {
        report_error(ps, "Ownership '%using' expressions are removed; keep '%using' as a statement or block lifetime fence only");
        return ZR_NULL;
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               zr_string_equals_literal(ps->lexer->t.seminfo.stringValue, "borrow")) {
        builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_BORROW;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               zr_string_equals_literal(ps->lexer->t.seminfo.stringValue, "loan")) {
        builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_LOAN;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier)) {
        if (ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
            ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
            report_error(ps, "'%borrowed' and '%loaned' are only valid in type and method-receiver positions");
            return ZR_NULL;
        }
        builtinKind = ownership_builtin_kind_from_flags(ownershipQualifier, ZR_FALSE);
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               zr_string_equals_literal(ps->lexer->t.seminfo.stringValue, "detach")) {
        builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_DETACH;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               zr_string_equals_literal(ps->lexer->t.seminfo.stringValue, "upgrade")) {
        builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               zr_string_equals_literal(ps->lexer->t.seminfo.stringValue, "release")) {
        builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_RELEASE;
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected ownership directive after '%'");
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_NEW) {
        if (builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE) {
            report_error(ps, "Only '%unique new ...' is supported in the ownership surface");
            return ZR_NULL;
        }
        return parse_construct_expression(ps, startLoc, ownershipQualifier, isUsing, builtinKind);
    }

    if (!consume_token(ps, ZR_TK_LPAREN)) {
        report_error(ps, "Expected '(' or 'new' after ownership directive");
        return ZR_NULL;
    }

    target = parse_expression(ps);
    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    args = create_empty_argument_list(ps);
    fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    node = create_construct_expression_node(ps,
                                            target,
                                            args,
                                            ownershipQualifier,
                                            isUsing,
                                            ZR_FALSE,
                                            builtinKind,
                                            fullLoc);
    if (node == ZR_NULL) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    return node;
}

SZrAstNode *parse_reserved_import_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *modulePath;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER || !current_identifier_equals(ps, "import")) {
        report_error(ps, "Expected 'import' after '%'");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    modulePath = parse_normalized_module_path(ps, "import");
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_IMPORT_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, modulePath);
        return ZR_NULL;
    }

    node->data.importExpression.modulePath = modulePath;
    return node;
}

static TZrBool type_literal_probe_can_start(EZrToken token) {
    return token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST || token == ZR_TK_LBRACKET ||
           token == ZR_TK_LPAREN || token == ZR_TK_PERCENT;
}

static TZrBool type_literal_probe_identifier_supports_bare_array_literal(SZrString *name) {
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_string_equals_literal(name, "int") ||
           zr_string_equals_literal(name, "uint") ||
           zr_string_equals_literal(name, "float") ||
           zr_string_equals_literal(name, "bool") ||
           zr_string_equals_literal(name, "string") ||
           zr_string_equals_literal(name, "null") ||
           zr_string_equals_literal(name, "void") ||
           zr_string_equals_literal(name, "i8") ||
           zr_string_equals_literal(name, "u8") ||
           zr_string_equals_literal(name, "i16") ||
           zr_string_equals_literal(name, "u16") ||
           zr_string_equals_literal(name, "i32") ||
           zr_string_equals_literal(name, "u32") ||
           zr_string_equals_literal(name, "i64") ||
           zr_string_equals_literal(name, "u64") ||
           zr_string_equals_literal(name, "Integer") ||
           zr_string_equals_literal(name, "Float") ||
           zr_string_equals_literal(name, "Double") ||
           zr_string_equals_literal(name, "String") ||
           zr_string_equals_literal(name, "Bool") ||
           zr_string_equals_literal(name, "Byte") ||
           zr_string_equals_literal(name, "Char") ||
           zr_string_equals_literal(name, "UInt64") ||
           zr_string_equals_literal(name, "TypeInfo") ||
           zr_string_equals_literal(name, "Object") ||
           zr_string_equals_literal(name, "Module");
}

static TZrBool type_literal_probe_identifier_is_reserved_type_query_name(SZrString *name) {
    if (type_literal_probe_identifier_supports_bare_array_literal(name)) {
        return ZR_TRUE;
    }

    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_string_equals_literal(name, "Class") ||
           zr_string_equals_literal(name, "Struct") ||
           zr_string_equals_literal(name, "Function") ||
           zr_string_equals_literal(name, "Field") ||
           zr_string_equals_literal(name, "Method") ||
           zr_string_equals_literal(name, "Property") ||
           zr_string_equals_literal(name, "Parameter");
}

static TZrBool type_literal_probe_has_unambiguous_marker(const SZrType *typeInfo) {
    if (typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((typeInfo->dimensions > 0 || typeInfo->hasArraySizeConstraint) &&
        typeInfo->name != ZR_NULL &&
        typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        typeInfo->subType == ZR_NULL &&
        !type_literal_probe_identifier_supports_bare_array_literal(typeInfo->name->data.identifier.name)) {
        return ZR_FALSE;
    }

    if (typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        typeInfo->dimensions > 0 ||
        typeInfo->hasArraySizeConstraint) {
        return ZR_TRUE;
    }

    if (typeInfo->name != ZR_NULL && typeInfo->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }

    return typeInfo->subType != ZR_NULL ? type_literal_probe_has_unambiguous_marker(typeInfo->subType) : ZR_FALSE;
}

static TZrBool type_literal_probe_is_reserved_type_query_target(const SZrType *typeInfo) {
    SZrString *identifierName;

    if (typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeInfo->name == ZR_NULL || typeInfo->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    identifierName = typeInfo->name->data.identifier.name;

    if ((typeInfo->dimensions > 0 || typeInfo->hasArraySizeConstraint) &&
        typeInfo->subType == ZR_NULL &&
        !type_literal_probe_identifier_supports_bare_array_literal(identifierName)) {
        return ZR_FALSE;
    }

    if (typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        typeInfo->dimensions > 0 ||
        typeInfo->hasArraySizeConstraint ||
        typeInfo->subType != ZR_NULL) {
        return type_literal_probe_identifier_is_reserved_type_query_name(identifierName);
    }

    return type_literal_probe_identifier_is_reserved_type_query_name(identifierName);
}

static TZrBool type_literal_probe_is_terminator(EZrToken token) {
    return token == ZR_TK_SEMICOLON || token == ZR_TK_COMMA || token == ZR_TK_RPAREN ||
           token == ZR_TK_RBRACE || token == ZR_TK_RBRACKET || token == ZR_TK_EOS;
}

static SZrAstNode *try_parse_unambiguous_type_literal_expression(SZrParserState *ps) {
    SZrParserCursor cursor;
    SZrType *typeInfo;
    SZrAstNode *node;
    SZrFileRange startLoc;

    if (ps == ZR_NULL || !type_literal_probe_can_start(ps->lexer->t.token)) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    startLoc = get_current_location(ps);
    typeInfo = parse_type(ps);
    if (typeInfo == ZR_NULL ||
        !type_literal_probe_has_unambiguous_marker(typeInfo) ||
        !type_literal_probe_is_terminator(ps->lexer->t.token)) {
        if (typeInfo != ZR_NULL) {
            free_owned_type(ps->state, typeInfo);
        }
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }

    node = create_ast_node(ps,
                           ZR_AST_TYPE_LITERAL_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        free_owned_type(ps->state, typeInfo);
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }

    node->data.typeLiteralExpression.typeInfo = typeInfo;
    return node;
}

SZrAstNode *parse_reserved_type_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *operand;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER || !current_identifier_equals(ps, "type")) {
        report_error(ps, "Expected 'type' after '%'");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    expect_token(ps, ZR_TK_LPAREN);
    if (!consume_token(ps, ZR_TK_LPAREN)) {
        return ZR_NULL;
    }

    operand = ZR_NULL;
    if (type_literal_probe_can_start(ps->lexer->t.token)) {
        operand = try_parse_unambiguous_type_literal_expression(ps);
    }

    if (operand == ZR_NULL && type_literal_probe_can_start(ps->lexer->t.token)) {
        SZrParserCursor cursor;
        SZrType *parsedType = ZR_NULL;

        save_parser_cursor(ps, &cursor);
        parsedType = parse_type(ps);
        if (parsedType != ZR_NULL &&
            ps->lexer->t.token == ZR_TK_RPAREN &&
            type_literal_probe_is_reserved_type_query_target(parsedType)) {
            operand = create_ast_node(ps,
                                      ZR_AST_TYPE_LITERAL_EXPRESSION,
                                      ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
            if (operand == ZR_NULL) {
                free_owned_type(ps->state, parsedType);
                return ZR_NULL;
            }
            operand->data.typeLiteralExpression.typeInfo = parsedType;
        } else {
            if (parsedType != ZR_NULL) {
                free_owned_type(ps->state, parsedType);
            }
            restore_parser_cursor(ps, &cursor);
        }
    }

    if (operand == ZR_NULL) {
        operand = parse_expression(ps);
        if (operand == ZR_NULL) {
            return ZR_NULL;
        }
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    node = create_ast_node(ps,
                           ZR_AST_TYPE_QUERY_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, operand);
        return ZR_NULL;
    }

    node->data.typeQueryExpression.operand = operand;
    return node;
}

SZrAstNode *parse_owned_class_declaration(SZrParserState *ps) {
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (!current_identifier_equals(ps, "owned")) {
        report_error(ps, "Expected 'owned' after '%'");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    node = parse_class_declaration(ps);
    if (node != ZR_NULL && node->type == ZR_AST_CLASS_DECLARATION) {
        node->data.classDeclaration.isOwned = ZR_TRUE;
    }
    return node;
}

// 解析成员访问和函数调用

static TZrBool is_member_name_token(EZrToken token) {
    return token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST ||
           (token >= ZR_TK_MODULE && token <= ZR_TK_NAN);
}

static SZrAstNode *parse_member_name(SZrParserState *ps) {
    SZrFileRange memberLoc;
    EZrToken token;
    SZrString *name = ZR_NULL;
    TZrNativeString nativeName = ZR_NULL;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    token = ps->lexer->t.token;
    if (!is_member_name_token(token)) {
        report_error(ps, "Expected identifier");
        return ZR_NULL;
    }

    memberLoc = get_current_token_location(ps);
    if (token == ZR_TK_IDENTIFIER) {
        name = ps->lexer->t.seminfo.stringValue;
        nativeName = name != ZR_NULL ? ZrCore_String_GetNativeString(name) : ZR_NULL;
        if (nativeName != ZR_NULL && strcmp(nativeName, "import") == 0 && peek_token(ps) == ZR_TK_LPAREN) {
            report_error(ps, "Legacy import() syntax is not supported; use %import");
            skip_legacy_import_call(ps);
            return ZR_NULL;
        }
    } else if (token == ZR_TK_TEST) {
        name = ps->lexer->t.seminfo.stringValue;
        if (name == ZR_NULL) {
            name = ZrCore_String_Create(ps->state, "test", 4);
        }
    } else {
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, token);
        if (tokenStr != ZR_NULL) {
            name = ZrCore_String_Create(ps->state, (TZrNativeString)tokenStr, strlen(tokenStr));
        }
    }

    if (name == ZR_NULL) {
        report_error(ps, "Failed to create member name");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    return create_identifier_node_with_location(ps, name, memberLoc);
}

static SZrAstNodeArray *try_parse_explicit_generic_call_arguments(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    SZrAstNodeArray *genericArguments;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_LESS_THAN) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    savedSuppressErrorOutput = ps->suppressErrorOutput;
    ps->suppressErrorOutput = ZR_TRUE;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    genericArguments = parse_generic_argument_list(ps);
    if (genericArguments == ZR_NULL || ps->lexer->t.token != ZR_TK_LPAREN) {
        if (genericArguments != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, genericArguments);
        }
        restore_parser_cursor(ps, &cursor);
        ps->suppressErrorOutput = savedSuppressErrorOutput;
        return ZR_NULL;
    }

    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->hasError = cursor.hasError;
    ps->errorMessage = cursor.errorMessage;
    return genericArguments;
}

SZrAstNode *parse_member_access(SZrParserState *ps, SZrAstNode *base) {
    SZrFileRange startLoc = base->location;

    while (ZR_TRUE) {
        // 点号成员访问
        if (consume_token(ps, ZR_TK_DOT)) {
            // 成员名上下文允许关键字以普通名称形式出现，例如 zr.ffi.out
            if (!is_member_name_token(ps->lexer->t.token)) {
                const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
                TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
                snprintf(errorMsg, sizeof(errorMsg), "Expected identifier after '.' (遇到 '%s')", tokenStr);
                report_error_with_token(ps, errorMsg, ps->lexer->t.token);
                return ZR_NULL;
            }

            SZrAstNode *property = parse_member_name(ps);
            if (property == ZR_NULL) {
                // parse_member_name 已经报告了错误
                return ZR_NULL;
            }

            SZrAstNode *memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
            if (memberNode == ZR_NULL) {
                return base;
            }
            memberNode->data.memberExpression.property = property;
            memberNode->data.memberExpression.computed = ZR_FALSE;

            base = append_primary_member(ps, base, memberNode, startLoc);
        }
        // 方括号成员访问
        else if (consume_token(ps, ZR_TK_LBRACKET)) {
            SZrAstNode *property = parse_expression(ps);
            if (property == ZR_NULL) {
                return base;
            }
            expect_token(ps, ZR_TK_RBRACKET);
            consume_token(ps, ZR_TK_RBRACKET);

            SZrAstNode *memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
            if (memberNode == ZR_NULL) {
                return base;
            }
            memberNode->data.memberExpression.property = property;
            memberNode->data.memberExpression.computed = ZR_TRUE;

            base = append_primary_member(ps, base, memberNode, startLoc);
        }
        // 显式泛型函数调用
        else if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
            SZrAstNodeArray *genericArguments = try_parse_explicit_generic_call_arguments(ps);
            SZrArray *argNames;
            SZrAstNodeArray *args;
            SZrAstNode *callNode;

            if (genericArguments == ZR_NULL) {
                break;
            }

            consume_token(ps, ZR_TK_LPAREN);
            argNames = ZR_NULL;
            args = parse_argument_list(ps, &argNames);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);

            callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, startLoc);
            if (callNode == ZR_NULL) {
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                free_ast_node_array_with_elements(ps->state, genericArguments);
                return base;
            }

            callNode->data.functionCall.args = args;
            callNode->data.functionCall.argNames = argNames;
            callNode->data.functionCall.genericArguments = genericArguments;
            callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
            if (argNames != ZR_NULL && argNames->length > 0) {
                for (TZrSize i = 0; i < argNames->length; i++) {
                    SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
                    if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
                        callNode->data.functionCall.hasNamedArgs = ZR_TRUE;
                        break;
                    }
                }
            }

            base = append_primary_member(ps, base, callNode, startLoc);
        }
        // 函数调用
        else if (consume_token(ps, ZR_TK_LPAREN)) {
            SZrArray *argNames = ZR_NULL;
            SZrAstNodeArray *args = parse_argument_list(ps, &argNames);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);

            if (base->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
                SZrAstNode *target = base->data.prototypeReferenceExpression.target;
                SZrAstNode *constructNode;
                SZrFileRange fullLoc;

                if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
                    if (args != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, args);
                    }
                    return base;
                }

                base->data.prototypeReferenceExpression.target = ZR_NULL;
                ZrCore_Memory_RawFreeWithType(ps->state->global, base, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);

                fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
                constructNode = create_construct_expression_node(ps,
                                                                 target,
                                                                 args,
                                                                 ZR_OWNERSHIP_QUALIFIER_NONE,
                                                                 ZR_FALSE,
                                                                 ZR_FALSE,
                                                                 ZR_OWNERSHIP_BUILTIN_KIND_NONE,
                                                                 fullLoc);
                if (constructNode == ZR_NULL) {
                    if (args != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, args);
                    }
                    ZrParser_Ast_Free(ps->state, target);
                    return ZR_NULL;
                }

                base = constructNode;
                continue;
            }

            SZrAstNode *callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, startLoc);
            if (callNode == ZR_NULL) {
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                return base;
            }
            callNode->data.functionCall.args = args;
            callNode->data.functionCall.argNames = argNames;
            callNode->data.functionCall.genericArguments = ZR_NULL;
            // 检查是否有命名参数
            callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
            if (argNames != ZR_NULL && argNames->length > 0) {
                // 检查是否有非空的参数名
                for (TZrSize i = 0; i < argNames->length; i++) {
                    SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
                    if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
                        callNode->data.functionCall.hasNamedArgs = ZR_TRUE;
                        break;
                    }
                }
            }

            base = append_primary_member(ps, base, callNode, startLoc);
        } else {
            break;
        }
    }

    return base;
}

// 解析主表达式

SZrAstNode *parse_primary_expression(SZrParserState *ps) {
    ZR_UNUSED_PARAMETER(get_current_location(ps));
    EZrToken token = ps->lexer->t.token;
    SZrAstNode *base = ZR_NULL;

    if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
        base = try_parse_unambiguous_type_literal_expression(ps);
        if (base != ZR_NULL) {
            return parse_member_access(ps, base);
        }
    }

    // 字面量
    if (token == ZR_TK_BOOLEAN || token == ZR_TK_INTEGER || token == ZR_TK_FLOAT || token == ZR_TK_STRING ||
        token == ZR_TK_TEMPLATE_STRING || token == ZR_TK_CHAR || token == ZR_TK_NULL || token == ZR_TK_INFINITY ||
        token == ZR_TK_NEG_INFINITY || token == ZR_TK_NAN) {
        base = parse_literal(ps);
    }
    // 标识符
    else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
        base = parse_identifier(ps);
        // 标识符解析后，需要继续解析可能的成员访问和函数调用
        // 这将在函数末尾统一处理
    }
    // super 关键字在表达式上下文中作为一个显式基类接收器使用
    else if (token == ZR_TK_SUPER) {
        SZrFileRange superLoc = get_current_token_location(ps);
        SZrString *superName = ZrCore_String_Create(ps->state, "super", 5);
        if (superName == ZR_NULL) {
            report_error(ps, "Failed to allocate 'super' identifier");
            return ZR_NULL;
        }
        base = create_identifier_node_with_location(ps, superName, superLoc);
        ZrParser_Lexer_Next(ps->lexer);
    }
    // 数组字面量
    else if (token == ZR_TK_LBRACKET) {
        base = parse_array_literal(ps);
    }
    // 生成器表达式（{{}}）
    else if (token == ZR_TK_LBRACE) {
        // 检查是否是生成器表达式 {{ 还是普通对象字面量 {
        EZrToken lookahead = peek_token(ps);
        if (lookahead == ZR_TK_LBRACE) {
            // 是生成器表达式 {{}}
            base = parse_generator_expression(ps);
        } else {
            // 检查是否是对象字面量（有键值对）还是块表达式
            // 更精确的判断逻辑：
            // 1. 如果下一个token是标识符、字符串或数字，可能是对象字面量的键
            // 2. 如果下一个token是语句关键字（var, if, while等），是块表达式
            // 3. 如果下一个token是右大括号，是空对象字面量
            EZrToken objectLookahead = peek_token(ps);
            if (objectLookahead == ZR_TK_IDENTIFIER || objectLookahead == ZR_TK_STRING ||
                objectLookahead == ZR_TK_INTEGER ||
                objectLookahead == ZR_TK_FLOAT || objectLookahead == ZR_TK_RBRACE) {
                // 可能是对象字面量
                base = parse_object_literal(ps);
            } else if (objectLookahead == ZR_TK_VAR || objectLookahead == ZR_TK_IF ||
                       objectLookahead == ZR_TK_WHILE ||
                       objectLookahead == ZR_TK_FOR || objectLookahead == ZR_TK_RETURN ||
                       objectLookahead == ZR_TK_BREAK ||
                       objectLookahead == ZR_TK_CONTINUE || objectLookahead == ZR_TK_THROW ||
                       objectLookahead == ZR_TK_TRY ||
                       objectLookahead == ZR_TK_SWITCH) {
                // 是块表达式
                base = parse_block(ps);
            } else {
                // 默认尝试解析为对象字面量
                base = parse_object_literal(ps);
            }
        }
    }
    // Lambda 表达式或括号表达式
    else if (token == ZR_TK_LPAREN) {
        if (is_lambda_expression_after_lparen(ps)) {
            SZrFileRange lambdaLoc;
            SZrAstNodeArray *params = ZR_NULL;
            SZrParameter *args = ZR_NULL;

            consume_token(ps, ZR_TK_LPAREN);
            lambdaLoc = get_current_location(ps);

            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                ZrParser_Lexer_Next(ps->lexer);
                {
                    SZrAstNode *argsNode = parse_parameter(ps);
                    if (argsNode != ZR_NULL) {
                        args = &argsNode->data.parameter;
                    }
                }
                params = ZrParser_AstNodeArray_New(ps->state, 0);
            } else {
                params = parse_parameter_list(ps);
                if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
                    ZrParser_Lexer_Next(ps->lexer);
                    {
                        SZrAstNode *argsNode = parse_parameter(ps);
                        if (argsNode != ZR_NULL) {
                            args = &argsNode->data.parameter;
                        }
                    }
                }
            }

            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);
            expect_token(ps, ZR_TK_RIGHT_ARROW);
            ZrParser_Lexer_Next(ps->lexer);

            {
                SZrAstNode *block = parse_block(ps);
                if (block != ZR_NULL) {
                    SZrFileRange endLoc = get_current_location(ps);
                    SZrFileRange fullLoc = ZrParser_FileRange_Merge(lambdaLoc, endLoc);
                    SZrAstNode *lambdaNode = create_ast_node(ps, ZR_AST_LAMBDA_EXPRESSION, fullLoc);
                    if (lambdaNode != ZR_NULL) {
                        lambdaNode->data.lambdaExpression.params = params;
                        lambdaNode->data.lambdaExpression.args = args;
                        lambdaNode->data.lambdaExpression.block = block;
                        lambdaNode->data.lambdaExpression.isAsync = ZR_FALSE;
                        return parse_member_access(ps, lambdaNode);
                    }
                }
            }
        } else {
            consume_token(ps, ZR_TK_LPAREN);
            base = parse_expression(ps);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);
        }
    } else {
        report_error(ps, "Expected primary expression");
        return ZR_NULL;
    }

    if (base == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析成员访问和函数调用
    // 注意：此时 lexer 应该指向标识符后的下一个 token（可能是 .、[、( 或其他）
    return parse_member_access(ps, base);
}

// 解析一元表达式
