#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_H

#include "type_inference_loop_assignment_join_internal.h"

typedef struct SZrTypeInferenceLoopAssignmentSequenceDeltaTracker {
    TZrInt64 deltaMin;
    TZrInt64 deltaMax;
    TZrBool canTrackSymbolicDelta;
    SZrAstNode *symbolicDeltaExpression;
    TZrInt64 symbolicDeltaBalance;
} SZrTypeInferenceLoopAssignmentSequenceDeltaTracker;

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepStarts(
        const SZrTypeInferenceLoopAssignmentStep *step);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepDeltaForName(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *name,
        TZrInt64 *outDeltaMin,
        TZrInt64 *outDeltaMax);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        TZrBool allowTargetRead);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepReadsName(
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName);

void ZrParser_TypeInferenceLoopAssignment_SequenceDeltaTrackerInit(
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceAccumulateStep(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        TZrSize stepIndex,
        SZrString *sequenceName,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker,
        TZrBool *outIsSequenceStep);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceAllowsTargetReadingInterleaves(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        SZrString *sequenceName,
        TZrBool *outAllowsTargetReading,
        TZrInt64 *outDeltaMin,
        TZrInt64 *outDeltaMax);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStoreIntermediate(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        const SZrInferredType *sourceType,
        TZrInt64 deltaMin,
        TZrInt64 deltaMax,
        TZrInt64 sequenceDeltaMin,
        TZrInt64 sequenceDeltaMax);

#endif
