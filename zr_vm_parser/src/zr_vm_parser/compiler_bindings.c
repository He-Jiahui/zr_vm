//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "compile_expression_internal.h"
#include "type_inference_internal.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/runtime_decorator.h"

static void compiler_free_collected_generic_parameters(SZrState *state, SZrArray *genericParameters) {
    if (state == ZR_NULL || genericParameters == ZR_NULL ||
        !genericParameters->isValid || genericParameters->head == ZR_NULL ||
        genericParameters->capacity == 0 || genericParameters->elementSize == 0) {
        return;
    }

    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *genericInfo =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(genericParameters, index);
        if (genericInfo != ZR_NULL &&
            genericInfo->constraintTypeNames.isValid &&
            genericInfo->constraintTypeNames.head != ZR_NULL &&
            genericInfo->constraintTypeNames.capacity > 0 &&
            genericInfo->constraintTypeNames.elementSize > 0) {
            ZrCore_Array_Free(state, &genericInfo->constraintTypeNames);
        }
    }

    ZrCore_Array_Free(state, genericParameters);
}

SZrString *extract_simple_type_name_from_type_node(SZrAstNode *typeNode) {
    if (typeNode == ZR_NULL || typeNode->type != ZR_AST_TYPE) {
        return ZR_NULL;
    }

    SZrType *type = &typeNode->data.type;
    if (type->name == ZR_NULL || type->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return type->name->data.identifier.name;
}

TZrBool compiler_type_has_constructor(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info == ZR_NULL || info->name == ZR_NULL || !ZrCore_String_Equal(info->name, typeName)) {
            continue;
        }

        for (TZrSize memberIndex = 0; memberIndex < info->members.length; memberIndex++) {
            SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, memberIndex);
            if (memberInfo != ZR_NULL && memberInfo->isMetaMethod &&
                memberInfo->metaType == ZR_META_CONSTRUCTOR) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    return ZR_FALSE;
}

void emit_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    SZrTypeValue constantValue = *value;
    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)slot,
                                               (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
}

void emit_string_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, SZrString *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    SZrTypeValue constantValue;
    ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    constantValue.type = ZR_VALUE_TYPE_STRING;
    emit_constant_to_slot(cs, slot, &constantValue);
}

void compiler_register_function_type_binding(SZrCompilerState *cs, SZrFunctionDeclaration *funcDecl) {
    SZrInferredType returnType;
    SZrArray paramTypes;
    SZrArray genericParameters;
    SZrArray parameterPassingModes;

    if (cs == ZR_NULL || funcDecl == ZR_NULL || cs->typeEnv == ZR_NULL ||
        funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL || cs->hasError) {
        return;
    }

    ZrCore_Array_Construct(&genericParameters);
    ZrCore_Array_Construct(&parameterPassingModes);
    compiler_collect_generic_parameter_info(cs, &genericParameters, funcDecl->generic);
    compiler_collect_parameter_passing_modes(cs->state, &parameterPassingModes, funcDecl->params);

    if (funcDecl->returnType != ZR_NULL) {
        if (ZrParser_AstTypeToInferredType_Convert(cs, funcDecl->returnType, &returnType)) {
            ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), ZR_PARSER_INITIAL_CAPACITY_SMALL);
            if (funcDecl->params != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    SZrAstNode *paramNode = funcDecl->params->nodes[i];
                    if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                        SZrParameter *param = &paramNode->data.parameter;
                        if (param->typeInfo != ZR_NULL) {
                            SZrInferredType paramType;
                            if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                            }
                        } else {
                            SZrInferredType paramType;
                            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                            ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                        }
                    }
                }
            }
            ZrParser_TypeEnvironment_RegisterFunctionEx(cs->state,
                                                        cs->typeEnv,
                                                        funcDecl->name->name,
                                                        &returnType,
                                                        &paramTypes,
                                                        &genericParameters,
                                                        &parameterPassingModes,
                                                        funcDecl->name != ZR_NULL ? cs->currentFunctionNode : ZR_NULL);
            ZrParser_InferredType_Free(cs->state, &returnType);
            for (TZrSize i = 0; i < paramTypes.length; i++) {
                SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
                if (paramType != ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, paramType);
                }
            }
            ZrCore_Array_Free(cs->state, &paramTypes);
            compiler_free_collected_generic_parameters(cs->state, &genericParameters);
            if (parameterPassingModes.isValid && parameterPassingModes.head != ZR_NULL) {
                ZrCore_Array_Free(cs->state, &parameterPassingModes);
            }
        }
    } else {
        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), ZR_PARSER_INITIAL_CAPACITY_SMALL);
        if (funcDecl->params != ZR_NULL) {
            for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                SZrAstNode *paramNode = funcDecl->params->nodes[i];
                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                    SZrParameter *param = &paramNode->data.parameter;
                    SZrInferredType paramType;

                    if (param->typeInfo != ZR_NULL) {
                        if (!ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                            continue;
                        }
                    } else {
                        ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                    }
                    ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                }
            }
        }
        ZrParser_TypeEnvironment_RegisterFunctionEx(cs->state,
                                                    cs->typeEnv,
                                                    funcDecl->name->name,
                                                    &returnType,
                                                    &paramTypes,
                                                    &genericParameters,
                                                    &parameterPassingModes,
                                                    funcDecl->name != ZR_NULL ? cs->currentFunctionNode : ZR_NULL);
        ZrParser_InferredType_Free(cs->state, &returnType);
        for (TZrSize i = 0; i < paramTypes.length; i++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, paramType);
            }
        }
        ZrCore_Array_Free(cs->state, &paramTypes);
        compiler_free_collected_generic_parameters(cs->state, &genericParameters);
        if (parameterPassingModes.isValid && parameterPassingModes.head != ZR_NULL) {
            ZrCore_Array_Free(cs->state, &parameterPassingModes);
        }
    }
}

