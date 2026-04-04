//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

static SZrAstNodeArray *compiler_get_current_type_members(const SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->currentTypeNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (cs->currentTypeNode->type == ZR_AST_CLASS_DECLARATION) {
        return cs->currentTypeNode->data.classDeclaration.members;
    }

    if (cs->currentTypeNode->type == ZR_AST_STRUCT_DECLARATION) {
        return cs->currentTypeNode->data.structDeclaration.members;
    }

    return ZR_NULL;
}

static TZrBool compiler_member_is_instance_const_field(SZrAstNode *member) {
    if (member == ZR_NULL) {
        return ZR_FALSE;
    }

    if (member->type == ZR_AST_CLASS_FIELD) {
        return member->data.classField.isConst && !member->data.classField.isStatic;
    }

    if (member->type == ZR_AST_STRUCT_FIELD) {
        return member->data.structField.isConst && !member->data.structField.isStatic;
    }

    return ZR_FALSE;
}

static SZrString *compiler_member_const_field_name(SZrAstNode *member) {
    if (member == ZR_NULL) {
        return ZR_NULL;
    }

    if (member->type == ZR_AST_CLASS_FIELD && member->data.classField.name != ZR_NULL) {
        return member->data.classField.name->name;
    }

    if (member->type == ZR_AST_STRUCT_FIELD && member->data.structField.name != ZR_NULL) {
        return member->data.structField.name->name;
    }

    return ZR_NULL;
}

static TZrBool compiler_member_const_field_has_initializer(SZrAstNode *member) {
    if (member == ZR_NULL) {
        return ZR_FALSE;
    }

    if (member->type == ZR_AST_CLASS_FIELD) {
        return member->data.classField.init != ZR_NULL;
    }

    if (member->type == ZR_AST_STRUCT_FIELD) {
        return member->data.structField.init != ZR_NULL;
    }

    return ZR_FALSE;
}

