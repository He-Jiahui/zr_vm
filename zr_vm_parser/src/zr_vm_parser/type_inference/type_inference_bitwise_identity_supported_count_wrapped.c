#include "type_inference_bitwise_identity_supported_count_wrapped.h"

#include "type_inference_bitwise_identity_direct_range.h"
#include "type_inference_bitwise_identity_supported_count_all_ones_side.h"

/* 本单元仅匹配受支持的 AST 形态并计算区间；它不执行或重排源表达式。 */

/** @brief 对闭区间取负；含 INT64_MIN 时拒绝，以免发生有符号溢出。 */
static TZrBool type_inference_bitwise_identity_supported_count_negate_range_in_place(
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

/** @brief 沿值不变的零 OR/XOR 与一元正号外壳下钻，仅供 AST 形态识别。 */
static const SZrAstNode *type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
        SZrCompilerState *cs,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;

    while (expression != ZR_NULL) {
        expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(
                cs,
                expression);
        expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
        if (expression == ZR_NULL ||
            expression->type != ZR_AST_BINARY_EXPRESSION) {
            break;
        }

        binary = &expression->data.binaryExpression;
        if (!type_inference_bitwise_identity_operator_is(binary->op.op, "|") &&
            !type_inference_bitwise_identity_operator_is(binary->op.op, "^")) {
            break;
        }

        if (type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->left)) {
            expression = binary->right;
            continue;
        }
        if (type_inference_bitwise_identity_expression_is_exact_zero_value(
                    cs,
                    binary->right)) {
            expression = binary->left;
            continue;
        }
        break;
    }
    return expression;
}

/** @brief 将 -(0-n) 识别为 n，并复用其已证明非负的闭区间。 */
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrAstNode *argument;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    argument = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            unary->argument);
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        argument == ZR_NULL ||
        argument->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &argument->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                binary->right,
                &minValue,
                &maxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 将 -~n 按 n+1 推导；上界达到 INT64_MAX 时保守拒绝。 */
TZrBool type_inference_bitwise_identity_expression_unary_minus_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "-") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "~") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue) ||
        maxValue == ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue + 1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue + 1;
    }
    return ZR_TRUE;
}

/** @brief 将 -(-n) 识别为 n，并要求内层已有非负范围。 */
TZrBool type_inference_bitwise_identity_expression_double_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "-") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 将 ~(-n) 按 n-1 推导；下界必须大于零以保持结果非负。 */
TZrBool type_inference_bitwise_identity_expression_bitwise_not_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "~") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue) ||
        minValue <= 0) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue - 1;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue - 1;
    }
    return ZR_TRUE;
}

/** @brief 将 ~~n 识别为 n，并复用内层非负闭区间。 */
TZrBool type_inference_bitwise_identity_expression_double_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *outerUnary;
    const SZrUnaryExpression *innerUnary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerUnary = &expression->data.unaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerUnary->argument);
    if (!type_inference_bitwise_identity_operator_is(outerUnary->op.op, "~") ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerUnary = &innerExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerUnary->op.op, "~") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerUnary->argument,
                &minValue,
                &maxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 将 0-(-n) 识别为 n，并复用内层非负闭区间。 */
TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrAstNode *rightExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    rightExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            binary->right);
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        rightExpression == ZR_NULL ||
        rightExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &rightExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                unary->argument,
                &minValue,
                &maxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 将 0-(0-n) 识别为 n；两个零减外壳均须匹配。 */
TZrBool type_inference_bitwise_identity_expression_double_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *outerBinary;
    const SZrBinaryExpression *innerBinary;
    const SZrAstNode *innerExpression;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    outerBinary = &expression->data.binaryExpression;
    innerExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            outerBinary->right);
    if (!type_inference_bitwise_identity_operator_is(outerBinary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                outerBinary->left) ||
        innerExpression == ZR_NULL ||
        innerExpression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    innerBinary = &innerExpression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(innerBinary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                innerBinary->left) ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                innerBinary->right,
                &minValue,
                &maxValue)) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 推导非负 ~(0-count) 范围并以无符号 64 位取反。 @note TODO: uint64_t 超出 int64_t 范围的转换在 C11 下由实现定义或可发信号；核对 CI 当前 MSVC/GCC/Clang 版本契约并测 UINT64_MAX 边界。 */
TZrBool type_inference_bitwise_identity_expression_bitwise_not_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    const SZrBinaryExpression *binary;
    const SZrAstNode *argument;
    TZrInt64 countMinValue;
    TZrInt64 countMaxValue;
    TZrInt64 operandMinValue;
    TZrInt64 operandMaxValue;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    expression = type_inference_bitwise_identity_skip_unary_plus_identity(expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    argument = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            unary->argument);
    argument = type_inference_bitwise_identity_skip_unary_plus_identity(argument);
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "~") ||
        argument == ZR_NULL ||
        argument->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &argument->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        !type_inference_bitwise_identity_expression_supported_nonnegative_range(
                cs,
                binary->right,
                &countMinValue,
                &countMaxValue)) {
        return ZR_FALSE;
    }

    operandMinValue = -countMaxValue;
    operandMaxValue = -countMinValue;
    minValue = (TZrInt64)(~((TZrUInt64)operandMaxValue));
    maxValue = (TZrInt64)(~((TZrUInt64)operandMinValue));
    if (minValue < 0) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 折算直接按位取反 count 外的零减链；每次符号翻转都检查溢出。 */
static TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_evaluated_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrAstNode *currentExpression = expression;
    const SZrAstNode *leafExpression;
    const SZrBinaryExpression *binary;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    int depth;
    int negationIndex;

    depth = 0;
    while (ZR_TRUE) {
        currentExpression = type_inference_bitwise_identity_skip_unary_plus_identity(
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
        leafExpression = type_inference_bitwise_identity_skip_zero_identity_wrappers(
                cs,
                currentExpression);
        if (!type_inference_bitwise_identity_expression_bitwise_not_direct_int64_range(
                    cs,
                    leafExpression,
                    &minValue,
                    &maxValue) ||
            minValue < 0) {
            ++depth;
            continue;
        }

        for (negationIndex = 0; negationIndex <= depth; ++negationIndex) {
            if (!type_inference_bitwise_identity_supported_count_negate_range_in_place(
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

/** @brief 对零减按位取反的直接 count 形态求非负范围，负结果不作为支持值。 */
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (!type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_evaluated_range(
                cs,
                expression,
                &minValue,
                &maxValue) ||
        minValue < 0) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 在直接 count 形态外处理一元负号，并拒绝溢出或负区间。 */
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrUnaryExpression *unary;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &expression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_evaluated_range(
                cs,
                unary->argument,
                &minValue,
                &maxValue) ||
        !type_inference_bitwise_identity_supported_count_negate_range_in_place(
                &minValue,
                &maxValue) ||
        minValue < 0) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 汇总全 1 掩码 AND 计数侧的可识别形态，只输出非负闭区间。 */
TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (!((type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                   cs,
                   expression,
                   &minValue,
                   &maxValue) &&
           minValue >= 0) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_supported_nonnegative_range(
                  cs,
                  expression,
                  &minValue,
                  &maxValue))) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}

/** @brief 在 AND 任一侧识别全 1 掩码，再推导另一侧的计数范围。 */
static TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_side_bitwise_not_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "&")) {
        return ZR_FALSE;
    }

    if ((type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                 cs,
                 binary->left,
                 ZR_NULL,
                 ZR_NULL) &&
         type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
                 cs,
                 binary->right,
                 &minValue,
                 &maxValue)) ||
        (type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
                 cs,
                 binary->right,
                 ZR_NULL,
                 ZR_NULL) &&
         type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
                 cs,
                 binary->left,
                 &minValue,
                 &maxValue))) {
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

/** @brief 识别带身份包装的 0-~count 并拒绝负范围。 @note TODO: uint64_t 超出 int64_t 范围的转换在 C11 下由实现定义或可发信号；核对 CI 当前 MSVC/GCC/Clang 版本契约并测 UINT64_MAX 边界。 */
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    const SZrAstNode *rightExpression;
    TZrInt64 oldMinValue;
    TZrInt64 oldMaxValue;
    TZrInt64 minValue;
    TZrInt64 maxValue;

    expression = type_inference_bitwise_identity_skip_zero_identity_wrappers(cs, expression);
    if (expression == ZR_NULL ||
        expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    rightExpression = type_inference_bitwise_identity_supported_count_skip_zero_bitwise_identity_wrappers(
            cs,
            binary->right);
    if (!type_inference_bitwise_identity_operator_is(binary->op.op, "-") ||
        !type_inference_bitwise_identity_expression_is_exact_zero_value(
                cs,
                binary->left) ||
        rightExpression == ZR_NULL ||
        rightExpression->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }

    unary = &rightExpression->data.unaryExpression;
    if (!type_inference_bitwise_identity_operator_is(unary->op.op, "~") ||
        !((type_inference_bitwise_identity_expression_zero_wrapped_bitwise_not_direct_int64_range(
                   cs,
                   unary->argument,
                   &minValue,
                   &maxValue) &&
           minValue >= 0) ||
          type_inference_bitwise_identity_expression_supported_count_all_ones_side_bitwise_not_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_supported_nonnegative_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue) ||
          type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
                  cs,
                  unary->argument,
                  &minValue,
                  &maxValue))) {
        return ZR_FALSE;
    }

    oldMinValue = minValue;
    oldMaxValue = maxValue;
    minValue = (TZrInt64)(~((TZrUInt64)oldMaxValue));
    maxValue = (TZrInt64)(~((TZrUInt64)oldMinValue));
    if (!type_inference_bitwise_identity_supported_count_negate_range_in_place(
                &minValue,
                &maxValue) ||
        minValue < 0) {
        return ZR_FALSE;
    }

    if (outMinValue != ZR_NULL) {
        *outMinValue = minValue;
    }
    if (outMaxValue != ZR_NULL) {
        *outMaxValue = maxValue;
    }
    return ZR_TRUE;
}