static TZrBool compiler_try_infer_expression_type_soft(SZrCompilerState *cs,
                                                       SZrAstNode *expr,
                                                       SZrInferredType *result) {
    TZrBool savedHasError;
    TZrBool savedHadRecoverableError;
    TZrBool savedHasFatalError;
    TZrBool savedHasCompileTimeError;
    SZrFileRange savedErrorLocation;
    const TZrChar *savedErrorMessage;

    if (cs == ZR_NULL || expr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    savedHasError = cs->hasError;
    savedHadRecoverableError = cs->hadRecoverableError;
    savedHasFatalError = cs->hasFatalError;
    savedHasCompileTimeError = cs->hasCompileTimeError;
    savedErrorLocation = cs->errorLocation;
    savedErrorMessage = cs->errorMessage;

    cs->hasError = ZR_FALSE;
    cs->hadRecoverableError = ZR_FALSE;
    cs->hasFatalError = ZR_FALSE;

    if (ZrParser_ExpressionType_Infer(cs, expr, result)) {
        return ZR_TRUE;
    }

    cs->hasError = savedHasError;
    cs->hadRecoverableError = savedHadRecoverableError;
    cs->hasFatalError = savedHasFatalError;
    cs->hasCompileTimeError = savedHasCompileTimeError;
    cs->errorLocation = savedErrorLocation;
    cs->errorMessage = savedErrorMessage;
    return ZR_FALSE;
}

static void compiler_merge_callable_return_type(SZrCompilerState *cs,
                                                const SZrInferredType *candidateType,
                                                TZrBool *hasReturnType,
                                                SZrInferredType *accumulatedType) {
    SZrInferredType mergedType;

    if (cs == ZR_NULL || candidateType == ZR_NULL || hasReturnType == ZR_NULL || accumulatedType == ZR_NULL) {
        return;
    }

    if (!*hasReturnType) {
        ZrParser_InferredType_Copy(cs->state, accumulatedType, candidateType);
        *hasReturnType = ZR_TRUE;
        return;
    }

    if (ZrParser_InferredType_Equal(accumulatedType, candidateType)) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &mergedType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_InferredType_GetCommonType(cs->state, &mergedType, accumulatedType, candidateType)) {
        ZrParser_InferredType_Free(cs->state, accumulatedType);
        ZrParser_InferredType_Copy(cs->state, accumulatedType, &mergedType);
    } else {
        ZrParser_InferredType_Free(cs->state, accumulatedType);
        ZrParser_InferredType_Init(cs->state, accumulatedType, ZR_VALUE_TYPE_OBJECT);
    }
    ZrParser_InferredType_Free(cs->state, &mergedType);
}

static void compiler_register_function_like_local_variable_type(SZrCompilerState *cs, SZrAstNode *node) {
    SZrVariableDeclaration *declaration;
    SZrInferredType bindingType;
    TZrBool hasBindingType = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    declaration = &node->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL || declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->pattern->data.identifier.name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (declaration->typeInfo != ZR_NULL) {
        hasBindingType = ZrParser_AstTypeToInferredType_Convert(cs, declaration->typeInfo, &bindingType);
    } else if (declaration->value != ZR_NULL) {
        hasBindingType = ZrParser_ExpressionType_Infer(cs, declaration->value, &bindingType);
    }

    if (cs->hasError) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
        return;
    }

    if (!hasBindingType) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
        ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    }

    ZrParser_TypeEnvironment_RegisterVariable(cs->state,
                                              cs->typeEnv,
                                              declaration->pattern->data.identifier.name,
                                              &bindingType);
    ZrParser_InferredType_Free(cs->state, &bindingType);
}

