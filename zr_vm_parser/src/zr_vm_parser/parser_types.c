#include "parser_internal.h"

static SZrAstNodeArray *parse_type_list(SZrParserState *ps) {
    SZrAstNodeArray *types = ZrParser_AstNodeArray_New(ps->state, 4);
    if (types == ZR_NULL) {
        return ZR_NULL;
    }

    {
        SZrType *first = parse_type(ps);
        if (first != ZR_NULL) {
            SZrAstNode *firstNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
            if (firstNode != ZR_NULL) {
                firstNode->data.type = *first;
                ZrCore_Memory_RawFreeWithType(ps->state->global, first, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Add(ps->state, types, firstNode);
            }
        }
    }

    while (consume_token(ps, ZR_TK_COMMA)) {
        SZrType *type = parse_type(ps);
        if (type != ZR_NULL) {
            SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
            if (typeNode != ZR_NULL) {
                typeNode->data.type = *type;
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Add(ps->state, types, typeNode);
            }
        } else {
            break;
        }
    }

    return types;
}

SZrAstNode *parse_generic_type(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_LESS_THAN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *params = parse_type_list(ps);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    if (!consume_type_closing_angle(ps)) {
        expect_token(ps, ZR_TK_GREATER_THAN);
        ZrParser_Lexer_Next(ps->lexer);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange genericLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERIC_TYPE, genericLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, params);
        return ZR_NULL;
    }

    node->data.genericType.name = &nameNode->data.identifier;
    node->data.genericType.params = params;
    return node;
}

// 解析元组类型

SZrAstNode *parse_tuple_type(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *elements = parse_type_list(ps);
    if (elements == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange tupleLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TUPLE_TYPE, tupleLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, elements);
        return ZR_NULL;
    }

    node->data.tupleType.elements = elements;
    return node;
}

// 解析类型

