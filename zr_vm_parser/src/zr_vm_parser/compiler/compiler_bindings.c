//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "compile_expression_internal.h"
#include "type_inference_internal.h"
#include "compiler_attribute_binding.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/project_imports.h"
#include "zr_vm_parser/semantic_facts.h"

TZrBool compiler_is_compile_tool_import_declaration(const SZrState *state,
                                                     const SZrAstNode *node) {
    const SZrVariableDeclaration *declaration;
    const SZrAstNode *modulePath;
    const TZrChar *moduleName;

    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }

    declaration = &node->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->value == ZR_NULL ||
        declaration->value->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_FALSE;
    }

    modulePath = declaration->value->data.importExpression.modulePath;
    if (modulePath == ZR_NULL ||
        modulePath->type != ZR_AST_STRING_LITERAL ||
        modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleName = ZrCore_String_GetNativeString(modulePath->data.stringLiteral.value);
    return ZrParser_CompileTool_IsModuleName(moduleName) ||
           ZrParser_ProjectImports_IsBuildDependencySpecifier(state, moduleName);
}

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
                                                      SZrAstNode *lambdaNode) {
    SZrLambdaExpression *lambda;
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *lambdaEnv;
    SZrInferredType returnType;
    SZrFunctionTypedTypeRef returnMetadata;
    SZrArray paramTypes;
    SZrArray parameterPassingModes;
    TZrSize bindingIndex;
    TZrBool hasReturnType = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL ||
        name == ZR_NULL || lambdaNode == ZR_NULL ||
        lambdaNode->type != ZR_AST_LAMBDA_EXPRESSION) {
        return;
    }
    lambda = &lambdaNode->data.lambdaExpression;

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
                ZrParser_TypeEnvironment_RegisterVariableEx(
                        cs->state,
                        cs->typeEnv,
                        parameter->name->name,
                        &paramType,
                        paramNode,
                        paramNode->location);
                ZrParser_InferredType_Free(cs->state, &paramType);
                continue;
            }

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_TypeEnvironment_RegisterVariableEx(
                    cs->state,
                    cs->typeEnv,
                    parameter->name->name,
                    &paramType,
                    paramNode,
                    paramNode->location);
            ZrParser_InferredType_Free(cs->state, &paramType);
        }
    }

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_NULL);
    (void)compiler_build_callable_return_type_metadata_with_inferred(
            cs,
            lambda->returnType,
            lambda->block,
            &returnMetadata,
            &hasReturnType,
            &returnType);

    compiler_collect_callable_parameter_types(cs, lambda->params, &paramTypes);
    ZrCore_Array_Construct(&parameterPassingModes);
    compiler_collect_parameter_passing_modes(cs->state, &parameterPassingModes, lambda->params);

    bindingIndex = savedEnv->functionReturnTypes.length;
    if (ZrParser_TypeEnvironment_RegisterCallableValueFunction(
                cs->state,
                savedEnv,
                name,
                &returnType,
                &paramTypes,
                ZR_NULL,
                &parameterPassingModes,
                lambdaNode)) {
        (void)compiler_publish_lambda_callable_binding_identity(
                cs, savedEnv, bindingIndex, lambdaNode);
    }

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

        if ((*candidatePtr)->declarationNode != ZR_NULL &&
            (*candidatePtr)->declarationNode->type == ZR_AST_FUNCTION_DECLARATION &&
            (*candidatePtr)->declarationNode->data.functionDeclaration.returnType == ZR_NULL) {
            SZrFunctionDeclaration *declaration =
                    &(*candidatePtr)->declarationNode->data.functionDeclaration;
            SZrFunctionTypedTypeRef returnMetadata;
            SZrInferredType inferredReturnType;
            TZrBool hasReturnType = ZR_FALSE;

            if (!compiler_build_callable_return_type_metadata_with_inferred(
                        cs,
                        ZR_NULL,
                        declaration->body,
                        &returnMetadata,
                        &hasReturnType,
                        &inferredReturnType)) {
                continue;
            }
            if (!hasReturnType ||
                !compiler_refine_function_type_binding_return(
                        cs,
                        (*candidatePtr)->declarationNode,
                        &inferredReturnType)) {
                ZrParser_InferredType_Free(cs->state, &inferredReturnType);
                continue;
            }
            ZrParser_InferredType_Free(cs->state, &inferredReturnType);
        }

        ZrParser_TypeEnvironment_RegisterCallableValueFunction(
                cs->state,
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

static void compiler_register_external_member_callable_binding(
        SZrCompilerState *cs,
        SZrString *name,
        SZrAstNode *valueNode) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;
    SZrAstNode *property;
    const SZrSemanticReferenceFact *reference;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL ||
        cs->semanticContext == ZR_NULL || name == ZR_NULL || valueNode == ZR_NULL ||
        valueNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return;
    }
    primary = &valueNode->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count == 0U) {
        return;
    }
    memberNode = primary->members->nodes[primary->members->count - 1U];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        return;
    }
    property = memberNode->data.memberExpression.property;
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext,
            property != ZR_NULL ? property : memberNode,
            ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
    if (reference == ZR_NULL || reference->isResolved ||
        reference->symbolId != ZR_SEMANTIC_ID_INVALID ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        reference->signatureDisplay == ZR_NULL ||
        reference->declarationRange.source != ZR_NULL ||
        reference->declarationRange.start.offset != 0U ||
        reference->declarationRange.end.offset != 0U) {
        return;
    }
    (void)ZrParser_TypeEnvironment_RegisterExternalCallableAlias(
            cs->state,
            cs->typeEnv,
            name,
            reference->typeId,
            reference->signatureDisplay);
}

