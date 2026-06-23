//
// Created by Auto on 2026/06/23.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"

#include "type_inference_loop_assignment_join_internal.h"
#include "type_inference_loop_assignment_syntax.h"
#include "type_inference_constant_eval.h"
#include "type_inference_semantic_facts.h"

#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool type_inference_loop_assignment_type_has_numeric_range(
        const SZrInferredType *type) {
    return type != ZR_NULL &&
           ZR_VALUE_IS_TYPE_NUMBER(type->baseType) &&
           type->hasRangeConstraint;
}

static TZrBool type_inference_loop_assignment_condition_is_known_bool(SZrCompilerState *cs,
                                                                      SZrAstNode *condition,
                                                                      TZrBool *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = ZR_FALSE;
    }
    if (condition == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (condition->type == ZR_AST_BOOLEAN_LITERAL) {
        *outValue = condition->data.booleanLiteral.value;
        return ZR_TRUE;
    }
    if (type_inference_logical_fact_known_bool_value(cs, condition, outValue, ZR_NULL)) {
        return ZR_TRUE;
    }
    return type_inference_node_bool_value(condition, outValue);
}

static TZrBool type_inference_loop_assignment_identifier_name(SZrAstNode *node,
                                                              SZrString **outName) {
    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (node == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.identifier.name == ZR_NULL ||
        outName == ZR_NULL) {
        return ZR_FALSE;
    }

    *outName = node->data.identifier.name;
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_push_for_init_scope(
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

static void type_inference_loop_assignment_pop_for_init_scope(
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

static TZrBool type_inference_loop_assignment_apply_for_init_assignment(
        SZrCompilerState *cs,
        SZrAstNode *init) {
    SZrAstNode *expression;
    SZrString *name = ZR_NULL;
    SZrAstNode *right = ZR_NULL;
    const SZrTypeBinding *targetBinding;
    SZrInferredType assignmentType;
    SZrInferredType updatedType;
    TZrBool assignmentTypeInitialized = ZR_FALSE;
    TZrBool updatedTypeInitialized = ZR_FALSE;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || init == ZR_NULL) {
        return ZR_FALSE;
    }

    expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(init);
    if (!ZrParser_TypeInferenceLoopAssignment_AssignmentParts(expression, &name, &right) ||
        right == ZR_NULL) {
        return ZR_FALSE;
    }

    targetBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    if (targetBinding == ZR_NULL ||
        !type_inference_loop_assignment_type_has_numeric_range(&targetBinding->type)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &assignmentType, ZR_VALUE_TYPE_OBJECT);
    assignmentTypeInitialized = ZR_TRUE;
    ZrParser_InferredType_Init(cs->state, &updatedType, ZR_VALUE_TYPE_OBJECT);
    updatedTypeInitialized = ZR_TRUE;

    if (ZrParser_AssignmentType_Infer(cs, expression, &assignmentType)) {
        ZrParser_InferredType_Copy(cs->state, &updatedType, &targetBinding->type);
        if (ZrParser_TypeInference_TryApplyInitializerNumericRange(cs->state,
                                                                   &updatedType,
                                                                   &assignmentType) &&
            ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(cs, name, &updatedType)) {
            success = ZR_TRUE;
        }
    }

    if (updatedTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &updatedType);
    }
    if (assignmentTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &assignmentType);
    }
    return success;
}

static TZrBool type_inference_loop_assignment_apply_for_init_declaration(
        SZrCompilerState *cs,
        SZrAstNode *init,
        SZrString **outLoopLocalName) {
    SZrVariableDeclaration *declaration;
    SZrString *name = ZR_NULL;
    SZrInferredType bindingType;
    TZrBool bindingTypeInitialized = ZR_FALSE;
    TZrBool hasBindingType = ZR_FALSE;
    TZrBool success = ZR_FALSE;

    if (outLoopLocalName != ZR_NULL) {
        *outLoopLocalName = ZR_NULL;
    }
    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        init == ZR_NULL ||
        init->type != ZR_AST_VARIABLE_DECLARATION ||
        outLoopLocalName == ZR_NULL) {
        return ZR_FALSE;
    }

    declaration = &init->data.variableDeclaration;
    if (declaration->value == ZR_NULL ||
        !type_inference_loop_assignment_identifier_name(declaration->pattern, &name)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    bindingTypeInitialized = ZR_TRUE;
    if (declaration->typeInfo != ZR_NULL) {
        hasBindingType = ZrParser_AstTypeToInferredType_Convert(cs, declaration->typeInfo, &bindingType);
        if (hasBindingType) {
            SZrInferredType initializerType;
            ZrParser_InferredType_Init(cs->state, &initializerType, ZR_VALUE_TYPE_OBJECT);
            if (ZrParser_ExpressionType_Infer(cs, declaration->value, &initializerType)) {
                (void)ZrParser_TypeInference_TryApplyInitializerNumericRange(cs->state,
                                                                             &bindingType,
                                                                             &initializerType);
            }
            ZrParser_InferredType_Free(cs->state, &initializerType);
        }
    } else {
        hasBindingType = ZrParser_ExpressionType_Infer(cs, declaration->value, &bindingType);
    }

    if (hasBindingType &&
        type_inference_loop_assignment_type_has_numeric_range(&bindingType) &&
        ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, name, &bindingType)) {
        *outLoopLocalName = name;
        success = ZR_TRUE;
    }

    if (bindingTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
    }
    return success;
}

