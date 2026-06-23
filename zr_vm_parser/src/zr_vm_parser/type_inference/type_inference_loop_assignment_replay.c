#include "type_inference_loop_assignment_join_internal.h"
#include "type_inference_loop_assignment_nested_loop.h"
#include "type_inference_loop_assignment_sequence.h"
#include "type_inference_loop_assignment_self_dependency.h"

#include "zr_vm_common/zr_parser_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

#include <string.h>

static SZrTypeInferenceLoopAssignmentStep *loop_assignment_replay_plan_step_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->steps.length) {
        return ZR_NULL;
    }
    return (SZrTypeInferenceLoopAssignmentStep *)ZrCore_Array_Get((SZrArray *)&plan->steps, index);
}

static SZrString *loop_assignment_replay_plan_target_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    SZrString **target;

    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->targetNames.length) {
        return ZR_NULL;
    }

    target = (SZrString **)ZrCore_Array_Get((SZrArray *)&plan->targetNames, index);
    return target != ZR_NULL ? *target : ZR_NULL;
}

static TZrBool loop_assignment_replay_numeric_range(const SZrInferredType *type,
                                                    TZrInt64 *outMin,
                                                    TZrInt64 *outMax) {
    if (type == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_NUMBER(type->baseType) ||
        !type->hasRangeConstraint) {
        return ZR_FALSE;
    }

    *outMin = type->minValue;
    *outMax = type->maxValue;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_ResultInit(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result,
        TZrSize capacity) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      &result->bindings,
                      sizeof(SZrTypeInferenceLoopAssignmentBindingType),
                      capacity > 0 ? capacity : ZR_PARSER_INITIAL_CAPACITY_TINY);
    result->isInitialized = ZR_TRUE;
    return ZR_TRUE;
}

void ZrParser_TypeInferenceLoopAssignment_ResultFree(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result) {
    TZrSize index;

    if (state == ZR_NULL || result == ZR_NULL || !result->isInitialized) {
        return;
    }

    for (index = 0; index < result->bindings.length; index++) {
        SZrTypeInferenceLoopAssignmentBindingType *binding =
                (SZrTypeInferenceLoopAssignmentBindingType *)ZrCore_Array_Get(&result->bindings, index);
        if (binding != ZR_NULL && binding->hasType) {
            ZrParser_InferredType_Free(state, &binding->type);
            binding->hasType = ZR_FALSE;
        }
    }
    ZrCore_Array_Free(state, &result->bindings);
    result->isInitialized = ZR_FALSE;
}