SZrType *parse_type(SZrParserState *ps) {
    SZrType *type = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    type->dimensions = 0;
    type->name = ZR_NULL;
    type->subType = ZR_NULL;
    type->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;

    // 初始化数组大小约束
    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->hasArraySizeConstraint = ZR_FALSE;
    type->arraySizeExpression = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        SZrType *innerType;

        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token != ZR_TK_IDENTIFIER ||
            !try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier)) {
            report_error(ps, "Expected ownership qualifier after '%' in type annotation");
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
        ZrParser_Lexer_Next(ps->lexer);

        innerType = parse_type(ps);
        if (innerType == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }

        *type = *innerType;
        type->ownershipQualifier = ownershipQualifier;
        ZrCore_Memory_RawFreeWithType(ps->state->global, innerType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return type;
    }

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier) &&
        peek_token(ps) == ZR_TK_LESS_THAN) {
        report_error(ps, "Legacy ownership type syntax is removed; use '%unique Type', '%shared Type', '%weak Type' or '%borrowed Type'");
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析类型名称（可能是标识符、泛型类型或元组类型）
    if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 元组类型
        SZrAstNode *tupleNode = parse_tuple_type(ps);
        if (tupleNode != ZR_NULL) {
            type->name = tupleNode;
        } else {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        // 检查是否是泛型类型
        EZrToken lookahead = peek_token(ps);
        if (lookahead == ZR_TK_LESS_THAN) {
            SZrAstNode *genericNode = parse_generic_type(ps);
            if (genericNode != ZR_NULL) {
                type->name = genericNode;
            } else {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
        } else {
            // 普通标识符类型
            SZrAstNode *idNode = parse_identifier(ps);
            if (idNode != ZR_NULL) {
                type->name = idNode;
            } else {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
        }
    } else {
        report_error(ps, "Expected type name");
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析子类型（可选，如 A.B）
    if (consume_token(ps, ZR_TK_DOT)) {
        type->subType = parse_type(ps);
    }

    // 解析数组维度和大小约束
    while (consume_token(ps, ZR_TK_LBRACKET)) {
        if (consume_token(ps, ZR_TK_RBRACKET)) {
            // 普通数组维度（无大小约束）
            type->dimensions++;
        } else {
            if (!parse_array_size_constraint(ps, type)) {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            if (!consume_token(ps, ZR_TK_RBRACKET)) {
                report_error(ps, "Expected ] after array size constraint");
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            type->dimensions++;
        }
    }

    return type;
}

// 解析类型（不解析泛型类型，用于 intermediate 声明的返回类型）

SZrType *parse_type_no_generic(SZrParserState *ps) {
    SZrType *type = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    type->dimensions = 0;
    type->name = ZR_NULL;
    type->subType = ZR_NULL;
    type->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;

    // 初始化数组大小约束
    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->hasArraySizeConstraint = ZR_FALSE;
    type->arraySizeExpression = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        SZrType *innerType;

        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token != ZR_TK_IDENTIFIER ||
            !try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier)) {
            report_error(ps, "Expected ownership qualifier after '%' in type annotation");
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
        ZrParser_Lexer_Next(ps->lexer);

        innerType = parse_type_no_generic(ps);
        if (innerType == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }

        *type = *innerType;
        type->ownershipQualifier = ownershipQualifier;
        ZrCore_Memory_RawFreeWithType(ps->state->global, innerType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return type;
    }

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier) &&
        peek_token(ps) == ZR_TK_LESS_THAN) {
        report_error(ps, "Legacy ownership type syntax is removed; use '%unique Type', '%shared Type', '%weak Type' or '%borrowed Type'");
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析类型名称（可能是标识符或元组类型，但不解析泛型类型）
    if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 元组类型
        SZrAstNode *tupleNode = parse_tuple_type(ps);
        if (tupleNode != ZR_NULL) {
            type->name = tupleNode;
        } else {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        // 普通标识符类型（不解析泛型类型，即使后面有 <）
        SZrAstNode *idNode = parse_identifier(ps);
        if (idNode != ZR_NULL) {
            type->name = idNode;
        } else {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else {
        report_error(ps, "Expected type name");
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析子类型（可选，如 A.B）
    if (consume_token(ps, ZR_TK_DOT)) {
        type->subType = parse_type_no_generic(ps);
    }

    // 解析数组维度和大小约束
    while (consume_token(ps, ZR_TK_LBRACKET)) {
        if (consume_token(ps, ZR_TK_RBRACKET)) {
            // 普通数组维度（无大小约束）
            type->dimensions++;
        } else {
            if (!parse_array_size_constraint(ps, type)) {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            if (!consume_token(ps, ZR_TK_RBRACKET)) {
                report_error(ps, "Expected ] after array size constraint");
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            type->dimensions++;
        }
    }

    return type;
}

// 解析数组大小约束
// 支持语法：
//   [N]      - 固定大小
//   [M..N]   - 范围约束
//   [N..]    - 最小大小

TZrBool parse_array_size_constraint(SZrParserState *ps, SZrType *type) {
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

    if (ps == ZR_NULL || type == ZR_NULL) {
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

    if (ps->lexer->t.token == ZR_TK_INTEGER) {
        TZrInt64 firstValue = ps->lexer->t.seminfo.intValue;
        if (firstValue < 0) {
            report_error(ps, "Array size must be non-negative");
            return ZR_FALSE;
        }

        ZrParser_Lexer_Next(ps->lexer);

        if (ps->lexer->t.token == ZR_TK_RBRACKET) {
            type->arrayFixedSize = (TZrSize) firstValue;
            type->arrayMinSize = (TZrSize) firstValue;
            type->arrayMaxSize = (TZrSize) firstValue;
            type->hasArraySizeConstraint = ZR_TRUE;
            type->arraySizeExpression = ZR_NULL;
            return ZR_TRUE;
        }

        if (consume_token(ps, ZR_TK_DOT_DOT)) {
            type->arrayMinSize = (TZrSize) firstValue;
            type->arrayMaxSize = 0;
            type->hasArraySizeConstraint = ZR_TRUE;
            type->arraySizeExpression = ZR_NULL;

            if (ps->lexer->t.token == ZR_TK_INTEGER) {
                TZrInt64 secondValue = ps->lexer->t.seminfo.intValue;
                if (secondValue < 0 || secondValue < firstValue) {
                    report_error(ps, "Invalid array size range: max must be >= min");
                    return ZR_FALSE;
                }
                type->arrayMaxSize = (TZrSize) secondValue;
                ZrParser_Lexer_Next(ps->lexer);
            }
            return ZR_TRUE;
        }
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

    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->arraySizeExpression = parse_expression(ps);
    if (type->arraySizeExpression == ZR_NULL) {
        report_error(ps, "Expected array size constraint expression");
        return ZR_FALSE;
    }
    type->hasArraySizeConstraint = ZR_TRUE;
    return ZR_TRUE;
}

// 解析泛型声明

SZrGenericDeclaration *parse_generic_declaration(SZrParserState *ps) {
    expect_token(ps, ZR_TK_LESS_THAN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *params = ZrParser_AstNodeArray_New(ps->state, 4);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    // 至少需要一个参数
    SZrAstNode *first = parse_parameter(ps);
    if (first != ZR_NULL) {
        ZrParser_AstNodeArray_Add(ps->state, params, first);
    }

    while (consume_token(ps, ZR_TK_COMMA)) {
        if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
            break;
        }
        SZrAstNode *param = parse_parameter(ps);
        if (param != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, params, param);
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_GREATER_THAN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrGenericDeclaration *generic = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrGenericDeclaration),
                                                                     ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (generic == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, params);
        return ZR_NULL;
    }

    generic->params = params;
    return generic;
}

// 解析元标识符

SZrAstNode *parse_meta_identifier(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_AT);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_META_IDENTIFIER, metaLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.metaIdentifier.name = &nameNode->data.identifier;
    return node;
}

// 解析装饰器表达式

SZrAstNode *parse_decorator_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_SHARP);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SHARP);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange decoratorLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DECORATOR_EXPRESSION, decoratorLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.decoratorExpression.expr = expr;
    return node;
}

// 解析解构对象模式

SZrAstNode *parse_destructuring_object(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *keys = ZrParser_AstNodeArray_New(ps->state, 4);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        SZrAstNode *first = parse_identifier(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, keys, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RBRACE) {
                break;
            }
            SZrAstNode *key = parse_identifier(ps);
            if (key != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, keys, key);
            } else {
                break;
            }
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange destructuringLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DESTRUCTURING_OBJECT, destructuringLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringObject.keys = keys;
    return node;
}

// 解析解构数组模式

SZrAstNode *parse_destructuring_array(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *keys = ZrParser_AstNodeArray_New(ps->state, 4);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        SZrAstNode *first = parse_identifier(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, keys, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }
            SZrAstNode *key = parse_identifier(ps);
            if (key != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, keys, key);
            } else {
                break;
            }
        }
    }

    expect_token(ps, ZR_TK_RBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange destructuringLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DESTRUCTURING_ARRAY, destructuringLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringArray.keys = keys;
    return node;
}

// 解析访问修饰符

EZrAccessModifier parse_access_modifier(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;
    if (token == ZR_TK_PUB) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_ACCESS_PUBLIC;
    } else if (token == ZR_TK_PRI) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_ACCESS_PRIVATE;
    } else if (token == ZR_TK_PRO) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_ACCESS_PROTECTED;
    }
    return ZR_ACCESS_PRIVATE; // 默认 private
}