static void compiler_collect_function_like_return_type(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       TZrBool *hasReturnType,
                                                       SZrInferredType *accumulatedType) {
    if (cs == ZR_NULL || node == ZR_NULL || hasReturnType == ZR_NULL || accumulatedType == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body == ZR_NULL) {
                return;
            }

            for (TZrSize index = 0; index < block->body->count; index++) {
                compiler_collect_function_like_return_type(cs,
                                                           block->body->nodes[index],
                                                           hasReturnType,
                                                           accumulatedType);
                if (cs->hasError) {
                    return;
                }
            }
            return;
        }

        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStatement = &node->data.returnStatement;
            SZrInferredType candidateType;

            ZrParser_InferredType_Init(cs->state, &candidateType, ZR_VALUE_TYPE_NULL);
            if (returnStatement->expr != ZR_NULL &&
                !compiler_try_infer_expression_type_soft(cs, returnStatement->expr, &candidateType)) {
                ZrParser_InferredType_Free(cs->state, &candidateType);
                ZrParser_InferredType_Init(cs->state, &candidateType, ZR_VALUE_TYPE_OBJECT);
            }

            compiler_merge_callable_return_type(cs, &candidateType, hasReturnType, accumulatedType);
            ZrParser_InferredType_Free(cs->state, &candidateType);
            return;
        }

        case ZR_AST_VARIABLE_DECLARATION:
            compiler_register_function_like_local_variable_type(cs, node);
            return;

        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpression = &node->data.ifExpression;
            compiler_collect_function_like_return_type(cs,
                                                       ifExpression->thenExpr,
                                                       hasReturnType,
                                                       accumulatedType);
            if (cs->hasError) {
                return;
            }
            compiler_collect_function_like_return_type(cs,
                                                       ifExpression->elseExpr,
                                                       hasReturnType,
                                                       accumulatedType);
            return;
        }

        case ZR_AST_WHILE_LOOP:
            compiler_collect_function_like_return_type(cs,
                                                       node->data.whileLoop.block,
                                                       hasReturnType,
                                                       accumulatedType);
            return;

        case ZR_AST_FOR_LOOP:
            compiler_collect_function_like_return_type(cs,
                                                       node->data.forLoop.block,
                                                       hasReturnType,
                                                       accumulatedType);
            return;

        case ZR_AST_FOREACH_LOOP:
            compiler_collect_function_like_return_type(cs,
                                                       node->data.foreachLoop.block,
                                                       hasReturnType,
                                                       accumulatedType);
            return;

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *tryStatement = &node->data.tryCatchFinallyStatement;
            compiler_collect_function_like_return_type(cs,
                                                       tryStatement->block,
                                                       hasReturnType,
                                                       accumulatedType);
            if (cs->hasError) {
                return;
            }

            if (tryStatement->catchClauses != ZR_NULL) {
                for (TZrSize index = 0; index < tryStatement->catchClauses->count; index++) {
                    SZrAstNode *catchNode = tryStatement->catchClauses->nodes[index];
                    if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE) {
                        continue;
                    }

                    compiler_collect_function_like_return_type(cs,
                                                               catchNode->data.catchClause.block,
                                                               hasReturnType,
                                                               accumulatedType);
                    if (cs->hasError) {
                        return;
                    }
                }
            }

            compiler_collect_function_like_return_type(cs,
                                                       tryStatement->finallyBlock,
                                                       hasReturnType,
                                                       accumulatedType);
            return;
        }

        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_LAMBDA_EXPRESSION:
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_TEST_DECLARATION:
            return;

        default:
            return;
    }
}

static void compiler_collect_callable_parameter_types(SZrCompilerState *cs,
                                                      SZrAstNodeArray *params,
                                                      SZrArray *paramTypes) {
    if (cs == ZR_NULL || paramTypes == ZR_NULL) {
        return;
    }

    ZrCore_Array_Construct(paramTypes);
    if (params == ZR_NULL || params->count == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state, paramTypes, sizeof(SZrInferredType), params->count);
    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrInferredType paramType;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        if (paramNode->data.parameter.typeInfo != ZR_NULL &&
            ZrParser_AstTypeToInferredType_Convert(cs, paramNode->data.parameter.typeInfo, &paramType)) {
            ZrCore_Array_Push(cs->state, paramTypes, &paramType);
            continue;
        }

        ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, paramTypes, &paramType);
    }
}

