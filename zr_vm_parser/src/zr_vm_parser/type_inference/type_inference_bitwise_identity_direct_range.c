#include "type_inference_bitwise_identity_direct_range.h"

#include "zr_vm_core/string.h"

#include <string.h>

TZrBool type_inference_bitwise_identity_operator_is(
        const TZrChar *actual,
        const TZrChar *expected) {
    return actual != ZR_NULL && expected != ZR_NULL && strcmp(actual, expected) == 0;
}

TZrBool type_inference_bitwise_identity_expressions_are_same_identifier(
        const SZrAstNode *left,
        const SZrAstNode *right) {
    return left != ZR_NULL &&
           right != ZR_NULL &&
           left->type == ZR_AST_IDENTIFIER_LITERAL &&
           right->type == ZR_AST_IDENTIFIER_LITERAL &&
           left->data.identifier.name != ZR_NULL &&
           right->data.identifier.name != ZR_NULL &&
           ZrCore_String_Equal(left->data.identifier.name, right->data.identifier.name);
}

const SZrAstNode *type_inference_bitwise_identity_skip_unary_plus_identity(
        const SZrAstNode *expression) {
    const SZrUnaryExpression *unary;

    while (expression != ZR_NULL &&
           expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (!type_inference_bitwise_identity_operator_is(unary->op.op, "+")) {
            break;
        }
        expression = unary->argument;
    }
    return expression;
}

const SZrAstNode *type_inference_bitwise_identity_skip_zero_identity_wrappers(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;

    while (expression != ZR_NULL) {
        expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
        if (expression == ZR_NULL ||
            expression->type != ZR_AST_BINARY_EXPRESSION) {
            break;
        }

        binary = &expression->data.binaryExpression;
        if (type_inference_bitwise_identity_operator_is(binary->op.op, "+")) {
            if (type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->right)) {
                expression = binary->left;
                continue;
            }
            if (type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->left)) {
                expression = binary->right;
                continue;
            }
            break;
        }

        if (type_inference_bitwise_identity_operator_is(binary->op.op, "-") &&
            type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->right)) {
            expression = binary->left;
            continue;
        }
        break;
    }
    return expression;
}

TZrBool type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        const SZrAstNode **outIdentifierExpression) {
    left = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, left);
    right = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, right);
    if (!type_inference_bitwise_identity_expressions_are_same_identifier(left, right)) {
        return ZR_FALSE;
    }
    if (outIdentifierExpression != ZR_NULL) {
        *outIdentifierExpression = left;
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_is_zero_literal(
        const SZrAstNode *expression) {
    return expression != ZR_NULL &&
           expression->type == ZR_AST_INTEGER_LITERAL &&
           expression->data.integerLiteral.value == 0;
}

const SZrTypeBinding *type_inference_bitwise_identity_expression_binding(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        expression == ZR_NULL ||
        expression->type != ZR_AST_IDENTIFIER_LITERAL ||
        expression->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_TypeEnvironment_FindVariableBinding(
            cs->typeEnv,
            expression->data.identifier.name);
}

TZrBool type_inference_bitwise_identity_expression_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrTypeBinding *binding;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }

    if (expression->type == ZR_AST_INTEGER_LITERAL) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = expression->data.integerLiteral.value;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = expression->data.integerLiteral.value;
        }
        return ZR_TRUE;
    }

    binding = type_inference_bitwise_identity_expression_binding(cs, expression);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint ||
        binding->type.maxValue < binding->type.minValue) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = binding->type.minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = binding->type.maxValue;
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    return type_inference_bitwise_identity_expression_direct_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_wrapped_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_zero_wrapped_direct_int64_range(
                cs,
                unary->argument,
                &operandMinValue,
                &operandMaxValue) ||
        operandMinValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -operandMaxValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -operandMinValue;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_unary_minus_zero_wrapped_direct_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_basic_zero_minus_chain_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const TZrChar *op;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    op = binary->op.op;
    if (type_inference_bitwise_identity_operator_is(op, "&") &&
        type_inference_bitwise_identity_expression_signed_all_ones_mask_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_operator_is(op, "|") &&
        type_inference_bitwise_identity_expression_signed_all_ones_or_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_signed_same_identifier_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_negate_range_in_place(
        TZrInt64 *minValue,
        TZrInt64 *maxValue) {
    TZrInt64 oldMinValue;
    TZrInt64 oldMaxValue;

    if (minValue == ZR_NULL ||
        maxValue == ZR_NULL ||
        *minValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    oldMinValue = *minValue;
    oldMaxValue = *maxValue;
    *minValue = -oldMaxValue;
    *maxValue = -oldMinValue;
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_basic_zero_minus_chain_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrAstNode *currentExpression = expression;
    const SZrBinaryExpression *binary;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    int depth;
    int negationIndex;

    depth = 0;
    while (ZR_TRUE) {
        currentExpression = type_inference_bitwise_identity_skip_zero_identity_wrappers(
                cs,
                currentExpression);
        if (currentExpression == ZR_NULL ||
            currentExpression->type != ZR_AST_BINARY_EXPRESSION) {
            return ZR_FALSE;
        }

        binary = &currentExpression->data.binaryExpression;
        if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
            !type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->left)) {
            return ZR_FALSE;
        }

        currentExpression = binary->right;
        if (!type_inference_bitwise_identity_expression_basic_zero_minus_chain_leaf_int64_range(
                    cs,
                    currentExpression,
                    &minValue,
                    &maxValue)) {
            ++depth;
            continue;
        }

        for (negationIndex = 0; negationIndex <= depth; ++negationIndex) {
            if (!type_inference_bitwise_identity_negate_range_in_place(
                        &minValue,
                        &maxValue)) {
                return ZR_FALSE;
            }
        }

        if (outMinValue != ZR_NULL) {
            *outMinValue = minValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = maxValue;
        }
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool type_inference_bitwise_identity_expression_zero_wrapped_or_basic_zero_minus_chain_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_basic_zero_minus_chain_direct_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_zero_wrapped_or_basic_zero_minus_chain_direct_int64_range(
                cs,
                unary->argument,
                &operandMinValue,
                &operandMaxValue) ||
        operandMinValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -operandMaxValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -operandMinValue;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_unary_minus_direct_int64_range(
                   cs,
                   expression,
                   outMinValue,
                   outMaxValue) ||
           type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
                   cs,
                   expression,
                   outMinValue,
                   outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_zero_wrapped_or_zero_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (type_inference_bitwise_identity_expression_zero_wrapped_or_basic_zero_minus_chain_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

TZrBool type_inference_bitwise_identity_expression_unary_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_zero_wrapped_or_zero_minus_direct_int64_range(
                cs,
                unary->argument,
                &operandMinValue,
                &operandMaxValue) ||
        operandMinValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -operandMaxValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -operandMinValue;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    return type_inference_bitwise_identity_expression_basic_zero_minus_chain_leaf_int64_range(
            cs,
            expression,
            outMinValue,
            outMaxValue);
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (!type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) ||
        !type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue)) {
        return ZR_FALSE;
    }

    if (type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                leftMinValue,
                leftMaxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = rightMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = rightMaxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                rightMinValue,
                rightMaxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = leftMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = leftMaxValue;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_or_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (!type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) ||
        !type_inference_bitwise_identity_expression_signed_all_ones_operand_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue)) {
        return ZR_FALSE;
    }

    if (!type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                leftMinValue,
                leftMaxValue) &&
        !type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                rightMinValue,
                rightMaxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -1;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_bitwise_identity_expression_bitwise_not_operand_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;

    if (type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if ((type_inference_bitwise_identity_operator_is(binary->op.op, "&") ||
         type_inference_bitwise_identity_operator_is(binary->op.op, "|")) &&
        type_inference_bitwise_identity_expression_signed_same_identifier_range(
                cs,
                expression,
                outMinValue,
                outMaxValue)) {
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_operator_is(binary->op.op, "&")) {
        return type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_mask_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue);
    }

    if (type_inference_bitwise_identity_operator_is(binary->op.op, "|")) {
        return type_inference_bitwise_identity_expression_bitwise_not_signed_all_ones_or_range(
                cs,
                binary->left,
                binary->right,
                outMinValue,
                outMaxValue);
    }

    return ZR_FALSE;
}

