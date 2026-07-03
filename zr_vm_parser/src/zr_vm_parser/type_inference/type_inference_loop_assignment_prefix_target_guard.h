#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_PREFIX_TARGET_GUARD_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_PREFIX_TARGET_GUARD_H

#include "type_inference_loop_assignment_join_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderPlanContains(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name);

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 *outDeltaMin,
        TZrInt64 *outDeltaMax);

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetSubtractOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offset);

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offset);

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddNegativeOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offsetMin,
        TZrInt64 offsetMax);

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddZeroInclusiveNegativeOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offsetMin,
        TZrInt64 offsetMax);

#endif
