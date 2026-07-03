#include "type_inference_loop_assignment_prefix_offset.h"
#include "type_inference_loop_assignment_prefix_zero_inclusive_negative.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/string.h"

#include <limits.h>
#include <string.h>

static TZrBool prefix_offset_operator_is(
        const TZrChar *actual,
        const TZrChar *expected) {
    return actual != ZR_NULL && expected != ZR_NULL && strcmp(actual, expected) == 0;
}

static TZrBool prefix_offset_expression_is_zero_literal(
        const SZrAstNode *expression) {
    return expression != ZR_NULL &&
           expression->type == ZR_AST_INTEGER_LITERAL &&
           expression->data.integerLiteral.value == 0;
}

static TZrBool prefix_offset_expressions_are_same_identifier(
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

static const SZrAstNode *prefix_offset_expression_skip_unary_plus_identity(
        const SZrAstNode *expression) {
    const SZrUnaryExpression *unary;

    while (expression != ZR_NULL &&
           expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (!prefix_offset_operator_is(unary->op.op, "+")) {
            break;
        }
        expression = unary->argument;
    }
    return expression;
}

static const SZrAstNode *prefix_offset_expression_skip_zero_identity_wrappers(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;

    while (expression != ZR_NULL) {
        expression = prefix_offset_expression_skip_unary_plus_identity(expression);
        if (expression == ZR_NULL ||
            expression->type != ZR_AST_BINARY_EXPRESSION) {
            break;
        }

        binary = &expression->data.binaryExpression;
        if (prefix_offset_operator_is(binary->op.op, "+")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                expression = binary->left;
                continue;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                expression = binary->right;
                continue;
            }
            break;
        }
        if (prefix_offset_operator_is(binary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
            expression = binary->left;
            continue;
        }
        break;
    }
    return expression;
}

static TZrBool prefix_offset_expressions_are_same_identity_wrapped_identifier(
        SZrCompilerState *cs,
        const SZrAstNode *left,
        const SZrAstNode *right,
        const SZrAstNode **outIdentifierExpression) {
    left = prefix_offset_expression_skip_zero_identity_wrappers(cs, left);
    right = prefix_offset_expression_skip_zero_identity_wrappers(cs, right);
    if (!prefix_offset_expressions_are_same_identifier(left, right)) {
        return ZR_FALSE;
    }
    if (outIdentifierExpression != ZR_NULL) {
        *outIdentifierExpression = left;
    }
    return ZR_TRUE;
}

static TZrBool prefix_offset_value_is_all_ones_mask(TZrInt64 value) {
    TZrUInt64 mask;

    if (value < 0) {
        return ZR_FALSE;
    }

    mask = (TZrUInt64)value;
    return (mask & (mask + 1u)) == 0;
}

static TZrBool prefix_offset_expression_is_zero_inclusive_negative_binding(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    return ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                   cs,
                   expression,
                   ZR_NULL,
                   ZR_NULL);
}

static TZrBool prefix_offset_expression_is_supported_negative_binding(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                   cs,
                   expression,
                   &minValue,
                   &maxValue) &&
           minValue != ZR_TYPE_RANGE_INT64_MIN &&
           maxValue < 0;
}

static TZrBool prefix_offset_expression_is_sign_crossing_binding(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrTypeBinding *binding;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        expression == ZR_NULL ||
        expression->type != ZR_AST_IDENTIFIER_LITERAL ||
        expression->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            cs->typeEnv,
            expression->data.identifier.name);
    return binding != ZR_NULL &&
           binding->type.baseType == ZR_VALUE_TYPE_INT64 &&
           binding->type.hasRangeConstraint &&
           binding->type.minValue < 0 &&
           binding->type.maxValue > 0 &&
           binding->type.maxValue >= binding->type.minValue;
}

static TZrBool prefix_offset_expression_is_sign_crossing_product_operand(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;

    if (prefix_offset_expression_is_sign_crossing_binding(cs, expression)) {
        return ZR_TRUE;
    }
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        return prefix_offset_operator_is(unary->op.op, "+") &&
               prefix_offset_expression_is_sign_crossing_product_operand(
                       cs,
                       unary->argument);
    }
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        if (prefix_offset_operator_is(binary->op.op, "+")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return prefix_offset_expression_is_sign_crossing_product_operand(
                        cs,
                        binary->right);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return prefix_offset_expression_is_sign_crossing_product_operand(
                        cs,
                        binary->left);
            }
        }
        if (prefix_offset_operator_is(binary->op.op, "-") &&
            ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
            return prefix_offset_expression_is_sign_crossing_product_operand(
                    cs,
                    binary->left);
        }
    }

    return ZR_FALSE;
}

static TZrBool prefix_offset_expression_is_exact_zero_product_operand(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                   cs,
                   expression,
                   ZR_NULL,
                   ZR_NULL) ||
           prefix_offset_expression_is_zero_inclusive_negative_binding(cs, expression) ||
           prefix_offset_expression_is_supported_negative_binding(cs, expression) ||
           prefix_offset_expression_is_sign_crossing_product_operand(cs, expression);
}

