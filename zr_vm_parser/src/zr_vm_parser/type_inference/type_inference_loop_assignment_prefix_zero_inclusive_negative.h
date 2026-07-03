#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_PREFIX_ZERO_INCLUSIVE_NEGATIVE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_PREFIX_ZERO_INCLUSIVE_NEGATIVE_H

#include "type_inference_loop_assignment_join_internal.h"
#include "zr_vm_parser/compiler.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
        SZrCompilerState *cs,
        const SZrAstNode *expression,
        TZrInt64 *outMinValue,
        TZrInt64 *outMaxValue);

#endif
