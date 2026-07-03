#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_PREFIX_READER_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_PREFIX_READER_H

#include "type_inference_loop_assignment_join_internal.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderRhsIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        TZrSize stepIndex);

#endif