static TZrBool compiler_constructor_const_field_is_tracked(const SZrCompilerState *cs, SZrString *fieldName) {
    if (cs == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->constructorInitializedConstFields.length; index++) {
        SZrString **trackedName =
                (SZrString **)ZrCore_Array_Get((SZrArray *)&cs->constructorInitializedConstFields, index);
        if (trackedName != ZR_NULL && *trackedName != ZR_NULL && ZrCore_String_Equal(*trackedName, fieldName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_track_constructor_const_field(SZrCompilerState *cs, SZrString *fieldName) {
    if (cs == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(cs->state, &cs->constructorInitializedConstFields, &fieldName);
    return ZR_TRUE;
}

typedef struct SZrTrackedConstructorConstFields {
    SZrString **names;
    TZrSize count;
} SZrTrackedConstructorConstFields;

static TZrInt32 tracked_constructor_const_field_index(const SZrTrackedConstructorConstFields *tracked,
                                                      SZrString *fieldName) {
    if (tracked == ZR_NULL || tracked->names == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_PARSER_I32_NONE;
    }

    for (TZrSize index = 0; index < tracked->count; index++) {
        if (tracked->names[index] != ZR_NULL && ZrCore_String_Equal(tracked->names[index], fieldName)) {
            return (TZrInt32)index;
        }
    }

    return ZR_PARSER_I32_NONE;
}

static void copy_constructor_const_assignment_state(const TZrBool *source, TZrBool *dest, TZrSize count) {
    if (source == ZR_NULL || dest == ZR_NULL || count == 0) {
        return;
    }

    memcpy(dest, source, sizeof(TZrBool) * count);
}

static TZrBool *allocate_constructor_const_assignment_state(SZrCompilerState *cs, TZrSize count) {
    if (cs == ZR_NULL || count == 0) {
        return ZR_NULL;
    }

    return (TZrBool *)ZrCore_Memory_RawMallocWithType(
            cs->state->global, sizeof(TZrBool) * count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

static void free_constructor_const_assignment_state(SZrCompilerState *cs, TZrBool *state, TZrSize count) {
    if (cs == ZR_NULL || state == ZR_NULL || count == 0) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  state,
                                  sizeof(TZrBool) * count,
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

static void intersect_constructor_const_assignment_state(const TZrBool *lhs,
                                                         const TZrBool *rhs,
                                                         TZrBool *dest,
                                                         TZrSize count) {
    if (lhs == ZR_NULL || rhs == ZR_NULL || dest == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < count; index++) {
        dest[index] = lhs[index] && rhs[index];
    }
}

static TZrBool report_first_uninitialized_constructor_const_field(SZrCompilerState *cs,
                                                                  const SZrTrackedConstructorConstFields *tracked,
                                                                  const TZrBool *assigned,
                                                                  SZrFileRange location) {
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL || tracked == ZR_NULL || assigned == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < tracked->count; index++) {
        const TZrChar *fieldNameText;

        if (assigned[index]) {
            continue;
        }

        fieldNameText = tracked->names[index] != ZR_NULL
                                ? ZrCore_String_GetNativeStringShort(tracked->names[index])
                                : ZR_NULL;
        if (fieldNameText != ZR_NULL) {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "Const field '%s' must be initialized in declaration or constructor",
                     fieldNameText);
        } else {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "Const field must be initialized in declaration or constructor");
        }
        ZrParser_Compiler_Error(cs, errorMsg, location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool compiler_primary_expression_is_direct_this_field_access(SZrAstNode *node, SZrString **outFieldName) {
    SZrPrimaryExpression *primaryExpr;
    SZrAstNode *memberNode;
    SZrMemberExpression *memberExpr;
    TZrNativeString rootName;

    if (outFieldName != ZR_NULL) {
        *outFieldName = ZR_NULL;
    }

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primaryExpr = &node->data.primaryExpression;
    if (primaryExpr->property == ZR_NULL ||
        primaryExpr->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primaryExpr->property->data.identifier.name == ZR_NULL ||
        primaryExpr->members == ZR_NULL ||
        primaryExpr->members->nodes == ZR_NULL ||
        primaryExpr->members->count != 1) {
        return ZR_FALSE;
    }

    rootName = ZrCore_String_GetNativeStringShort(primaryExpr->property->data.identifier.name);
    if (rootName == ZR_NULL || strcmp(rootName, "this") != 0) {
        return ZR_FALSE;
    }

    memberNode = primaryExpr->members->nodes[0];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_FALSE;
    }

    memberExpr = &memberNode->data.memberExpression;
    if (memberExpr->computed ||
        memberExpr->property == ZR_NULL ||
        memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        memberExpr->property->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outFieldName != ZR_NULL) {
        *outFieldName = memberExpr->property->data.identifier.name;
    }
    return ZR_TRUE;
}

static TZrBool mark_constructor_const_field_assignment(SZrCompilerState *cs,
                                                       const SZrTrackedConstructorConstFields *tracked,
                                                       SZrString *fieldName,
                                                       TZrBool *assignedState,
                                                       SZrFileRange location) {
    TZrInt32 trackedIndex;
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrNativeString fieldNameText;

    if (cs == ZR_NULL || tracked == ZR_NULL || fieldName == ZR_NULL || assignedState == ZR_NULL) {
        return ZR_FALSE;
    }

    trackedIndex = tracked_constructor_const_field_index(tracked, fieldName);
    if (trackedIndex == ZR_PARSER_I32_NONE) {
        return ZR_TRUE;
    }

    if (assignedState[trackedIndex]) {
        fieldNameText = ZrCore_String_GetNativeStringShort(fieldName);
        if (fieldNameText != ZR_NULL) {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "Cannot assign to const field '%s' more than once",
                     fieldNameText);
        } else {
            snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const field more than once");
        }
        ZrParser_Compiler_Error(cs, errorMsg, location);
        return ZR_FALSE;
    }

    assignedState[trackedIndex] = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool analyze_constructor_const_field_expression(SZrCompilerState *cs,
                                                          const SZrTrackedConstructorConstFields *tracked,
                                                          SZrAstNode *node,
                                                          TZrBool *assignedState);

static TZrBool analyze_constructor_const_field_statement(SZrCompilerState *cs,
                                                         const SZrTrackedConstructorConstFields *tracked,
                                                         SZrAstNode *node,
                                                         const TZrBool *assignedBefore,
                                                         TZrBool *assignedAfter,
                                                         TZrBool *continuesPastNode) {
    TZrBool thenContinues = ZR_TRUE;
    TZrBool elseContinues = ZR_TRUE;

    if (cs == ZR_NULL || tracked == ZR_NULL || assignedAfter == ZR_NULL || continuesPastNode == ZR_NULL) {
        return ZR_FALSE;
    }

    *continuesPastNode = ZR_TRUE;
    copy_constructor_const_assignment_state(assignedBefore, assignedAfter, tracked->count);
    if (node == ZR_NULL || tracked->count == 0) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_BLOCK: {
            TZrBool currentContinues = ZR_TRUE;
            TZrBool *currentAssigned;
            TZrBool *nextAssigned;

            currentAssigned = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global, sizeof(TZrBool) * tracked->count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
            nextAssigned = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global, sizeof(TZrBool) * tracked->count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
            if (currentAssigned == ZR_NULL || nextAssigned == ZR_NULL) {
                if (currentAssigned != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                  currentAssigned,
                                                  sizeof(TZrBool) * tracked->count,
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (nextAssigned != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                  nextAssigned,
                                                  sizeof(TZrBool) * tracked->count,
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                return ZR_FALSE;
            }

            copy_constructor_const_assignment_state(assignedBefore, currentAssigned, tracked->count);
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    SZrAstNode *statement = node->data.block.body->nodes[index];
                    if (!currentContinues || statement == ZR_NULL) {
                        break;
                    }

                    if (!analyze_constructor_const_field_statement(cs,
                                                                   tracked,
                                                                   statement,
                                                                   currentAssigned,
                                                                   nextAssigned,
                                                                   &currentContinues)) {
                        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                      currentAssigned,
                                                      sizeof(TZrBool) * tracked->count,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                      nextAssigned,
                                                      sizeof(TZrBool) * tracked->count,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        return ZR_FALSE;
                    }

                    if (currentContinues) {
                        copy_constructor_const_assignment_state(nextAssigned, currentAssigned, tracked->count);
                    }
                }
            }

            copy_constructor_const_assignment_state(currentAssigned, assignedAfter, tracked->count);
            *continuesPastNode = currentContinues;
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          currentAssigned,
                                          sizeof(TZrBool) * tracked->count,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          nextAssigned,
                                          sizeof(TZrBool) * tracked->count,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_TRUE;
        }

        case ZR_AST_RETURN_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
            *continuesPastNode = ZR_FALSE;
            return report_first_uninitialized_constructor_const_field(cs, tracked, assignedBefore, node->location);

        case ZR_AST_EXPRESSION_STATEMENT:
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.expressionStatement.expr,
                                                              assignedAfter);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return analyze_constructor_const_field_expression(cs, tracked, node, assignedAfter);

        case ZR_AST_IF_EXPRESSION: {
            const SZrIfExpression *ifExpr = &node->data.ifExpression;
            TZrBool *conditionAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
            TZrBool *thenAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
            TZrBool *elseAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);

            if (conditionAssigned == ZR_NULL || thenAssigned == ZR_NULL || elseAssigned == ZR_NULL) {
                free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                return ZR_FALSE;
            }

            copy_constructor_const_assignment_state(assignedBefore, conditionAssigned, tracked->count);
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            ifExpr->condition,
                                                            conditionAssigned)) {
                free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                return ZR_FALSE;
            }

            if (ifExpr->isStatement) {
                if (!analyze_constructor_const_field_statement(cs,
                                                               tracked,
                                                               ifExpr->thenExpr,
                                                               conditionAssigned,
                                                               thenAssigned,
                                                               &thenContinues)) {
                    free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                    return ZR_FALSE;
                }

                if (ifExpr->elseExpr != ZR_NULL) {
                    if (!analyze_constructor_const_field_statement(cs,
                                                                   tracked,
                                                                   ifExpr->elseExpr,
                                                                   conditionAssigned,
                                                                   elseAssigned,
                                                                   &elseContinues)) {
                        free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                        return ZR_FALSE;
                    }
                } else {
                    copy_constructor_const_assignment_state(conditionAssigned, elseAssigned, tracked->count);
                    elseContinues = ZR_TRUE;
                }
            } else {
                copy_constructor_const_assignment_state(conditionAssigned, thenAssigned, tracked->count);
                if (!analyze_constructor_const_field_expression(cs,
                                                                tracked,
                                                                ifExpr->thenExpr,
                                                                thenAssigned)) {
                    free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                    return ZR_FALSE;
                }

                if (ifExpr->elseExpr != ZR_NULL) {
                    copy_constructor_const_assignment_state(conditionAssigned, elseAssigned, tracked->count);
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    ifExpr->elseExpr,
                                                                    elseAssigned)) {
                        free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                        return ZR_FALSE;
                    }
                } else {
                    copy_constructor_const_assignment_state(conditionAssigned, elseAssigned, tracked->count);
                }
                thenContinues = ZR_TRUE;
                elseContinues = ZR_TRUE;
            }

            if (thenContinues && elseContinues) {
                intersect_constructor_const_assignment_state(thenAssigned,
                                                             elseAssigned,
                                                             assignedAfter,
                                                             tracked->count);
                *continuesPastNode = ZR_TRUE;
            } else if (thenContinues) {
                copy_constructor_const_assignment_state(thenAssigned, assignedAfter, tracked->count);
                *continuesPastNode = ZR_TRUE;
            } else if (elseContinues) {
                copy_constructor_const_assignment_state(elseAssigned, assignedAfter, tracked->count);
                *continuesPastNode = ZR_TRUE;
            } else {
                *continuesPastNode = ZR_FALSE;
            }

            free_constructor_const_assignment_state(cs, conditionAssigned, tracked->count);
            free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
            free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
            return ZR_TRUE;
        }

        case ZR_AST_SWITCH_EXPRESSION: {
            const SZrSwitchExpression *switchExpr = &node->data.switchExpression;
            TZrBool *discriminantAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
            TZrBool *branchAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
            TZrBool *mergedAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
            TZrBool hasContinuingBranch = ZR_FALSE;

            if (discriminantAssigned == ZR_NULL || branchAssigned == ZR_NULL || mergedAssigned == ZR_NULL) {
                free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
                return ZR_FALSE;
            }

            copy_constructor_const_assignment_state(assignedBefore, discriminantAssigned, tracked->count);
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            switchExpr->expr,
                                                            discriminantAssigned)) {
                free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
                return ZR_FALSE;
            }

            if (switchExpr->cases != ZR_NULL && switchExpr->cases->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < switchExpr->cases->count; index++) {
                    SZrAstNode *caseNode = switchExpr->cases->nodes[index];
                    const SZrSwitchCase *switchCase;
                    TZrBool branchContinues = ZR_TRUE;

                    if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                        continue;
                    }

                    switchCase = &caseNode->data.switchCase;
                    if (switchExpr->isStatement) {
                        if (!analyze_constructor_const_field_statement(cs,
                                                                       tracked,
                                                                       switchCase->block,
                                                                       discriminantAssigned,
                                                                       branchAssigned,
                                                                       &branchContinues)) {
                            free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
                            free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
                            free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
                            return ZR_FALSE;
                        }
                    } else {
                        copy_constructor_const_assignment_state(discriminantAssigned, branchAssigned, tracked->count);
                        if (!analyze_constructor_const_field_expression(cs,
                                                                        tracked,
                                                                        switchCase->block,
                                                                        branchAssigned)) {
                            free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
                            free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
                            free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
                            return ZR_FALSE;
                        }
                    }

                    if (!branchContinues) {
                        continue;
                    }

                    if (!hasContinuingBranch) {
                        copy_constructor_const_assignment_state(branchAssigned, mergedAssigned, tracked->count);
                        hasContinuingBranch = ZR_TRUE;
                    } else {
                        intersect_constructor_const_assignment_state(mergedAssigned,
                                                                     branchAssigned,
                                                                     mergedAssigned,
                                                                     tracked->count);
                    }
                }
            }

            if (switchExpr->defaultCase != ZR_NULL && switchExpr->defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
                TZrBool branchContinues = ZR_TRUE;
                const SZrSwitchDefault *switchDefault = &switchExpr->defaultCase->data.switchDefault;

                if (switchExpr->isStatement) {
                    if (!analyze_constructor_const_field_statement(cs,
                                                                   tracked,
                                                                   switchDefault->block,
                                                                   discriminantAssigned,
                                                                   branchAssigned,
                                                                   &branchContinues)) {
                        free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
                        return ZR_FALSE;
                    }
                } else {
                    copy_constructor_const_assignment_state(discriminantAssigned, branchAssigned, tracked->count);
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    switchDefault->block,
                                                                    branchAssigned)) {
                        free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
                        free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
                        return ZR_FALSE;
                    }
                }

                if (branchContinues) {
                    if (!hasContinuingBranch) {
                        copy_constructor_const_assignment_state(branchAssigned, mergedAssigned, tracked->count);
                        hasContinuingBranch = ZR_TRUE;
                    } else {
                        intersect_constructor_const_assignment_state(mergedAssigned,
                                                                     branchAssigned,
                                                                     mergedAssigned,
                                                                     tracked->count);
                    }
                }
            } else {
                if (!hasContinuingBranch) {
                    copy_constructor_const_assignment_state(discriminantAssigned, mergedAssigned, tracked->count);
                    hasContinuingBranch = ZR_TRUE;
                } else {
                    intersect_constructor_const_assignment_state(mergedAssigned,
                                                                 discriminantAssigned,
                                                                 mergedAssigned,
                                                                 tracked->count);
                }
            }

            if (hasContinuingBranch) {
                copy_constructor_const_assignment_state(mergedAssigned, assignedAfter, tracked->count);
                *continuesPastNode = ZR_TRUE;
            } else {
                *continuesPastNode = ZR_FALSE;
            }

            free_constructor_const_assignment_state(cs, discriminantAssigned, tracked->count);
            free_constructor_const_assignment_state(cs, branchAssigned, tracked->count);
            free_constructor_const_assignment_state(cs, mergedAssigned, tracked->count);
            return ZR_TRUE;
        }

        case ZR_AST_WHILE_LOOP:
            if (node->data.whileLoop.block != ZR_NULL &&
                !analyze_constructor_const_field_statement(cs,
                                                           tracked,
                                                           node->data.whileLoop.block,
                                                           assignedBefore,
                                                           assignedAfter,
                                                           &thenContinues)) {
                return ZR_FALSE;
            }
            copy_constructor_const_assignment_state(assignedBefore, assignedAfter, tracked->count);
            *continuesPastNode = ZR_TRUE;
            return ZR_TRUE;

        case ZR_AST_FOR_LOOP:
            if (node->data.forLoop.block != ZR_NULL &&
                !analyze_constructor_const_field_statement(cs,
                                                           tracked,
                                                           node->data.forLoop.block,
                                                           assignedBefore,
                                                           assignedAfter,
                                                           &thenContinues)) {
                return ZR_FALSE;
            }
            copy_constructor_const_assignment_state(assignedBefore, assignedAfter, tracked->count);
            *continuesPastNode = ZR_TRUE;
            return ZR_TRUE;

        case ZR_AST_FOREACH_LOOP:
            if (node->data.foreachLoop.block != ZR_NULL &&
                !analyze_constructor_const_field_statement(cs,
                                                           tracked,
                                                           node->data.foreachLoop.block,
                                                           assignedBefore,
                                                           assignedAfter,
                                                           &thenContinues)) {
                return ZR_FALSE;
            }
            copy_constructor_const_assignment_state(assignedBefore, assignedAfter, tracked->count);
            *continuesPastNode = ZR_TRUE;
            return ZR_TRUE;

        default:
            return ZR_TRUE;
    }
}