TZrBool type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "~") ||
        !type_inference_bitwise_identity_expression_bitwise_not_operand_int64_range(
                cs,
                unary->argument,
                &operandMinValue,
                &operandMaxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = (TZrInt64)(~((TZrUInt64)operandMaxValue));
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = (TZrInt64)(~((TZrUInt64)operandMinValue));
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_zero_minus_direct_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_zero_wrapped_or_basic_zero_minus_direct_int64_range(
                cs,
                binary->right,
                &operandMinValue,
                &operandMaxValue) ||
        operandMinValue == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -operandMaxValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -operandMinValue;
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
        TZrInt64 minValue,
        TZrInt64 maxValue) {
    return minValue == -1 && maxValue == -1;
}

TZrBool type_inference_bitwise_identity_expression_signed_all_ones_mask_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (!type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_direct_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) ||
        !type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_direct_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue)) {
        return ZR_FALSE;
    }

    if (type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                leftMinValue,
                leftMaxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = rightMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = rightMaxValue;
        }
        return ZR_TRUE;
    }

    if (type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                rightMinValue,
                rightMaxValue)) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = leftMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = leftMaxValue;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool type_inference_bitwise_identity_expression_signed_all_ones_or_range(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;

    if (!type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_direct_int64_range(
                cs,
                left,
                &leftMinValue,
                &leftMaxValue) ||
        !type_inference_bitwise_identity_expression_zero_wrapped_or_unary_minus_direct_int64_range(
                cs,
                right,
                &rightMinValue,
                &rightMaxValue)) {
        return ZR_FALSE;
    }

    if (!type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                leftMinValue,
                leftMaxValue) &&
        !type_inference_bitwise_identity_range_is_exact_signed_all_ones_mask(
                rightMinValue,
                rightMaxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = -1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = -1;
    }
    return ZR_TRUE;
}

TZrBool type_inference_bitwise_identity_expression_signed_same_identifier_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrAstNode *sameIdentifierExpression;
    const TZrChar *op;

    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    op = binary->op.op;
    if (!type_inference_bitwise_identity_operator_is(op, "&") &&
        !type_inference_bitwise_identity_operator_is(op, "|")) {
        return ZR_FALSE;
    }

    if (!type_inference_bitwise_identity_expressions_are_same_identity_wrapped_identifier(
                cs,
                binary->left,
                binary->right,
                &sameIdentifierExpression)) {
        return ZR_FALSE;
    }

    return type_inference_bitwise_identity_expression_direct_int64_range(
            cs,
            sameIdentifierExpression,
            outMinValue,
            outMaxValue);
}
