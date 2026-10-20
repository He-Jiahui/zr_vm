#include "type_inference_control_flow_expression.h"

#include "zr_vm_parser/compiler.h"

TZrBool type_inference_control_flow_expression(
        SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    TZrBool success = ZR_FALSE;

    switch (node->type) {
        case ZR_AST_WHILE_LOOP: {
            SZrInferredType conditionType;
            ZrParser_InferredType_Init(cs->state, &conditionType, ZR_VALUE_TYPE_OBJECT);
            if (node->data.whileLoop.cond != ZR_NULL) {
                (void)ZrParser_ExpressionType_Infer(cs, node->data.whileLoop.cond, &conditionType);
            }
            ZrParser_InferredType_Free(cs->state, &conditionType);
            (void)ZrParser_TypeInference_TryJoinWhileNumericAssignments(cs, node);
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            success = ZR_TRUE;
            break;
        }

        case ZR_AST_FOR_LOOP: {
            SZrInferredType conditionType;
            ZrParser_InferredType_Init(cs->state, &conditionType, ZR_VALUE_TYPE_OBJECT);
            if (node->data.forLoop.cond != ZR_NULL) {
                (void)ZrParser_ExpressionType_Infer(cs, node->data.forLoop.cond, &conditionType);
            }
            ZrParser_InferredType_Free(cs->state, &conditionType);
            (void)ZrParser_TypeInference_TryJoinForNumericAssignments(cs, node);
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            success = ZR_TRUE;
            break;
        }

        case ZR_AST_FOREACH_LOOP: {
            SZrInferredType iterableType;
            ZrParser_InferredType_Init(cs->state, &iterableType, ZR_VALUE_TYPE_OBJECT);
            if (node->data.foreachLoop.expr != ZR_NULL) {
                (void)ZrParser_ExpressionType_Infer(cs, node->data.foreachLoop.expr, &iterableType);
            }
            ZrParser_InferredType_Free(cs->state, &iterableType);
            (void)ZrParser_TypeInference_TryJoinForeachNumericAssignments(cs, node);
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            success = ZR_TRUE;
            break;
        }
        case ZR_AST_IF_EXPRESSION:
            // 实现if expression的类型推断
            // if expression的类型是thenExpr和elseExpr的公共类型
            {
                SZrIfExpression *ifExpr = &node->data.ifExpression;
                SZrInferredType thenType, elseType;
                SZrTypeInferenceBranchScope branchScope;
                TZrBool hasThenType = ZR_FALSE;
                TZrBool hasElseType = ZR_FALSE;
                ZrParser_InferredType_Init(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
                if (ifExpr->thenExpr != ZR_NULL && ifExpr->elseExpr != ZR_NULL) {
                    (void)ZrParser_TypeInference_PushTrueBranchNumericRangeScope(cs,
                                                                                  ifExpr->condition,
                                                                                  &branchScope);
                    hasThenType = ZrParser_ExpressionType_Infer(cs, ifExpr->thenExpr, &thenType);
                    ZrParser_TypeInference_PopBranchScope(cs, &branchScope);
                    if (hasThenType) {
                        (void)ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
                            cs,
                            ifExpr->condition,
                            &branchScope);
                        hasElseType = ZrParser_ExpressionType_Infer(cs, ifExpr->elseExpr, &elseType);
                        ZrParser_TypeInference_PopBranchScope(cs, &branchScope);
                    }
                    if (hasThenType && hasElseType) {
                        // 获取公共类型
                        if (ZrParser_InferredType_GetCommonType(cs->state, result, &thenType, &elseType)) {
                            (void)ZrParser_TypeInference_TryJoinIfElseNumericAssignments(cs, node);
                            ZrParser_InferredType_Free(cs->state, &thenType);
                            ZrParser_InferredType_Free(cs->state, &elseType);
                            success = ZR_TRUE;
                            break;
                        }
                        ZrParser_InferredType_Free(cs->state, &thenType);
                        ZrParser_InferredType_Free(cs->state, &elseType);
                    }
                } else if (ifExpr->thenExpr != ZR_NULL) {
                    (void)ZrParser_TypeInference_PushTrueBranchNumericRangeScope(cs,
                                                                                  ifExpr->condition,
                                                                                  &branchScope);
                    success = ZrParser_ExpressionType_Infer(cs, ifExpr->thenExpr, result);
                    ZrParser_TypeInference_PopBranchScope(cs, &branchScope);
                    (void)ZrParser_TypeInference_TryJoinIfElseNumericAssignments(cs, node);
                    if (!success) {
                        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        success = ZR_TRUE;
                    }
                    break;
                } else if (ifExpr->elseExpr != ZR_NULL) {
                    (void)ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
                        cs,
                        ifExpr->condition,
                        &branchScope);
                    success = ZrParser_ExpressionType_Infer(cs, ifExpr->elseExpr, result);
                    ZrParser_TypeInference_PopBranchScope(cs, &branchScope);
                    break;
                }
                // 默认返回对象类型
                (void)ZrParser_TypeInference_TryJoinIfElseNumericAssignments(cs, node);
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                success = ZR_TRUE;
                break;
            }
        case ZR_AST_SWITCH_EXPRESSION:
            // 实现switch expression的类型推断
            // switch expression的类型是所有case和default的公共类型
            {
                SZrSwitchExpression *switchExpr = &node->data.switchExpression;
                SZrInferredType commonType;
                ZrParser_InferredType_Init(cs->state, &commonType, ZR_VALUE_TYPE_OBJECT);
                TZrBool hasType = ZR_FALSE;
                // 遍历所有case，推断类型
                if (switchExpr->cases != ZR_NULL) {
                    for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                        SZrAstNode *caseNode = switchExpr->cases->nodes[i];
                        if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                            if (switchCase->block != ZR_NULL) {
                                SZrInferredType caseType;
                                ZrParser_InferredType_Init(cs->state, &caseType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrParser_ExpressionType_Infer(cs, switchCase->block, &caseType)) {
                                    if (!hasType) {
                                        ZrParser_InferredType_Copy(cs->state, &commonType, &caseType);
                                        hasType = ZR_TRUE;
                                    } else {
                                        SZrInferredType newCommonType;
                                        ZrParser_InferredType_Init(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                        if (ZrParser_InferredType_GetCommonType(cs->state, &newCommonType, &commonType, &caseType)) {
                                            ZrParser_InferredType_Free(cs->state, &commonType);
                                            commonType = newCommonType;
                                        }
                                    }
                                    ZrParser_InferredType_Free(cs->state, &caseType);
                                }
                            }
                        }
                    }
                }
                // 处理default case
                if (switchExpr->defaultCase != ZR_NULL && switchExpr->defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
                    SZrSwitchDefault *switchDefault = &switchExpr->defaultCase->data.switchDefault;
                    if (switchDefault->block != ZR_NULL) {
                        SZrInferredType defaultType;
                        ZrParser_InferredType_Init(cs->state, &defaultType, ZR_VALUE_TYPE_OBJECT);
                        if (ZrParser_ExpressionType_Infer(cs, switchDefault->block, &defaultType)) {
                            if (!hasType) {
                                ZrParser_InferredType_Copy(cs->state, &commonType, &defaultType);
                                hasType = ZR_TRUE;
                            } else {
                                SZrInferredType newCommonType;
                                ZrParser_InferredType_Init(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrParser_InferredType_GetCommonType(cs->state, &newCommonType, &commonType, &defaultType)) {
                                    ZrParser_InferredType_Free(cs->state, &commonType);
                                    commonType = newCommonType;
                                }
                            }
                            ZrParser_InferredType_Free(cs->state, &defaultType);
                        }
                    }
                }
                if (hasType) {
                    ZrParser_InferredType_Copy(cs->state, result, &commonType);
                    ZrParser_InferredType_Free(cs->state, &commonType);
                    success = ZR_TRUE;
                    break;
                }
                // 默认返回对象类型
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                success = ZR_TRUE;
                break;
            }
        default:
            return ZR_FALSE;
    }

    return success;
}