static void compiler_register_lambda_callable_binding(SZrCompilerState *cs,
                                                      SZrString *name,
                                                      SZrLambdaExpression *lambda) {
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *lambdaEnv;
    SZrInferredType returnType;
    SZrArray paramTypes;
    SZrArray parameterPassingModes;
    TZrBool hasReturnType = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL || lambda == ZR_NULL) {
        return;
    }

    lambdaEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (lambdaEnv == ZR_NULL) {
        return;
    }

    lambdaEnv->parent = cs->typeEnv;
    lambdaEnv->semanticContext = cs->typeEnv->semanticContext;
    savedEnv = cs->typeEnv;
    cs->typeEnv = lambdaEnv;

    if (lambda->params != ZR_NULL) {
        for (TZrSize index = 0; index < lambda->params->count; index++) {
            SZrAstNode *paramNode = lambda->params->nodes[index];
            SZrParameter *parameter;
            SZrInferredType paramType;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            parameter = &paramNode->data.parameter;
            if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
                continue;
            }

            if (parameter->typeInfo != ZR_NULL &&
                ZrParser_AstTypeToInferredType_Convert(cs, parameter->typeInfo, &paramType)) {
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, parameter->name->name, &paramType);
                ZrParser_InferredType_Free(cs->state, &paramType);
                continue;
            }

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, parameter->name->name, &paramType);
            ZrParser_InferredType_Free(cs->state, &paramType);
        }
    }

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_NULL);
    compiler_collect_function_like_return_type(cs, lambda->block, &hasReturnType, &returnType);
    if (!hasReturnType) {
        ZrParser_InferredType_Free(cs->state, &returnType);
        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_NULL);
    }

    compiler_collect_callable_parameter_types(cs, lambda->params, &paramTypes);
    ZrCore_Array_Construct(&parameterPassingModes);
    compiler_collect_parameter_passing_modes(cs->state, &parameterPassingModes, lambda->params);

    ZrParser_TypeEnvironment_RegisterFunctionEx(cs->state,
                                                savedEnv,
                                                name,
                                                &returnType,
                                                &paramTypes,
                                                ZR_NULL,
                                                &parameterPassingModes,
                                                ZR_NULL);

    if (parameterPassingModes.isValid && parameterPassingModes.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &parameterPassingModes);
    }
    if (paramTypes.isValid && paramTypes.head != ZR_NULL) {
        for (TZrSize index = 0; index < paramTypes.length; index++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, index);
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, paramType);
            }
        }
        ZrCore_Array_Free(cs->state, &paramTypes);
    }
    ZrParser_InferredType_Free(cs->state, &returnType);

    cs->typeEnv = savedEnv;
    ZrParser_TypeEnvironment_Free(cs->state, lambdaEnv);
}

static void compiler_register_identifier_callable_binding(SZrCompilerState *cs,
                                                          SZrString *name,
                                                          SZrString *sourceName) {
    SZrArray candidates;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL || sourceName == ZR_NULL) {
        return;
    }

    ZrCore_Array_Construct(&candidates);
    if (!ZrParser_TypeEnvironment_LookupFunctions(cs->state, cs->typeEnv, sourceName, &candidates)) {
        return;
    }

    for (TZrSize index = 0; index < candidates.length; index++) {
        SZrFunctionTypeInfo **candidatePtr =
                (SZrFunctionTypeInfo **)ZrCore_Array_Get(&candidates, index);
        if (candidatePtr == ZR_NULL || *candidatePtr == ZR_NULL) {
            continue;
        }

        ZrParser_TypeEnvironment_RegisterFunctionEx(cs->state,
                                                    cs->typeEnv,
                                                    name,
                                                    &(*candidatePtr)->returnType,
                                                    &(*candidatePtr)->paramTypes,
                                                    &(*candidatePtr)->genericParameters,
                                                    &(*candidatePtr)->parameterPassingModes,
                                                    (*candidatePtr)->declarationNode);
    }

    if (candidates.isValid && candidates.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &candidates);
    }
}

void compiler_register_callable_value_binding(SZrCompilerState *cs,
                                              SZrString *name,
                                              SZrAstNode *valueNode) {
    if (cs == ZR_NULL || name == ZR_NULL || valueNode == ZR_NULL || cs->typeEnv == ZR_NULL) {
        return;
    }

    if (valueNode->type == ZR_AST_LAMBDA_EXPRESSION) {
        compiler_register_lambda_callable_binding(cs, name, &valueNode->data.lambdaExpression);
        return;
    }

    if (valueNode->type == ZR_AST_IDENTIFIER_LITERAL && valueNode->data.identifier.name != ZR_NULL) {
        compiler_register_identifier_callable_binding(cs, name, valueNode->data.identifier.name);
    }
}

