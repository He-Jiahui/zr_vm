#include "type_inference_loop_assignment_prefix_target_guard.h"
#include "type_inference_loop_assignment_self_dependency.h"
#include "type_inference_loop_assignment_sequence.h"
#include "zr_vm_common/zr_type_conf.h"

static SZrTypeInferenceLoopAssignmentStep *prefix_target_guard_plan_step_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->steps.length) {
        return ZR_NULL;
    }
    return (SZrTypeInferenceLoopAssignmentStep *)ZrCore_Array_Get((SZrArray *)&plan->steps, index);
}

static SZrString *prefix_target_guard_plan_target_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    SZrString **target;

    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->targetNames.length) {
        return ZR_NULL;
    }

    target = (SZrString **)ZrCore_Array_Get((SZrArray *)&plan->targetNames, index);
    return target != ZR_NULL ? *target : ZR_NULL;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderPlanContains(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name) {
    TZrSize index;

    if (plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *target = prefix_target_guard_plan_target_at(plan, index);
        if (target != ZR_NULL && ZrCore_String_Equal(target, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 *outDeltaMin,
        TZrInt64 *outDeltaMax) {
    TZrSize sequenceIndex;

    if (plan == ZR_NULL ||
        !plan->isInitialized ||
        targetName == ZR_NULL ||
        readerIndex >= plan->steps.length) {
        return ZR_FALSE;
    }

    for (sequenceIndex = readerIndex + 1u; sequenceIndex < plan->steps.length; sequenceIndex++) {
        SZrTypeInferenceLoopAssignmentStep *candidate =
                prefix_target_guard_plan_step_at(plan, sequenceIndex);

        if (candidate == ZR_NULL) {
            return ZR_FALSE;
        }
        if (candidate->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
            candidate->name != ZR_NULL &&
            ZrCore_String_Equal(candidate->name, targetName) &&
            (candidate->hasSelfDependentDelta || candidate->resolveSelfDependentDeltaOnReplay)) {
            TZrSize prefixIndex;

            if (!candidate->hasSelfDependentDelta || candidate->resolveSelfDependentDeltaOnReplay) {
                return ZR_FALSE;
            }
            for (prefixIndex = readerIndex; prefixIndex < sequenceIndex; prefixIndex++) {
                SZrTypeInferenceLoopAssignmentStep *prefixStep =
                        prefix_target_guard_plan_step_at(plan, prefixIndex);

                if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
                            prefixStep,
                            targetName,
                            ZR_TRUE) ||
                    (prefixStep->name != ZR_NULL &&
                     candidate->right != ZR_NULL &&
                     ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                             candidate->right,
                             prefixStep->name))) {
                    return ZR_FALSE;
                }
            }
            if (outDeltaMin != ZR_NULL) {
                *outDeltaMin = candidate->selfDependentDeltaMin;
            }
            if (outDeltaMax != ZR_NULL) {
                *outDeltaMax = candidate->selfDependentDeltaMax;
            }
            return ZR_TRUE;
        }
        if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
                    candidate,
                    targetName,
                    ZR_TRUE)) {
            return ZR_FALSE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetSubtractOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offset) {
    const SZrTypeBinding *targetBinding;
    TZrInt64 deltaMin = 0;
    TZrInt64 deltaMax = 0;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        targetName == ZR_NULL ||
        offset < 0 ||
        !ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
                plan,
                readerIndex,
                targetName,
                &deltaMin,
                &deltaMax) ||
        deltaMin < 0 ||
        deltaMax <= 0) {
        return ZR_FALSE;
    }

    targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, targetName);
    return targetBinding != ZR_NULL &&
           targetBinding->type.baseType == ZR_VALUE_TYPE_INT64 &&
           targetBinding->type.hasRangeConstraint &&
           targetBinding->type.minValue >= ZR_TYPE_RANGE_INT64_MIN + offset;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offset) {
    const SZrTypeBinding *targetBinding;
    TZrInt64 deltaMin = 0;
    TZrInt64 deltaMax = 0;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        targetName == ZR_NULL ||
        offset < 0 ||
        !ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
                plan,
                readerIndex,
                targetName,
                &deltaMin,
                &deltaMax) ||
        deltaMin >= 0 ||
        deltaMax > 0) {
        return ZR_FALSE;
    }

    targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, targetName);
    return targetBinding != ZR_NULL &&
           targetBinding->type.baseType == ZR_VALUE_TYPE_INT64 &&
           targetBinding->type.hasRangeConstraint &&
           targetBinding->type.maxValue <= ZR_TYPE_RANGE_INT64_MAX - offset;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddNegativeOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offsetMin,
        TZrInt64 offsetMax) {
    const SZrTypeBinding *targetBinding;
    TZrInt64 deltaMin = 0;
    TZrInt64 deltaMax = 0;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        targetName == ZR_NULL ||
        offsetMin > offsetMax ||
        offsetMax >= 0 ||
        offsetMin == ZR_TYPE_RANGE_INT64_MIN ||
        !ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
                plan,
                readerIndex,
                targetName,
                &deltaMin,
                &deltaMax) ||
        deltaMin < 0 ||
        deltaMax <= 0) {
        return ZR_FALSE;
    }

    targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, targetName);
    return targetBinding != ZR_NULL &&
           targetBinding->type.baseType == ZR_VALUE_TYPE_INT64 &&
           targetBinding->type.hasRangeConstraint &&
           targetBinding->type.minValue >= ZR_TYPE_RANGE_INT64_MIN - offsetMin;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddZeroInclusiveNegativeOffsetIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        SZrString *targetName,
        TZrInt64 offsetMin,
        TZrInt64 offsetMax) {
    const SZrTypeBinding *targetBinding;
    TZrInt64 deltaMin = 0;
    TZrInt64 deltaMax = 0;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        targetName == ZR_NULL ||
        offsetMin > offsetMax ||
        offsetMin >= 0 ||
        offsetMax != 0 ||
        offsetMin == ZR_TYPE_RANGE_INT64_MIN ||
        !ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
                plan,
                readerIndex,
                targetName,
                &deltaMin,
                &deltaMax) ||
        deltaMin < 0 ||
        deltaMax <= 0) {
        return ZR_FALSE;
    }

    targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, targetName);
    return targetBinding != ZR_NULL &&
           targetBinding->type.baseType == ZR_VALUE_TYPE_INT64 &&
           targetBinding->type.hasRangeConstraint &&
           targetBinding->type.minValue >= ZR_TYPE_RANGE_INT64_MIN - offsetMin;
}
