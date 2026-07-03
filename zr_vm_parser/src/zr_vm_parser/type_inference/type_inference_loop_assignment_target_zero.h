#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_TARGET_ZERO_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_TARGET_ZERO_H

#include "zr_vm_core/array.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/type_inference.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingRange(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax);
TZrBool ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
        SZrAstNode *deltaNode,
        SZrString *targetName,
        const SZrArray *targetNames);

#endif