void compiler_register_named_value_binding_to_env(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *name,
                                                         SZrString *typeName) {
    SZrInferredType existingType;
    SZrInferredType inferredType;

    if (cs == ZR_NULL || env == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &existingType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_TypeEnvironment_LookupVariable(cs->state, env, name, &existingType)) {
        ZrParser_InferredType_Free(cs->state, &existingType);
        return;
    }
    ZrParser_InferredType_Free(cs->state, &existingType);

    if (typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    } else {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    }
    ZrParser_TypeEnvironment_RegisterVariable(cs->state, env, name, &inferredType);
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

void compiler_register_extern_function_type_binding_to_env(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeEnvironment *env,
                                                           SZrExternFunctionDeclaration *functionDecl) {
    SZrInferredType returnType;
    SZrArray paramTypes;
    SZrArray parameterPassingModes;

    if (cs == ZR_NULL || env == ZR_NULL || functionDecl == ZR_NULL ||
        functionDecl->name == ZR_NULL || functionDecl->name->name == ZR_NULL) {
        return;
    }

    ZrCore_Array_Construct(&parameterPassingModes);
    compiler_collect_parameter_passing_modes(cs->state, &parameterPassingModes, functionDecl->params);

    if (functionDecl->returnType != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(cs, functionDecl->returnType, &returnType)) {
            return;
        }
    } else {
        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_NULL);
    }

    ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), functionDecl->params != ZR_NULL
                                                                         ? functionDecl->params->count
                                                                         : 0);
    if (functionDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < functionDecl->params->count; i++) {
            SZrAstNode *paramNode = functionDecl->params->nodes[i];
            SZrInferredType paramType;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.typeInfo != ZR_NULL) {
                if (!ZrParser_AstTypeToInferredType_Convert(cs, paramNode->data.parameter.typeInfo, &paramType)) {
                    continue;
                }
            } else {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }
            ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
        }
    }

    ZrParser_TypeEnvironment_RegisterFunctionEx(cs->state,
                                                env,
                                                functionDecl->name->name,
                                                &returnType,
                                                &paramTypes,
                                                ZR_NULL,
                                                &parameterPassingModes,
                                                declarationNode);

    ZrParser_InferredType_Free(cs->state, &returnType);
    for (TZrSize i = 0; i < paramTypes.length; i++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, paramType);
        }
    }
    ZrCore_Array_Free(cs->state, &paramTypes);
    if (parameterPassingModes.isValid && parameterPassingModes.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &parameterPassingModes);
    }
}

TZrUInt32 find_local_var_in_current_scope(SZrCompilerState *cs, SZrString *name) {
    SZrScope *scope;
    TZrSize startIndex;

    if (cs == ZR_NULL || name == ZR_NULL || cs->scopeStack.length == 0) {
        return ZR_PARSER_SLOT_NONE;
    }

    scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
    startIndex = scope != ZR_NULL ? scope->startVarIndex : 0;
    if (startIndex > cs->localVars.length) {
        startIndex = cs->localVars.length;
    }

    for (TZrSize i = cs->localVars.length; i > startIndex; i--) {
        SZrFunctionLocalVariable *var =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, i - 1);
        if (var != ZR_NULL && var->name != ZR_NULL && ZrCore_String_Equal(var->name, name)) {
            return var->stackSlot;
        }
    }

    return ZR_PARSER_SLOT_NONE;
}

static void compiler_predeclare_visible_module_alias_binding(SZrCompilerState *cs, SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrString *aliasName;
    SZrString *moduleName;
    SZrInferredType moduleType;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    decl = &node->data.variableDeclaration;
    if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL || decl->value == ZR_NULL ||
        decl->value->type != ZR_AST_IMPORT_EXPRESSION ||
        decl->value->data.importExpression.modulePath == ZR_NULL ||
        decl->value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
        decl->value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
        return;
    }

    aliasName = decl->pattern->data.identifier.name;
    moduleName = decl->value->data.importExpression.modulePath->data.stringLiteral.value;
    if (aliasName == ZR_NULL || moduleName == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_InitFull(cs->state, &moduleType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, moduleName);
    ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, aliasName, &moduleType);
    ZrParser_InferredType_Free(cs->state, &moduleType);
}

static void compiler_predeclare_visible_type_value_alias_binding(SZrCompilerState *cs, SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrString *aliasName;
    SZrInferredType aliasType;

    if (cs == ZR_NULL || cs->state == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    decl = &node->data.variableDeclaration;
    if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL || decl->value == ZR_NULL ||
        decl->value->type != ZR_AST_TYPE_LITERAL_EXPRESSION ||
        decl->value->data.typeLiteralExpression.typeInfo == ZR_NULL) {
        return;
    }

    aliasName = decl->pattern->data.identifier.name;
    if (aliasName == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(cs,
                                                decl->value->data.typeLiteralExpression.typeInfo,
                                                &aliasType)) {
        ZrParser_InferredType_Free(cs->state, &aliasType);
        return;
    }

    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, aliasName)) {
            ZrParser_InferredType_Free(cs->state, &binding->type);
            ZrParser_InferredType_Copy(cs->state, &binding->type, &aliasType);
            ZrParser_InferredType_Free(cs->state, &aliasType);
            return;
        }
    }

    {
        SZrTypeBinding binding;
        binding.name = aliasName;
        ZrParser_InferredType_Init(cs->state, &binding.type, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &binding.type, &aliasType);
        ZrCore_Array_Push(cs->state, &cs->typeValueAliases, &binding);
    }

    ZrParser_InferredType_Free(cs->state, &aliasType);
}

