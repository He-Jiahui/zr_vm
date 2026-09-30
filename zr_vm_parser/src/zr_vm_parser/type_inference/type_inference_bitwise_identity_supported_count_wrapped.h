#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_WRAPPED_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_WRAPPED_H

#include "type_inference_internal.h"

/** @brief 将 -(0-n) 识别为 n，并复用其非负区间。@return 成功时写入各非空边界；形态不支持或数值条件不满足时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 将 -~n 按 n+1 推导。@return 成功时写入各非空边界；溢出或形态不支持时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_unary_minus_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 将 -(-n) 识别为 n，并要求内层是非负闭区间。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_double_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 将 ~(-n) 按 n-1 推导；要求内层下界大于零。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_bitwise_not_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 将 ~~n 识别为 n，并复用内层非负闭区间。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_double_bitwise_not_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 将 0-(-n) 识别为 n，并复用内层非负闭区间。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_zero_minus_unary_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 将 0-(0-n) 识别为 n；只接受完整匹配的两层零减结构。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_double_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 推导 ~(0-count) 的非负范围，并检查无符号补码换算后的边界。@return 成功时写入各非空边界；形态不支持或结果为负时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_bitwise_not_zero_minus_supported_nonnegative_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 对直接零减按位取反的 count 形态求非负范围。@return 成功时写入各非空边界；形态不支持或结果为负时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 识别直接 count 形态外的一元负号，并检查翻转溢出及非负约束。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_unary_minus_zero_minus_bitwise_not_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 识别带身份包装的 0-~count，并检查 64 位换算结果。@return 成功时写入各非空边界；形态不支持或范围不安全时返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_zero_minus_bitwise_not_wrapped_direct_supported_count_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);
/** @brief 汇总全 1 掩码 AND 的 count 侧识别器。@return 成功时写入各非空边界并返回 ZR_TRUE，否则返回 ZR_FALSE。 */
TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_mask_count_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif
