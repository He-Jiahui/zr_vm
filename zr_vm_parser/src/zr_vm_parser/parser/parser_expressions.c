#include "parser_internal.h"

static SZrAstNode *parse_type_literal_expression(SZrParserState *ps) {
    SZrType *typeInfo;
    SZrAstNode *node;
    SZrFileRange startLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    typeInfo = parse_type(ps);
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps,
                           ZR_AST_TYPE_LITERAL_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        free_owned_type(ps->state, typeInfo);
        return ZR_NULL;
    }

    node->data.typeLiteralExpression.typeInfo = typeInfo;
    return node;
}

static SZrFileRange parser_expression_node_range(SZrAstNode *left, SZrAstNode *right) {
    if (left == ZR_NULL) {
        return right != ZR_NULL ? right->location : (SZrFileRange){{0, 1, 1}, {0, 1, 1}, ZR_NULL};
    }
    if (right == ZR_NULL) {
        return left->location;
    }
    return ZrParser_FileRange_Merge(left->location, right->location);
}

static TZrBool parser_token_can_start_expression(EZrToken token) {
    switch (token) {
        case ZR_TK_IDENTIFIER:
        case ZR_TK_TEST:
        case ZR_TK_BOOLEAN:
        case ZR_TK_INTEGER:
        case ZR_TK_FLOAT:
        case ZR_TK_STRING:
        case ZR_TK_TEMPLATE_STRING:
        case ZR_TK_CHAR:
        case ZR_TK_NULL:
        case ZR_TK_INFINITY:
        case ZR_TK_NEG_INFINITY:
        case ZR_TK_NAN:
        case ZR_TK_LPAREN:
        case ZR_TK_LBRACKET:
        case ZR_TK_LBRACE:
        case ZR_TK_BANG:
        case ZR_TK_TILDE:
        case ZR_TK_PLUS:
        case ZR_TK_MINUS:
        case ZR_TK_DOLLAR:
        case ZR_TK_NEW:
        case ZR_TK_USING:
        case ZR_TK_PERCENT:
        case ZR_TK_REF:
        case ZR_TK_SUPER:
        case ZR_TK_LESS_THAN:
        case ZR_TK_TYPEID:
        case ZR_TK_TYPEOF:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static SZrAstNode *parse_reflection_type_query_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    EZrTypeQueryKind kind;
    SZrAstNode *operand = ZR_NULL;
    SZrType *typeOperand = ZR_NULL;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL ||
        (ps->lexer->t.token != ZR_TK_TYPEID && ps->lexer->t.token != ZR_TK_TYPEOF)) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    kind = ps->lexer->t.token == ZR_TK_TYPEID
                   ? ZR_TYPE_QUERY_CANONICAL_IDENTITY
                   : ZR_TYPE_QUERY_RUNTIME_DESCRIPTOR;
    ZrParser_Lexer_Next(ps->lexer);
    expect_token(ps, ZR_TK_LPAREN);
    if (!consume_token(ps, ZR_TK_LPAREN)) {
        return ZR_NULL;
    }

    if (kind == ZR_TYPE_QUERY_CANONICAL_IDENTITY) {
        typeOperand = parse_type(ps);
        if (typeOperand == ZR_NULL) {
            report_error(ps, "Expected a type reference in typeid(...)");
            return ZR_NULL;
        }
    } else {
        operand = parse_expression(ps);
        if (operand == ZR_NULL) {
            report_error(ps, "Expected an expression in typeof(...)");
            return ZR_NULL;
        }
    }

    expect_token(ps, ZR_TK_RPAREN);
    if (!consume_token(ps, ZR_TK_RPAREN)) {
        free_owned_type(ps->state, typeOperand);
        if (operand != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, operand);
        }
        return ZR_NULL;
    }

    node = create_ast_node(ps,
                           ZR_AST_TYPE_QUERY_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        free_owned_type(ps->state, typeOperand);
        if (operand != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, operand);
        }
        return ZR_NULL;
    }

    node->data.typeQueryExpression.kind = kind;
    node->data.typeQueryExpression.operand = operand;
    node->data.typeQueryExpression.typeOperand = typeOperand;
    return node;
}