void ZrParser_Compiler_PredeclareFunctionBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError) {
        return;
    }

    for (TZrSize i = 0; i < statements->count; i++) {
        SZrAstNode *stmt = statements->nodes[i];
        SZrFunctionDeclaration *funcDecl;
        TZrUInt32 slot;
        SZrTypeValue nullValue;

        if (stmt == ZR_NULL) {
            continue;
        }

        if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
            compiler_predeclare_visible_module_alias_binding(cs, stmt);
            if (cs->hasError) {
                return;
            }

            compiler_predeclare_visible_type_value_alias_binding(cs, stmt);
            if (cs->hasError) {
                return;
            }
            continue;
        }

        if (stmt->type != ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }

        funcDecl = &stmt->data.functionDeclaration;
        if (funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL) {
            continue;
        }

        {
            SZrAstNode *previousFunctionNode = cs->currentFunctionNode;
            cs->currentFunctionNode = stmt;
            compiler_register_function_type_binding(cs, funcDecl);
            cs->currentFunctionNode = previousFunctionNode;
        }
        if (cs->hasError) {
            return;
        }

        if (find_local_var_in_current_scope(cs, funcDecl->name->name) != ZR_PARSER_SLOT_NONE) {
            continue;
        }

        slot = allocate_local_var(cs, funcDecl->name->name);
        ZrCore_Value_ResetAsNull(&nullValue);
        emit_constant_to_slot(cs, slot, &nullValue);
        if (cs->hasError) {
            return;
        }
    }
}

TZrUInt32 emit_load_global_identifier(SZrCompilerState *cs, SZrString *name) {
    TZrUInt32 globalSlot;
    TZrUInt32 memberId;

    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    globalSlot = allocate_stack_slot(cs);
    TZrInstruction getGlobalInst = create_instruction_0(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)globalSlot);
    emit_instruction(cs, getGlobalInst);

    memberId = compiler_get_or_add_member_entry(cs, name);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        return ZR_PARSER_SLOT_NONE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                          (TZrUInt16)globalSlot,
                                          (TZrUInt16)globalSlot,
                                          (TZrUInt16)memberId));
    return globalSlot;
}

static SZrString *compiler_create_runtime_decorator_temp_name(SZrCompilerState *cs, const TZrChar *prefix, TZrUInt32 index) {
    TZrChar buffer[96];

    if (cs == ZR_NULL || prefix == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(buffer,
             sizeof(buffer),
             "__zr_%s_runtime_decorator_%u_%u",
             prefix,
             (unsigned int)cs->instructionCount,
             (unsigned int)index);
    return ZrCore_String_Create(cs->state, buffer, strlen(buffer));
}

static TZrBool compiler_emit_copy_stack_value(SZrCompilerState *cs, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (cs == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK),
                                          (TZrUInt16)destinationSlot,
                                          (TZrInt32)sourceSlot));
    return !cs->hasError;
}

static TZrBool compiler_emit_runtime_decorator_application(SZrCompilerState *cs,
                                                           TZrUInt32 targetSlot,
                                                           TZrUInt32 decoratorSlot,
                                                           TZrBool persistTarget,
                                                           TZrSize trimTargetCount,
                                                           SZrFileRange location) {
    SZrClosureNative *helperClosure;
    SZrTypeValue helperValue;
    TZrUInt32 helperSlot;
    TZrUInt32 targetArgumentSlot;
    TZrUInt32 decoratorArgumentSlot;

    if (cs == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    helperClosure = ZrCore_ClosureNative_New(cs->state, 0);
    if (helperClosure == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to create runtime decorator helper", location);
        return ZR_FALSE;
    }

    helperClosure->nativeFunction = ZrCore_RuntimeDecorator_ApplyNativeEntry;
    ZrCore_RawObject_MarkAsPermanent(cs->state, ZR_CAST_RAW_OBJECT_AS_SUPER(helperClosure));

    ZrCore_Value_InitAsRawObject(cs->state, &helperValue, ZR_CAST_RAW_OBJECT_AS_SUPER(helperClosure));
    helperValue.isNative = ZR_TRUE;

    helperSlot = allocate_stack_slot(cs);
    emit_constant_to_slot(cs, helperSlot, &helperValue);

    targetArgumentSlot = allocate_stack_slot(cs);
    if (!compiler_emit_copy_stack_value(cs, targetArgumentSlot, targetSlot)) {
        return ZR_FALSE;
    }

    decoratorArgumentSlot = allocate_stack_slot(cs);
    if (!compiler_emit_copy_stack_value(cs, decoratorArgumentSlot, decoratorSlot)) {
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)helperSlot,
                                          (TZrUInt16)helperSlot,
                                          2));
    if (persistTarget) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)targetSlot,
                                              (TZrInt32)helperSlot));
    }
    ZrParser_Compiler_TrimStackToCount(cs, trimTargetCount);
    return !cs->hasError;
}

TZrBool emit_runtime_decorator_applications(SZrCompilerState *cs,
                                            SZrAstNodeArray *decorators,
                                            TZrUInt32 targetSlot,
                                            TZrBool persistTarget,
                                            SZrFileRange location) {
    TZrUInt32 *decoratorSlots = ZR_NULL;
    TZrSize runtimeDecoratorCount = 0;
    TZrSize decoratorIndex = 0;
    TZrSize trimTargetCount;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || decorators == ZR_NULL || decorators->count == 0 || cs->hasError) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            runtimeDecoratorCount++;
        } else if (cs->hasError) {
            goto cleanup;
        }
    }

    if (runtimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    decoratorSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(TZrUInt32) * runtimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (decoratorSlots == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to allocate runtime decorator slots", location);
        goto cleanup;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrString *tempName;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }
        if (decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION || decoratorNode->data.decoratorExpression.expr == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "invalid runtime decorator expression", decoratorNode->location);
            goto cleanup;
        }

        tempName = compiler_create_runtime_decorator_temp_name(cs, "definition", (TZrUInt32)decoratorIndex);
        if (tempName == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "failed to allocate runtime decorator temp", decoratorNode->location);
            goto cleanup;
        }

        decoratorSlots[decoratorIndex] = allocate_local_var(cs, tempName);
        if (compile_expression_into_slot(cs,
                                         decoratorNode->data.decoratorExpression.expr,
                                         decoratorSlots[decoratorIndex]) == ZR_PARSER_SLOT_NONE ||
            cs->hasError) {
            goto cleanup;
        }

        decoratorIndex++;
    }

    trimTargetCount = cs->stackSlotCount;
    for (TZrSize index = runtimeDecoratorCount; index > 0; index--) {
        if (!compiler_emit_runtime_decorator_application(cs,
                                                         targetSlot,
                                                         decoratorSlots[index - 1],
                                                         persistTarget,
                                                         trimTargetCount,
                                                         location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (decoratorSlots != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorSlots,
                                      sizeof(TZrUInt32) * runtimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

static TZrBool compiler_emit_runtime_member_decorator_application(SZrCompilerState *cs,
                                                                  TZrUInt32 prototypeSlot,
                                                                  SZrString *memberName,
                                                                  EZrRuntimeDecoratorTargetKind targetKind,
                                                                  TZrUInt32 decoratorSlot,
                                                                  TZrSize trimTargetCount,
                                                                  SZrFileRange location) {
    SZrClosureNative *helperClosure;
    SZrTypeValue helperValue;
    SZrTypeValue kindConstant;
    TZrUInt32 helperSlot;
    TZrUInt32 prototypeArgumentSlot;
    TZrUInt32 memberNameSlot;
    TZrUInt32 kindSlot;
    TZrUInt32 decoratorArgumentSlot;

    if (cs == ZR_NULL || memberName == ZR_NULL || targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_INVALID || cs->hasError) {
        return ZR_FALSE;
    }

    helperClosure = ZrCore_ClosureNative_New(cs->state, 0);
    if (helperClosure == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to create runtime member decorator helper", location);
        return ZR_FALSE;
    }

    helperClosure->nativeFunction = ZrCore_RuntimeDecorator_ApplyMemberNativeEntry;
    ZrCore_RawObject_MarkAsPermanent(cs->state, ZR_CAST_RAW_OBJECT_AS_SUPER(helperClosure));

    ZrCore_Value_InitAsRawObject(cs->state, &helperValue, ZR_CAST_RAW_OBJECT_AS_SUPER(helperClosure));
    helperValue.isNative = ZR_TRUE;

    helperSlot = allocate_stack_slot(cs);
    emit_constant_to_slot(cs, helperSlot, &helperValue);

    prototypeArgumentSlot = allocate_stack_slot(cs);
    if (!compiler_emit_copy_stack_value(cs, prototypeArgumentSlot, prototypeSlot)) {
        return ZR_FALSE;
    }

    memberNameSlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, memberNameSlot, memberName);

    kindSlot = allocate_stack_slot(cs);
    ZrCore_Value_InitAsInt(cs->state, &kindConstant, (TZrInt64)targetKind);
    emit_constant_to_slot(cs, kindSlot, &kindConstant);

    decoratorArgumentSlot = allocate_stack_slot(cs);
    if (!compiler_emit_copy_stack_value(cs, decoratorArgumentSlot, decoratorSlot)) {
        return ZR_FALSE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)helperSlot,
                                          (TZrUInt16)helperSlot,
                                          4));
    ZrParser_Compiler_TrimStackToCount(cs, trimTargetCount);
    return !cs->hasError;
}

TZrBool emit_runtime_member_decorator_applications(SZrCompilerState *cs,
                                                   SZrAstNodeArray *decorators,
                                                   SZrString *typeName,
                                                   SZrString *memberName,
                                                   EZrRuntimeDecoratorTargetKind targetKind,
                                                   SZrFileRange location) {
    TZrUInt32 *decoratorSlots = ZR_NULL;
    TZrSize runtimeDecoratorCount = 0;
    TZrSize decoratorIndex = 0;
    TZrUInt32 prototypeSlot;
    TZrSize trimTargetCount;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || decorators == ZR_NULL || decorators->count == 0 || typeName == ZR_NULL ||
        memberName == ZR_NULL || targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_INVALID || cs->hasError) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            runtimeDecoratorCount++;
        } else if (cs->hasError) {
            goto cleanup;
        }
    }

    if (runtimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    decoratorSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(TZrUInt32) * runtimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (decoratorSlots == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to allocate runtime member decorator slots", location);
        goto cleanup;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrString *tempName;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }
        if (decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION || decoratorNode->data.decoratorExpression.expr == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "invalid runtime decorator expression", decoratorNode->location);
            goto cleanup;
        }

        tempName = compiler_create_runtime_decorator_temp_name(cs, "member_definition", (TZrUInt32)decoratorIndex);
        if (tempName == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "failed to allocate runtime member decorator temp", decoratorNode->location);
            goto cleanup;
        }

        decoratorSlots[decoratorIndex] = allocate_local_var(cs, tempName);
        if (compile_expression_into_slot(cs,
                                         decoratorNode->data.decoratorExpression.expr,
                                         decoratorSlots[decoratorIndex]) == ZR_PARSER_SLOT_NONE ||
            cs->hasError) {
            goto cleanup;
        }

        decoratorIndex++;
    }

    prototypeSlot = emit_load_global_identifier(cs, typeName);
    if (prototypeSlot == ZR_PARSER_SLOT_NONE || cs->hasError) {
        goto cleanup;
    }

    trimTargetCount = cs->stackSlotCount;
    for (TZrSize index = runtimeDecoratorCount; index > 0; index--) {
        if (!compiler_emit_runtime_member_decorator_application(cs,
                                                                prototypeSlot,
                                                                memberName,
                                                                targetKind,
                                                                decoratorSlots[index - 1],
                                                                trimTargetCount,
                                                                location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (decoratorSlots != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorSlots,
                                      sizeof(TZrUInt32) * runtimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportModuleExpression(SZrCompilerState *cs,
                                                                     SZrString *moduleName,
                                                                     SZrFileRange location) {
    SZrClosureNative *importClosure;
    SZrTypeValue importCallable;
    TZrUInt32 functionSlot;
    TZrUInt32 argumentSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    importClosure = ZrCore_ClosureNative_New(cs->state, 0);
    if (importClosure == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to create internal import callable", location);
        return ZR_PARSER_SLOT_NONE;
    }
    importClosure->nativeFunction = ZrCore_Module_ImportNativeEntry;
    ZrCore_RawObject_MarkAsPermanent(cs->state, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));

    ZrCore_Value_InitAsRawObject(cs->state, &importCallable, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));
    importCallable.isNative = ZR_TRUE;

    functionSlot = allocate_stack_slot(cs);
    emit_constant_to_slot(cs, functionSlot, &importCallable);

    argumentSlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, argumentSlot, moduleName);

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          1));
    ZrParser_Compiler_TrimStackToSlot(cs, functionSlot);
    return functionSlot;
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitTypeQueryExpression(SZrCompilerState *cs,
                                                                  SZrAstNode *operand,
                                                                  SZrFileRange location) {
    TZrUInt32 argumentSlot;

    if (cs == ZR_NULL || operand == ZR_NULL || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    argumentSlot = allocate_stack_slot(cs);
    if (compile_expression_into_slot(cs, operand, argumentSlot) == ZR_PARSER_SLOT_NONE || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(TYPEOF),
                                          (TZrUInt16)argumentSlot,
                                          (TZrUInt16)argumentSlot,
                                          0));
    ZrParser_Compiler_TrimStackToSlot(cs, argumentSlot);
    ZR_UNUSED_PARAMETER(location);
    return argumentSlot;
}