static TZrBool type_inference_loop_assignment_apply_for_init(
        SZrCompilerState *cs,
        SZrAstNode *init,
        SZrString **outLoopLocalName) {
    if (outLoopLocalName != ZR_NULL) {
        *outLoopLocalName = ZR_NULL;
    }
    if (type_inference_loop_assignment_apply_for_init_assignment(cs, init)) {
        return ZR_TRUE;
    }
    return type_inference_loop_assignment_apply_for_init_declaration(cs, init, outLoopLocalName);
}

static TZrBool type_inference_loop_assignment_binding_is_skipped(
        const SZrTypeBinding *binding,
        SZrString *skipName) {
    return binding != ZR_NULL &&
           binding->name != ZR_NULL &&
           skipName != ZR_NULL &&
           ZrCore_String_Equal(binding->name, skipName);
}

static TZrBool type_inference_loop_assignment_commit_scope_except(
        SZrCompilerState *cs,
        SZrTypeEnvironment *sourceEnv,
        SZrTypeEnvironment *targetEnv,
        SZrString *skipName) {
    TZrSize index;
    TZrSize committedCount = 0;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        sourceEnv == ZR_NULL ||
        targetEnv == ZR_NULL ||
        sourceEnv->variableTypes.length == 0) {
        return ZR_FALSE;
    }

    for (index = 0; index < sourceEnv->variableTypes.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&sourceEnv->variableTypes, index);
        if (type_inference_loop_assignment_binding_is_skipped(binding, skipName)) {
            continue;
        }
        if (binding == ZR_NULL ||
            binding->name == ZR_NULL ||
            !type_inference_loop_assignment_type_has_numeric_range(&binding->type)) {
            return ZR_FALSE;
        }
        committedCount++;
    }

    if (committedCount == 0) {
        return ZR_FALSE;
    }

    for (index = 0; index < sourceEnv->variableTypes.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&sourceEnv->variableTypes, index);
        if (type_inference_loop_assignment_binding_is_skipped(binding, skipName)) {
            continue;
        }
        if (!ZrParser_TypeEnvironment_RegisterVariable(cs->state, targetEnv, binding->name, &binding->type)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_join_body_with_optional_step(
        SZrCompilerState *cs,
        SZrAstNode *body,
        SZrAstNode *step) {
    SZrAstNode *expression;

    if (step == ZR_NULL) {
        return ZrParser_TypeInferenceLoopAssignment_JoinBody(cs, body);
    }

    expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(step);
    if (expression != ZR_NULL && expression->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZrParser_TypeInferenceLoopAssignment_JoinBodyWithStep(cs, body, step);
    }
    return ZrParser_TypeInferenceLoopAssignment_JoinBody(cs, body);
}

static TZrBool type_inference_loop_assignment_statement_guarantees_break_before_step(
        SZrCompilerState *cs,
        SZrAstNode *statement) {
    SZrAstNode *expression;
    SZrIfExpression *ifExpression;
    SZrAstNodeArray *bodyStatements;
    TZrBool conditionValue;
    TZrSize index;

    if (statement == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(statement)) {
        return ZR_TRUE;
    }
    if (statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
        return ZR_FALSE;
    }
    if (statement->type == ZR_AST_BLOCK) {
        bodyStatements = statement->data.block.body;
        if (bodyStatements == ZR_NULL || bodyStatements->nodes == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0; index < bodyStatements->count; index++) {
            if (type_inference_loop_assignment_statement_guarantees_break_before_step(
                    cs,
                    bodyStatements->nodes[index])) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(statement);
    if (expression == ZR_NULL || expression->type != ZR_AST_IF_EXPRESSION) {
        return ZR_FALSE;
    }

    ifExpression = &expression->data.ifExpression;
    if (!type_inference_loop_assignment_condition_is_known_bool(
            cs,
            ifExpression->condition,
            &conditionValue)) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_statement_guarantees_break_before_step(
            cs,
            conditionValue ? ifExpression->thenExpr : ifExpression->elseExpr);
}

static TZrBool type_inference_loop_assignment_body_has_top_level_plain_break(
        SZrCompilerState *cs,
        SZrAstNode *body) {
    if (body == ZR_NULL) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_statement_guarantees_break_before_step(cs, body);
}

static TZrBool type_inference_loop_assignment_join_body_at_least_once_before_step(
        SZrCompilerState *cs,
        SZrAstNode *body,
        SZrAstNode *step) {
    if (step != ZR_NULL && !type_inference_loop_assignment_body_has_top_level_plain_break(cs, body)) {
        return ZR_FALSE;
    }
    return ZrParser_TypeInferenceLoopAssignment_JoinBodyAtLeastOnce(cs, body);
}

static TZrBool type_inference_loop_assignment_join_for_body_at_least_once_no_header_effects(
        SZrCompilerState *cs,
        SZrForLoop *forLoop) {
    if (forLoop == ZR_NULL || forLoop->init != ZR_NULL) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_join_body_at_least_once_before_step(
            cs,
            forLoop->block,
            forLoop->step);
}

static TZrBool type_inference_loop_assignment_join_for_with_init(
        SZrCompilerState *cs,
        SZrForLoop *forLoop,
        TZrBool bodyRunsAtLeastOnce) {
    SZrTypeInferenceBranchScope scope;
    SZrTypeEnvironment *joinedEnv;
    SZrString *loopLocalInitName = ZR_NULL;
    TZrBool pushed;
    TZrBool success;

    if (cs == ZR_NULL || forLoop == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&scope, 0, sizeof(scope));
    pushed = type_inference_loop_assignment_push_for_init_scope(cs, &scope);
    success = pushed &&
              type_inference_loop_assignment_apply_for_init(cs, forLoop->init, &loopLocalInitName) &&
              (bodyRunsAtLeastOnce
                       ? type_inference_loop_assignment_join_body_at_least_once_before_step(
                                 cs,
                                 forLoop->block,
                                 forLoop->step)
                       : type_inference_loop_assignment_join_body_with_optional_step(
                                 cs,
                                 forLoop->block,
                                 forLoop->step));
    joinedEnv = cs != ZR_NULL ? cs->typeEnv : ZR_NULL;
    if (success) {
        success = type_inference_loop_assignment_commit_scope_except(
                cs,
                joinedEnv,
                scope.savedEnv,
                loopLocalInitName);
    }
    if (pushed) {
        type_inference_loop_assignment_pop_for_init_scope(cs, &scope);
    }
    return success;
}

static TZrBool type_inference_loop_assignment_apply_false_condition_for_init(
        SZrCompilerState *cs,
        SZrForLoop *forLoop) {
    if (forLoop == ZR_NULL || forLoop->init == ZR_NULL) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_apply_for_init_assignment(cs, forLoop->init);
}

static TZrBool type_inference_loop_assignment_iterable_is_nonempty(
        const SZrInferredType *iterableType) {
    return iterableType != ZR_NULL &&
           iterableType->baseType == ZR_VALUE_TYPE_ARRAY &&
           iterableType->hasArraySizeConstraint &&
           (iterableType->arrayMinSize > 0 || iterableType->arrayFixedSize > 0);
}

static TZrBool type_inference_loop_assignment_register_foreach_item(
        SZrCompilerState *cs,
        SZrForeachLoop *foreachLoop,
        SZrString **outItemName,
        TZrBool *outIterableIsNonempty) {
    SZrString *itemName;
    SZrInferredType iterableType;
    SZrInferredType elementType;
    TZrBool success;

    if (outItemName != ZR_NULL) {
        *outItemName = ZR_NULL;
    }
    if (outIterableIsNonempty != ZR_NULL) {
        *outIterableIsNonempty = ZR_FALSE;
    }
    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        foreachLoop == ZR_NULL ||
        foreachLoop->expr == ZR_NULL ||
        outItemName == ZR_NULL ||
        !type_inference_loop_assignment_identifier_name(foreachLoop->pattern, &itemName)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &iterableType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, foreachLoop->expr, &iterableType) &&
              bind_foreach_element_type_from_inferred_iterable(cs, &iterableType, &elementType) &&
              ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, itemName, &elementType);
    if (success) {
        *outItemName = itemName;
        if (outIterableIsNonempty != ZR_NULL) {
            *outIterableIsNonempty =
                    type_inference_loop_assignment_iterable_is_nonempty(&iterableType);
        }
    }
    ZrParser_InferredType_Free(cs->state, &elementType);
    ZrParser_InferredType_Free(cs->state, &iterableType);
    return success;
}

static TZrBool type_inference_loop_assignment_join_foreach_with_item(
        SZrCompilerState *cs,
        SZrForeachLoop *foreachLoop) {
    SZrTypeInferenceBranchScope scope;
    SZrTypeEnvironment *joinedEnv;
    SZrString *itemName = ZR_NULL;
    TZrBool iterableIsNonempty = ZR_FALSE;
    TZrBool pushed;
    TZrBool success;

    if (cs == ZR_NULL || foreachLoop == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&scope, 0, sizeof(scope));
    pushed = type_inference_loop_assignment_push_for_init_scope(cs, &scope);
    success = pushed &&
              type_inference_loop_assignment_register_foreach_item(
                      cs,
                      foreachLoop,
                      &itemName,
                      &iterableIsNonempty) &&
              (iterableIsNonempty
                       ? ZrParser_TypeInferenceLoopAssignment_JoinBodyAtLeastOnce(cs, foreachLoop->block)
                       : ZrParser_TypeInferenceLoopAssignment_JoinBody(cs, foreachLoop->block));
    joinedEnv = cs != ZR_NULL ? cs->typeEnv : ZR_NULL;
    if (success) {
        success = type_inference_loop_assignment_commit_scope_except(
                cs,
                joinedEnv,
                scope.savedEnv,
                itemName);
    }
    if (pushed) {
        type_inference_loop_assignment_pop_for_init_scope(cs, &scope);
    }
    return success;
}

TZrBool ZrParser_TypeInference_TryJoinWhileNumericAssignments(SZrCompilerState *cs,
                                                              SZrAstNode *whileNode) {
    SZrWhileLoop *whileLoop;
    TZrBool conditionValue;

    if (whileNode == ZR_NULL || whileNode->type != ZR_AST_WHILE_LOOP) {
        return ZR_FALSE;
    }

    whileLoop = &whileNode->data.whileLoop;
    if (type_inference_loop_assignment_condition_is_known_bool(cs, whileLoop->cond, &conditionValue)) {
        return ZR_FALSE;
    }

    return ZrParser_TypeInferenceLoopAssignment_JoinBody(cs, whileLoop->block);
}

TZrBool ZrParser_TypeInference_TryJoinForNumericAssignments(SZrCompilerState *cs,
                                                            SZrAstNode *forNode) {
    SZrForLoop *forLoop;
    TZrBool conditionValue;

    if (forNode == ZR_NULL || forNode->type != ZR_AST_FOR_LOOP) {
        return ZR_FALSE;
    }

    forLoop = &forNode->data.forLoop;
    if (forLoop->cond == ZR_NULL) {
        if (forLoop->init != ZR_NULL) {
            return type_inference_loop_assignment_join_for_with_init(cs, forLoop, ZR_TRUE);
        }
        return type_inference_loop_assignment_join_for_body_at_least_once_no_header_effects(
                cs,
                forLoop);
    }

    if (type_inference_loop_assignment_condition_is_known_bool(cs, forLoop->cond, &conditionValue)) {
        if (!conditionValue) {
            return type_inference_loop_assignment_apply_false_condition_for_init(cs, forLoop);
        }
        if (forLoop->init != ZR_NULL) {
            return type_inference_loop_assignment_join_for_with_init(cs, forLoop, ZR_TRUE);
        }
        return type_inference_loop_assignment_join_for_body_at_least_once_no_header_effects(
                cs,
                forLoop);
    }

    if (forLoop->init != ZR_NULL) {
        return type_inference_loop_assignment_join_for_with_init(cs, forLoop, ZR_FALSE);
    }

    return type_inference_loop_assignment_join_body_with_optional_step(
            cs,
            forLoop->block,
            forLoop->step);
}

TZrBool ZrParser_TypeInference_TryJoinForeachNumericAssignments(SZrCompilerState *cs,
                                                                SZrAstNode *foreachNode) {
    SZrForeachLoop *foreachLoop;

    if (foreachNode == ZR_NULL || foreachNode->type != ZR_AST_FOREACH_LOOP) {
        return ZR_FALSE;
    }

    foreachLoop = &foreachNode->data.foreachLoop;
    if (type_inference_loop_assignment_join_foreach_with_item(cs, foreachLoop)) {
        return ZR_TRUE;
    }
    return ZrParser_TypeInferenceLoopAssignment_JoinBody(cs, foreachLoop->block);
}
