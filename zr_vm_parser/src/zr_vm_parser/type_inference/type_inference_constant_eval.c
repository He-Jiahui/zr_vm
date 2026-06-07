#include "type_inference_constant_eval.h"

#include <stdint.h>
#include <string.h>

static TZrBool type_inference_int64_add(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) {
        return ZR_FALSE;
    }

    *outValue = left + right;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_subtract(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) {
        return ZR_FALSE;
    }

    *outValue = left - right;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_multiply(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right) ||
            (right < 0 && right < INT64_MIN / left)) {
            return ZR_FALSE;
        }
    } else if (left < 0) {
        if ((right > 0 && left < INT64_MIN / right) ||
            (right < 0 && left < INT64_MAX / right)) {
            return ZR_FALSE;
        }
    }

    *outValue = left * right;
    return ZR_TRUE;
}

TZrBool type_inference_node_integer_value(SZrAstNode *node, TZrInt64 *outValue) {
    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            *outValue = node->data.integerLiteral.value;
            return ZR_TRUE;
        case ZR_AST_CHAR_LITERAL:
            *outValue = node->data.charLiteral.value;
            return ZR_TRUE;
        case ZR_AST_UNARY_EXPRESSION: {
            TZrInt64 operand;
            if (node->data.unaryExpression.op.op == ZR_NULL ||
                !type_inference_node_integer_value(node->data.unaryExpression.argument, &operand)) {
                return ZR_FALSE;
            }
            if (strcmp(node->data.unaryExpression.op.op, "+") == 0) {
                *outValue = operand;
                return ZR_TRUE;
            }
            if (strcmp(node->data.unaryExpression.op.op, "-") == 0) {
                return type_inference_int64_subtract(0, operand, outValue);
            }
            return ZR_FALSE;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            TZrInt64 left;
            TZrInt64 right;
            const TZrChar *op = node->data.binaryExpression.op.op;

            if (op == ZR_NULL ||
                !type_inference_node_integer_value(node->data.binaryExpression.left, &left) ||
                !type_inference_node_integer_value(node->data.binaryExpression.right, &right)) {
                return ZR_FALSE;
            }

            if (strcmp(op, "+") == 0) {
                return type_inference_int64_add(left, right, outValue);
            }
            if (strcmp(op, "-") == 0) {
                return type_inference_int64_subtract(left, right, outValue);
            }
            if (strcmp(op, "*") == 0) {
                return type_inference_int64_multiply(left, right, outValue);
            }
            return ZR_FALSE;
        }
        default:
            return ZR_FALSE;
    }
}

TZrBool type_inference_node_double_value(SZrAstNode *node, TZrDouble *outValue) {
    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_FLOAT_LITERAL:
            *outValue = node->data.floatLiteral.value;
            return ZR_TRUE;
        case ZR_AST_INTEGER_LITERAL:
            *outValue = (TZrDouble)node->data.integerLiteral.value;
            return ZR_TRUE;
        case ZR_AST_CHAR_LITERAL:
            *outValue = (TZrDouble)node->data.charLiteral.value;
            return ZR_TRUE;
        case ZR_AST_UNARY_EXPRESSION: {
            TZrDouble operand;
            if (node->data.unaryExpression.op.op == ZR_NULL ||
                !type_inference_node_double_value(node->data.unaryExpression.argument, &operand)) {
                return ZR_FALSE;
            }
            if (strcmp(node->data.unaryExpression.op.op, "+") == 0) {
                *outValue = operand;
                return ZR_TRUE;
            }
            if (strcmp(node->data.unaryExpression.op.op, "-") == 0) {
                *outValue = -operand;
                return ZR_TRUE;
            }
            return ZR_FALSE;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            TZrDouble left;
            TZrDouble right;
            const TZrChar *op = node->data.binaryExpression.op.op;

            if (op == ZR_NULL ||
                !type_inference_node_double_value(node->data.binaryExpression.left, &left) ||
                !type_inference_node_double_value(node->data.binaryExpression.right, &right)) {
                return ZR_FALSE;
            }

            if (strcmp(op, "+") == 0) {
                *outValue = left + right;
                return ZR_TRUE;
            }
            if (strcmp(op, "-") == 0) {
                *outValue = left - right;
                return ZR_TRUE;
            }
            if (strcmp(op, "*") == 0) {
                *outValue = left * right;
                return ZR_TRUE;
            }
            if (strcmp(op, "/") == 0 && right != 0.0) {
                *outValue = left / right;
                return ZR_TRUE;
            }
            return ZR_FALSE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_node_numeric_comparison_bool_value(SZrAstNode *node,
                                                                 TZrBool *outValue) {
    TZrDouble left;
    TZrDouble right;
    const TZrChar *op;

    if (node == ZR_NULL ||
        outValue == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    op = node->data.binaryExpression.op.op;
    if (op == ZR_NULL ||
        !type_inference_node_double_value(node->data.binaryExpression.left, &left) ||
        !type_inference_node_double_value(node->data.binaryExpression.right, &right)) {
        return ZR_FALSE;
    }

    if (strcmp(op, "==") == 0) {
        *outValue = left == right ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    if (strcmp(op, "!=") == 0) {
        *outValue = left != right ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    if (strcmp(op, "<") == 0) {
        *outValue = left < right ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    if (strcmp(op, ">") == 0) {
        *outValue = left > right ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    if (strcmp(op, "<=") == 0) {
        *outValue = left <= right ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }
    if (strcmp(op, ">=") == 0) {
        *outValue = left >= right ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool type_inference_node_unary_bool_value(SZrAstNode *node, TZrBool *outValue) {
    TZrBool operandValue;
    const TZrChar *op;

    if (node == ZR_NULL ||
        outValue == ZR_NULL ||
        node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    op = node->data.unaryExpression.op.op;
    if (op == ZR_NULL ||
        strcmp(op, "!") != 0 ||
        !type_inference_node_bool_value(node->data.unaryExpression.argument, &operandValue)) {
        return ZR_FALSE;
    }

    *outValue = operandValue ? ZR_FALSE : ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool type_inference_node_logical_bool_value(SZrAstNode *node, TZrBool *outValue) {
    TZrBool leftValue;
    TZrBool rightValue;
    const TZrChar *op;

    if (node == ZR_NULL ||
        outValue == ZR_NULL ||
        node->type != ZR_AST_LOGICAL_EXPRESSION ||
        !type_inference_node_bool_value(node->data.logicalExpression.left, &leftValue)) {
        return ZR_FALSE;
    }

    op = node->data.logicalExpression.op;
    if (op == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "&&") == 0) {
        if (!leftValue) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
        if (!type_inference_node_bool_value(node->data.logicalExpression.right, &rightValue)) {
            return ZR_FALSE;
        }
        *outValue = rightValue;
        return ZR_TRUE;
    }

    if (strcmp(op, "||") == 0) {
        if (leftValue) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (!type_inference_node_bool_value(node->data.logicalExpression.right, &rightValue)) {
            return ZR_FALSE;
        }
        *outValue = rightValue;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool type_inference_node_bool_value(SZrAstNode *node, TZrBool *outValue) {
    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            *outValue = node->data.booleanLiteral.value;
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return type_inference_node_numeric_comparison_bool_value(node, outValue);
        case ZR_AST_UNARY_EXPRESSION:
            return type_inference_node_unary_bool_value(node, outValue);
        case ZR_AST_LOGICAL_EXPRESSION:
            return type_inference_node_logical_bool_value(node, outValue);
        default:
            return ZR_FALSE;
    }
}