static TZrBool analyze_constructor_const_field_expression(SZrCompilerState *cs,
                                                          const SZrTrackedConstructorConstFields *tracked,
                                                          SZrAstNode *node,
                                                          TZrBool *assignedState) {
    SZrString *fieldName = ZR_NULL;

    if (cs == ZR_NULL || tracked == ZR_NULL || assignedState == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_UNARY_EXPRESSION:
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.unaryExpression.argument,
                                                              assignedState);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.typeCastExpression.expression,
                                                              assignedState);

        case ZR_AST_BINARY_EXPRESSION:
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.binaryExpression.left,
                                                            assignedState)) {
                return ZR_FALSE;
            }
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.binaryExpression.right,
                                                              assignedState);

        case ZR_AST_LOGICAL_EXPRESSION:
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.logicalExpression.left,
                                                            assignedState)) {
                return ZR_FALSE;
            }
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.logicalExpression.right,
                                                              assignedState);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.assignmentExpression.left,
                                                            assignedState)) {
                return ZR_FALSE;
            }

            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.assignmentExpression.right,
                                                            assignedState)) {
                return ZR_FALSE;
            }

            if (compiler_primary_expression_is_direct_this_field_access(node->data.assignmentExpression.left,
                                                                        &fieldName)) {
                return mark_constructor_const_field_assignment(cs,
                                                               tracked,
                                                               fieldName,
                                                               assignedState,
                                                               node->location);
            }
            return ZR_TRUE;

        case ZR_AST_CONDITIONAL_EXPRESSION: {
            TZrBool *consequentAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
            TZrBool *alternateAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);

            if (consequentAssigned == ZR_NULL || alternateAssigned == ZR_NULL) {
                free_constructor_const_assignment_state(cs, consequentAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, alternateAssigned, tracked->count);
                return ZR_FALSE;
            }

            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.conditionalExpression.test,
                                                            assignedState)) {
                free_constructor_const_assignment_state(cs, consequentAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, alternateAssigned, tracked->count);
                return ZR_FALSE;
            }

            copy_constructor_const_assignment_state(assignedState, consequentAssigned, tracked->count);
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.conditionalExpression.consequent,
                                                            consequentAssigned)) {
                free_constructor_const_assignment_state(cs, consequentAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, alternateAssigned, tracked->count);
                return ZR_FALSE;
            }

            copy_constructor_const_assignment_state(assignedState, alternateAssigned, tracked->count);
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.conditionalExpression.alternate,
                                                            alternateAssigned)) {
                free_constructor_const_assignment_state(cs, consequentAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, alternateAssigned, tracked->count);
                return ZR_FALSE;
            }

            intersect_constructor_const_assignment_state(consequentAssigned,
                                                         alternateAssigned,
                                                         assignedState,
                                                         tracked->count);
            free_constructor_const_assignment_state(cs, consequentAssigned, tracked->count);
            free_constructor_const_assignment_state(cs, alternateAssigned, tracked->count);
            return ZR_TRUE;
        }

        case ZR_AST_IF_EXPRESSION:
            if (!node->data.ifExpression.isStatement) {
                TZrBool *thenAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);
                TZrBool *elseAssigned = allocate_constructor_const_assignment_state(cs, tracked->count);

                if (thenAssigned == ZR_NULL || elseAssigned == ZR_NULL) {
                    free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                    return ZR_FALSE;
                }

                if (!analyze_constructor_const_field_expression(cs,
                                                                tracked,
                                                                node->data.ifExpression.condition,
                                                                assignedState)) {
                    free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                    return ZR_FALSE;
                }

                copy_constructor_const_assignment_state(assignedState, thenAssigned, tracked->count);
                if (!analyze_constructor_const_field_expression(cs,
                                                                tracked,
                                                                node->data.ifExpression.thenExpr,
                                                                thenAssigned)) {
                    free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                    return ZR_FALSE;
                }

                copy_constructor_const_assignment_state(assignedState, elseAssigned, tracked->count);
                if (node->data.ifExpression.elseExpr != ZR_NULL &&
                    !analyze_constructor_const_field_expression(cs,
                                                                tracked,
                                                                node->data.ifExpression.elseExpr,
                                                                elseAssigned)) {
                    free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                    free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
                    return ZR_FALSE;
                }

                intersect_constructor_const_assignment_state(thenAssigned,
                                                             elseAssigned,
                                                             assignedState,
                                                             tracked->count);
                free_constructor_const_assignment_state(cs, thenAssigned, tracked->count);
                free_constructor_const_assignment_state(cs, elseAssigned, tracked->count);
            }
            return ZR_TRUE;

        case ZR_AST_MEMBER_EXPRESSION:
            if (node->data.memberExpression.computed) {
                return analyze_constructor_const_field_expression(cs,
                                                                  tracked,
                                                                  node->data.memberExpression.property,
                                                                  assignedState);
            }
            return ZR_TRUE;

        case ZR_AST_PRIMARY_EXPRESSION:
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.primaryExpression.property,
                                                            assignedState)) {
                return ZR_FALSE;
            }

            if (node->data.primaryExpression.members != ZR_NULL &&
                node->data.primaryExpression.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    node->data.primaryExpression.members->nodes[index],
                                                                    assignedState)) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.prototypeReferenceExpression.target,
                                                              assignedState);

        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (!analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.constructExpression.target,
                                                            assignedState)) {
                return ZR_FALSE;
            }
            if (node->data.constructExpression.args != ZR_NULL &&
                node->data.constructExpression.args->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.constructExpression.args->count; index++) {
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    node->data.constructExpression.args->nodes[index],
                                                                    assignedState)) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;

        case ZR_AST_ARRAY_LITERAL:
            if (node->data.arrayLiteral.elements != ZR_NULL &&
                node->data.arrayLiteral.elements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.arrayLiteral.elements->count; index++) {
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    node->data.arrayLiteral.elements->nodes[index],
                                                                    assignedState)) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;

        case ZR_AST_OBJECT_LITERAL:
            if (node->data.objectLiteral.properties != ZR_NULL &&
                node->data.objectLiteral.properties->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.objectLiteral.properties->count; index++) {
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    node->data.objectLiteral.properties->nodes[index],
                                                                    assignedState)) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;

        case ZR_AST_KEY_VALUE_PAIR:
            if (node->data.keyValuePair.key != ZR_NULL &&
                node->data.keyValuePair.key->type != ZR_AST_IDENTIFIER_LITERAL &&
                node->data.keyValuePair.key->type != ZR_AST_STRING_LITERAL &&
                !analyze_constructor_const_field_expression(cs,
                                                            tracked,
                                                            node->data.keyValuePair.key,
                                                            assignedState)) {
                return ZR_FALSE;
            }
            return analyze_constructor_const_field_expression(cs,
                                                              tracked,
                                                              node->data.keyValuePair.value,
                                                              assignedState);

        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL && node->data.functionCall.args->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.functionCall.args->count; index++) {
                    if (!analyze_constructor_const_field_expression(cs,
                                                                    tracked,
                                                                    node->data.functionCall.args->nodes[index],
                                                                    assignedState)) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;

        default:
            return ZR_TRUE;
    }
}

