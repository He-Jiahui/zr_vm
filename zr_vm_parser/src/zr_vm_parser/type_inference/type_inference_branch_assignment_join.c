//
// Created by Auto on 2026/06/22.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"

#include "type_inference_branch_assignment_segments.h"

#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_parser_conf.h"

#include <string.h>

typedef struct SZrTypeInferenceBranchAssignmentPlan {
    SZrArray targetNames;
    TZrBool isInitialized;
    TZrBool hasAssignment;
} SZrTypeInferenceBranchAssignmentPlan;

typedef struct SZrTypeInferenceBranchAssignmentBindingType {
    SZrString *name;
    SZrInferredType type;
    TZrBool hasType;
} SZrTypeInferenceBranchAssignmentBindingType;

typedef struct SZrTypeInferenceBranchAssignmentResult {
    SZrArray bindings;
    TZrBool isInitialized;
} SZrTypeInferenceBranchAssignmentResult;

static SZrAstNode *type_inference_branch_assignment_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL) {
        return ZR_NULL;
    }

    if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
        return statement->data.expressionStatement.expr;
    }
    return statement;
}

static TZrBool type_inference_branch_assignment_parts(SZrAstNode *assignmentNode,
                                                      SZrString **outName,
                                                      SZrAstNode **outRight);
static TZrBool type_inference_branch_assignment_analyze(
        SZrState *state,
        SZrAstNode *branchNode,
        SZrTypeInferenceBranchAssignmentPlan *outPlan);

