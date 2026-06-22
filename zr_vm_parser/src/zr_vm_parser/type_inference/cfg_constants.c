#include "cfg_internal.h"

#include <math.h>
#include <stdint.h>

static TZrBool cfg_integer_add_checked(TZrInt64 leftValue,
                                       TZrInt64 rightValue,
                                       TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((rightValue > 0 && leftValue > INT64_MAX - rightValue) ||
        (rightValue < 0 && leftValue < INT64_MIN - rightValue)) {
        return ZR_FALSE;
    }

    *outValue = leftValue + rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_subtract_checked(TZrInt64 leftValue,
                                            TZrInt64 rightValue,
                                            TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((rightValue > 0 && leftValue < INT64_MIN + rightValue) ||
        (rightValue < 0 && leftValue > INT64_MAX + rightValue)) {
        return ZR_FALSE;
    }

    *outValue = leftValue - rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_multiply_checked(TZrInt64 leftValue,
                                            TZrInt64 rightValue,
                                            TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (leftValue == 0 || rightValue == 0) {
        *outValue = 0;
        return ZR_TRUE;
    }
    if ((leftValue == -1 && rightValue == INT64_MIN) ||
        (rightValue == -1 && leftValue == INT64_MIN)) {
        return ZR_FALSE;
    }

    if (leftValue > 0) {
        if ((rightValue > 0 && leftValue > INT64_MAX / rightValue) ||
            (rightValue < 0 && rightValue < INT64_MIN / leftValue)) {
            return ZR_FALSE;
        }
    } else if (rightValue > 0) {
        if (leftValue < INT64_MIN / rightValue) {
            return ZR_FALSE;
        }
    } else if (rightValue < INT64_MAX / leftValue) {
        return ZR_FALSE;
    }

    *outValue = leftValue * rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_divide_checked(TZrInt64 leftValue,
                                          TZrInt64 rightValue,
                                          TZrInt64 *outValue) {
    if (outValue == ZR_NULL || rightValue == 0 ||
        (leftValue == INT64_MIN && rightValue == -1)) {
        return ZR_FALSE;
    }

    *outValue = leftValue / rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_modulo_checked(TZrInt64 leftValue,
                                          TZrInt64 rightValue,
                                          TZrInt64 *outValue) {
    if (outValue == ZR_NULL || rightValue == 0 ||
        (leftValue == INT64_MIN && rightValue == -1)) {
        return ZR_FALSE;
    }

    *outValue = leftValue % rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_bitwise_and_checked(TZrInt64 leftValue,
                                               TZrInt64 rightValue,
                                               TZrInt64 *outValue) {
    if (outValue == ZR_NULL || leftValue < 0 || rightValue < 0) {
        return ZR_FALSE;
    }

    *outValue = leftValue & rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_bitwise_or_checked(TZrInt64 leftValue,
                                              TZrInt64 rightValue,
                                              TZrInt64 *outValue) {
    if (outValue == ZR_NULL || leftValue < 0 || rightValue < 0) {
        return ZR_FALSE;
    }

    *outValue = leftValue | rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_bitwise_xor_checked(TZrInt64 leftValue,
                                               TZrInt64 rightValue,
                                               TZrInt64 *outValue) {
    if (outValue == ZR_NULL || leftValue < 0 || rightValue < 0) {
        return ZR_FALSE;
    }

    *outValue = leftValue ^ rightValue;
    return ZR_TRUE;
}

static TZrBool cfg_integer_shift_left_checked(TZrInt64 leftValue,
                                              TZrInt64 rightValue,
                                              TZrInt64 *outValue) {
    if (outValue == ZR_NULL || leftValue < 0 || rightValue < 0 || rightValue >= 63) {
        return ZR_FALSE;
    }
    if (leftValue > (INT64_MAX >> rightValue)) {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)((uint64_t)leftValue << (unsigned int)rightValue);
    return ZR_TRUE;
}

static TZrBool cfg_integer_shift_right_checked(TZrInt64 leftValue,
                                               TZrInt64 rightValue,
                                               TZrInt64 *outValue) {
    if (outValue == ZR_NULL || leftValue < 0 || rightValue < 0 || rightValue >= 63) {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)((uint64_t)leftValue >> (unsigned int)rightValue);
    return ZR_TRUE;
}

static TZrBool cfg_float_binary_checked(TZrDouble leftValue,
                                        TZrDouble rightValue,
                                        TZrChar op,
                                        TZrDouble *outValue) {
    TZrDouble result;

    if (outValue == ZR_NULL || !isfinite(leftValue) || !isfinite(rightValue)) {
        return ZR_FALSE;
    }

    if (op == '+') {
        result = leftValue + rightValue;
    } else if (op == '-') {
        result = leftValue - rightValue;
    } else if (op == '*') {
        result = leftValue * rightValue;
    } else if (op == '/') {
        if (rightValue == 0.0) {
            return ZR_FALSE;
        }
        result = leftValue / rightValue;
    } else {
        return ZR_FALSE;
    }
    if (!isfinite(result)) {
        return ZR_FALSE;
    }

    *outValue = result;
    return ZR_TRUE;
}

static TZrBool cfg_unary_integer_arithmetic_constant(SZrAstNode *node,
                                                     SZrParserCfgConstant *outValue) {
    SZrParserCfgConstant operand;
    TZrInt64 result = 0;
    const TZrChar *op;

    if (node == ZR_NULL || outValue == ZR_NULL ||
        node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    op = node->data.unaryExpression.op.op;
    if (op == ZR_NULL || (op[0] != '+' && op[0] != '-' && op[0] != '~') ||
        op[1] != '\0' ||
        !cfg_node_constant(node->data.unaryExpression.argument, &operand) ||
        operand.kind != ZR_PARSER_CFG_CONSTANT_INTEGER) {
        return ZR_FALSE;
    }

    if (op[0] == '+') {
        result = operand.integerValue;
    } else if (op[0] == '~') {
        if (!cfg_integer_subtract_checked(-1, operand.integerValue, &result)) {
            return ZR_FALSE;
        }
    } else if (!cfg_integer_subtract_checked(0, operand.integerValue, &result)) {
        return ZR_FALSE;
    }

    outValue->kind = ZR_PARSER_CFG_CONSTANT_INTEGER;
    outValue->integerValue = result;
    return ZR_TRUE;
}

static TZrBool cfg_unary_float_arithmetic_constant(SZrAstNode *node,
                                                   SZrParserCfgConstant *outValue) {
    SZrParserCfgConstant operand;
    TZrDouble result = 0.0;
    const TZrChar *op;

    if (node == ZR_NULL || outValue == ZR_NULL ||
        node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    op = node->data.unaryExpression.op.op;
    if (op == ZR_NULL || (op[0] != '+' && op[0] != '-') || op[1] != '\0' ||
        !cfg_node_constant(node->data.unaryExpression.argument, &operand) ||
        operand.kind != ZR_PARSER_CFG_CONSTANT_FLOAT ||
        !isfinite(operand.floatValue)) {
        return ZR_FALSE;
    }

    result = op[0] == '+' ? operand.floatValue : -operand.floatValue;
    if (!isfinite(result)) {
        return ZR_FALSE;
    }

    outValue->kind = ZR_PARSER_CFG_CONSTANT_FLOAT;
    outValue->floatValue = result;
    return ZR_TRUE;
}

static TZrBool cfg_binary_integer_arithmetic_constant(SZrAstNode *node,
                                                      SZrParserCfgConstant *outValue) {
    SZrParserCfgConstant leftValue;
    SZrParserCfgConstant rightValue;
    TZrInt64 result = 0;
    const TZrChar *op;
    TZrBool isSingleCharOp = ZR_FALSE;
    TZrBool isLeftShiftOp = ZR_FALSE;
    TZrBool isRightShiftOp = ZR_FALSE;

    if (node == ZR_NULL || outValue == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    op = node->data.binaryExpression.op.op;
    if (op == ZR_NULL) {
        return ZR_FALSE;
    }
    isSingleCharOp = (TZrBool)(op[0] != '\0' && op[1] == '\0');
    isLeftShiftOp = (TZrBool)(op[0] == '<' && op[1] == '<' && op[2] == '\0');
    isRightShiftOp = (TZrBool)(op[0] == '>' && op[1] == '>' && op[2] == '\0');
    if ((!isSingleCharOp ||
         (op[0] != '+' && op[0] != '-' && op[0] != '*' && op[0] != '/' &&
          op[0] != '%' && op[0] != '&' && op[0] != '|' && op[0] != '^')) &&
        !isLeftShiftOp && !isRightShiftOp) {
        return ZR_FALSE;
    }
    if (!cfg_node_constant(node->data.binaryExpression.left, &leftValue) ||
        !cfg_node_constant(node->data.binaryExpression.right, &rightValue) ||
        leftValue.kind != ZR_PARSER_CFG_CONSTANT_INTEGER ||
        rightValue.kind != ZR_PARSER_CFG_CONSTANT_INTEGER) {
        return ZR_FALSE;
    }
    if (op[0] == '+') {
        if (!cfg_integer_add_checked(leftValue.integerValue, rightValue.integerValue, &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '-') {
        if (!cfg_integer_subtract_checked(leftValue.integerValue,
                                          rightValue.integerValue,
                                          &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '*') {
        if (!cfg_integer_multiply_checked(leftValue.integerValue,
                                          rightValue.integerValue,
                                          &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '/') {
        if (!cfg_integer_divide_checked(leftValue.integerValue,
                                        rightValue.integerValue,
                                        &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '%') {
        if (!cfg_integer_modulo_checked(leftValue.integerValue,
                                        rightValue.integerValue,
                                        &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '&') {
        if (!cfg_integer_bitwise_and_checked(leftValue.integerValue,
                                            rightValue.integerValue,
                                            &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '|') {
        if (!cfg_integer_bitwise_or_checked(leftValue.integerValue,
                                           rightValue.integerValue,
                                           &result)) {
            return ZR_FALSE;
        }
    } else if (op[0] == '^') {
        if (!cfg_integer_bitwise_xor_checked(leftValue.integerValue,
                                            rightValue.integerValue,
                                            &result)) {
            return ZR_FALSE;
        }
    } else if (isLeftShiftOp) {
        if (!cfg_integer_shift_left_checked(leftValue.integerValue,
                                           rightValue.integerValue,
                                           &result)) {
            return ZR_FALSE;
        }
    } else if (isRightShiftOp) {
        if (!cfg_integer_shift_right_checked(leftValue.integerValue,
                                            rightValue.integerValue,
                                            &result)) {
            return ZR_FALSE;
        }
    }

    outValue->kind = ZR_PARSER_CFG_CONSTANT_INTEGER;
    outValue->integerValue = result;
    return ZR_TRUE;
}

static TZrBool cfg_binary_float_arithmetic_constant(SZrAstNode *node,
                                                    SZrParserCfgConstant *outValue) {
    SZrParserCfgConstant leftValue;
    SZrParserCfgConstant rightValue;
    TZrDouble result = 0.0;
    const TZrChar *op;

    if (node == ZR_NULL || outValue == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    op = node->data.binaryExpression.op.op;
    if (op == ZR_NULL || op[1] != '\0' ||
        (op[0] != '+' && op[0] != '-' && op[0] != '*' && op[0] != '/')) {
        return ZR_FALSE;
    }
    if (!cfg_node_constant(node->data.binaryExpression.left, &leftValue) ||
        !cfg_node_constant(node->data.binaryExpression.right, &rightValue) ||
        leftValue.kind != ZR_PARSER_CFG_CONSTANT_FLOAT ||
        rightValue.kind != ZR_PARSER_CFG_CONSTANT_FLOAT) {
        return ZR_FALSE;
    }
    if (!cfg_float_binary_checked(leftValue.floatValue,
                                  rightValue.floatValue,
                                  op[0],
                                  &result)) {
        return ZR_FALSE;
    }

    outValue->kind = ZR_PARSER_CFG_CONSTANT_FLOAT;
    outValue->floatValue = result;
    return ZR_TRUE;
}

TZrBool cfg_node_constant(SZrAstNode *node, SZrParserCfgConstant *outValue) {
    TZrBool boolValue = ZR_FALSE;

    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    outValue->kind = ZR_PARSER_CFG_CONSTANT_UNKNOWN;
    outValue->boolValue = ZR_FALSE;
    outValue->integerValue = 0;
    outValue->stringValue = ZR_NULL;
    outValue->charValue = '\0';
    outValue->floatValue = 0.0;
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cfg_node_bool_constant(node, &boolValue)) {
        outValue->kind = ZR_PARSER_CFG_CONSTANT_BOOL;
        outValue->boolValue = boolValue;
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            outValue->kind = ZR_PARSER_CFG_CONSTANT_INTEGER;
            outValue->integerValue = node->data.integerLiteral.value;
            return ZR_TRUE;
        case ZR_AST_STRING_LITERAL:
            if (node->data.stringLiteral.hasError ||
                node->data.stringLiteral.value == ZR_NULL) {
                return ZR_FALSE;
            }
            outValue->kind = ZR_PARSER_CFG_CONSTANT_STRING;
            outValue->stringValue = node->data.stringLiteral.value;
            return ZR_TRUE;
        case ZR_AST_CHAR_LITERAL:
            if (node->data.charLiteral.hasError) {
                return ZR_FALSE;
            }
            outValue->kind = ZR_PARSER_CFG_CONSTANT_CHAR;
            outValue->charValue = node->data.charLiteral.value;
            return ZR_TRUE;
        case ZR_AST_FLOAT_LITERAL:
            outValue->kind = ZR_PARSER_CFG_CONSTANT_FLOAT;
            outValue->floatValue = node->data.floatLiteral.value;
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return (TZrBool)(cfg_binary_integer_arithmetic_constant(node, outValue) ||
                             cfg_binary_float_arithmetic_constant(node, outValue));
        case ZR_AST_UNARY_EXPRESSION:
            return (TZrBool)(cfg_unary_integer_arithmetic_constant(node, outValue) ||
                             cfg_unary_float_arithmetic_constant(node, outValue));
        default:
            return ZR_FALSE;
    }
}

TZrBool cfg_constants_can_compare(const SZrParserCfgConstant *left,
                                  const SZrParserCfgConstant *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->kind == ZR_PARSER_CFG_CONSTANT_UNKNOWN ||
        right->kind == ZR_PARSER_CFG_CONSTANT_UNKNOWN) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool cfg_constants_equal(const SZrParserCfgConstant *left,
                            const SZrParserCfgConstant *right) {
    if (!cfg_constants_can_compare(left, right)) {
        return ZR_FALSE;
    }
    if (left->kind != right->kind) {
        return ZR_FALSE;
    }

    switch (left->kind) {
        case ZR_PARSER_CFG_CONSTANT_BOOL:
            return (TZrBool)(left->boolValue == right->boolValue);
        case ZR_PARSER_CFG_CONSTANT_INTEGER:
            return (TZrBool)(left->integerValue == right->integerValue);
        case ZR_PARSER_CFG_CONSTANT_STRING:
            return ZrCore_String_Equal(left->stringValue, right->stringValue);
        case ZR_PARSER_CFG_CONSTANT_CHAR:
            return (TZrBool)(left->charValue == right->charValue);
        case ZR_PARSER_CFG_CONSTANT_FLOAT:
            return (TZrBool)(left->floatValue == right->floatValue);
        default:
            return ZR_FALSE;
    }
}
