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
        case ZR_TK_SUPER:
        case ZR_TK_LESS_THAN:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
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
        SZrAstNode *node = ZR_NULL;
        if (current_percent_directive_equals(ps, "import")) {
            node = parse_reserved_import_expression(ps);
        } else if (current_percent_directive_equals(ps, "await")) {
            node = parse_reserved_await_expression(ps);
        } else if (current_percent_directive_equals(ps, "type")) {
            node = parse_reserved_type_expression(ps);
        } else if (current_percent_directive_equals(ps, "func")) {
            node = parse_type_literal_expression(ps);
        } else {
            node = parse_percent_ownership_expression(ps);
        }
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
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
        SZrAstNode *node = parse_prototype_reference_expression(ps);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
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
            report_missing_conditional_colon(ps, questionLoc);
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
