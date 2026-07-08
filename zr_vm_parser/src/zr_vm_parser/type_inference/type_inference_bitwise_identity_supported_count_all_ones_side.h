#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_ALL_ONES_SIDE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_SUPPORTED_COUNT_ALL_ONES_SIDE_H

#include "type_inference_internal.h"

TZrBool type_inference_bitwise_identity_expression_supported_count_all_ones_side_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif
