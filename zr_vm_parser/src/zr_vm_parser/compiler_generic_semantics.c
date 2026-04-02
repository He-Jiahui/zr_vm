//
// Created by Auto on 2026/04/02.
//

#include "compiler_internal.h"
#include "compile_expression_internal.h"

typedef struct SZrTrackedOutParameters {
    SZrString **names;
    TZrSize count;
} SZrTrackedOutParameters;

typedef enum EZrVariancePosition {
    ZR_VARIANCE_POSITION_INVARIANT = 0,
    ZR_VARIANCE_POSITION_OUT = 1,
    ZR_VARIANCE_POSITION_IN = -1,
} EZrVariancePosition;

static const TZrChar *compiler_string_native(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

TZrBool compiler_parameter_is_readonly(const SZrParameter *parameter) {
    return parameter != ZR_NULL &&
           (parameter->isConst || parameter->passingMode == ZR_PARAMETER_PASSING_MODE_IN);
}

void compiler_register_readonly_parameter_name(SZrCompilerState *cs,
                                               const SZrParameter *parameter,
                                               SZrString *parameterName) {
    if (cs == ZR_NULL || parameter == ZR_NULL || parameterName == ZR_NULL) {
        return;
    }

    if (compiler_parameter_is_readonly(parameter)) {
        ZrCore_Array_Push(cs->state, &cs->constParameters, &parameterName);
    }
}

TZrBool compiler_expression_is_assignable_storage_location(const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return ZR_TRUE;

        case ZR_AST_MEMBER_EXPRESSION:
            return ZR_TRUE;

        case ZR_AST_PRIMARY_EXPRESSION: {
            const SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;

            if (!compiler_expression_is_assignable_storage_location(primaryExpr->property)) {
                return ZR_FALSE;
            }

            if (primaryExpr->members == ZR_NULL || primaryExpr->members->count == 0) {
                return ZR_TRUE;
            }

            for (TZrSize index = 0; index < primaryExpr->members->count; index++) {
                const SZrAstNode *memberNode = primaryExpr->members->nodes[index];
                if (memberNode == ZR_NULL) {
                    continue;
                }

                if (memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
                    return ZR_FALSE;
                }
            }

            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

static TZrInt32 tracked_out_parameter_index(const SZrTrackedOutParameters *tracked, SZrString *name) {
    if (tracked == ZR_NULL || tracked->names == ZR_NULL || name == ZR_NULL) {
        return -1;
    }

    for (TZrSize index = 0; index < tracked->count; index++) {
        if (tracked->names[index] != ZR_NULL && ZrCore_String_Equal(tracked->names[index], name)) {
            return (TZrInt32)index;
        }
    }

    return -1;
}

static void copy_assignment_state(const TZrBool *source, TZrBool *dest, TZrSize count) {
    if (source == ZR_NULL || dest == ZR_NULL || count == 0) {
        return;
    }

    memcpy(dest, source, sizeof(TZrBool) * count);
}

static TZrBool all_out_parameters_assigned(const TZrBool *assigned, TZrSize count) {
    if (assigned == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < count; index++) {
        if (!assigned[index]) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool report_first_unassigned_out_parameter(SZrCompilerState *cs,
                                                     const SZrTrackedOutParameters *tracked,
                                                     const TZrBool *assigned,
                                                     SZrFileRange location) {
    TZrChar errorBuffer[256];

    if (cs == ZR_NULL || tracked == ZR_NULL || assigned == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < tracked->count; index++) {
        const TZrChar *nameText;

        if (assigned[index]) {
            continue;
        }

        nameText = compiler_string_native(tracked->names[index]);
        snprintf(errorBuffer,
                 sizeof(errorBuffer),
                 "%%out parameter '%s' must be assigned on all control-flow paths",
                 nameText != ZR_NULL ? nameText : "<unknown>");
        ZrParser_Compiler_Error(cs, errorBuffer, location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void mark_expression_out_assignments(const SZrTrackedOutParameters *tracked,
                                            SZrAstNode *node,
                                            TZrBool *assignedState) {
    TZrInt32 trackedIndex;

    if (tracked == ZR_NULL || node == ZR_NULL || assignedState == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (node->data.assignmentExpression.left != ZR_NULL &&
                node->data.assignmentExpression.left->type == ZR_AST_IDENTIFIER_LITERAL) {
                trackedIndex = tracked_out_parameter_index(tracked,
                                                           node->data.assignmentExpression.left->data.identifier.name);
                if (trackedIndex >= 0) {
                    assignedState[trackedIndex] = ZR_TRUE;
                }
            }
            mark_expression_out_assignments(tracked, node->data.assignmentExpression.right, assignedState);
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            if (node->data.primaryExpression.property != ZR_NULL) {
                mark_expression_out_assignments(tracked, node->data.primaryExpression.property, assignedState);
            }
            if (node->data.primaryExpression.members != ZR_NULL &&
                node->data.primaryExpression.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
                    mark_expression_out_assignments(tracked,
                                                    node->data.primaryExpression.members->nodes[index],
                                                    assignedState);
                }
            }
            break;

        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL && node->data.functionCall.args->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.functionCall.args->count; index++) {
                    mark_expression_out_assignments(tracked, node->data.functionCall.args->nodes[index], assignedState);
                }
            }
            break;

        default:
            break;
    }
}

static TZrBool analyze_out_assignment_statement(SZrCompilerState *cs,
                                                const SZrTrackedOutParameters *tracked,
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
    copy_assignment_state(assignedBefore, assignedAfter, tracked->count);
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

            copy_assignment_state(assignedBefore, currentAssigned, tracked->count);
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    SZrAstNode *statement = node->data.block.body->nodes[index];
                    if (!currentContinues || statement == ZR_NULL) {
                        break;
                    }

                    if (!analyze_out_assignment_statement(cs,
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
                        copy_assignment_state(nextAssigned, currentAssigned, tracked->count);
                    }
                }
            }

            copy_assignment_state(currentAssigned, assignedAfter, tracked->count);
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
            return report_first_unassigned_out_parameter(cs, tracked, assignedBefore, node->location);

        case ZR_AST_EXPRESSION_STATEMENT:
            mark_expression_out_assignments(tracked, node->data.expressionStatement.expr, assignedAfter);
            return ZR_TRUE;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            mark_expression_out_assignments(tracked, node, assignedAfter);
            return ZR_TRUE;

        case ZR_AST_IF_EXPRESSION:
            if (node->data.ifExpression.isStatement) {
                TZrBool *thenAssigned;
                TZrBool *elseAssigned;

                thenAssigned = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                        cs->state->global, sizeof(TZrBool) * tracked->count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                elseAssigned = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                        cs->state->global, sizeof(TZrBool) * tracked->count, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (thenAssigned == ZR_NULL || elseAssigned == ZR_NULL) {
                    if (thenAssigned != ZR_NULL) {
                        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                      thenAssigned,
                                                      sizeof(TZrBool) * tracked->count,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    }
                    if (elseAssigned != ZR_NULL) {
                        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                      elseAssigned,
                                                      sizeof(TZrBool) * tracked->count,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    }
                    return ZR_FALSE;
                }

                if (!analyze_out_assignment_statement(cs,
                                                      tracked,
                                                      node->data.ifExpression.thenExpr,
                                                      assignedBefore,
                                                      thenAssigned,
                                                      &thenContinues)) {
                    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                  thenAssigned,
                                                  sizeof(TZrBool) * tracked->count,
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                  elseAssigned,
                                                  sizeof(TZrBool) * tracked->count,
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    return ZR_FALSE;
                }

                if (node->data.ifExpression.elseExpr != ZR_NULL) {
                    if (!analyze_out_assignment_statement(cs,
                                                          tracked,
                                                          node->data.ifExpression.elseExpr,
                                                          assignedBefore,
                                                          elseAssigned,
                                                          &elseContinues)) {
                        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                      thenAssigned,
                                                      sizeof(TZrBool) * tracked->count,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                                      elseAssigned,
                                                      sizeof(TZrBool) * tracked->count,
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        return ZR_FALSE;
                    }
                } else {
                    copy_assignment_state(assignedBefore, elseAssigned, tracked->count);
                    elseContinues = ZR_TRUE;
                }

                if (thenContinues && elseContinues) {
                    for (TZrSize index = 0; index < tracked->count; index++) {
                        assignedAfter[index] = thenAssigned[index] && elseAssigned[index];
                    }
                    *continuesPastNode = ZR_TRUE;
                } else if (thenContinues) {
                    copy_assignment_state(thenAssigned, assignedAfter, tracked->count);
                    *continuesPastNode = ZR_TRUE;
                } else if (elseContinues) {
                    copy_assignment_state(elseAssigned, assignedAfter, tracked->count);
                    *continuesPastNode = ZR_TRUE;
                } else {
                    *continuesPastNode = ZR_FALSE;
                }

                ZrCore_Memory_RawFreeWithType(cs->state->global,
                                              thenAssigned,
                                              sizeof(TZrBool) * tracked->count,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global,
                                              elseAssigned,
                                              sizeof(TZrBool) * tracked->count,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_TRUE;

        case ZR_AST_WHILE_LOOP:
            if (node->data.whileLoop.block != ZR_NULL &&
                !analyze_out_assignment_statement(cs,
                                                  tracked,
                                                  node->data.whileLoop.block,
                                                  assignedBefore,
                                                  assignedAfter,
                                                  &thenContinues)) {
                return ZR_FALSE;
            }
            copy_assignment_state(assignedBefore, assignedAfter, tracked->count);
            *continuesPastNode = ZR_TRUE;
            return ZR_TRUE;

        case ZR_AST_FOR_LOOP:
            if (node->data.forLoop.block != ZR_NULL &&
                !analyze_out_assignment_statement(cs,
                                                  tracked,
                                                  node->data.forLoop.block,
                                                  assignedBefore,
                                                  assignedAfter,
                                                  &thenContinues)) {
                return ZR_FALSE;
            }
            copy_assignment_state(assignedBefore, assignedAfter, tracked->count);
            *continuesPastNode = ZR_TRUE;
            return ZR_TRUE;

        case ZR_AST_FOREACH_LOOP:
            if (node->data.foreachLoop.block != ZR_NULL &&
                !analyze_out_assignment_statement(cs,
                                                  tracked,
                                                  node->data.foreachLoop.block,
                                                  assignedBefore,
                                                  assignedAfter,
                                                  &thenContinues)) {
                return ZR_FALSE;
            }
            copy_assignment_state(assignedBefore, assignedAfter, tracked->count);
            *continuesPastNode = ZR_TRUE;
            return ZR_TRUE;

        default:
            return ZR_TRUE;
    }
}

TZrBool compiler_validate_out_parameter_definite_assignment(SZrCompilerState *cs,
                                                            SZrAstNodeArray *params,
                                                            SZrAstNode *body,
                                                            SZrFileRange fallbackLocation) {
    SZrTrackedOutParameters tracked = {0};
    TZrBool *assignedBefore;
    TZrBool *assignedAfter;
    TZrBool continuesPastBody = ZR_TRUE;

    if (cs == ZR_NULL || params == ZR_NULL || params->count == 0) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode != ZR_NULL &&
            paramNode->type == ZR_AST_PARAMETER &&
            paramNode->data.parameter.passingMode == ZR_PARAMETER_PASSING_MODE_OUT &&
            paramNode->data.parameter.name != ZR_NULL &&
            paramNode->data.parameter.name->name != ZR_NULL) {
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
        return ZR_FALSE;
    }

    tracked.count = 0;
    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode != ZR_NULL &&
            paramNode->type == ZR_AST_PARAMETER &&
            paramNode->data.parameter.passingMode == ZR_PARAMETER_PASSING_MODE_OUT &&
            paramNode->data.parameter.name != ZR_NULL &&
            paramNode->data.parameter.name->name != ZR_NULL) {
            tracked.names[tracked.count++] = paramNode->data.parameter.name->name;
        }
    }

    memset(assignedBefore, 0, sizeof(TZrBool) * tracked.count);
    memset(assignedAfter, 0, sizeof(TZrBool) * tracked.count);
    if (!analyze_out_assignment_statement(cs, &tracked, body, assignedBefore, assignedAfter, &continuesPastBody)) {
        return ZR_FALSE;
    }

    if (continuesPastBody && !all_out_parameters_assigned(assignedAfter, tracked.count)) {
        report_first_unassigned_out_parameter(cs, &tracked, assignedAfter, fallbackLocation);
    }

    return !cs->hasError;
}

static EZrGenericVariance interface_generic_variance_for_name(SZrAstNodeArray *params, SZrString *name) {
    if (params == ZR_NULL || name == ZR_NULL) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode == ZR_NULL ||
            paramNode->type != ZR_AST_PARAMETER ||
            paramNode->data.parameter.name == ZR_NULL ||
            paramNode->data.parameter.name->name == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(paramNode->data.parameter.name->name, name)) {
            return paramNode->data.parameter.variance;
        }
    }

    return ZR_GENERIC_VARIANCE_NONE;
}

static EZrGenericVariance prototype_generic_variance_at(SZrCompilerState *cs, SZrString *typeName, TZrSize index) {
    SZrTypePrototypeInfo *prototype;
    SZrTypeGenericParameterInfo *parameterInfo;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    prototype = find_compiler_type_prototype(cs, typeName);
    if (prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        index >= prototype->genericParameters.length) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    parameterInfo = (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&prototype->genericParameters, index);
    return parameterInfo != ZR_NULL ? parameterInfo->variance : ZR_GENERIC_VARIANCE_NONE;
}

static EZrVariancePosition combine_variance_position(EZrVariancePosition outerPosition,
                                                     EZrGenericVariance declaredVariance) {
    if (outerPosition == ZR_VARIANCE_POSITION_INVARIANT || declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
        return ZR_VARIANCE_POSITION_INVARIANT;
    }

    if (declaredVariance == ZR_GENERIC_VARIANCE_OUT) {
        return outerPosition;
    }

    return outerPosition == ZR_VARIANCE_POSITION_OUT ? ZR_VARIANCE_POSITION_IN : ZR_VARIANCE_POSITION_OUT;
}

static TZrBool report_invalid_variance_usage(SZrCompilerState *cs,
                                             SZrString *parameterName,
                                             EZrGenericVariance declaredVariance,
                                             const TZrChar *context,
                                             TZrBool nestedUsage,
                                             SZrFileRange location) {
    const TZrChar *nameText = compiler_string_native(parameterName);
    TZrChar errorBuffer[256];

    if (cs == ZR_NULL || parameterName == ZR_NULL || context == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(errorBuffer,
             sizeof(errorBuffer),
             "%s generic parameter '%s' cannot be used in %s%s position",
             declaredVariance == ZR_GENERIC_VARIANCE_OUT ? "covariant" : "contravariant",
             nameText != ZR_NULL ? nameText : "<unknown>",
             nestedUsage ? "nested " : "",
             context);
    ZrParser_Compiler_Error(cs, errorBuffer, location);
    return ZR_FALSE;
}

static TZrBool validate_interface_type_variance(SZrCompilerState *cs,
                                                SZrAstNodeArray *interfaceGenericParams,
                                                SZrType *typeNode,
                                                EZrVariancePosition position,
                                                const TZrChar *context,
                                                TZrBool nestedUsage,
                                                SZrFileRange location) {
    SZrString *typeName = ZR_NULL;
    EZrGenericVariance declaredVariance;

    if (cs == ZR_NULL || interfaceGenericParams == ZR_NULL || typeNode == ZR_NULL || context == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeNode->subType != ZR_NULL &&
        !validate_interface_type_variance(cs,
                                          interfaceGenericParams,
                                          typeNode->subType,
                                          ZR_VARIANCE_POSITION_INVARIANT,
                                          context,
                                          nestedUsage,
                                          location)) {
        return ZR_FALSE;
    }

    if (typeNode->name == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        typeName = typeNode->name->data.identifier.name;
        declaredVariance = interface_generic_variance_for_name(interfaceGenericParams, typeName);
        if (declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
            return ZR_TRUE;
        }

        if (position == ZR_VARIANCE_POSITION_INVARIANT ||
            (declaredVariance == ZR_GENERIC_VARIANCE_OUT && position == ZR_VARIANCE_POSITION_IN) ||
            (declaredVariance == ZR_GENERIC_VARIANCE_IN && position == ZR_VARIANCE_POSITION_OUT)) {
            return report_invalid_variance_usage(cs, typeName, declaredVariance, context, nestedUsage, location);
        }

        return ZR_TRUE;
    }

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;
        SZrString *outerTypeName = genericType->name != ZR_NULL ? genericType->name->name : ZR_NULL;

        for (TZrSize index = 0; genericType->params != ZR_NULL && index < genericType->params->count; index++) {
            SZrAstNode *argNode = genericType->params->nodes[index];
            EZrGenericVariance declaredOuterVariance = prototype_generic_variance_at(cs, outerTypeName, index);
            EZrVariancePosition childPosition = combine_variance_position(position, declaredOuterVariance);

            if (argNode != ZR_NULL &&
                argNode->type == ZR_AST_TYPE &&
                !validate_interface_type_variance(cs,
                                                 interfaceGenericParams,
                                                 &argNode->data.type,
                                                 childPosition,
                                                 context,
                                                 ZR_TRUE,
                                                 location)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool compiler_validate_interface_variance_rules(SZrCompilerState *cs,
                                                   SZrAstNode *interfaceNode) {
    SZrInterfaceDeclaration *interfaceDecl;

    if (cs == ZR_NULL || interfaceNode == ZR_NULL || interfaceNode->type != ZR_AST_INTERFACE_DECLARATION) {
        return ZR_TRUE;
    }

    interfaceDecl = &interfaceNode->data.interfaceDeclaration;
    if (interfaceDecl->generic == ZR_NULL || interfaceDecl->generic->params == ZR_NULL || interfaceDecl->members == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize memberIndex = 0; memberIndex < interfaceDecl->members->count; memberIndex++) {
        SZrAstNode *member = interfaceDecl->members->nodes[memberIndex];

        if (member == ZR_NULL) {
            continue;
        }

        switch (member->type) {
            case ZR_AST_INTERFACE_FIELD_DECLARATION:
                if (!validate_interface_type_variance(cs,
                                                      interfaceDecl->generic->params,
                                                      member->data.interfaceFieldDeclaration.typeInfo,
                                                      ZR_VARIANCE_POSITION_INVARIANT,
                                                      "field",
                                                      ZR_FALSE,
                                                      member->location)) {
                    return ZR_FALSE;
                }
                break;

            case ZR_AST_INTERFACE_METHOD_SIGNATURE:
                if (member->data.interfaceMethodSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMethodSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode = member->data.interfaceMethodSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL &&
                            paramNode->type == ZR_AST_PARAMETER &&
                            !validate_interface_type_variance(cs,
                                                              interfaceDecl->generic->params,
                                                              paramNode->data.parameter.typeInfo,
                                                              ZR_VARIANCE_POSITION_IN,
                                                              "contravariant parameter",
                                                              ZR_FALSE,
                                                              paramNode->location)) {
                            return ZR_FALSE;
                        }
                    }
                }
                if (!validate_interface_type_variance(cs,
                                                      interfaceDecl->generic->params,
                                                      member->data.interfaceMethodSignature.returnType,
                                                      ZR_VARIANCE_POSITION_OUT,
                                                      "covariant return",
                                                      ZR_FALSE,
                                                      member->location)) {
                    return ZR_FALSE;
                }
                break;

            case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: {
                SZrInterfacePropertySignature *property = &member->data.interfacePropertySignature;
                EZrVariancePosition propertyPosition = ZR_VARIANCE_POSITION_INVARIANT;
                const TZrChar *context = "property";

                if (property->hasGet && !property->hasSet) {
                    propertyPosition = ZR_VARIANCE_POSITION_OUT;
                    context = "getter";
                } else if (property->hasSet && !property->hasGet) {
                    propertyPosition = ZR_VARIANCE_POSITION_IN;
                    context = "setter";
                }

                if (!validate_interface_type_variance(cs,
                                                      interfaceDecl->generic->params,
                                                      property->typeInfo,
                                                      propertyPosition,
                                                      context,
                                                      ZR_FALSE,
                                                      member->location)) {
                    return ZR_FALSE;
                }
                break;
            }

            case ZR_AST_INTERFACE_META_SIGNATURE:
                if (member->data.interfaceMetaSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMetaSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode = member->data.interfaceMetaSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL &&
                            paramNode->type == ZR_AST_PARAMETER &&
                            !validate_interface_type_variance(cs,
                                                              interfaceDecl->generic->params,
                                                              paramNode->data.parameter.typeInfo,
                                                              ZR_VARIANCE_POSITION_IN,
                                                              "contravariant parameter",
                                                              ZR_FALSE,
                                                              paramNode->location)) {
                            return ZR_FALSE;
                        }
                    }
                }
                if (!validate_interface_type_variance(cs,
                                                      interfaceDecl->generic->params,
                                                      member->data.interfaceMetaSignature.returnType,
                                                      ZR_VARIANCE_POSITION_OUT,
                                                      "covariant return",
                                                      ZR_FALSE,
                                                      member->location)) {
                    return ZR_FALSE;
                }
                break;

            default:
                break;
        }
    }

    return ZR_TRUE;
}