void emit_object_field_assignment_from_expression(SZrCompilerState *cs,
                                                         TZrUInt32 objectSlot,
                                                         SZrString *fieldName,
                                                         SZrAstNode *expression) {
    TZrUInt32 memberId;

    if (cs == ZR_NULL || fieldName == ZR_NULL || expression == ZR_NULL || cs->hasError) {
        return;
    }

    ZrParser_Expression_Compile(cs, expression);
    if (cs->hasError || cs->stackSlotCount == 0) {
        return;
    }

    TZrUInt32 valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    memberId = compiler_get_or_add_member_entry(cs, fieldName);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        return;
    }

    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER),
                                                       (TZrUInt16)valueSlot,
                                                       (TZrUInt16)objectSlot,
                                                       (TZrUInt16)memberId);
    emit_instruction(cs, setTableInst);
}

void emit_class_static_field_initializers(SZrCompilerState *cs, SZrAstNode *classNode) {
    if (cs == ZR_NULL || classNode == ZR_NULL || classNode->type != ZR_AST_CLASS_DECLARATION || cs->hasError) {
        return;
    }

    SZrClassDeclaration *classDecl = &classNode->data.classDeclaration;
    if (classDecl->name == ZR_NULL || classDecl->name->name == ZR_NULL || classDecl->members == ZR_NULL) {
        return;
    }

    TZrUInt32 prototypeSlot = ZR_PARSER_SLOT_NONE;
    for (TZrSize i = 0; i < classDecl->members->count; i++) {
        SZrAstNode *member = classDecl->members->nodes[i];
        if (member == ZR_NULL || member->type != ZR_AST_CLASS_FIELD) {
            continue;
        }

        SZrClassField *field = &member->data.classField;
        if (!field->isStatic || field->init == ZR_NULL || field->name == ZR_NULL || field->name->name == ZR_NULL) {
            continue;
        }

        if (prototypeSlot == ZR_PARSER_SLOT_NONE) {
            prototypeSlot = emit_load_global_identifier(cs, classDecl->name->name);
            if (prototypeSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
                return;
            }
        }

        emit_object_field_assignment_from_expression(cs, prototypeSlot, field->name->name, field->init);
        if (cs->hasError) {
            return;
        }
    }
}