void ZrParser_Compiler_RegisterCallableValueBinding(SZrCompilerState *cs,
                                                     SZrString *name,
                                                     SZrAstNode *valueNode) {
    if (cs == ZR_NULL || name == ZR_NULL || valueNode == ZR_NULL || cs->typeEnv == ZR_NULL) {
        return;
    }

    if (valueNode->type == ZR_AST_LAMBDA_EXPRESSION) {
        compiler_register_lambda_callable_binding(cs, name, valueNode);
        return;
    }

    if (valueNode->type == ZR_AST_IDENTIFIER_LITERAL && valueNode->data.identifier.name != ZR_NULL) {
        compiler_register_identifier_callable_binding(cs, name, valueNode->data.identifier.name);
        return;
    }

    compiler_register_external_member_callable_binding(cs, name, valueNode);
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

static TZrBool compiler_convert_extern_type_to_inferred(SZrCompilerState *cs,
                                                        SZrType *typeInfo,
                                                        SZrInferredType *result) {
    TZrBool originalImplicitBuiltinType;
    TZrBool isPointerDescriptor = ZR_FALSE;
    TZrBool converted;

    if (cs == ZR_NULL || typeInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeInfo->name != ZR_NULL && typeInfo->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeInfo->name->data.genericType;
        isPointerDescriptor = genericType->name != ZR_NULL &&
                              extern_compiler_string_equals(genericType->name->name, "pointer");
    }

    originalImplicitBuiltinType = typeInfo->isImplicitBuiltinType;
    if (isPointerDescriptor) {
        typeInfo->isImplicitBuiltinType = ZR_TRUE;
    }
    converted = ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, result);
    typeInfo->isImplicitBuiltinType = originalImplicitBuiltinType;
    return converted;
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
        if (!compiler_convert_extern_type_to_inferred(cs, functionDecl->returnType, &returnType)) {
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
                if (!compiler_convert_extern_type_to_inferred(
                            cs, paramNode->data.parameter.typeInfo, &paramType)) {
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

        if (compiler_is_compile_tool_import_declaration(cs->state, stmt)) {
            continue;
        }

        if (stmt->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            stmt->data.compileTimeDeclaration.isConditionalPruning &&
            stmt->data.compileTimeDeclaration.selectedBranch != ZR_NULL) {
            SZrAstNode *selectedBranch = stmt->data.compileTimeDeclaration.selectedBranch;
            if (selectedBranch->type == ZR_AST_BLOCK) {
                ZrParser_Compiler_PredeclareFunctionBindings(cs, selectedBranch->data.block.body);
                if (cs->hasError) {
                    return;
                }
                continue;
            }
            stmt = selectedBranch;
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
    cs->lastExpressionSlot = globalSlot;
    return globalSlot;
}

static TZrUInt32 compiler_emit_import_module_expression_with_helper(SZrCompilerState *cs,
                                                                    SZrString *moduleName,
                                                                    FZrNativeFunction nativeFunction,
                                                                    SZrFileRange location) {
    SZrClosureNative *importClosure;
    SZrTypeValue importCallable;
    TZrUInt32 functionSlot;
    TZrUInt32 argumentSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || nativeFunction == ZR_NULL || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    importClosure = ZrCore_ClosureNative_New(cs->state, 0);
    if (importClosure == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to create internal import callable", location);
        return ZR_PARSER_SLOT_NONE;
    }
    importClosure->nativeFunction = nativeFunction;
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
    cs->lastExpressionSlot = functionSlot;
    return functionSlot;
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportModuleExpression(SZrCompilerState *cs,
                                                                     SZrString *moduleName,
                                                                     SZrFileRange location) {
    return compiler_emit_import_module_expression_with_helper(cs,
                                                             moduleName,
                                                             ZrCore_Module_ImportNativeEntry,
                                                             location);
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportGuardModuleExpression(SZrCompilerState *cs,
                                                                          SZrString *moduleName,
                                                                          SZrFileRange location) {
    return compiler_emit_import_module_expression_with_helper(cs,
                                                             moduleName,
                                                             ZrCore_Module_ImportGuardNativeEntry,
                                                             location);
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitTypeQueryExpression(SZrCompilerState *cs,
                                                                  SZrAstNode *operand,
                                                                  SZrFileRange location) {
    TZrUInt32 argumentSlot;
    TZrUInt32 resultSlot;

    if (cs == ZR_NULL || operand == ZR_NULL || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    argumentSlot = allocate_stack_slot(cs);
    if (compile_expression_into_slot(cs, operand, argumentSlot) == ZR_PARSER_SLOT_NONE || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    resultSlot = allocate_fresh_stack_slot_after(cs, argumentSlot);
    if (resultSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_PARSER_SLOT_NONE;
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(TYPEOF),
                                          (TZrUInt16)resultSlot,
                                          (TZrUInt16)argumentSlot,
                                          0));
    ZrParser_Compiler_TrimStackToSlot(cs, resultSlot);
    cs->lastExpressionSlot = resultSlot;
    ZR_UNUSED_PARAMETER(location);
    return resultSlot;
}

