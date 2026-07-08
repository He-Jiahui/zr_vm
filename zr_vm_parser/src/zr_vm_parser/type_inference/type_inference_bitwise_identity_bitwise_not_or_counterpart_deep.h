#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_BITWISE_NOT_OR_COUNTERPART_DEEP_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_BITWISE_NOT_OR_COUNTERPART_DEEP_H

#include "type_inference_bitwise_identity_direct_range.h"

TZrBool type_inference_bitwise_identity_expression_bitwise_not_or_deep_counterpart_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif