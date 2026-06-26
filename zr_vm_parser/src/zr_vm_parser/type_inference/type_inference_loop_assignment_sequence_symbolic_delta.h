#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_DELTA_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_DELTA_H

#include "type_inference_loop_assignment_sequence.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaUpdate(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCurrentRange(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker,
        TZrInt64 *outMin,
        TZrInt64 *outMax);

#endif
