#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "type_inference_branch_assignment_segments.h"
#include "type_inference_loop_assignment_join_internal.h"
#include "type_inference_loop_assignment_nested_if.h"
#include "type_inference_loop_assignment_nested_loop.h"
#include "type_inference_loop_assignment_self_dependency.h"
#include "type_inference_loop_assignment_sequence.h"
#include "type_inference_loop_assignment_syntax.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_parser_conf.h"

static TZrBool type_inference_loop_assignment_deferred_self_dependent_delta_is_supported(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        TZrSize stepIndex);

static TZrBool type_inference_loop_assignment_plan_init(SZrState *state,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (state == ZR_NULL || plan == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      &plan->steps,
                      sizeof(SZrTypeInferenceLoopAssignmentStep),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state,
                      &plan->targetNames,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    plan->isInitialized = ZR_TRUE;
    return ZR_TRUE;
}

static void type_inference_loop_assignment_plan_free(SZrState *state,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (state == ZR_NULL || plan == ZR_NULL || !plan->isInitialized) {
        return;
    }

    ZrCore_Array_Free(state, &plan->steps);
    ZrCore_Array_Free(state, &plan->targetNames);
    plan->isInitialized = ZR_FALSE;
}

static TZrSize type_inference_loop_assignment_plan_count(const SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (plan == ZR_NULL || !plan->isInitialized) {
        return 0;
    }
    return plan->steps.length;
}

static SZrTypeInferenceLoopAssignmentStep *type_inference_loop_assignment_plan_step_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan, TZrSize index) {
    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->steps.length) {
        return ZR_NULL;
    }
    return (SZrTypeInferenceLoopAssignmentStep *)ZrCore_Array_Get((SZrArray *)&plan->steps, index);
}

static TZrSize type_inference_loop_assignment_plan_target_count(const SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (plan == ZR_NULL || !plan->isInitialized) {
        return 0;
    }
    return plan->targetNames.length;
}

static SZrString *type_inference_loop_assignment_plan_target_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan, TZrSize index) {
    SZrString **target;

    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->targetNames.length) {
        return ZR_NULL;
    }

    target = (SZrString **)ZrCore_Array_Get((SZrArray *)&plan->targetNames, index);
    return target != ZR_NULL ? *target : ZR_NULL;
}

static TZrBool type_inference_loop_assignment_plan_contains(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name) {
    TZrSize index;

    if (plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *target = type_inference_loop_assignment_plan_target_at(plan, index);
        if (target != ZR_NULL && ZrCore_String_Equal(target, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool type_inference_loop_assignment_plan_add_target(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name) {
    if (state == ZR_NULL || plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    if (!type_inference_loop_assignment_plan_contains(plan, name)) {
        ZrCore_Array_Push(state, &plan->targetNames, &name);
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_plan_assigned_before(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name,
        TZrSize beforeIndex) {
    TZrSize index;

    if (plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < beforeIndex && index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index);
        if (step == ZR_NULL) {
            continue;
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
            step->name != ZR_NULL &&
            ZrCore_String_Equal(step->name, name)) {
            return ZR_TRUE;
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF &&
            ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(step->assignment, name)) {
            return ZR_TRUE;
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP &&
            ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(step->assignment, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool type_inference_loop_assignment_step_is_zero_delta_preserving(
        const SZrTypeInferenceLoopAssignmentStep *step) {
    return step != ZR_NULL &&
           step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
           step->hasSelfDependentDelta &&
           !step->resolveSelfDependentDeltaOnReplay &&
           step->selfDependentDeltaMin == 0 &&
           step->selfDependentDeltaMax == 0;
}

static TZrBool type_inference_loop_assignment_plan_preserves_target_before(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name,
        TZrSize beforeIndex) {
    TZrSize index;
    TZrBool foundTargetWrite = ZR_FALSE;

    if (plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < beforeIndex && index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index);
        if (step == ZR_NULL) {
            continue;
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT &&
            step->name != ZR_NULL &&
            ZrCore_String_Equal(step->name, name)) {
            foundTargetWrite = ZR_TRUE;
            if (!type_inference_loop_assignment_step_is_zero_delta_preserving(step)) {
                return ZR_FALSE;
            }
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF &&
            ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(step->assignment, name)) {
            return ZR_FALSE;
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP &&
            ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(step->assignment, name)) {
            return ZR_FALSE;
        }
    }
    return foundTargetWrite;
}

static TZrBool type_inference_loop_assignment_int64_add(TZrInt64 left,
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

static TZrBool type_inference_loop_assignment_int64_range_add(TZrInt64 leftMin,
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
    if (!type_inference_loop_assignment_int64_add(leftMin, rightMin, &minValue) ||
        !type_inference_loop_assignment_int64_add(leftMax, rightMax, &maxValue)) {
        return ZR_FALSE;
    }
    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_self_dependent_delta_is_supported(
        TZrInt64 deltaMin, TZrInt64 deltaMax) {
    return (deltaMin == 0 && deltaMax == 0) ||
           (deltaMin >= 0 && deltaMax > 0) ||
           (deltaMin < 0 && deltaMax <= 0) ||
           (deltaMin < 0 && deltaMax > 0);
}

static TZrBool type_inference_loop_assignment_plan_extends_self_dependent_target_sequence(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name,
        TZrInt64 currentDeltaMin,
        TZrInt64 currentDeltaMax) {
    TZrSize index;
    TZrInt64 previousDeltaMin = 0;
    TZrInt64 previousDeltaMax = 0;
    TZrBool sawTargetReadingInterleave = ZR_FALSE;

    if (plan == ZR_NULL ||
        name == ZR_NULL ||
        !plan->isInitialized ||
        plan->steps.length == 0) {
        return ZR_FALSE;
    }

    for (index = plan->steps.length; index > 0; index--) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index - 1);

        if (step == ZR_NULL) {
            return ZR_FALSE;
        }
        if (step->kind != ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT ||
            step->name == ZR_NULL ||
            step->right == ZR_NULL) {
            return ZR_FALSE;
        }
        if (ZrCore_String_Equal(step->name, name)) {
            TZrInt64 nextDeltaMin;
            TZrInt64 nextDeltaMax;
            TZrInt64 totalDeltaMin;
            TZrInt64 totalDeltaMax;

            if (!step->hasSelfDependentDelta ||
                step->resolveSelfDependentDeltaOnReplay ||
                !type_inference_loop_assignment_int64_range_add(
                        previousDeltaMin,
                        previousDeltaMax,
                        step->selfDependentDeltaMin,
                        step->selfDependentDeltaMax,
                        &nextDeltaMin,
                        &nextDeltaMax)) {
                return ZR_FALSE;
            }
            previousDeltaMin = nextDeltaMin;
            previousDeltaMax = nextDeltaMax;
            if (!sawTargetReadingInterleave) {
                return ZR_TRUE;
            }
            if (!type_inference_loop_assignment_int64_range_add(
                    previousDeltaMin,
                    previousDeltaMax,
                    currentDeltaMin,
                    currentDeltaMax,
                    &totalDeltaMin,
                    &totalDeltaMax)) {
                return ZR_FALSE;
            }
            return type_inference_loop_assignment_self_dependent_delta_is_supported(
                    totalDeltaMin,
                    totalDeltaMax);
        }
        if (step->hasSelfDependentDelta ||
            step->resolveSelfDependentDeltaOnReplay) {
            return ZR_FALSE;
        }
        if (ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(step->right, name)) {
            sawTargetReadingInterleave = ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool type_inference_loop_assignment_plan_can_extend_replay_resolved_target_sequence(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name) {
    TZrSize index;
    TZrBool sawSameTarget = ZR_FALSE;

    if (plan == ZR_NULL ||
        name == ZR_NULL ||
        !plan->isInitialized ||
        plan->steps.length == 0) {
        return ZR_FALSE;
    }

    for (index = plan->steps.length; index > 0; index--) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index - 1);

        if (step == ZR_NULL ||
            step->kind != ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT ||
            step->name == ZR_NULL ||
            step->right == ZR_NULL) {
            return ZR_FALSE;
        }
        if (ZrCore_String_Equal(step->name, name)) {
            if (!step->hasSelfDependentDelta &&
                !step->resolveSelfDependentDeltaOnReplay) {
                return ZR_FALSE;
            }
            sawSameTarget = ZR_TRUE;
            continue;
        }
        if (step->hasSelfDependentDelta ||
            step->resolveSelfDependentDeltaOnReplay) {
            return ZR_FALSE;
        }
    }
    return sawSameTarget;
}

static TZrBool type_inference_loop_assignment_plan_add(SZrCompilerState *cs,
                                                       SZrTypeInferenceLoopAssignmentPlan *plan,
                                                       SZrAstNode *assignmentNode) {
    SZrTypeInferenceLoopAssignmentStep step;
    TZrBool assignedBefore;
    TZrBool preservesTargetBefore;
    TZrBool extendsSelfDependentTargetSequence;
    TZrBool extendsReplayResolvedSelfDependentTargetSequence;
    TZrBool canReplayStableSelfDependentDelta;
    TZrBool hasStableSelfDependentDelta;
    TZrBool hasDeferredSelfDependentDelta;
    TZrInt64 selfDependentDeltaMin = 0;
    TZrInt64 selfDependentDeltaMax = 0;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized ||
        assignmentNode == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&step, 0, sizeof(step));
    if (!ZrParser_TypeInferenceLoopAssignment_AssignmentParts(assignmentNode, &step.name, &step.right)) {
        return ZR_FALSE;
    }

    step.kind = ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT;
    step.assignment = assignmentNode;
    assignedBefore = type_inference_loop_assignment_plan_assigned_before(
            plan,
            step.name,
            plan->steps.length);
    preservesTargetBefore = assignedBefore &&
                            type_inference_loop_assignment_plan_preserves_target_before(
                                    plan,
                                    step.name,
                                    plan->steps.length);
    hasStableSelfDependentDelta =
            ZrParser_TypeInferenceLoopAssignment_TrySelfDependentDelta(
                    cs,
                    assignmentNode,
                    step.name,
                    &selfDependentDeltaMin,
                    &selfDependentDeltaMax);
    extendsSelfDependentTargetSequence =
            assignedBefore &&
            hasStableSelfDependentDelta &&
            type_inference_loop_assignment_plan_extends_self_dependent_target_sequence(
                    plan,
                    step.name,
                    selfDependentDeltaMin,
                    selfDependentDeltaMax);
    hasDeferredSelfDependentDelta =
            type_inference_loop_assignment_deferred_self_dependent_delta_is_supported(
                    plan,
                    &step,
                    plan->steps.length);
    extendsReplayResolvedSelfDependentTargetSequence =
            assignedBefore &&
            hasDeferredSelfDependentDelta &&
            type_inference_loop_assignment_plan_can_extend_replay_resolved_target_sequence(
                    plan,
                    step.name);
    canReplayStableSelfDependentDelta = assignedBefore && hasStableSelfDependentDelta;
    step.resolveSelfDependentDeltaOnReplay =
            (!assignedBefore || extendsReplayResolvedSelfDependentTargetSequence) &&
            hasDeferredSelfDependentDelta;
    step.hasSelfDependentDelta =
            (!assignedBefore ||
             preservesTargetBefore ||
             extendsSelfDependentTargetSequence ||
             canReplayStableSelfDependentDelta) &&
            !step.resolveSelfDependentDeltaOnReplay &&
            hasStableSelfDependentDelta;
    step.selfDependentDeltaMin = selfDependentDeltaMin;
    step.selfDependentDeltaMax = selfDependentDeltaMax;
    if (!type_inference_loop_assignment_plan_add_target(cs->state, plan, step.name)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &plan->steps, &step);
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_plan_add_nested_if(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrAstNode *ifNode) {
    SZrTypeInferenceLoopAssignmentStep step;
    TZrBool hasAssignment = ZR_FALSE;

    if (state == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized ||
        ifNode == ZR_NULL ||
        ifNode->type != ZR_AST_IF_EXPRESSION ||
        !ZrParser_TypeInferenceLoopAssignment_NestedIfCollectTargets(
                state,
                ifNode,
                &plan->targetNames,
                &hasAssignment)) {
        return ZR_FALSE;
    }
    if (!hasAssignment) {
        return ZR_TRUE;
    }

    memset(&step, 0, sizeof(step));
    step.kind = ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF;
    step.assignment = ifNode;
    ZrCore_Array_Push(state, &plan->steps, &step);
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_plan_add_nested_loop(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrAstNode *loopNode) {
    SZrTypeInferenceLoopAssignmentStep step;
    TZrBool hasAssignment = ZR_FALSE;

    if (state == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized ||
        loopNode == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_NestedLoopCollectTargets(
                state,
                loopNode,
                &plan->targetNames,
                &hasAssignment)) {
        return ZR_FALSE;
    }
    if (!hasAssignment) {
        return ZR_TRUE;
    }

    memset(&step, 0, sizeof(step));
    step.kind = ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP;
    step.assignment = loopNode;
    ZrCore_Array_Push(state, &plan->steps, &step);
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_statement_plan(
        SZrCompilerState *cs,
        SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrAstNode *statement) {
    SZrAstNode *expression;

    if (cs == ZR_NULL || cs->state == ZR_NULL || plan == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }
    if (statement == ZR_NULL) {
        return ZR_TRUE;
    }

    expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(statement);
    if (expression == ZR_NULL) {
        return ZR_TRUE;
    }
    if (expression->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        return type_inference_loop_assignment_plan_add(cs, plan, expression);
    }
    if (expression->type == ZR_AST_IF_EXPRESSION) {
        return type_inference_loop_assignment_plan_add_nested_if(cs->state, plan, expression);
    }
    if (expression->type == ZR_AST_WHILE_LOOP ||
        expression->type == ZR_AST_FOR_LOOP ||
        expression->type == ZR_AST_FOREACH_LOOP) {
        return type_inference_loop_assignment_plan_add_nested_loop(cs->state, plan, expression);
    }
    if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool type_inference_loop_assignment_collect_body_plan(
        SZrCompilerState *cs,
        SZrAstNode *block,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        block == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized) {
        return ZR_FALSE;
    }

    if (block->type != ZR_AST_BLOCK) {
        return type_inference_loop_assignment_collect_statement_plan(cs, plan, block);
    }

    body = block->data.block.body;
    if (body == ZR_NULL || body->count == 0 || body->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < body->count; index++) {
        if (ZrParser_TypeInferenceLoopAssignment_StatementIsPlainBreak(body->nodes[index])) {
            return ZR_TRUE;
        }
        if (!type_inference_loop_assignment_collect_statement_plan(cs, plan, body->nodes[index])) {
            return ZR_FALSE;
        }
        if (ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(body->nodes[index])) {
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_plan(
        SZrCompilerState *cs,
        SZrAstNode *block,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (!type_inference_loop_assignment_collect_body_plan(cs, block, plan)) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_plan_count(plan) > 0;
}

static TZrBool type_inference_loop_assignment_collect_step_plan(
        SZrCompilerState *cs,
        SZrAstNode *stepNode,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    SZrAstNode *expression;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        stepNode == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized) {
        return ZR_FALSE;
    }

    expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(stepNode);
    if (expression == ZR_NULL || expression->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_plan_add(cs, plan, expression);
}

static TZrBool type_inference_loop_assignment_collect_plan_with_step(
        SZrCompilerState *cs,
        SZrAstNode *body,
        SZrAstNode *stepNode,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (!type_inference_loop_assignment_collect_body_plan(cs, body, plan) ||
        !type_inference_loop_assignment_collect_step_plan(cs, stepNode, plan)) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_plan_count(plan) > 0;
}

static TZrBool type_inference_loop_assignment_numeric_range(const SZrInferredType *type,
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

static TZrBool type_inference_loop_assignment_rhs_is_supported(
        SZrAstNode *right,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize stepIndex) {
    SZrString *name;
    SZrAstNodeArray *members;

    if (right == ZR_NULL || plan == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    switch (right->type) {
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_TRUE;
        case ZR_AST_IDENTIFIER_LITERAL:
            name = right->data.identifier.name;
            return name != ZR_NULL &&
                   (!type_inference_loop_assignment_plan_contains(plan, name) ||
                    type_inference_loop_assignment_plan_assigned_before(plan, name, stepIndex));
        case ZR_AST_BINARY_EXPRESSION:
            return type_inference_loop_assignment_rhs_is_supported(
                           right->data.binaryExpression.left,
                           plan,
                           stepIndex) &&
                   type_inference_loop_assignment_rhs_is_supported(
                           right->data.binaryExpression.right,
                           plan,
                           stepIndex);
        case ZR_AST_LOGICAL_EXPRESSION:
            return type_inference_loop_assignment_rhs_is_supported(
                           right->data.logicalExpression.left,
                           plan,
                           stepIndex) &&
                   type_inference_loop_assignment_rhs_is_supported(
                           right->data.logicalExpression.right,
                           plan,
                           stepIndex);
        case ZR_AST_UNARY_EXPRESSION:
            return type_inference_loop_assignment_rhs_is_supported(
                    right->data.unaryExpression.argument,
                    plan,
                    stepIndex);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return type_inference_loop_assignment_rhs_is_supported(
                    right->data.typeCastExpression.expression,
                    plan,
                    stepIndex);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return type_inference_loop_assignment_rhs_is_supported(
                           right->data.conditionalExpression.test,
                           plan,
                           stepIndex) &&
                   type_inference_loop_assignment_rhs_is_supported(
                           right->data.conditionalExpression.consequent,
                           plan,
                           stepIndex) &&
                   type_inference_loop_assignment_rhs_is_supported(
                           right->data.conditionalExpression.alternate,
                           plan,
                           stepIndex);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = right->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_FALSE;
            }
            return type_inference_loop_assignment_rhs_is_supported(
                    right->data.primaryExpression.property,
                    plan,
                    stepIndex);
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_loop_assignment_deferred_self_dependent_delta_is_supported(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        TZrSize stepIndex) {
    SZrAstNode *deltaExpression;

    if (plan == ZR_NULL || step == ZR_NULL || step->right == ZR_NULL || step->name == ZR_NULL) {
        return ZR_FALSE;
    }

    deltaExpression = ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(
            step->right,
            step->name);
    return deltaExpression != ZR_NULL &&
           ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaUsesAnyTarget(
                   step->right,
                   step->name,
                   &plan->targetNames) &&
           type_inference_loop_assignment_rhs_is_supported(deltaExpression, plan, stepIndex);
}

static TZrBool type_inference_loop_assignment_plan_validate(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan) {
    TZrSize index;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || plan == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index);
        const SZrTypeBinding *targetBinding;

        if (step == ZR_NULL) {
            return ZR_FALSE;
        }
        if (step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF ||
            step->kind == ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP) {
            continue;
        }
        if (step->kind != ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT ||
            step->name == ZR_NULL ||
            step->right == ZR_NULL) {
            return ZR_FALSE;
        }

        targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, step->name);
        if (targetBinding == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType) ||
            !targetBinding->type.hasRangeConstraint ||
            ((step->hasSelfDependentDelta || step->resolveSelfDependentDeltaOnReplay) &&
             (targetBinding->type.baseType != ZR_VALUE_TYPE_INT64 ||
              (step->hasSelfDependentDelta &&
               ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaUsesAnyTarget(
                      step->right,
                      step->name,
                      &plan->targetNames) &&
               !ZrParser_TypeInferenceLoopAssignment_SequenceFutureWritesExpressionDependency(
                       plan,
                       index,
                       step->name,
                       step->right)) ||
              (step->resolveSelfDependentDeltaOnReplay &&
               !type_inference_loop_assignment_deferred_self_dependent_delta_is_supported(
                       plan,
                       step,
                       index)))) ||
            (!step->hasSelfDependentDelta &&
             !step->resolveSelfDependentDeltaOnReplay &&
             !type_inference_loop_assignment_rhs_is_supported(step->right, plan, index))) {
            return ZR_FALSE;
        }
    }
    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *name = type_inference_loop_assignment_plan_target_at(plan, index);
        const SZrTypeBinding *targetBinding;

        if (name == ZR_NULL) {
            return ZR_FALSE;
        }
        targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
        if (targetBinding == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType) ||
            !targetBinding->type.hasRangeConstraint) {
            return ZR_FALSE;
        }
    }
    return type_inference_loop_assignment_plan_count(plan) > 0 &&
           type_inference_loop_assignment_plan_target_count(plan) > 0;
}

static TZrBool type_inference_loop_assignment_join_type(SZrCompilerState *cs,
                                                        const SZrTypeBinding *targetBinding,
                                                        const SZrInferredType *preLoopType,
                                                        const SZrInferredType *loopType,
                                                        SZrInferredType *outType) {
    SZrInferredType commonType;
    TZrInt64 preMin;
    TZrInt64 preMax;
    TZrInt64 loopMin;
    TZrInt64 loopMax;
    TZrBool hasCommon;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        targetBinding == ZR_NULL ||
        preLoopType == ZR_NULL ||
        loopType == ZR_NULL ||
        outType == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType) ||
        !type_inference_loop_assignment_numeric_range(preLoopType, &preMin, &preMax) ||
        !type_inference_loop_assignment_numeric_range(loopType, &loopMin, &loopMax)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &commonType, ZR_VALUE_TYPE_OBJECT);
    hasCommon = ZrParser_InferredType_GetCommonType(cs->state, &commonType, preLoopType, loopType);
    if (!hasCommon || !ZR_VALUE_IS_TYPE_NUMBER(commonType.baseType)) {
        ZrParser_InferredType_Free(cs->state, &commonType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(cs->state, outType, &targetBinding->type);
    outType->baseType = targetBinding->type.baseType;
    outType->hasRangeConstraint = ZR_TRUE;
    outType->minValue = preMin < loopMin ? preMin : loopMin;
    outType->maxValue = preMax > loopMax ? preMax : loopMax;
    if (!ZrParser_TypeInferenceBranchAssignment_ApplyJoinedSegments(cs, preLoopType, loopType, outType)) {
        ZrParser_InferredType_Free(cs->state, &commonType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(cs->state, &commonType);
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_join_plan(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrBool includePreLoopPath) {
    SZrTypeInferenceLoopAssignmentResult loopResult;
    SZrTypeInferenceLoopAssignmentResult joinedResult;
    TZrBool joined = ZR_FALSE;
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        plan == ZR_NULL ||
        !plan->isInitialized) {
        return ZR_FALSE;
    }

    memset(&loopResult, 0, sizeof(loopResult));
    memset(&joinedResult, 0, sizeof(joinedResult));

    if (!type_inference_loop_assignment_plan_validate(cs, plan) ||
        !ZrParser_TypeInferenceLoopAssignment_ResultInit(
                cs->state,
                &loopResult,
                type_inference_loop_assignment_plan_target_count(plan)) ||
        !ZrParser_TypeInferenceLoopAssignment_ResultInit(
                cs->state,
                &joinedResult,
                type_inference_loop_assignment_plan_target_count(plan)) ||
        !ZrParser_TypeInferenceLoopAssignment_InferPlan(cs, plan, &loopResult)) {
        goto cleanup;
    }

    for (index = 0; index < loopResult.bindings.length; index++) {
        SZrTypeInferenceLoopAssignmentBindingType *binding =
                (SZrTypeInferenceLoopAssignmentBindingType *)ZrCore_Array_Get(&loopResult.bindings, index);
        const SZrTypeBinding *targetBinding;
        SZrInferredType preLoopType;
        SZrInferredType joinedType;
        TZrBool initializedPreLoop = ZR_FALSE;
        TZrBool initializedJoined = ZR_FALSE;

        if (binding == ZR_NULL || binding->name == ZR_NULL || !binding->hasType) {
            goto cleanup;
        }

        targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, binding->name);
        if (targetBinding == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType) ||
            !targetBinding->type.hasRangeConstraint ||
            ZrParser_TypeInferenceLoopAssignment_ResultFindType(&joinedResult, binding->name) != ZR_NULL) {
            goto cleanup;
        }

        ZrParser_InferredType_Init(cs->state, &preLoopType, ZR_VALUE_TYPE_OBJECT);
        initializedPreLoop = ZR_TRUE;
        ZrParser_InferredType_Copy(cs->state,
                                   &preLoopType,
                                   includePreLoopPath ? &targetBinding->type : &binding->type);

        ZrParser_InferredType_Init(cs->state, &joinedType, ZR_VALUE_TYPE_OBJECT);
        initializedJoined = ZR_TRUE;
        if (!type_inference_loop_assignment_join_type(cs,
                                                      targetBinding,
                                                      &preLoopType,
                                                      &binding->type,
                                                      &joinedType) ||
            !ZrParser_TypeInferenceLoopAssignment_ResultStoreType(cs->state,
                                                                  &joinedResult,
                                                                  binding->name,
                                                                  &joinedType)) {
            if (initializedJoined) {
                ZrParser_InferredType_Free(cs->state, &joinedType);
            }
            if (initializedPreLoop) {
                ZrParser_InferredType_Free(cs->state, &preLoopType);
            }
            goto cleanup;
        }

        if (initializedJoined) {
            ZrParser_InferredType_Free(cs->state, &joinedType);
        }
        if (initializedPreLoop) {
            ZrParser_InferredType_Free(cs->state, &preLoopType);
        }
    }

    joined = ZR_TRUE;
    for (index = 0; index < joinedResult.bindings.length; index++) {
        SZrTypeInferenceLoopAssignmentBindingType *binding =
                (SZrTypeInferenceLoopAssignmentBindingType *)ZrCore_Array_Get(&joinedResult.bindings, index);
        if (binding == ZR_NULL ||
            binding->name == ZR_NULL ||
            !binding->hasType ||
            !ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, binding->name, &binding->type)) {
            joined = ZR_FALSE;
            break;
        }
    }

cleanup:
    ZrParser_TypeInferenceLoopAssignment_ResultFree(cs->state, &joinedResult);
    ZrParser_TypeInferenceLoopAssignment_ResultFree(cs->state, &loopResult);
    return joined;
}

static TZrBool type_inference_loop_assignment_join_body_with_cardinality(
        SZrCompilerState *cs,
        SZrAstNode *body,
        TZrBool includePreLoopPath) {
    SZrTypeInferenceLoopAssignmentPlan plan;
    TZrBool joined = ZR_FALSE;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        body == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&plan, 0, sizeof(plan));
    if (type_inference_loop_assignment_plan_init(cs->state, &plan) &&
        type_inference_loop_assignment_collect_plan(cs, body, &plan)) {
        joined = type_inference_loop_assignment_join_plan(cs, &plan, includePreLoopPath);
    }
    type_inference_loop_assignment_plan_free(cs->state, &plan);
    return joined;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_JoinBody(SZrCompilerState *cs,
                                                      SZrAstNode *body) {
    return type_inference_loop_assignment_join_body_with_cardinality(cs, body, ZR_TRUE);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_JoinBodyAtLeastOnce(SZrCompilerState *cs,
                                                                 SZrAstNode *body) {
    return type_inference_loop_assignment_join_body_with_cardinality(cs, body, ZR_FALSE);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_JoinBodyWithStep(SZrCompilerState *cs,
                                                              SZrAstNode *body,
                                                              SZrAstNode *step) {
    SZrTypeInferenceLoopAssignmentPlan plan;
    TZrBool joined = ZR_FALSE;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        body == ZR_NULL ||
        step == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&plan, 0, sizeof(plan));
    if (type_inference_loop_assignment_plan_init(cs->state, &plan) &&
        type_inference_loop_assignment_collect_plan_with_step(cs, body, step, &plan)) {
        joined = type_inference_loop_assignment_join_plan(cs, &plan, ZR_TRUE);
    }
    type_inference_loop_assignment_plan_free(cs->state, &plan);
    return joined;
}
