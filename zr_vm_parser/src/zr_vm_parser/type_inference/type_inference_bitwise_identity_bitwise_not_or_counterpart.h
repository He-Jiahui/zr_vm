#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_BITWISE_NOT_OR_COUNTERPART_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BITWISE_IDENTITY_BITWISE_NOT_OR_COUNTERPART_H

#include "type_inference_bitwise_identity_direct_range.h"

TZrBool type_inference_bitwise_identity_bitwise_not_negate_range_in_place(
        TZrInt64 *minValue,
        TZrInt64 *maxValue);
const SZrAstNode *type_inference_bitwise_identity_bitwise_not_signed_all_ones_operand_skip_zero_bitwise_identity_wrappers(
        SZrCompilerState *cs,
        const SZrAstNode *expression);
TZrBool type_inference_bitwise_identity_expression_bitwise_not_or_counterpart_leaf_int64_range(
        SZrCompilerState *cs,
        const SZrAstNode *expression);

#endif