static SZrAstNode *parse_required_right_operand(SZrParserState *ps,
                                                const TZrChar *operatorText,
                                                SZrFileRange operatorLocation,
                                                SZrAstNode *(*parseOperand)(SZrParserState *)) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || parseOperand == ZR_NULL) {
        return ZR_NULL;
    }

    if (!parser_token_can_start_expression(ps->lexer->t.token)) {
        report_missing_right_operand(ps, operatorText, operatorLocation);
        return ZR_NULL;
    }

    return parseOperand(ps);
}

SZrAstNode *parse_unary_expression(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    if (token == ZR_TK_TYPEID || token == ZR_TK_TYPEOF) {
        SZrAstNode *query = parse_reflection_type_query_expression(ps);
        return query != ZR_NULL ? parse_member_access(ps, query) : ZR_NULL;
    }

    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "await")) {
        return parse_await_expression(ps);
    }

    if (token == ZR_TK_REF) {
        return parse_reference_expression(ps);
    }

    if (token == ZR_TK_IDENTIFIER &&
        current_identifier_equals(ps, "init") &&
        peek_token(ps) != ZR_TK_LPAREN &&
        peek_token(ps) != ZR_TK_LESS_THAN) {
        SZrAstNode *node = parse_struct_init_expression(ps);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
    }

    // 检查类型转换表达式: <Type> expression
    if (token == ZR_TK_LESS_THAN) {
        SZrParserCursor cursor;
        TZrBool savedSuppressErrorOutput;
        TZrParserErrorCallback savedErrorCallback;
        TZrParserStructuredErrorCallback savedStructuredErrorCallback;
        TZrPtr savedErrorUserData;

        // 可能是类型转换，需要向前看以区分泛型类型和类型转换
        // 类型转换: <Type> expression (后面跟着表达式)
        // 泛型类型: Type<...> (在类型解析上下文中)
        // 这里我们尝试解析类型，如果成功且后面跟着表达式，就是类型转换
        SZrFileRange startLoc = get_current_location(ps);
        save_parser_cursor(ps, &cursor);
        savedSuppressErrorOutput = ps->suppressErrorOutput;
        savedErrorCallback = ps->errorCallback;
        savedStructuredErrorCallback = ps->structuredErrorCallback;
        savedErrorUserData = ps->errorUserData;
        ps->suppressErrorOutput = ZR_TRUE;
        ps->errorCallback = ZR_NULL;
        ps->structuredErrorCallback = ZR_NULL;
        ps->errorUserData = ZR_NULL;
        ps->hasError = ZR_FALSE;
        ps->errorMessage = ZR_NULL;

        // 尝试解析类型
        ZrParser_Lexer_Next(ps->lexer); // 跳过 <
        SZrType *targetType = parse_type(ps);

        if (targetType != ZR_NULL && ps->lexer->t.token == ZR_TK_GREATER_THAN) {
            ZrParser_Lexer_Next(ps->lexer); // 跳过 >

            // 检查后面是否是表达式（不是类型声明上下文）
            // 如果后面是标识符、字面量、一元操作符等，就是类型转换
            EZrToken nextToken = ps->lexer->t.token;
            if (nextToken == ZR_TK_IDENTIFIER || nextToken == ZR_TK_INTEGER || nextToken == ZR_TK_FLOAT ||
                nextToken == ZR_TK_STRING || nextToken == ZR_TK_CHAR || nextToken == ZR_TK_BOOLEAN ||
                nextToken == ZR_TK_NULL || nextToken == ZR_TK_LPAREN || nextToken == ZR_TK_LBRACKET ||
                nextToken == ZR_TK_LBRACE || nextToken == ZR_TK_BANG || nextToken == ZR_TK_TILDE ||
                nextToken == ZR_TK_PLUS || nextToken == ZR_TK_MINUS || nextToken == ZR_TK_DOLLAR ||
                nextToken == ZR_TK_NEW || nextToken == ZR_TK_USING || nextToken == ZR_TK_LESS_THAN) {
                // 是类型转换表达式
                SZrAstNode *expression = parse_unary_expression(ps); // 递归解析表达式

                if (expression != ZR_NULL) {
                    SZrFileRange endLoc = get_current_location(ps);
                    SZrFileRange castLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
                    SZrAstNode *node = create_ast_node(ps, ZR_AST_TYPE_CAST_EXPRESSION, castLoc);
                    if (node != ZR_NULL) {
                        ps->suppressErrorOutput = savedSuppressErrorOutput;
                        ps->errorCallback = savedErrorCallback;
                        ps->structuredErrorCallback = savedStructuredErrorCallback;
                        ps->errorUserData = savedErrorUserData;
                        ps->hasError = cursor.hasError;
                        ps->errorMessage = cursor.errorMessage;
                        node->data.typeCastExpression.targetType = targetType;
                        node->data.typeCastExpression.expression = expression;
                        return node;
                    }
                }

                // 如果创建节点失败，释放类型
                if (targetType != ZR_NULL) {
                    free_owned_type(ps->state, targetType);
                }
                ps->suppressErrorOutput = savedSuppressErrorOutput;
                ps->errorCallback = savedErrorCallback;
                ps->structuredErrorCallback = savedStructuredErrorCallback;
                ps->errorUserData = savedErrorUserData;
            } else {
                // 不是类型转换，恢复状态
                restore_parser_cursor(ps, &cursor);
                ps->suppressErrorOutput = savedSuppressErrorOutput;
                ps->errorCallback = savedErrorCallback;
                ps->structuredErrorCallback = savedStructuredErrorCallback;
                ps->errorUserData = savedErrorUserData;
                if (targetType != ZR_NULL) {
                    free_owned_type(ps->state, targetType);
                }
            }
        } else {
            // 解析类型失败，恢复状态
            restore_parser_cursor(ps, &cursor);
            ps->suppressErrorOutput = savedSuppressErrorOutput;
            ps->errorCallback = savedErrorCallback;
            ps->structuredErrorCallback = savedStructuredErrorCallback;
            ps->errorUserData = savedErrorUserData;
            if (targetType != ZR_NULL) {
                free_owned_type(ps->state, targetType);
            }
        }
    }

    if (token == ZR_TK_PERCENT) {
        if (report_removed_percent_syntax(ps)) {
            return ZR_NULL;
        }
        report_error(ps, "Unexpected '%' at the start of an expression");
        return ZR_NULL;
    }

    if (token == ZR_TK_NEW) {
        SZrAstNode *node = parse_construct_expression(ps,
                                                      get_current_location(ps),
                                                      ZR_OWNERSHIP_QUALIFIER_NONE,
                                                      ZR_FALSE,
                                                      ZR_OWNERSHIP_BUILTIN_KIND_NONE);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
    }

    if (token == ZR_TK_DOLLAR) {
        report_removed_legacy_syntax(
                ps,
                "$ construct",
                "Use `init TypeRef(...)` for struct values, `own TypeRef(...)` for resources, or an ordinary call for callable values.");
        return ZR_NULL;
    }

    // 检查一元操作符
    if (token == ZR_TK_BANG || token == ZR_TK_TILDE || token == ZR_TK_PLUS || token == ZR_TK_MINUS) {
        SZrFileRange startLoc = get_current_token_location(ps);
        SZrUnaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *argument = parse_unary_expression(ps); // 右结合

        SZrFileRange unaryLoc = argument != ZR_NULL
                                     ? ZrParser_FileRange_Merge(startLoc, argument->location)
                                     : startLoc;
        SZrAstNode *node = create_ast_node(ps, ZR_AST_UNARY_EXPRESSION, unaryLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.unaryExpression.op = op;
        node->data.unaryExpression.argument = argument;
        return node;
    }

    return parse_primary_expression(ps);
}

// 解析乘法表达式

SZrAstNode *parse_multiplicative_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_unary_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_STAR || ps->lexer->t.token == ZR_TK_SLASH ||
           ps->lexer->t.token == ZR_TK_PERCENT) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_unary_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析加法表达式

SZrAstNode *parse_additive_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_multiplicative_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_PLUS || ps->lexer->t.token == ZR_TK_MINUS) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_multiplicative_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析位移表达式

SZrAstNode *parse_shift_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_additive_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_LEFT_SHIFT || ps->lexer->t.token == ZR_TK_RIGHT_SHIFT) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_additive_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析关系表达式

SZrAstNode *parse_relational_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_shift_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_LESS_THAN || ps->lexer->t.token == ZR_TK_GREATER_THAN ||
           ps->lexer->t.token == ZR_TK_LESS_THAN_EQUALS || ps->lexer->t.token == ZR_TK_GREATER_THAN_EQUALS) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_shift_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析相等表达式

SZrAstNode *parse_equality_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_relational_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_DOUBLE_EQUALS || ps->lexer->t.token == ZR_TK_BANG_EQUALS) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_relational_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析按位与表达式

SZrAstNode *parse_binary_and_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_equality_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_AND) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_equality_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析按位异或表达式

SZrAstNode *parse_binary_xor_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_binary_and_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_XOR) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_binary_and_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析按位或表达式