// 解析参数

SZrAstNode *parse_parameter(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);

    // 检查是否是可变参数 (...name: type)
    TZrBool isVariadic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        isVariadic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析 const 关键字（可选）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    SZrIdentifier *name = &nameNode->data.identifier;

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
        // 如果类型解析失败，仍然创建参数节点（typeInfo 为 NULL）
        // 这样可以进行错误恢复
    }

    // 可选默认值（可变参数不能有默认值）
    SZrAstNode *defaultValue = ZR_NULL;
    if (!isVariadic && consume_token(ps, ZR_TK_EQUALS)) {
        defaultValue = parse_expression(ps);
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_PARAMETER, startLoc);
    if (node == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.parameter.name = name;
    node->data.parameter.typeInfo = typeInfo;
    node->data.parameter.defaultValue = defaultValue;
    node->data.parameter.isConst = isConst;
    node->data.parameter.decorators = decorators;
    return node;
}

// 解析参数列表

SZrAstNodeArray *parse_parameter_list(SZrParserState *ps) {
    SZrAstNodeArray *params = ZrParser_AstNodeArray_New(ps->state, 4);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_PARAMS) {
        SZrAstNode *first = parse_parameter(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, params, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
            SZrAstNode *param = parse_parameter(ps);
            if (param != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, params, param);
            } else {
                break;
            }
        }
    }

    return params;
}

// ==================== 声明解析 ====================

// 解析模块声明
