#include "type_inference_loop_assignment_sequence.h"

#include "type_inference_loop_assignment_nested_if.h"
#include "type_inference_loop_assignment_nested_loop.h"
#include "type_inference_loop_assignment_self_dependency.h"
#include "type_inference_loop_assignment_sequence_symbolic_delta.h"

#include "zr_vm_common/zr_parser_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

static SZrTypeInferenceLoopAssignmentStep *loop_assignment_sequence_plan_step_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->steps.length) {
        return ZR_NULL;
    }
    return (SZrTypeInferenceLoopAssignmentStep *)ZrCore_Array_Get((SZrArray *)&plan->steps, index);
}

static SZrString *loop_assignment_sequence_plan_target_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    SZrString **target;

    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->targetNames.length) {
        return ZR_NULL;
    }

    target = (SZrString **)ZrCore_Array_Get((SZrArray *)&plan->targetNames, index);
    return target != ZR_NULL ? *target : ZR_NULL;
}

static TZrBool loop_assignment_sequence_int64_add(TZrInt64 left,
                                                  TZrInt64 right,
                                                  TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((right > 0 && left > ZR_TYPE_RANGE_INT64_MAX - right) ||
        (right < 0 && left < ZR_TYPE_RANGE_INT64_MIN - right)) {
        return ZR_FALSE;
    }

    *outValue = left + right;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_int64_range_add(TZrInt64 leftMin,
                                                        TZrInt64 leftMax,
                                                        TZrInt64 rightMin,
                                                        TZrInt64 rightMax,
                                                        TZrInt64 *outMin,
                                                        TZrInt64 *outMax) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (outMin == ZR_NULL || outMax == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!loop_assignment_sequence_int64_add(leftMin, rightMin, &minValue) ||
        !loop_assignment_sequence_int64_add(leftMax, rightMax, &maxValue)) {
        return ZR_FALSE;
    }

    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_delta_is_supported(TZrInt64 deltaMin,
                                                           TZrInt64 deltaMax) {
    return (deltaMin == 0 && deltaMax == 0) ||
           (deltaMin >= 0 && deltaMax > 0) ||
           (deltaMin < 0 && deltaMax <= 0) ||
           (deltaMin < 0 && deltaMax > 0);
}

static TZrBool loop_assignment_sequence_delta_is_sign_crossing(TZrInt64 deltaMin,
                                                               TZrInt64 deltaMax) {
    return deltaMin < 0 && deltaMax > 0;
}

static TZrBool loop_assignment_sequence_delta_contains(TZrInt64 outerMin,
                                                       TZrInt64 outerMax,
                                                       TZrInt64 innerMin,
                                                       TZrInt64 innerMax) {
    return outerMin <= innerMin && innerMax <= outerMax;
}

static TZrBool loop_assignment_sequence_symbolic_delta_range_is_preferred(
        TZrBool hasCoefficientRange,
        TZrInt64 currentMin,
        TZrInt64 currentMax,
        TZrInt64 symbolicMin,
        TZrInt64 symbolicMax) {
    if (!loop_assignment_sequence_delta_is_supported(symbolicMin, symbolicMax)) {
        return ZR_FALSE;
    }
    if (!hasCoefficientRange) {
        return ZR_TRUE;
    }
    if (loop_assignment_sequence_delta_contains(
                currentMin,
                currentMax,
                symbolicMin,
                symbolicMax)) {
        return ZR_TRUE;
    }
    return loop_assignment_sequence_delta_is_sign_crossing(currentMin, currentMax) &&
           !loop_assignment_sequence_delta_is_sign_crossing(symbolicMin, symbolicMax);
}

static TZrBool loop_assignment_sequence_symbolic_delta_has_coefficient_range(
        const SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker) {
    TZrSize index;

    if (tracker == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < tracker->symbolicResidualTermCount; index++) {
        if (tracker->symbolicResidualTerms[index].hasCoefficientRange) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_sequence_resolved_target_reading_delta_is_supported(
        TZrInt64 deltaMin,
        TZrInt64 deltaMax) {
    return (deltaMin == 0 && deltaMax == 0) ||
           (deltaMin < 0 && deltaMax <= 0);
}

static TZrBool loop_assignment_sequence_source_plus_delta_type(
        SZrCompilerState *cs,
        const SZrInferredType *sourceType,
        TZrInt64 deltaMin,
        TZrInt64 deltaMax,
        SZrInferredType *outType) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        sourceType == ZR_NULL ||
        outType == ZR_NULL ||
        sourceType->baseType != ZR_VALUE_TYPE_INT64 ||
        !sourceType->hasRangeConstraint ||
        (sourceType->minValue != ZR_TYPE_RANGE_INT64_MIN &&
         !loop_assignment_sequence_int64_add(sourceType->minValue, deltaMin, &minValue)) ||
        (sourceType->maxValue != ZR_TYPE_RANGE_INT64_MAX &&
         !loop_assignment_sequence_int64_add(sourceType->maxValue, deltaMax, &maxValue))) {
        return ZR_FALSE;
    }

    if (sourceType->minValue == ZR_TYPE_RANGE_INT64_MIN) {
        minValue = ZR_TYPE_RANGE_INT64_MIN;
    }
    if (sourceType->maxValue == ZR_TYPE_RANGE_INT64_MAX) {
        maxValue = ZR_TYPE_RANGE_INT64_MAX;
    }

    ZrParser_InferredType_Copy(cs->state, outType, sourceType);
    ZrParser_NumericRangeSegments_Free(cs->state,
                                       &outType->rangeSegmentCount,
                                       outType->rangeSegments,
                                       &outType->rangeExtraSegments);
    outType->baseType = ZR_VALUE_TYPE_INT64;
    outType->hasRangeConstraint = ZR_TRUE;
    outType->minValue = minValue;
    outType->maxValue = maxValue;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_join_int64_envelope(
        SZrCompilerState *cs,
        const SZrInferredType *leftType,
        const SZrInferredType *rightType,
        SZrInferredType *outType) {
    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        outType == ZR_NULL ||
        leftType->baseType != ZR_VALUE_TYPE_INT64 ||
        rightType->baseType != ZR_VALUE_TYPE_INT64 ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(cs->state, outType, leftType);
    ZrParser_NumericRangeSegments_Free(cs->state,
                                       &outType->rangeSegmentCount,
                                       outType->rangeSegments,
                                       &outType->rangeExtraSegments);
    outType->baseType = ZR_VALUE_TYPE_INT64;
    outType->hasRangeConstraint = ZR_TRUE;
    outType->minValue =
            leftType->minValue < rightType->minValue ? leftType->minValue : rightType->minValue;
    outType->maxValue =
            leftType->maxValue > rightType->maxValue ? leftType->maxValue : rightType->maxValue;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepStarts(
        const SZrTypeInferenceLoopAssignmentStep *step) {
    return step != ZR_NULL &&
           step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
           step->name != ZR_NULL &&
           (step->hasSelfDependentDelta || step->resolveSelfDependentDeltaOnReplay);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepDeltaForName(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *name,
        TZrInt64 *outDeltaMin,
        TZrInt64 *outDeltaMax) {
    TZrInt64 deltaMin;
    TZrInt64 deltaMax;

    if (cs == ZR_NULL ||
        step == ZR_NULL ||
        name == ZR_NULL ||
        outDeltaMin == ZR_NULL ||
        outDeltaMax == ZR_NULL ||
        step->kind != ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT ||
        step->name == ZR_NULL ||
        !ZrCore_String_Equal(step->name, name) ||
        (!step->hasSelfDependentDelta && !step->resolveSelfDependentDeltaOnReplay)) {
        return ZR_FALSE;
    }

    deltaMin = step->selfDependentDeltaMin;
    deltaMax = step->selfDependentDeltaMax;
    if (step->resolveSelfDependentDeltaOnReplay &&
        !ZrParser_TypeInferenceLoopAssignment_TrySelfDependentDelta(
                cs,
                step->assignment,
                step->name,
                &deltaMin,
                &deltaMax)) {
        return ZR_FALSE;
    }
    if (!loop_assignment_sequence_delta_is_supported(deltaMin, deltaMax)) {
        return ZR_FALSE;
    }

    *outDeltaMin = deltaMin;
    *outDeltaMax = deltaMax;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        TZrBool allowTargetRead) {
    TZrBool readsSequenceName;

    if (step == ZR_NULL ||
        sequenceName == ZR_NULL ||
        step->kind != ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT ||
        step->name == ZR_NULL ||
        step->right == ZR_NULL ||
        ZrCore_String_Equal(step->name, sequenceName) ||
        step->hasSelfDependentDelta ||
        step->resolveSelfDependentDeltaOnReplay) {
        return ZR_FALSE;
    }

    readsSequenceName = ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
            step->right,
            sequenceName);
    return !readsSequenceName || allowTargetRead;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStepReadsName(
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName) {
    return step != ZR_NULL &&
           sequenceName != ZR_NULL &&
           step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
           step->name != ZR_NULL &&
           step->right != ZR_NULL &&
           !ZrCore_String_Equal(step->name, sequenceName) &&
           !step->hasSelfDependentDelta &&
           !step->resolveSelfDependentDeltaOnReplay &&
           ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(step->right, sequenceName);
}

void ZrParser_TypeInferenceLoopAssignment_SequenceDeltaTrackerInit(
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker) {
    if (tracker == ZR_NULL) {
        return;
    }

    tracker->deltaMin = 0;
    tracker->deltaMax = 0;
    tracker->canTrackSymbolicDelta = ZR_TRUE;
    tracker->symbolicDeltaExpression = ZR_NULL;
    tracker->symbolicDeltaBalance = 0;
    tracker->hasSymbolicConstantResidualDelta = ZR_FALSE;
    tracker->symbolicConstantResidualDelta = 0;
    tracker->symbolicResidualTermCount = 0;
    tracker->symbolicResidualIntegerLiteralSum = 0;
}

static void loop_assignment_sequence_invalidate_symbolic_delta(
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker) {
    if (tracker == ZR_NULL) {
        return;
    }

    tracker->canTrackSymbolicDelta = ZR_FALSE;
    tracker->deltaMin = -1;
    tracker->deltaMax = 1;
}

static TZrBool loop_assignment_sequence_step_writes_expression_dependency(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        SZrAstNode *expression);

static TZrBool loop_assignment_sequence_prior_interleave_writes_delta_dependency(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        TZrSize currentIndex,
        SZrString *sequenceName,
        const SZrTypeInferenceLoopAssignmentStep *deltaStep) {
    TZrSize index;

    if (plan == ZR_NULL ||
        sequenceName == ZR_NULL ||
        deltaStep == ZR_NULL ||
        deltaStep->right == ZR_NULL ||
        !plan->isInitialized ||
        currentIndex > plan->steps.length) {
        return ZR_TRUE;
    }

    for (index = startIndex; index < currentIndex; index++) {
        SZrTypeInferenceLoopAssignmentStep *prior =
                loop_assignment_sequence_plan_step_at(plan, index);

        if (loop_assignment_sequence_step_writes_expression_dependency(
                    plan,
                    prior,
                    sequenceName,
                    deltaStep->right)) {
            return ZR_TRUE;
        }
        if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
                    prior,
                    sequenceName,
                    ZR_TRUE)) {
            continue;
        }
        if (prior->name != ZR_NULL &&
            ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    deltaStep->right,
                    prior->name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_sequence_step_is_delta_for_name(
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName) {
    return step != ZR_NULL &&
           sequenceName != ZR_NULL &&
           step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
           step->name != ZR_NULL &&
           step->right != ZR_NULL &&
           ZrCore_String_Equal(step->name, sequenceName) &&
           (step->hasSelfDependentDelta || step->resolveSelfDependentDeltaOnReplay);
}

static TZrBool loop_assignment_sequence_step_assigns_dependency_name(
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        SZrString *dependencyName) {
    if (step == ZR_NULL ||
        sequenceName == ZR_NULL ||
        dependencyName == ZR_NULL ||
        ZrCore_String_Equal(dependencyName, sequenceName)) {
        return ZR_FALSE;
    }

    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT) {
        return step->name != ZR_NULL && ZrCore_String_Equal(step->name, dependencyName);
    }
    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF) {
        return ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(
                step->assignment,
                dependencyName);
    }
    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP) {
        return ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                step->assignment,
                dependencyName);
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_sequence_step_writes_expression_dependency(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        SZrAstNode *expression) {
    TZrSize index;

    if (step == ZR_NULL || sequenceName == ZR_NULL || expression == ZR_NULL) {
        return ZR_FALSE;
    }

    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
        step->name != ZR_NULL &&
        !ZrCore_String_Equal(step->name, sequenceName) &&
        ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(expression, step->name)) {
        return ZR_TRUE;
    }

    if (plan == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }
    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *dependencyName = loop_assignment_sequence_plan_target_at(plan, index);

        if (dependencyName != ZR_NULL &&
            !ZrCore_String_Equal(dependencyName, sequenceName) &&
            ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    expression,
                    dependencyName) &&
            loop_assignment_sequence_step_assigns_dependency_name(
                    step,
                    sequenceName,
                    dependencyName)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_sequence_prior_interleave_writes_tracked_delta_dependency(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        TZrSize currentIndex,
        SZrString *sequenceName) {
    TZrSize writerIndex;

    if (plan == ZR_NULL ||
        sequenceName == ZR_NULL ||
        !plan->isInitialized ||
        currentIndex > plan->steps.length) {
        return ZR_TRUE;
    }

    for (writerIndex = startIndex; writerIndex < currentIndex; writerIndex++) {
        TZrSize deltaIndex;
        SZrTypeInferenceLoopAssignmentStep *writer =
                loop_assignment_sequence_plan_step_at(plan, writerIndex);

        if (writer == ZR_NULL) {
            continue;
        }

        for (deltaIndex = startIndex; deltaIndex < writerIndex; deltaIndex++) {
            SZrTypeInferenceLoopAssignmentStep *deltaStep =
                    loop_assignment_sequence_plan_step_at(plan, deltaIndex);

            if (loop_assignment_sequence_step_is_delta_for_name(deltaStep, sequenceName) &&
                loop_assignment_sequence_step_writes_expression_dependency(
                        plan,
                        writer,
                        sequenceName,
                        deltaStep->right)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceFutureWritesExpressionDependency(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize currentIndex,
        SZrString *sequenceName,
        SZrAstNode *expression) {
    TZrSize index;

    if (plan == ZR_NULL ||
        sequenceName == ZR_NULL ||
        expression == ZR_NULL ||
        !plan->isInitialized ||
        currentIndex >= plan->steps.length) {
        return ZR_FALSE;
    }

    for (index = currentIndex + 1; index < plan->steps.length; index++) {
        if (loop_assignment_sequence_step_writes_expression_dependency(
                    plan,
                    loop_assignment_sequence_plan_step_at(plan, index),
                    sequenceName,
                    expression)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_sequence_future_interleave_writes_delta_dependency(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize currentIndex,
        SZrString *sequenceName,
        const SZrTypeInferenceLoopAssignmentStep *deltaStep) {
    TZrSize index;

    if (plan == ZR_NULL ||
        sequenceName == ZR_NULL ||
        deltaStep == ZR_NULL ||
        deltaStep->right == ZR_NULL ||
        !plan->isInitialized ||
        currentIndex >= plan->steps.length) {
        return ZR_TRUE;
    }

    for (index = currentIndex + 1; index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *future =
                loop_assignment_sequence_plan_step_at(plan, index);

        if (loop_assignment_sequence_step_is_delta_for_name(future, sequenceName)) {
            break;
        }
        if (loop_assignment_sequence_step_writes_expression_dependency(
                    plan,
                    future,
                    sequenceName,
                    deltaStep->right)) {
            return ZR_TRUE;
        }
        if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
                    future,
                    sequenceName,
                    ZR_TRUE)) {
            break;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceAccumulateStep(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        TZrSize stepIndex,
        SZrString *sequenceName,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker,
        TZrBool *outIsSequenceStep) {
    TZrInt64 stepDeltaMin = 0;
    TZrInt64 stepDeltaMax = 0;
    TZrInt64 nextDeltaMin;
    TZrInt64 nextDeltaMax;

    if (outIsSequenceStep != ZR_NULL) {
        *outIsSequenceStep = ZR_FALSE;
    }
    if (tracker == ZR_NULL || outIsSequenceStep == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepDeltaForName(
                cs,
                step,
                sequenceName,
                &stepDeltaMin,
                &stepDeltaMax)) {
        return ZR_TRUE;
    }
    if (!loop_assignment_sequence_int64_range_add(tracker->deltaMin,
                                                  tracker->deltaMax,
                                                  stepDeltaMin,
                                                  stepDeltaMax,
                                                  &nextDeltaMin,
                                                  &nextDeltaMax)) {
        return ZR_FALSE;
    }

    tracker->deltaMin = nextDeltaMin;
    tracker->deltaMax = nextDeltaMax;
    if (step->resolveSelfDependentDeltaOnReplay) {
        if (loop_assignment_sequence_prior_interleave_writes_delta_dependency(
                    plan,
                    startIndex,
                    stepIndex,
                    sequenceName,
                    step)) {
            return ZR_FALSE;
        }
        tracker->canTrackSymbolicDelta = ZR_FALSE;
    } else if (tracker->canTrackSymbolicDelta &&
               loop_assignment_sequence_prior_interleave_writes_delta_dependency(
                       plan,
                       startIndex,
                       stepIndex,
                       sequenceName,
                       step)) {
        tracker->canTrackSymbolicDelta = ZR_FALSE;
    } else if (tracker->canTrackSymbolicDelta &&
               loop_assignment_sequence_prior_interleave_writes_tracked_delta_dependency(
                       plan,
                       startIndex,
                       stepIndex,
                       sequenceName)) {
        loop_assignment_sequence_invalidate_symbolic_delta(tracker);
    } else if (loop_assignment_sequence_future_interleave_writes_delta_dependency(
                       plan,
                       stepIndex,
                       sequenceName,
                       step)) {
        loop_assignment_sequence_invalidate_symbolic_delta(tracker);
    }
    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaUpdate(
                cs,
                step,
                sequenceName,
                tracker)) {
        return ZR_FALSE;
    }
    if (tracker->hasSymbolicConstantResidualDelta) {
        tracker->deltaMin = tracker->symbolicConstantResidualDelta;
        tracker->deltaMax = tracker->symbolicConstantResidualDelta;
        tracker->hasSymbolicConstantResidualDelta = ZR_FALSE;
    } else if (ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCurrentRange(
                       cs,
                       tracker,
                       &nextDeltaMin,
                       &nextDeltaMax) &&
               loop_assignment_sequence_symbolic_delta_range_is_preferred(
                       loop_assignment_sequence_symbolic_delta_has_coefficient_range(tracker),
                       tracker->deltaMin,
                       tracker->deltaMax,
                       nextDeltaMin,
                       nextDeltaMax)) {
        tracker->deltaMin = nextDeltaMin;
        tracker->deltaMax = nextDeltaMax;
    }

    *outIsSequenceStep = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceAllowsTargetReadingInterleaves(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        SZrString *sequenceName,
        TZrBool *outAllowsTargetReading,
        TZrInt64 *outDeltaMin,
        TZrInt64 *outDeltaMax) {
    SZrTypeInferenceLoopAssignmentSequenceDeltaTracker tracker;
    TZrSize index;
    TZrBool sawTargetReadingInterleave = ZR_FALSE;
    TZrBool sawReplayResolvedDelta = ZR_FALSE;

    if (outAllowsTargetReading != ZR_NULL) {
        *outAllowsTargetReading = ZR_FALSE;
    }
    if (outDeltaMin != ZR_NULL) {
        *outDeltaMin = 0;
    }
    if (outDeltaMax != ZR_NULL) {
        *outDeltaMax = 0;
    }
    if (cs == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized ||
        sequenceName == ZR_NULL ||
        outAllowsTargetReading == ZR_NULL ||
        outDeltaMin == ZR_NULL ||
        outDeltaMax == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_TypeInferenceLoopAssignment_SequenceDeltaTrackerInit(&tracker);
    for (index = startIndex; index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *step =
                loop_assignment_sequence_plan_step_at(plan, index);
        TZrBool isSequenceStep = ZR_FALSE;

        if (!ZrParser_TypeInferenceLoopAssignment_SequenceAccumulateStep(
                    cs,
                    plan,
                    startIndex,
                    index,
                    sequenceName,
                    step,
                    &tracker,
                    &isSequenceStep)) {
            return ZR_FALSE;
        }
        if (isSequenceStep) {
            if (step->resolveSelfDependentDeltaOnReplay) {
                sawReplayResolvedDelta = ZR_TRUE;
            }
            continue;
        }
        if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
                    step,
                    sequenceName,
                    ZR_TRUE)) {
            break;
        }
        if (ZrParser_TypeInferenceLoopAssignment_SequenceStepReadsName(step, sequenceName)) {
            sawTargetReadingInterleave = ZR_TRUE;
        }
    }

    *outAllowsTargetReading =
            sawTargetReadingInterleave &&
            (!sawReplayResolvedDelta ||
             loop_assignment_sequence_resolved_target_reading_delta_is_supported(
                     tracker.deltaMin,
                     tracker.deltaMax)) &&
            loop_assignment_sequence_delta_is_supported(tracker.deltaMin, tracker.deltaMax);
    *outDeltaMin = tracker.deltaMin;
    *outDeltaMax = tracker.deltaMax;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceStoreIntermediate(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        const SZrInferredType *sourceType,
        TZrInt64 deltaMin,
        TZrInt64 deltaMax,
        TZrInt64 sequenceDeltaMin,
        TZrInt64 sequenceDeltaMax) {
    SZrInferredType intermediateType;
    SZrInferredType sequenceFinalType;
    SZrInferredType futureIntermediateType;
    SZrInferredType joinedIntermediateType;
    TZrBool initializedSequenceFinal = ZR_FALSE;
    TZrBool initializedFutureIntermediate = ZR_FALSE;
    TZrBool initializedJoinedIntermediate = ZR_FALSE;
    TZrBool success;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        sequenceName == ZR_NULL ||
        sourceType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &intermediateType, ZR_VALUE_TYPE_OBJECT);
    success = loop_assignment_sequence_source_plus_delta_type(
            cs,
            sourceType,
            deltaMin,
            deltaMax,
            &intermediateType);
    if (success && (sequenceDeltaMin != 0 || sequenceDeltaMax != 0)) {
        ZrParser_InferredType_Init(cs->state, &sequenceFinalType, ZR_VALUE_TYPE_OBJECT);
        initializedSequenceFinal = ZR_TRUE;
        ZrParser_InferredType_Init(cs->state, &futureIntermediateType, ZR_VALUE_TYPE_OBJECT);
        initializedFutureIntermediate = ZR_TRUE;
        ZrParser_InferredType_Init(cs->state, &joinedIntermediateType, ZR_VALUE_TYPE_OBJECT);
        initializedJoinedIntermediate = ZR_TRUE;
        success = ZrParser_TypeInferenceLoopAssignment_WidenSelfDependentType(
                          cs,
                          sourceType,
                          sequenceDeltaMin,
                          sequenceDeltaMax,
                          &sequenceFinalType) &&
                  loop_assignment_sequence_source_plus_delta_type(
                          cs,
                          &sequenceFinalType,
                          deltaMin,
                          deltaMax,
                          &futureIntermediateType) &&
                  loop_assignment_sequence_join_int64_envelope(
                          cs,
                          &intermediateType,
                          &futureIntermediateType,
                          &joinedIntermediateType) &&
                  ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(
                          cs,
                          sequenceName,
                          &joinedIntermediateType);
    } else if (success) {
        success = ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(
                cs,
                sequenceName,
                &intermediateType);
    }

    if (initializedJoinedIntermediate) {
        ZrParser_InferredType_Free(cs->state, &joinedIntermediateType);
    }
    if (initializedFutureIntermediate) {
        ZrParser_InferredType_Free(cs->state, &futureIntermediateType);
    }
    if (initializedSequenceFinal) {
        ZrParser_InferredType_Free(cs->state, &sequenceFinalType);
    }
    ZrParser_InferredType_Free(cs->state, &intermediateType);
    return success;
}
