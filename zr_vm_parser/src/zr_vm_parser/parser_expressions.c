#include "parser_internal.h"

SZrAstNode *parse_unary_expression(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    // 检查类型转换表达式: <Type> expression
    if (token == ZR_TK_LESS_THAN) {
        // 可能是类型转换，需要向前看以区分泛型类型和类型转换
        // 类型转换: <Type> expression (后面跟着表达式)
        // 泛型类型: Type<...> (在类型解析上下文中)
        // 这里我们尝试解析类型，如果成功且后面跟着表达式，就是类型转换
        SZrFileRange startLoc = get_current_location(ps);

        // 保存状态以便回退
        TZrSize savedPos = ps->lexer->currentPos;
        TZrInt32 savedChar = ps->lexer->currentChar;
        TZrInt32 savedLine = ps->lexer->lineNumber;
        TZrInt32 savedLastLine = ps->lexer->lastLine;
        SZrToken savedToken = ps->lexer->t;

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
                        node->data.typeCastExpression.targetType = targetType;
                        node->data.typeCastExpression.expression = expression;
                        return node;
                    }
                }

                // 如果创建节点失败，释放类型
                if (targetType != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(ps->state->global, targetType, sizeof(SZrType),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
            } else {
                // 不是类型转换，恢复状态
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                if (targetType != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(ps->state->global, targetType, sizeof(SZrType),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
            }
        } else {
            // 解析类型失败，恢复状态
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            if (targetType != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(ps->state->global, targetType, sizeof(SZrType),
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
    }

    if (token == ZR_TK_PERCENT) {
        SZrAstNode *node = ZR_NULL;
        if (current_percent_directive_equals(ps, "import")) {
            node = parse_reserved_import_expression(ps);
        } else if (current_percent_directive_equals(ps, "type")) {
            node = parse_reserved_type_expression(ps);
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
                                                      ZR_FALSE);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrUnaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *argument = parse_unary_expression(ps); // 右结合

        SZrAstNode *node = create_ast_node(ps, ZR_AST_UNARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_unary_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_multiplicative_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_additive_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_shift_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_relational_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_equality_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_binary_and_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_binary_xor_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_binary_or_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_LOGICAL_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_logical_and_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_LOGICAL_EXPRESSION, startLoc);
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

    if (consume_token(ps, ZR_TK_QUESTIONMARK)) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrAstNode *consequent = parse_expression(ps);
        expect_token(ps, ZR_TK_COLON);
        consume_token(ps, ZR_TK_COLON);
        SZrAstNode *alternate = parse_conditional_expression(ps); // 右结合

        SZrAstNode *node = create_ast_node(ps, ZR_AST_CONDITIONAL_EXPRESSION, startLoc);
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
        SZrFileRange startLoc = get_current_location(ps);
        SZrAssignmentOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, token);

        ZrParser_Lexer_Next(ps->lexer);
        SZrAstNode *right = parse_assignment_expression(ps); // 右结合

        SZrAstNode *node = create_ast_node(ps, ZR_AST_ASSIGNMENT_EXPRESSION, startLoc);
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