static SZrTypeInferenceLoopAssignmentBindingType *loop_assignment_result_find(
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name) {
    TZrSize index;

    if (result == ZR_NULL || name == ZR_NULL || !result->isInitialized) {
        return ZR_NULL;
    }

    for (index = 0; index < result->bindings.length; index++) {
        SZrTypeInferenceLoopAssignmentBindingType *binding =
                (SZrTypeInferenceLoopAssignmentBindingType *)ZrCore_Array_Get(&result->bindings, index);
        if (binding != ZR_NULL &&
            binding->name != ZR_NULL &&
            ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }
    return ZR_NULL;
}

const SZrInferredType *ZrParser_TypeInferenceLoopAssignment_ResultFindType(
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name) {
    SZrTypeInferenceLoopAssignmentBindingType *binding =
            loop_assignment_result_find(result, name);

    return binding != ZR_NULL && binding->hasType ? &binding->type : ZR_NULL;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_ResultStoreType(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name,
        const SZrInferredType *type) {
    SZrTypeInferenceLoopAssignmentBindingType *existing;
    SZrTypeInferenceLoopAssignmentBindingType binding;

    if (state == ZR_NULL ||
        result == ZR_NULL ||
        name == ZR_NULL ||
        type == ZR_NULL ||
        !result->isInitialized) {
        return ZR_FALSE;
    }

    existing = loop_assignment_result_find(result, name);
    if (existing != ZR_NULL) {
        if (existing->hasType) {
            ZrParser_InferredType_Free(state, &existing->type);
        }
        ZrParser_InferredType_Init(state, &existing->type, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(state, &existing->type, type);
        existing->hasType = ZR_TRUE;
        return ZR_TRUE;
    }

    binding.name = name;
    ZrParser_InferredType_Init(state, &binding.type, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(state, &binding.type, type);
    binding.hasType = ZR_TRUE;
    ZrCore_Array_Push(state, &result->bindings, &binding);
    return ZR_TRUE;
}

static TZrBool loop_assignment_replay_assignment(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrTypeInferenceLoopAssignmentResult *result) {
    SZrInferredType rhsType;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrBool success;

    if (cs == ZR_NULL || step == ZR_NULL || step->assignment == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &rhsType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, step->right, &rhsType) &&
              loop_assignment_replay_numeric_range(&rhsType, &minValue, &maxValue) &&
              ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(cs, step->name, &rhsType) &&
              ZrParser_TypeInferenceLoopAssignment_ResultStoreType(cs->state, result, step->name, &rhsType);
    ZrParser_InferredType_Free(cs->state, &rhsType);
    return success;
}

static TZrBool loop_assignment_replay_self_dependent_assignment(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrTypeInferenceLoopAssignmentResult *result) {
    const SZrTypeBinding *sourceBinding;
    SZrInferredType widenedType;
    TZrInt64 deltaMin;
    TZrInt64 deltaMax;
    TZrBool success;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        step == ZR_NULL ||
        step->name == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, step->name);
    if (sourceBinding == ZR_NULL) {
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

    ZrParser_InferredType_Init(cs->state, &widenedType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_TypeInferenceLoopAssignment_WidenSelfDependentType(
                      cs,
                      &sourceBinding->type,
                      deltaMin,
                      deltaMax,
                      &widenedType) &&
              ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(cs, step->name, &widenedType) &&
              ZrParser_TypeInferenceLoopAssignment_ResultStoreType(cs->state, result, step->name, &widenedType);
    ZrParser_InferredType_Free(cs->state, &widenedType);
    return success;
}

static TZrBool loop_assignment_replay_step(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrTypeInferenceLoopAssignmentResult *result);

static TZrBool loop_assignment_replay_self_dependent_sequence(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize startIndex,
        SZrTypeInferenceLoopAssignmentResult *result,
        TZrSize *outNextIndex) {
    SZrTypeInferenceLoopAssignmentStep *firstStep;
    const SZrTypeBinding *sourceBinding;
    SZrInferredType widenedType;
    TZrInt64 deltaMin = 0;
    TZrInt64 deltaMax = 0;
    TZrInt64 sequenceDeltaMin = 0;
    TZrInt64 sequenceDeltaMax = 0;
    TZrSize index;
    TZrBool success;
    TZrBool allowTargetReadingInterleaves = ZR_FALSE;
    SZrTypeInferenceLoopAssignmentSequenceDeltaTracker deltaTracker;

    if (outNextIndex != ZR_NULL) {
        *outNextIndex = startIndex;
    }
    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized ||
        result == ZR_NULL ||
        outNextIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    firstStep = loop_assignment_replay_plan_step_at(plan, startIndex);
    if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepStarts(firstStep)) {
        return ZR_FALSE;
    }

    sourceBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, firstStep->name);
    if (sourceBinding == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_TypeInferenceLoopAssignment_SequenceAllowsTargetReadingInterleaves(
            cs,
            plan,
            startIndex,
            firstStep->name,
            &allowTargetReadingInterleaves,
            &sequenceDeltaMin,
            &sequenceDeltaMax)) {
        return ZR_FALSE;
    }

    ZrParser_TypeInferenceLoopAssignment_SequenceDeltaTrackerInit(&deltaTracker);
    index = startIndex;
    while (index < plan->steps.length) {
        SZrTypeInferenceLoopAssignmentStep *step =
                loop_assignment_replay_plan_step_at(plan, index);
        TZrBool isSequenceStep = ZR_FALSE;

        if (!ZrParser_TypeInferenceLoopAssignment_SequenceAccumulateStep(
                    cs,
                    plan,
                    startIndex,
                    index,
                    firstStep->name,
                    step,
                    &deltaTracker,
                    &isSequenceStep)) {
            return ZR_FALSE;
        }
        deltaMin = deltaTracker.deltaMin;
        deltaMax = deltaTracker.deltaMax;
        if (!isSequenceStep) {
            if (!ZrParser_TypeInferenceLoopAssignment_SequenceStepIsInterleavable(
                        step,
                        firstStep->name,
                        allowTargetReadingInterleaves)) {
                break;
            }
            if (ZrParser_TypeInferenceLoopAssignment_SequenceStepReadsName(step, firstStep->name) &&
                allowTargetReadingInterleaves) {
                if (!ZrParser_TypeInferenceLoopAssignment_SequenceStoreIntermediate(
                        cs,
                        firstStep->name,
                        &sourceBinding->type,
                        deltaMin,
                        deltaMax,
                        sequenceDeltaMin,
                        sequenceDeltaMax)) {
                    return ZR_FALSE;
                }
            }
            if (!loop_assignment_replay_step(cs, plan, step, result)) {
                return ZR_FALSE;
            }
            index++;
            continue;
        }
        index++;
    }

    ZrParser_InferredType_Init(cs->state, &widenedType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_TypeInferenceLoopAssignment_WidenSelfDependentType(
                      cs,
                      &sourceBinding->type,
                      deltaMin,
                      deltaMax,
                      &widenedType) &&
              ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(
                      cs,
                      firstStep->name,
                      &widenedType) &&
              ZrParser_TypeInferenceLoopAssignment_ResultStoreType(
                      cs->state,
                      result,
                      firstStep->name,
                      &widenedType);
    ZrParser_InferredType_Free(cs->state, &widenedType);
    if (success) {
        *outNextIndex = index;
    }
    return success;
}

static TZrBool loop_assignment_replay_store_current_plan_bindings(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrTypeInferenceLoopAssignmentResult *result) {
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        plan == ZR_NULL ||
        result == ZR_NULL ||
        !result->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *name = loop_assignment_replay_plan_target_at(plan, index);
        const SZrTypeBinding *binding;

        if (name == ZR_NULL) {
            return ZR_FALSE;
        }
        binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
        if (binding == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NUMBER(binding->type.baseType) ||
            !binding->type.hasRangeConstraint ||
            !ZrParser_TypeInferenceLoopAssignment_ResultStoreType(
                    cs->state,
                    result,
                    name,
                    &binding->type)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool loop_assignment_replay_step(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrTypeInferenceLoopAssignmentResult *result) {
    if (cs == ZR_NULL || plan == ZR_NULL || step == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT) {
        if (step->hasSelfDependentDelta || step->resolveSelfDependentDeltaOnReplay) {
            return loop_assignment_replay_self_dependent_assignment(cs, step, result);
        }
        return loop_assignment_replay_assignment(cs, step, result);
    }
    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF) {
        return ZrParser_TypeInference_TryJoinIfElseNumericAssignments(cs, step->assignment) &&
               loop_assignment_replay_store_current_plan_bindings(cs, plan, result);
    }
    if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP) {
        return ZrParser_TypeInferenceLoopAssignment_TryJoinNestedLoop(cs, step->assignment) &&
               loop_assignment_replay_store_current_plan_bindings(cs, plan, result);
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_InferPlan(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrTypeInferenceLoopAssignmentResult *result) {
    SZrTypeInferenceBranchScope scope;
    TZrBool pushed;
    TZrBool success;
    TZrSize index;

    if (cs == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized ||
        plan->steps.length == 0 ||
        result == ZR_NULL ||
        !result->isInitialized) {
        return ZR_FALSE;
    }

    memset(&scope, 0, sizeof(scope));
    pushed = ZrParser_TypeInferenceLoopAssignment_PushReplayScope(cs, &scope);
    success = pushed;
    for (index = 0; success && index < plan->steps.length;) {
        SZrTypeInferenceLoopAssignmentStep *step =
                loop_assignment_replay_plan_step_at(plan, index);
        if (ZrParser_TypeInferenceLoopAssignment_SequenceStepStarts(step)) {
            success = loop_assignment_replay_self_dependent_sequence(
                    cs,
                    plan,
                    index,
                    result,
                    &index);
        } else {
            success = loop_assignment_replay_step(cs, plan, step, result);
            index++;
        }
    }
    if (pushed) {
        ZrParser_TypeInferenceLoopAssignment_PopReplayScope(cs, &scope);
    }
    return success;
}