static TZrBool prefix_offset_bitwise_and_identity_mask_range(
        TZrInt64 leftMinValue,
        TZrInt64 leftMaxValue,
        TZrInt64 rightMinValue,
        TZrInt64 rightMaxValue,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    if (rightMinValue == rightMaxValue &&
        prefix_offset_value_is_all_ones_mask(rightMinValue) &&
        leftMinValue >= 0 &&
        leftMaxValue <= rightMinValue) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = leftMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = leftMaxValue;
        }
        return ZR_TRUE;
    }

    if (leftMinValue == leftMaxValue &&
        prefix_offset_value_is_all_ones_mask(leftMinValue) &&
        rightMinValue >= 0 &&
        rightMaxValue <= leftMinValue) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = rightMinValue;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = rightMaxValue;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrTypeBinding *binding;
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrAstNode *sameIdentifierExpression;
    TZrInt64 sameIdentifierMinValue;

    if (prefix_offset_expression_is_zero_literal(expression)) {
        return ZR_TRUE;
    }
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        return (prefix_offset_operator_is(unary->op.op, "+") ||
                prefix_offset_operator_is(unary->op.op, "-")) &&
               ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                       cs,
                       unary->argument);
    }
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        if (prefix_offset_operator_is(binary->op.op, "-") &&
            prefix_offset_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    sameIdentifierExpression,
                    ZR_NULL,
                    ZR_NULL);
        }
        if (prefix_offset_operator_is(binary->op.op, "%") &&
            prefix_offset_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                           cs,
                           sameIdentifierExpression,
                           &sameIdentifierMinValue,
                           ZR_NULL) &&
                   sameIdentifierMinValue > 0;
        }
        if (prefix_offset_operator_is(binary->op.op, "+") ||
            prefix_offset_operator_is(binary->op.op, "-")) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                           cs,
                           binary->left) &&
                   ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                           cs,
                           binary->right);
        }
        if (prefix_offset_operator_is(binary->op.op, "*")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return prefix_offset_expression_is_exact_zero_product_operand(
                        cs,
                        binary->right);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return prefix_offset_expression_is_exact_zero_product_operand(
                        cs,
                        binary->left);
            }
        }
        if (prefix_offset_operator_is(binary->op.op, "&")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        ZR_NULL,
                        ZR_NULL);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        ZR_NULL,
                        ZR_NULL);
            }
        }
        if (prefix_offset_operator_is(binary->op.op, "^") &&
            prefix_offset_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    sameIdentifierExpression,
                    ZR_NULL,
                    ZR_NULL);
        }
        return ZR_FALSE;
    }
    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        expression == ZR_NULL ||
        expression->type != ZR_AST_IDENTIFIER_LITERAL ||
        expression->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            cs->typeEnv,
            expression->data.identifier.name);
    return binding != ZR_NULL &&
           binding->type.baseType == ZR_VALUE_TYPE_INT64 &&
           binding->type.hasRangeConstraint &&
           binding->type.minValue == 0 &&
           binding->type.maxValue == 0;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrTypeBinding *binding;
    const SZrAstNode *sameIdentifierExpression;
    TZrInt64 leftMinValue;
    TZrInt64 leftMaxValue;
    TZrInt64 rightMinValue;
    TZrInt64 rightMaxValue;
    TZrInt64 moduloMaxValue;
    TZrInt64 int64SignBitShiftCount;
    TZrInt64 shiftedMinValue;
    TZrInt64 shiftedMaxValue;
    TZrInt64 negativeMinValue;
    TZrInt64 negativeMaxValue;

    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }

    int64SignBitShiftCount = (TZrInt64)(sizeof(TZrInt64) * CHAR_BIT - 1u);

    if (expression->type == ZR_AST_INTEGER_LITERAL &&
        expression->data.integerLiteral.value > 0) {
        if (outMinValue != ZR_NULL) {
            *outMinValue = expression->data.integerLiteral.value;
        }
        if (outMaxValue != ZR_NULL) {
            *outMaxValue = expression->data.integerLiteral.value;
        }
        return ZR_TRUE;
    }
    if (expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (prefix_offset_operator_is(unary->op.op, "+")) {
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    unary->argument,
                    outMinValue,
                    outMaxValue);
        }
        if (prefix_offset_operator_is(unary->op.op, "-")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                    cs,
                    unary->argument,
                    &negativeMinValue,
                    &negativeMaxValue)) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = -negativeMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = -negativeMinValue;
            }
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_BINARY_EXPRESSION) {
        binary = &expression->data.binaryExpression;
        if (prefix_offset_operator_is(binary->op.op, "+")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        outMinValue,
                        outMaxValue);
            }
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                leftMaxValue > ZR_TYPE_RANGE_INT64_MAX - rightMaxValue) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue + rightMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue + rightMaxValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, "-")) {
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                    cs,
                    binary->right,
                    &negativeMinValue,
                    &negativeMaxValue) &&
                negativeMinValue != ZR_TYPE_RANGE_INT64_MIN &&
                negativeMaxValue < 0) {
                if (outMinValue != ZR_NULL) {
                    *outMinValue = -negativeMaxValue;
                }
                if (outMaxValue != ZR_NULL) {
                    *outMaxValue = -negativeMinValue;
                }
                return ZR_TRUE;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left) &&
                ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                    cs,
                    binary->right,
                    &negativeMinValue,
                    &negativeMaxValue)) {
                if (outMinValue != ZR_NULL) {
                    *outMinValue = -negativeMaxValue;
                }
                if (outMaxValue != ZR_NULL) {
                    *outMaxValue = -negativeMinValue;
                }
                return ZR_TRUE;
            }
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                leftMinValue < rightMaxValue) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue - rightMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue - rightMinValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, "*")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                (leftMaxValue != 0 && rightMaxValue > ZR_TYPE_RANGE_INT64_MAX / leftMaxValue)) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue * rightMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue * rightMaxValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, "/")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                rightMinValue <= 0) {
                return ZR_FALSE;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = leftMinValue / rightMaxValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = leftMaxValue / rightMinValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, "%")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                rightMinValue <= 0) {
                return ZR_FALSE;
            }
            moduloMaxValue = rightMaxValue - 1;
            if (leftMaxValue < moduloMaxValue) {
                moduloMaxValue = leftMaxValue;
            }
            if (outMinValue != ZR_NULL) {
                *outMinValue = 0;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = moduloMaxValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, "<<")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                rightMinValue < 0 ||
                rightMaxValue >= int64SignBitShiftCount ||
                leftMaxValue > (ZR_TYPE_RANGE_INT64_MAX >> (unsigned int)rightMaxValue)) {
                return ZR_FALSE;
            }
            shiftedMinValue = (TZrInt64)((TZrUInt64)leftMinValue << (unsigned int)rightMinValue);
            shiftedMaxValue = (TZrInt64)((TZrUInt64)leftMaxValue << (unsigned int)rightMaxValue);
            if (outMinValue != ZR_NULL) {
                *outMinValue = shiftedMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = shiftedMaxValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, ">>")) {
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue) ||
                rightMinValue < 0 ||
                rightMaxValue >= int64SignBitShiftCount) {
                return ZR_FALSE;
            }
            shiftedMinValue = (TZrInt64)((TZrUInt64)leftMinValue >> (unsigned int)rightMaxValue);
            shiftedMaxValue = (TZrInt64)((TZrUInt64)leftMaxValue >> (unsigned int)rightMinValue);
            if (outMinValue != ZR_NULL) {
                *outMinValue = shiftedMinValue;
            }
            if (outMaxValue != ZR_NULL) {
                *outMaxValue = shiftedMaxValue;
            }
            return ZR_TRUE;
        }
        if (prefix_offset_operator_is(binary->op.op, "&")) {
            if (prefix_offset_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        sameIdentifierExpression,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        &leftMinValue,
                        &leftMaxValue)) {
                    return ZR_FALSE;
                }
                if (outMinValue != ZR_NULL) {
                    *outMinValue = 0;
                }
                if (outMaxValue != ZR_NULL) {
                    *outMaxValue = 0;
                }
                return ZR_TRUE;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue)) {
                    return ZR_FALSE;
                }
                if (outMinValue != ZR_NULL) {
                    *outMinValue = 0;
                }
                if (outMaxValue != ZR_NULL) {
                    *outMaxValue = 0;
                }
                return ZR_TRUE;
            }
            if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                    cs,
                    binary->left,
                    &leftMinValue,
                    &leftMaxValue) ||
                !ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        &rightMinValue,
                        &rightMaxValue)) {
                return ZR_FALSE;
            }
            return prefix_offset_bitwise_and_identity_mask_range(
                    leftMinValue,
                    leftMaxValue,
                    rightMinValue,
                    rightMaxValue,
                    outMinValue,
                    outMaxValue);
        }
        if (prefix_offset_operator_is(binary->op.op, "|")) {
            if (prefix_offset_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        sameIdentifierExpression,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        outMinValue,
                        outMaxValue);
            }
            return ZR_FALSE;
        }
        if (prefix_offset_operator_is(binary->op.op, "^")) {
            if (prefix_offset_expressions_are_same_identity_wrapped_identifier(
                    cs,
                    binary->left,
                    binary->right,
                    &sameIdentifierExpression)) {
                if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        sameIdentifierExpression,
                        &leftMinValue,
                        &leftMaxValue)) {
                    return ZR_FALSE;
                }
                if (outMinValue != ZR_NULL) {
                    *outMinValue = 0;
                }
                if (outMaxValue != ZR_NULL) {
                    *outMaxValue = 0;
                }
                return ZR_TRUE;
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->right)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->left,
                        outMinValue,
                        outMaxValue);
            }
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                    cs,
                    binary->left)) {
                return ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                        cs,
                        binary->right,
                        outMinValue,
                        outMaxValue);
            }
            return ZR_FALSE;
        }
        return ZR_FALSE;
    }
    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        expression->type != ZR_AST_IDENTIFIER_LITERAL ||
        expression->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            cs->typeEnv,
            expression->data.identifier.name);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint ||
        binding->type.minValue < 0 ||
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