static TZrBool type_inference_branch_assignment_parts(SZrAstNode *assignmentNode,
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

static TZrBool type_inference_branch_assignment_plan_init(SZrState *state,
                                                          SZrTypeInferenceBranchAssignmentPlan *plan) {
    if (state == ZR_NULL || plan == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      &plan->targetNames,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    plan->isInitialized = ZR_TRUE;
    plan->hasAssignment = ZR_FALSE;
    return ZR_TRUE;
}

static void type_inference_branch_assignment_plan_free(SZrState *state,
                                                       SZrTypeInferenceBranchAssignmentPlan *plan) {
    if (state == ZR_NULL || plan == ZR_NULL || !plan->isInitialized) {
        return;
    }

    ZrCore_Array_Free(state, &plan->targetNames);
    plan->isInitialized = ZR_FALSE;
    plan->hasAssignment = ZR_FALSE;
}

static TZrSize type_inference_branch_assignment_plan_target_count(
        const SZrTypeInferenceBranchAssignmentPlan *plan) {
    if (plan == ZR_NULL || !plan->isInitialized) {
        return 0;
    }

    return plan->targetNames.length;
}

static SZrString *type_inference_branch_assignment_plan_target_at(
        const SZrTypeInferenceBranchAssignmentPlan *plan,
        TZrSize index) {
    SZrString **target;

    if (plan == ZR_NULL || !plan->isInitialized || index >= plan->targetNames.length) {
        return ZR_NULL;
    }

    target = (SZrString **)ZrCore_Array_Get((SZrArray *)&plan->targetNames, index);
    return target != ZR_NULL ? *target : ZR_NULL;
}

static TZrBool type_inference_branch_assignment_plan_contains(
        const SZrTypeInferenceBranchAssignmentPlan *plan,
        SZrString *name) {
    TZrSize index;

    if (plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *target = type_inference_branch_assignment_plan_target_at(plan, index);
        if (target != ZR_NULL && ZrCore_String_Equal(target, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool type_inference_branch_assignment_plan_add(SZrState *state,
                                                         SZrTypeInferenceBranchAssignmentPlan *plan,
                                                         SZrString *name) {
    if (state == ZR_NULL || plan == ZR_NULL || name == ZR_NULL || !plan->isInitialized) {
        return ZR_FALSE;
    }

    if (!type_inference_branch_assignment_plan_contains(plan, name)) {
        ZrCore_Array_Push(state, &plan->targetNames, &name);
    }
    plan->hasAssignment = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool type_inference_branch_assignment_plan_extend(SZrState *state,
                                                            SZrTypeInferenceBranchAssignmentPlan *target,
                                                            const SZrTypeInferenceBranchAssignmentPlan *source) {
    TZrSize index;

    if (state == ZR_NULL ||
        target == ZR_NULL ||
        source == ZR_NULL ||
        !target->isInitialized ||
        !source->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < source->targetNames.length; index++) {
        SZrString *name = type_inference_branch_assignment_plan_target_at(source, index);
        if (name == ZR_NULL || !type_inference_branch_assignment_plan_add(state, target, name)) {
            return ZR_FALSE;
        }
    }
    if (source->hasAssignment) {
        target->hasAssignment = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_branch_assignment_analyze_expression(
        SZrState *state,
        SZrAstNode *expression,
        SZrTypeInferenceBranchAssignmentPlan *plan) {
    SZrString *name = ZR_NULL;
    SZrAstNode *right = ZR_NULL;

    if (expression == ZR_NULL) {
        return ZR_TRUE;
    }
    if (expression->type == ZR_AST_IF_EXPRESSION) {
        return type_inference_branch_assignment_analyze(state,
                                                        expression->data.ifExpression.thenExpr,
                                                        plan) &&
               type_inference_branch_assignment_analyze(state,
                                                        expression->data.ifExpression.elseExpr,
                                                        plan);
    }
    if (expression->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_TRUE;
    }
    if (!type_inference_branch_assignment_parts(expression, &name, &right)) {
        return ZR_FALSE;
    }

    return type_inference_branch_assignment_plan_add(state, plan, name);
}

static TZrBool type_inference_branch_assignment_analyze(
        SZrState *state,
        SZrAstNode *branchNode,
        SZrTypeInferenceBranchAssignmentPlan *outPlan) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (state == ZR_NULL || outPlan == ZR_NULL || !outPlan->isInitialized) {
        return ZR_FALSE;
    }

    if (branchNode == ZR_NULL) {
        return ZR_TRUE;
    }

    if (branchNode->type != ZR_AST_BLOCK) {
        return type_inference_branch_assignment_analyze_expression(
                state,
                type_inference_branch_assignment_statement_expression(branchNode),
                outPlan);
    }

    body = branchNode->data.block.body;
    if (body == ZR_NULL || body->count == 0 || body->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < body->count; index++) {
        if (!type_inference_branch_assignment_analyze_expression(
                state,
                type_inference_branch_assignment_statement_expression(body->nodes[index]),
                outPlan)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool type_inference_branch_assignment_numeric_range(const SZrInferredType *type,
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

static TZrBool type_inference_branch_assignment_result_init(SZrState *state,
                                                            SZrTypeInferenceBranchAssignmentResult *result,
                                                            TZrSize capacity) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      &result->bindings,
                      sizeof(SZrTypeInferenceBranchAssignmentBindingType),
                      capacity > 0 ? capacity : ZR_PARSER_INITIAL_CAPACITY_TINY);
    result->isInitialized = ZR_TRUE;
    return ZR_TRUE;
}

static void type_inference_branch_assignment_result_free(SZrState *state,
                                                         SZrTypeInferenceBranchAssignmentResult *result) {
    TZrSize index;

    if (state == ZR_NULL || result == ZR_NULL || !result->isInitialized) {
        return;
    }

    for (index = 0; index < result->bindings.length; index++) {
        SZrTypeInferenceBranchAssignmentBindingType *binding =
                (SZrTypeInferenceBranchAssignmentBindingType *)ZrCore_Array_Get(&result->bindings, index);
        if (binding != ZR_NULL && binding->hasType) {
            ZrParser_InferredType_Free(state, &binding->type);
            binding->hasType = ZR_FALSE;
        }
    }
    ZrCore_Array_Free(state, &result->bindings);
    result->isInitialized = ZR_FALSE;
}

static SZrTypeInferenceBranchAssignmentBindingType *type_inference_branch_assignment_result_find(
        SZrTypeInferenceBranchAssignmentResult *result,
        SZrString *name) {
    TZrSize index;

    if (result == ZR_NULL || name == ZR_NULL || !result->isInitialized) {
        return ZR_NULL;
    }

    for (index = 0; index < result->bindings.length; index++) {
        SZrTypeInferenceBranchAssignmentBindingType *binding =
                (SZrTypeInferenceBranchAssignmentBindingType *)ZrCore_Array_Get(&result->bindings, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }
    return ZR_NULL;
}

static const SZrInferredType *type_inference_branch_assignment_result_find_type(
        SZrTypeInferenceBranchAssignmentResult *result,
        SZrString *name) {
    SZrTypeInferenceBranchAssignmentBindingType *binding =
            type_inference_branch_assignment_result_find(result, name);

    return binding != ZR_NULL && binding->hasType ? &binding->type : ZR_NULL;
}

static TZrBool type_inference_branch_assignment_result_store_type(
        SZrState *state,
        SZrTypeInferenceBranchAssignmentResult *result,
        SZrString *name,
        const SZrInferredType *type) {
    SZrTypeInferenceBranchAssignmentBindingType *existing;
    SZrTypeInferenceBranchAssignmentBindingType binding;

    if (state == ZR_NULL || result == ZR_NULL || name == ZR_NULL || type == ZR_NULL || !result->isInitialized) {
        return ZR_FALSE;
    }

    existing = type_inference_branch_assignment_result_find(result, name);
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

static TZrBool type_inference_branch_assignment_copy_numeric_binding_type(SZrCompilerState *cs,
                                                                          const SZrTypeBinding *binding,
                                                                          SZrInferredType *outType) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        binding == ZR_NULL ||
        outType == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_NUMBER(binding->type.baseType) ||
        !type_inference_branch_assignment_numeric_range(&binding->type, &minValue, &maxValue)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(cs->state, outType, &binding->type);
    return ZR_TRUE;
}

static TZrBool type_inference_branch_assignment_retained_branch_type(SZrCompilerState *cs,
                                                                     SZrAstNode *condition,
                                                                     SZrString *name,
                                                                     TZrBool falseBranch,
                                                                     SZrInferredType *outType) {
    SZrTypeInferenceBranchScope scope;
    const SZrTypeBinding *binding;
    TZrBool pushed;
    TZrBool copied;

    if (cs == ZR_NULL || name == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&scope, 0, sizeof(scope));
    pushed = falseBranch
                 ? ZrParser_TypeInference_PushFalseBranchNumericRangeScope(cs, condition, &scope)
                 : ZrParser_TypeInference_PushTrueBranchNumericRangeScope(cs, condition, &scope);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    copied = type_inference_branch_assignment_copy_numeric_binding_type(cs, binding, outType);
    if (pushed) {
        ZrParser_TypeInference_PopBranchScope(cs, &scope);
    }
    return copied;
}

static TZrBool type_inference_branch_assignment_push_replay_scope(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        TZrBool falseBranch,
        SZrTypeInferenceBranchScope *scope) {
    SZrTypeEnvironment *newEnv;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || scope == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(scope, 0, sizeof(*scope));
    if (falseBranch
            ? ZrParser_TypeInference_PushFalseBranchNumericRangeScope(cs, condition, scope)
            : ZrParser_TypeInference_PushTrueBranchNumericRangeScope(cs, condition, scope)) {
        return ZR_TRUE;
    }

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

static TZrBool type_inference_branch_assignment_store_replayed_binding(SZrCompilerState *cs,
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

static TZrBool type_inference_branch_assignment_replay_assignment(SZrCompilerState *cs,
                                                                  SZrAstNode *assignmentNode,
                                                                  SZrTypeInferenceBranchAssignmentResult *result) {
    SZrString *name = ZR_NULL;
    SZrAstNode *right = ZR_NULL;
    SZrInferredType rhsType;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrBool success;

    if (cs == ZR_NULL || assignmentNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!type_inference_branch_assignment_parts(assignmentNode, &name, &right)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &rhsType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, right, &rhsType) &&
              type_inference_branch_assignment_numeric_range(&rhsType, &minValue, &maxValue) &&
              type_inference_branch_assignment_store_replayed_binding(cs, name, &rhsType) &&
              type_inference_branch_assignment_result_store_type(cs->state, result, name, &rhsType);
    ZrParser_InferredType_Free(cs->state, &rhsType);
    return success;
}

static TZrBool type_inference_branch_assignment_store_current_plan_bindings(
        SZrCompilerState *cs,
        const SZrTypeInferenceBranchAssignmentPlan *plan,
        SZrTypeInferenceBranchAssignmentResult *result) {
    TZrSize index;

    if (cs == ZR_NULL ||
        plan == ZR_NULL ||
        result == ZR_NULL ||
        !plan->isInitialized ||
        !result->isInitialized) {
        return ZR_FALSE;
    }

    for (index = 0; index < plan->targetNames.length; index++) {
        SZrString *name = type_inference_branch_assignment_plan_target_at(plan, index);
        const SZrTypeBinding *binding;

        if (name == ZR_NULL) {
            return ZR_FALSE;
        }
        binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
        if (binding == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NUMBER(binding->type.baseType) ||
            !binding->type.hasRangeConstraint ||
            !type_inference_branch_assignment_result_store_type(cs->state, result, name, &binding->type)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool type_inference_branch_assignment_replay_nested_if(
        SZrCompilerState *cs,
        SZrAstNode *ifNode,
        const SZrTypeInferenceBranchAssignmentPlan *plan,
        SZrTypeInferenceBranchAssignmentResult *result) {
    if (cs == ZR_NULL ||
        ifNode == ZR_NULL ||
        ifNode->type != ZR_AST_IF_EXPRESSION ||
        plan == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrParser_TypeInference_TryJoinIfElseNumericAssignments(cs, ifNode) &&
           type_inference_branch_assignment_store_current_plan_bindings(cs, plan, result);
}

static TZrBool type_inference_branch_assignment_replay_branch(SZrCompilerState *cs,
                                                              SZrAstNode *branchNode,
                                                              const SZrTypeInferenceBranchAssignmentPlan *plan,
                                                              SZrTypeInferenceBranchAssignmentResult *result) {
    SZrAstNodeArray *body;
    TZrBool replayed;
    TZrSize index;

    if (cs == ZR_NULL ||
        branchNode == ZR_NULL ||
        plan == ZR_NULL ||
        result == ZR_NULL ||
        !result->isInitialized) {
        return ZR_FALSE;
    }

    if (branchNode->type != ZR_AST_BLOCK) {
        SZrAstNode *expression = type_inference_branch_assignment_statement_expression(branchNode);
        if (expression != ZR_NULL && expression->type == ZR_AST_IF_EXPRESSION) {
            return type_inference_branch_assignment_replay_nested_if(cs, expression, plan, result);
        }
        return type_inference_branch_assignment_replay_assignment(cs, expression, result);
    }

    body = branchNode->data.block.body;
    if (body == ZR_NULL || body->count == 0 || body->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    replayed = ZR_FALSE;
    for (index = 0; index < body->count; index++) {
        SZrAstNode *expression = type_inference_branch_assignment_statement_expression(body->nodes[index]);
        SZrString *name = ZR_NULL;
        SZrAstNode *right = ZR_NULL;

        if (expression == ZR_NULL) {
            continue;
        }
        if (expression->type == ZR_AST_IF_EXPRESSION) {
            if (!type_inference_branch_assignment_replay_nested_if(cs, expression, plan, result)) {
                return ZR_FALSE;
            }
            replayed = ZR_TRUE;
            continue;
        }
        if (expression->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
            continue;
        }
        if (!type_inference_branch_assignment_parts(expression, &name, &right)) {
            return ZR_FALSE;
        }
        if (!type_inference_branch_assignment_replay_assignment(cs, expression, result)) {
            return ZR_FALSE;
        }
        replayed = ZR_TRUE;
    }
    return replayed;
}

static TZrBool type_inference_branch_assignment_infer_plan(SZrCompilerState *cs,
                                                           SZrAstNode *condition,
                                                           SZrAstNode *branchNode,
                                                           const SZrTypeInferenceBranchAssignmentPlan *plan,
                                                           TZrBool falseBranch,
                                                           SZrTypeInferenceBranchAssignmentResult *result) {
    SZrTypeInferenceBranchScope scope;
    TZrBool pushed;
    TZrBool success;

    if (cs == ZR_NULL || branchNode == ZR_NULL || plan == ZR_NULL || !plan->hasAssignment || result == ZR_NULL) {
        return ZR_FALSE;
    }

    pushed = type_inference_branch_assignment_push_replay_scope(cs, condition, falseBranch, &scope);
    success = pushed && type_inference_branch_assignment_replay_branch(cs, branchNode, plan, result);
    if (pushed) {
        ZrParser_TypeInference_PopBranchScope(cs, &scope);
    }
    return success;
}

static TZrBool type_inference_branch_assignment_join_type(SZrCompilerState *cs,
                                                          const SZrTypeBinding *targetBinding,
                                                          const SZrInferredType *thenType,
                                                          const SZrInferredType *elseType,
                                                          SZrInferredType *outType) {
    SZrInferredType commonType;
    TZrInt64 thenMin;
    TZrInt64 thenMax;
    TZrInt64 elseMin;
    TZrInt64 elseMax;
    TZrBool hasCommon;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        targetBinding == ZR_NULL ||
        thenType == ZR_NULL ||
        elseType == ZR_NULL ||
        outType == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType) ||
        !type_inference_branch_assignment_numeric_range(thenType, &thenMin, &thenMax) ||
        !type_inference_branch_assignment_numeric_range(elseType, &elseMin, &elseMax)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &commonType, ZR_VALUE_TYPE_OBJECT);
    hasCommon = ZrParser_InferredType_GetCommonType(cs->state, &commonType, thenType, elseType);
    if (!hasCommon || !ZR_VALUE_IS_TYPE_NUMBER(commonType.baseType)) {
        ZrParser_InferredType_Free(cs->state, &commonType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(cs->state, outType, &targetBinding->type);
    outType->baseType = targetBinding->type.baseType;
    outType->hasRangeConstraint = ZR_TRUE;
    outType->minValue = thenMin < elseMin ? thenMin : elseMin;
    outType->maxValue = thenMax > elseMax ? thenMax : elseMax;
    if (!ZrParser_TypeInferenceBranchAssignment_ApplyJoinedSegments(cs, thenType, elseType, outType)) {
        ZrParser_InferredType_Free(cs->state, &commonType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(cs->state, &commonType);
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInference_TryJoinIfElseNumericAssignments(SZrCompilerState *cs,
                                                               SZrAstNode *ifNode) {
    SZrIfExpression *ifExpr;
    SZrTypeInferenceBranchAssignmentPlan thenPlan;
    SZrTypeInferenceBranchAssignmentPlan elsePlan;
    SZrTypeInferenceBranchAssignmentPlan targetPlan;
    SZrTypeInferenceBranchAssignmentResult thenResult;
    SZrTypeInferenceBranchAssignmentResult elseResult;
    SZrTypeInferenceBranchAssignmentResult joinedResult;
    TZrBool initialized;
    TZrBool joined = ZR_FALSE;
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        ifNode == ZR_NULL ||
        ifNode->type != ZR_AST_IF_EXPRESSION) {
        return ZR_FALSE;
    }

    ifExpr = &ifNode->data.ifExpression;
    if (ifExpr->thenExpr == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&thenPlan, 0, sizeof(thenPlan));
    memset(&elsePlan, 0, sizeof(elsePlan));
    memset(&targetPlan, 0, sizeof(targetPlan));
    memset(&thenResult, 0, sizeof(thenResult));
    memset(&elseResult, 0, sizeof(elseResult));
    memset(&joinedResult, 0, sizeof(joinedResult));

    initialized =
            type_inference_branch_assignment_plan_init(cs->state, &thenPlan) &&
            type_inference_branch_assignment_plan_init(cs->state, &elsePlan) &&
            type_inference_branch_assignment_plan_init(cs->state, &targetPlan);
    if (!initialized) {
        goto cleanup;
    }

    if (!type_inference_branch_assignment_analyze(cs->state, ifExpr->thenExpr, &thenPlan) ||
        !type_inference_branch_assignment_analyze(cs->state, ifExpr->elseExpr, &elsePlan) ||
        (!thenPlan.hasAssignment && !elsePlan.hasAssignment) ||
        !type_inference_branch_assignment_plan_extend(cs->state, &targetPlan, &thenPlan) ||
        !type_inference_branch_assignment_plan_extend(cs->state, &targetPlan, &elsePlan)) {
        goto cleanup;
    }

    initialized =
            type_inference_branch_assignment_result_init(
                    cs->state,
                    &thenResult,
                    type_inference_branch_assignment_plan_target_count(&targetPlan)) &&
            type_inference_branch_assignment_result_init(
                    cs->state,
                    &elseResult,
                    type_inference_branch_assignment_plan_target_count(&targetPlan)) &&
            type_inference_branch_assignment_result_init(
                    cs->state,
                    &joinedResult,
                    type_inference_branch_assignment_plan_target_count(&targetPlan));
    if (!initialized) {
        goto cleanup;
    }

    if (thenPlan.hasAssignment &&
        !type_inference_branch_assignment_infer_plan(cs,
                                                     ifExpr->condition,
                                                     ifExpr->thenExpr,
                                                     &thenPlan,
                                                     ZR_FALSE,
                                                     &thenResult)) {
        goto cleanup;
    }
    if (ifExpr->elseExpr != ZR_NULL &&
        elsePlan.hasAssignment &&
        !type_inference_branch_assignment_infer_plan(cs,
                                                     ifExpr->condition,
                                                     ifExpr->elseExpr,
                                                     &elsePlan,
                                                     ZR_TRUE,
                                                     &elseResult)) {
        goto cleanup;
    }

    for (index = 0; index < type_inference_branch_assignment_plan_target_count(&targetPlan); index++) {
        SZrString *targetName = type_inference_branch_assignment_plan_target_at(&targetPlan, index);
        const SZrTypeBinding *targetBinding;
        const SZrInferredType *thenType;
        const SZrInferredType *elseType;
        SZrInferredType retainedThenType;
        SZrInferredType retainedElseType;
        SZrInferredType joinedType;
        TZrBool retainedThenInitialized = ZR_FALSE;
        TZrBool retainedElseInitialized = ZR_FALSE;
        TZrBool joinedTypeInitialized = ZR_FALSE;

        if (targetName == ZR_NULL) {
            goto cleanup;
        }

        targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, targetName);
        if (targetBinding == ZR_NULL || !ZR_VALUE_IS_TYPE_NUMBER(targetBinding->type.baseType)) {
            goto cleanup;
        }

        thenType = type_inference_branch_assignment_result_find_type(&thenResult, targetName);
        if (thenType == ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, &retainedThenType, ZR_VALUE_TYPE_OBJECT);
            retainedThenInitialized = ZR_TRUE;
            if (!type_inference_branch_assignment_retained_branch_type(cs,
                                                                       ifExpr->condition,
                                                                       targetName,
                                                                       ZR_FALSE,
                                                                       &retainedThenType)) {
                if (retainedThenInitialized) {
                    ZrParser_InferredType_Free(cs->state, &retainedThenType);
                }
                goto cleanup;
            }
            thenType = &retainedThenType;
        }

        elseType = type_inference_branch_assignment_result_find_type(&elseResult, targetName);
        if (elseType == ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, &retainedElseType, ZR_VALUE_TYPE_OBJECT);
            retainedElseInitialized = ZR_TRUE;
            if (!type_inference_branch_assignment_retained_branch_type(cs,
                                                                       ifExpr->condition,
                                                                       targetName,
                                                                       ZR_TRUE,
                                                                       &retainedElseType)) {
                if (retainedThenInitialized) {
                    ZrParser_InferredType_Free(cs->state, &retainedThenType);
                }
                if (retainedElseInitialized) {
                    ZrParser_InferredType_Free(cs->state, &retainedElseType);
                }
                goto cleanup;
            }
            elseType = &retainedElseType;
        }

        ZrParser_InferredType_Init(cs->state, &joinedType, ZR_VALUE_TYPE_OBJECT);
        joinedTypeInitialized = ZR_TRUE;
        if (!type_inference_branch_assignment_join_type(cs, targetBinding, thenType, elseType, &joinedType) ||
            !type_inference_branch_assignment_result_store_type(cs->state,
                                                                &joinedResult,
                                                                targetName,
                                                                &joinedType)) {
            if (joinedTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &joinedType);
            }
            if (retainedThenInitialized) {
                ZrParser_InferredType_Free(cs->state, &retainedThenType);
            }
            if (retainedElseInitialized) {
                ZrParser_InferredType_Free(cs->state, &retainedElseType);
            }
            goto cleanup;
        }

        if (joinedTypeInitialized) {
            ZrParser_InferredType_Free(cs->state, &joinedType);
        }
        if (retainedThenInitialized) {
            ZrParser_InferredType_Free(cs->state, &retainedThenType);
        }
        if (retainedElseInitialized) {
            ZrParser_InferredType_Free(cs->state, &retainedElseType);
        }
    }

    joined = ZR_TRUE;
    for (index = 0; index < joinedResult.bindings.length; index++) {
        SZrTypeInferenceBranchAssignmentBindingType *binding =
                (SZrTypeInferenceBranchAssignmentBindingType *)ZrCore_Array_Get(&joinedResult.bindings, index);
        if (binding == ZR_NULL ||
            binding->name == ZR_NULL ||
            !binding->hasType ||
            !ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, binding->name, &binding->type)) {
            joined = ZR_FALSE;
            break;
        }
    }

cleanup:
    type_inference_branch_assignment_result_free(cs->state, &joinedResult);
    type_inference_branch_assignment_result_free(cs->state, &elseResult);
    type_inference_branch_assignment_result_free(cs->state, &thenResult);
    type_inference_branch_assignment_plan_free(cs->state, &targetPlan);
    type_inference_branch_assignment_plan_free(cs->state, &elsePlan);
    type_inference_branch_assignment_plan_free(cs->state, &thenPlan);
    return joined;
}