void compiler_begin_constructor_const_field_tracking(SZrCompilerState *cs) {
    SZrAstNodeArray *members;

    if (cs == ZR_NULL) {
        return;
    }

    cs->constructorInitializedConstFields.length = 0;
    members = compiler_get_current_type_members(cs);
    if (members == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        SZrString *fieldName;

        if (!compiler_member_is_instance_const_field(member) || !compiler_member_const_field_has_initializer(member)) {
            continue;
        }

        fieldName = compiler_member_const_field_name(member);
        if (fieldName != ZR_NULL && !compiler_constructor_const_field_is_tracked(cs, fieldName)) {
            compiler_track_constructor_const_field(cs, fieldName);
        }
    }
}

void compiler_end_constructor_const_field_tracking(SZrCompilerState *cs) {
    if (cs != ZR_NULL) {
        cs->constructorInitializedConstFields.length = 0;
    }
}

TZrBool compiler_record_constructor_const_field_assignment(SZrCompilerState *cs,
                                                                  SZrString *fieldName,
                                                                  const TZrChar *op,
                                                                  SZrFileRange location) {
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrNativeString fieldNameText;

    if (cs == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldNameText = ZrCore_String_GetNativeStringShort(fieldName);
    if (op != ZR_NULL && strcmp(op, "=") != 0) {
        if (fieldNameText != ZR_NULL) {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "Const field '%s' can only be initialized with '=' in constructor",
                     fieldNameText);
        } else {
            snprintf(errorMsg, sizeof(errorMsg), "Const field can only be initialized with '=' in constructor");
        }
        ZrParser_Compiler_Error(cs, errorMsg, location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool compiler_validate_constructor_const_field_initialization(SZrCompilerState *cs,
                                                                 SZrAstNode *body,
                                                                 SZrFileRange location) {
    SZrTrackedConstructorConstFields tracked = {0};
    SZrAstNodeArray *members;
    TZrBool *assignedBefore = ZR_NULL;
    TZrBool *assignedAfter = ZR_NULL;
    TZrBool continuesPastBody = ZR_TRUE;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }

    members = compiler_get_current_type_members(cs);
    if (members == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        if (compiler_member_is_instance_const_field(members->nodes[index])) {
            tracked.count++;
        }
    }

    if (tracked.count == 0) {
        return ZR_TRUE;
    }

    tracked.names = (SZrString **)ZrCore_Memory_RawMallocWithType(
            cs->state->global, sizeof(SZrString *) * tracked.count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    assignedBefore = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            cs->state->global, sizeof(TZrBool) * tracked.count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    assignedAfter = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            cs->state->global, sizeof(TZrBool) * tracked.count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (tracked.names == ZR_NULL || assignedBefore == ZR_NULL || assignedAfter == ZR_NULL) {
        if (tracked.names != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          tracked.names,
                                          sizeof(SZrString *) * tracked.count,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (assignedBefore != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          assignedBefore,
                                          sizeof(TZrBool) * tracked.count,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (assignedAfter != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          assignedAfter,
                                          sizeof(TZrBool) * tracked.count,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        return ZR_FALSE;
    }

    tracked.count = 0;
    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];

        if (!compiler_member_is_instance_const_field(member)) {
            continue;
        }

        tracked.names[tracked.count] = compiler_member_const_field_name(member);
        assignedBefore[tracked.count] = compiler_member_const_field_has_initializer(member);
        assignedAfter[tracked.count] = assignedBefore[tracked.count];
        tracked.count++;
    }

    if (body != ZR_NULL &&
        !analyze_constructor_const_field_statement(cs,
                                                   &tracked,
                                                   body,
                                                   assignedBefore,
                                                   assignedAfter,
                                                   &continuesPastBody)) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      tracked.names,
                                      sizeof(SZrString *) * tracked.count,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      assignedBefore,
                                      sizeof(TZrBool) * tracked.count,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      assignedAfter,
                                      sizeof(TZrBool) * tracked.count,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_FALSE;
    }

    if (continuesPastBody &&
        !report_first_uninitialized_constructor_const_field(cs, &tracked, assignedAfter, location)) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      tracked.names,
                                      sizeof(SZrString *) * tracked.count,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      assignedBefore,
                                      sizeof(TZrBool) * tracked.count,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      assignedAfter,
                                      sizeof(TZrBool) * tracked.count,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  tracked.names,
                                  sizeof(SZrString *) * tracked.count,
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  assignedBefore,
                                  sizeof(TZrBool) * tracked.count,
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  assignedAfter,
                                  sizeof(TZrBool) * tracked.count,
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return !cs->hasError;
}

SZrString *compiler_create_hidden_property_accessor_name(SZrCompilerState *cs, SZrString *propertyName,
                                                         TZrBool isSetter) {
    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    TZrSize prefixLength = strlen(prefix);
    TZrSize propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                         ? propertyName->shortStringLength
                                         : propertyName->longStringLength;
    TZrSize bufferSize = prefixLength + propertyNameLength + 1;
    TZrChar *buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    SZrString *result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

void emit_super_constructor_call(SZrCompilerState *cs, SZrString *superTypeName, SZrAstNodeArray *superArgs) {
    if (cs == ZR_NULL || superTypeName == ZR_NULL || cs->hasError) {
        return;
    }

    TZrUInt32 prototypeSlot = emit_load_global_identifier(cs, superTypeName);
    if (prototypeSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
        return;
    }

    SZrString *constructorName = ZrCore_String_CreateFromNative(cs->state, "__constructor");
    if (constructorName == ZR_NULL) {
        SZrFileRange location = cs->currentFunctionNode != ZR_NULL ? cs->currentFunctionNode->location
                                                                   : cs->errorLocation;
        ZrParser_Compiler_Error(cs, "Failed to create super constructor lookup key", location);
        return;
    }

    TZrUInt32 constructorMemberId = compiler_get_or_add_member_entry(cs, constructorName);
    TZrUInt32 functionSlot = allocate_stack_slot(cs);
    TZrInstruction getConstructorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                                             (TZrUInt16)functionSlot,
                                                             (TZrUInt16)prototypeSlot,
                                                             (TZrUInt16)constructorMemberId);
    emit_instruction(cs, getConstructorInst);

    TZrUInt32 receiverSlot = allocate_stack_slot(cs);
    TZrInstruction copyReceiverInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)receiverSlot, 0);
    emit_instruction(cs, copyReceiverInst);

    TZrUInt32 argCount = 1;
    if (superArgs != ZR_NULL) {
        for (TZrSize i = 0; i < superArgs->count; i++) {
            SZrAstNode *argNode = superArgs->nodes[i];
            if (argNode == ZR_NULL) {
                continue;
            }

            TZrUInt32 targetSlot = allocate_stack_slot(cs);
            ZrParser_Expression_Compile(cs, argNode);
            if (cs->hasError || cs->stackSlotCount == 0) {
                return;
            }

            TZrUInt32 valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
            if (valueSlot != targetSlot) {
                TZrInstruction moveArgInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                  (TZrUInt16)targetSlot, (TZrInt32)valueSlot);
                emit_instruction(cs, moveArgInst);
            }
            argCount++;
        }
    }

    TZrInstruction callSuperInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), (TZrUInt16)functionSlot,
                                                        (TZrUInt16)functionSlot, (TZrUInt16)argCount);
    emit_instruction(cs, callSuperInst);
}