SZrAstNode *parse_binary_or_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_binary_xor_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_OR) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_binary_xor_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析逻辑与表达式

SZrAstNode *parse_logical_and_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_binary_or_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_AMPERSAND_AMPERSAND) {
        SZrFileRange operatorLoc = get_current_token_location(ps);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, "&&", operatorLoc, parse_binary_or_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_LOGICAL_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.logicalExpression.left = left;
        node->data.logicalExpression.right = right;
        node->data.logicalExpression.op = "&&";
        left = node;
    }

    return left;
}

// 解析逻辑或表达式

SZrAstNode *parse_logical_or_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_logical_and_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_PIPE_PIPE) {
        SZrFileRange operatorLoc = get_current_token_location(ps);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, "||", operatorLoc, parse_logical_and_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_LOGICAL_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.logicalExpression.left = left;
        node->data.logicalExpression.right = right;
        node->data.logicalExpression.op = "||";
        left = node;
    }

    return left;
}

// 解析条件表达式（三元运算符）

SZrAstNode *parse_conditional_expression(SZrParserState *ps) {
    SZrAstNode *test = parse_logical_or_expression(ps);
    if (test == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_QUESTIONMARK) {
        SZrFileRange questionLoc = get_current_token_location(ps);
        SZrFileRange colonLoc;
        SZrAstNode *consequent;
        SZrAstNode *alternate;

        ZrParser_Lexer_Next(ps->lexer);
        if (!parser_token_can_start_expression(ps->lexer->t.token) || ps->lexer->t.token == ZR_TK_COLON) {
            report_missing_conditional_consequent(ps, questionLoc);
            ZrParser_Ast_Free(ps->state, test);
            return ZR_NULL;
        }

        consequent = parse_expression(ps);
        if (consequent == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, test);
            return ZR_NULL;
        }

        if (ps->lexer->t.token != ZR_TK_COLON) {
            report_missing_conditional_colon(
                    ps,
                    questionLoc,
                    parser_token_can_start_expression(ps->lexer->t.token));
            ZrParser_Ast_Free(ps->state, consequent);
            ZrParser_Ast_Free(ps->state, test);
            return ZR_NULL;
        }

        colonLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_COLON);
        if (!parser_token_can_start_expression(ps->lexer->t.token)) {
            report_missing_conditional_alternate(ps, colonLoc);
            ZrParser_Ast_Free(ps->state, consequent);
            ZrParser_Ast_Free(ps->state, test);
            return ZR_NULL;
        }

        alternate = parse_conditional_expression(ps); // 右结合
        if (alternate == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, consequent);
            ZrParser_Ast_Free(ps->state, test);
            return ZR_NULL;
        }

        SZrAstNode *node = create_ast_node(ps,
                                           ZR_AST_CONDITIONAL_EXPRESSION,
                                           parser_expression_node_range(test, alternate));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.conditionalExpression.test = test;
        node->data.conditionalExpression.consequent = consequent;
        node->data.conditionalExpression.alternate = alternate;
        return node;
    }

    return test;
}

// 解析赋值表达式

SZrAstNode *parse_assignment_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_conditional_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    // 检查赋值操作符
    EZrToken token = ps->lexer->t.token;
    if (token == ZR_TK_EQUALS || token == ZR_TK_PLUS_EQUALS || token == ZR_TK_MINUS_EQUALS ||
        token == ZR_TK_STAR_EQUALS || token == ZR_TK_SLASH_EQUALS || token == ZR_TK_PERCENT_EQUALS) {
        SZrFileRange operatorLoc = get_current_token_location(ps);
        SZrAssignmentOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_required_right_operand(ps, op.op, operatorLoc, parse_assignment_expression);
        if (right == ZR_NULL) {
            return ZR_NULL;
        }

        SZrAstNode *node =
            create_ast_node(ps, ZR_AST_ASSIGNMENT_EXPRESSION, parser_expression_node_range(left, right));
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.assignmentExpression.left = left;
        node->data.assignmentExpression.right = right;
        node->data.assignmentExpression.op = op;
        return node;
    }

    return left;
}

// 解析表达式（入口函数）

SZrAstNode *parse_expression(SZrParserState *ps) { return parse_assignment_expression(ps); }
