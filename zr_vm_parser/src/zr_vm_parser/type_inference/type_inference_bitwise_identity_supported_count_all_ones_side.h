#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_ALL_ONES_SIDE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_ALL_ONES_SIDE_H

#include "type_inference_internal.h"

/**
 * @brief 判断 AST 是否表示精确的有符号 64 位全 1 掩码（-1）。
 * @return 成功时可分别写出非空边界指针；不支持的形态返回 ZR_FALSE。
 */
TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif
