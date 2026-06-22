//
// Created by Auto on 2026/06/23.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"

#include "type_inference_branch_assignment_segments.h"

#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_parser_conf.h"

#include <string.h>

typedef struct SZrTypeInferenceLoopAssignmentStep {
    SZrString *name;
    SZrAstNode *assignment;
    SZrAstNode *right;
} SZrTypeInferenceLoopAssignmentStep;

typedef struct SZrTypeInferenceLoopAssignmentPlan {
    SZrArray steps;
    TZrBool isInitialized;
} SZrTypeInferenceLoopAssignmentPlan;

typedef struct SZrTypeInferenceLoopAssignmentBindingType {
    SZrString *name;
    SZrInferredType type;
    TZrBool hasType;
} SZrTypeInferenceLoopAssignmentBindingType;

typedef struct SZrTypeInferenceLoopAssignmentResult {
    SZrArray bindings;
    TZrBool isInitialized;
} SZrTypeInferenceLoopAssignmentResult;

static SZrAstNode *type_inference_loop_assignment_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL) {
        return ZR_NULL;
    }

    if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
        return statement->data.expressionStatement.expr;
    }
    return statement;
}

static TZrBool type_inference_loop_assignment_parts(SZrAstNode *assignmentNode,
                                                    SZrString **outName,
                                                    SZrAstNode **outRight) {
    SZrAssignmentExpression *assignment;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (outRight != ZR_NULL) {
        *outRight = ZR_NULL;
    }

    if (assignmentNode == ZR_NULL ||
        assignmentNode->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        outName == ZR_NULL ||
        outRight == ZR_NULL) {
        return ZR_FALSE;
    }

    assignment = &assignmentNode->data.assignmentExpression;
    if (assignment->op.op == ZR_NULL ||
        strcmp(assignment->op.op, "=") != 0 ||
        assignment->left == ZR_NULL ||
        assignment->left->type != ZR_AST_IDENTIFIER_LITERAL ||
        assignment->left->data.identifier.name == ZR_NULL ||
        assignment->right == ZR_NULL) {
        return ZR_FALSE;
    }

    *outName = assignment->left->data.identifier.name;
    *outRight = assignment->right;
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_plan_init(SZrState *state,
                                                        SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (state == ZR_NULL || plan == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      &plan->steps,
                      sizeof(SZrTypeInferenceLoopAssignmentStep),
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
    plan->isInitialized = ZR_FALSE;
}

static TZrSize type_inference_loop_assignment_plan_count(
        const SZrTypeInferenceLoopAssignmentPlan *plan) {
    if (plan == ZR_NULL || !plan->isInitialized) {
        return 0;
    }
    return plan->steps.length;
}

static SZrTypeInferenceLoopAssignmentStep *type_inference_loop_assignment_plan_step_at(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize index) {
    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->steps.length) {
        return ZR_NULL;
    }
    return (SZrTypeInferenceLoopAssignmentStep *)ZrCore_Array_Get((SZrArray *)&plan->steps, index);
}

static TZrBool type_inference_loop_assignment_plan_contains(
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrString *name) {
    TZrSize index;

    if (plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index);
        if (step != ZR_NULL && step->name != ZR_NULL && ZrCore_String_Equal(step->name, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
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
        if (step != ZR_NULL && step->name != ZR_NULL && ZrCore_String_Equal(step->name, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool type_inference_loop_assignment_plan_add(SZrState *state,
                                                       SZrTypeInferenceLoopAssignmentPlan *plan,
                                                       SZrAstNode *assignmentNode) {
    SZrTypeInferenceLoopAssignmentStep step;

    if (state == ZR_NULL || plan == ZR_NULL || !plan->isInitialized || assignmentNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!type_inference_loop_assignment_parts(assignmentNode, &step.name, &step.right)) {
        return ZR_FALSE;
    }

    step.assignment = assignmentNode;
    ZrCore_Array_Push(state, &plan->steps, &step);
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_plan(
        SZrState *state,
        SZrAstNode *block,
        SZrTypeInferenceLoopAssignmentPlan *plan) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (state == ZR_NULL || block == ZR_NULL || plan == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    if (block->type != ZR_AST_BLOCK) {
        SZrAstNode *expression = type_inference_loop_assignment_statement_expression(block);
        if (expression == ZR_NULL || expression->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
            return ZR_FALSE;
        }
        return type_inference_loop_assignment_plan_add(state, plan, expression);
    }

    body = block->data.block.body;
    if (body == ZR_NULL || body->count == 0 || body->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < body->count; index++) {
        SZrAstNode *statement = body->nodes[index];
        SZrAstNode *expression = type_inference_loop_assignment_statement_expression(statement);

        if (statement == ZR_NULL || expression == ZR_NULL) {
            continue;
        }
        if (expression->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
            if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
                continue;
            }
            return ZR_FALSE;
        }
        if (!type_inference_loop_assignment_plan_add(state, plan, expression)) {
            return ZR_FALSE;
        }
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

static TZrBool type_inference_loop_assignment_condition_is_known_bool(SZrAstNode *condition,
                                                                      TZrBool *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = ZR_FALSE;
    }
    if (condition == ZR_NULL ||
        condition->type != ZR_AST_BOOLEAN_LITERAL ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *outValue = condition->data.booleanLiteral.value;
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

        if (step == ZR_NULL || step->name == ZR_NULL || step->right == ZR_NULL) {
            return ZR_FALSE;
        }

        targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, step->name);
        if (targetBinding == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType) ||
            !targetBinding->type.hasRangeConstraint ||
            !type_inference_loop_assignment_rhs_is_supported(step->right, plan, index)) {
            return ZR_FALSE;
        }
    }
    return type_inference_loop_assignment_plan_count(plan) > 0;
}

static TZrBool type_inference_loop_assignment_result_init(
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

static void type_inference_loop_assignment_result_free(
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

static SZrTypeInferenceLoopAssignmentBindingType *type_inference_loop_assignment_result_find(
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name) {
    TZrSize index;

    if (result == ZR_NULL || name == ZR_NULL || !result->isInitialized) {
        return ZR_NULL;
    }

    for (index = 0; index < result->bindings.length; index++) {
        SZrTypeInferenceLoopAssignmentBindingType *binding =
                (SZrTypeInferenceLoopAssignmentBindingType *)ZrCore_Array_Get(&result->bindings, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }
    return ZR_NULL;
}

static const SZrInferredType *type_inference_loop_assignment_result_find_type(
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name) {
    SZrTypeInferenceLoopAssignmentBindingType *binding =
            type_inference_loop_assignment_result_find(result, name);

    return binding != ZR_NULL && binding->hasType ? &binding->type : ZR_NULL;
}

static TZrBool type_inference_loop_assignment_result_store_type(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name,
        const SZrInferredType *type) {
    SZrTypeInferenceLoopAssignmentBindingType *existing;
    SZrTypeInferenceLoopAssignmentBindingType binding;

    if (state == ZR_NULL || result == ZR_NULL || name == ZR_NULL || type == ZR_NULL || !result->isInitialized) {
        return ZR_FALSE;
    }

    existing = type_inference_loop_assignment_result_find(result, name);
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

static TZrBool type_inference_loop_assignment_push_replay_scope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope) {
    SZrTypeEnvironment *newEnv;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || scope == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(scope, 0, sizeof(*scope));
    newEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (newEnv == ZR_NULL) {
        return ZR_FALSE;
    }

    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = ZR_NULL;
    scope->savedEnv = cs->typeEnv;
    scope->isActive = ZR_TRUE;
    cs->typeEnv = newEnv;
    return ZR_TRUE;
}

static void type_inference_loop_assignment_pop_replay_scope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope) {
    SZrTypeEnvironment *activeEnv;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        scope == ZR_NULL ||
        !scope->isActive ||
        scope->savedEnv == ZR_NULL) {
        return;
    }

    activeEnv = cs->typeEnv;
    cs->typeEnv = scope->savedEnv;
    if (activeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(cs->state, activeEnv);
    }
    memset(scope, 0, sizeof(*scope));
}

static TZrBool type_inference_loop_assignment_store_replayed_binding(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrInferredType *type) {
    const SZrTypeBinding *sourceBinding;
    SZrTypeBinding binding;
    TZrSize index;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < cs->typeEnv->variableTypes.length; index++) {
        SZrTypeBinding *existing = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeEnv->variableTypes, index);
        if (existing != ZR_NULL && existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, name)) {
            ZrParser_InferredType_Free(cs->state, &existing->type);
            ZrParser_InferredType_Copy(cs->state, &existing->type, type);
            return ZR_TRUE;
        }
    }

    sourceBinding = cs->typeEnv->parent != ZR_NULL
                        ? ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv->parent, name)
                        : ZR_NULL;
    if (sourceBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding.name = name;
    binding.declarationRange = sourceBinding->declarationRange;
    binding.hasDeclarationRange = sourceBinding->hasDeclarationRange;
    binding.typeId = sourceBinding->typeId;
    binding.symbolId = sourceBinding->symbolId;
    ZrParser_InferredType_Copy(cs->state, &binding.type, type);
    ZrCore_Array_Push(cs->state, &cs->typeEnv->variableTypes, &binding);
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_replay_assignment(
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
              type_inference_loop_assignment_numeric_range(&rhsType, &minValue, &maxValue) &&
              type_inference_loop_assignment_store_replayed_binding(cs, step->name, &rhsType) &&
              type_inference_loop_assignment_result_store_type(cs->state, result, step->name, &rhsType);
    ZrParser_InferredType_Free(cs->state, &rhsType);
    return success;
}

static TZrBool type_inference_loop_assignment_infer_plan(
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
        type_inference_loop_assignment_plan_count(plan) == 0 ||
        result == ZR_NULL ||
        !result->isInitialized) {
        return ZR_FALSE;
    }

    memset(&scope, 0, sizeof(scope));
    pushed = type_inference_loop_assignment_push_replay_scope(cs, &scope);
    success = pushed;
    for (index = 0; success && index < plan->steps.length; index++) {
        SZrTypeInferenceLoopAssignmentStep *step =
                type_inference_loop_assignment_plan_step_at(plan, index);
        success = step != ZR_NULL &&
                  type_inference_loop_assignment_replay_assignment(cs, step, result);
    }
    if (pushed) {
        type_inference_loop_assignment_pop_replay_scope(cs, &scope);
    }
    return success;
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

TZrBool ZrParser_TypeInference_TryJoinWhileNumericAssignments(SZrCompilerState *cs,
                                                              SZrAstNode *whileNode) {
    SZrWhileLoop *whileLoop;
    SZrTypeInferenceLoopAssignmentPlan plan;
    SZrTypeInferenceLoopAssignmentResult loopResult;
    SZrTypeInferenceLoopAssignmentResult joinedResult;
    TZrBool conditionValue;
    TZrBool joined = ZR_FALSE;
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        whileNode == ZR_NULL ||
        whileNode->type != ZR_AST_WHILE_LOOP) {
        return ZR_FALSE;
    }

    whileLoop = &whileNode->data.whileLoop;
    if (type_inference_loop_assignment_condition_is_known_bool(whileLoop->cond, &conditionValue)) {
        return ZR_FALSE;
    }

    memset(&plan, 0, sizeof(plan));
    memset(&loopResult, 0, sizeof(loopResult));
    memset(&joinedResult, 0, sizeof(joinedResult));

    if (!type_inference_loop_assignment_plan_init(cs->state, &plan) ||
        !type_inference_loop_assignment_collect_plan(cs->state, whileLoop->block, &plan) ||
        !type_inference_loop_assignment_plan_validate(cs, &plan) ||
        !type_inference_loop_assignment_result_init(
                cs->state,
                &loopResult,
                type_inference_loop_assignment_plan_count(&plan)) ||
        !type_inference_loop_assignment_result_init(
                cs->state,
                &joinedResult,
                type_inference_loop_assignment_plan_count(&plan)) ||
        !type_inference_loop_assignment_infer_plan(cs, &plan, &loopResult)) {
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
            type_inference_loop_assignment_result_find_type(&joinedResult, binding->name) != ZR_NULL) {
            goto cleanup;
        }

        ZrParser_InferredType_Init(cs->state, &preLoopType, ZR_VALUE_TYPE_OBJECT);
        initializedPreLoop = ZR_TRUE;
        ZrParser_InferredType_Copy(cs->state, &preLoopType, &targetBinding->type);

        ZrParser_InferredType_Init(cs->state, &joinedType, ZR_VALUE_TYPE_OBJECT);
        initializedJoined = ZR_TRUE;
        if (!type_inference_loop_assignment_join_type(cs,
                                                      targetBinding,
                                                      &preLoopType,
                                                      &binding->type,
                                                      &joinedType) ||
            !type_inference_loop_assignment_result_store_type(cs->state,
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
    type_inference_loop_assignment_result_free(cs->state, &joinedResult);
    type_inference_loop_assignment_result_free(cs->state, &loopResult);
    type_inference_loop_assignment_plan_free(cs->state, &plan);
    return joined;
}
