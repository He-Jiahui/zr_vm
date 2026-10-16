#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_COEFFICIENT_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_COEFFICIENT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrInt64 *outMin,
        TZrInt64 *outMax);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(
        SZrAstNode *node);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(
        SZrAstNode *node);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsSupportedCrossingCoefficient(
        SZrCompilerState *cs,
        SZrAstNode *node);

#endif
