#include "debug_internal.h"

#include <stdint.h>

#include "zr_vm_core/debug.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

static void zr_debug_semantic_apply_exact_value_range(const SZrTypeValue *value,
                                                      SZrInferredType *inferredType) {
    TZrUInt64 unsignedValue;

    if (value == ZR_NULL || inferredType == ZR_NULL) {
        return;
    }

    if (value->type == ZR_VALUE_TYPE_BOOL) {
        inferredType->knownBoolValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        inferredType->hasKnownBoolValue = ZR_TRUE;
        return;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        inferredType->minValue = value->value.nativeObject.nativeInt64;
        inferredType->maxValue = value->value.nativeObject.nativeInt64;
        inferredType->hasRangeConstraint = ZR_TRUE;
        return;
    }

    if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return;
    }

    unsignedValue = value->value.nativeObject.nativeUInt64;
    if (unsignedValue <= (TZrUInt64)INT64_MAX) {
        inferredType->minValue = (TZrInt64)unsignedValue;
        inferredType->maxValue = (TZrInt64)unsignedValue;
        inferredType->hasRangeConstraint = ZR_TRUE;
    }
}

static TZrBool zr_debug_semantic_register_runtime_root(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState) {
    SZrDebugEvaluationContext context;
    SZrDebugRuntimeRootBinding runtimeRoot;
    SZrTypeValue value;
    SZrInferredType inferredType;
    SZrString *name;
    EZrDebugEvaluationContextStatus status;
    TZrBool registered;

    if (agent == ZR_NULL || agent->state == ZR_NULL || compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return ZR_FALSE;
    }

    name = ZrCore_String_CreateFromNative(compilerState->state, "zr");
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name) != ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&context, 0, sizeof(context));
    status = ZrCore_Debug_GetEvaluationContext(
            agent->state, frameId == 0u ? 0u : frameId - 1u, &context);
    if (status == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT ||
        status == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE) {
        return ZR_TRUE;
    }
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    memset(&runtimeRoot, 0, sizeof(runtimeRoot));
    status = ZrCore_Debug_EvaluationContext_GetRuntimeRoot(
            agent->state, &context, ZR_DEBUG_RUNTIME_ROOT_ZR, &runtimeRoot);
    if (status == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE) {
        return ZR_TRUE;
    }
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&value);
    if (ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
                agent->state, &context, &runtimeRoot, &value) !=
        ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(compilerState->state, &inferredType, value.type);
    zr_debug_semantic_apply_exact_value_range(&value, &inferredType);
    registered = ZrParser_TypeEnvironment_RegisterRuntimeRoot(
            compilerState->state,
            compilerState->typeEnv,
            name,
            &inferredType,
            ZR_SEMANTIC_RUNTIME_ROOT_ZR,
            runtimeRoot.token);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return registered;
}

static void zr_debug_semantic_register_summary_value_binding(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState,
        const SZrAstNode *identifier) {
    SZrString *name;
    SZrTypeValue value;
    SZrInferredType inferredType;
    SZrFileRange emptyRange;

    if (agent == ZR_NULL || compilerState == ZR_NULL || compilerState->typeEnv == ZR_NULL ||
        identifier == ZR_NULL || identifier->type != ZR_AST_IDENTIFIER_LITERAL) {
        return;
    }
    name = identifier->data.identifier.name;
    if (name == ZR_NULL ||
        ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name) != ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(&value);
    if (!zr_debug_resolve_identifier_value(agent,
                                           frameId,
                                           zr_debug_string_native(name),
                                           &value,
                                           ZR_NULL,
                                           0u)) {
        return;
    }

    memset(&emptyRange, 0, sizeof(emptyRange));
    ZrParser_InferredType_Init(compilerState->state, &inferredType, value.type);
    zr_debug_semantic_apply_exact_value_range(&value, &inferredType);
    (void)ZrParser_TypeEnvironment_RegisterVariableEx(compilerState->state,
                                                      compilerState->typeEnv,
                                                      name,
                                                      &inferredType,
                                                      ZR_NULL,
                                                      emptyRange);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
}

static void zr_debug_semantic_register_summary_node(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *node);

static void zr_debug_semantic_register_summary_list(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return;
    }
    for (index = 0u; index < nodes->count; ++index) {
        zr_debug_semantic_register_summary_node(
                agent, frameId, compilerState, nodes->nodes[index]);
    }
}

static void zr_debug_semantic_register_summary_node(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            zr_debug_semantic_register_summary_value_binding(
                    agent, frameId, compilerState, node);
            break;
        case ZR_AST_BINARY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.binaryExpression.left);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.binaryExpression.right);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.logicalExpression.left);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.logicalExpression.right);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.unaryExpression.argument);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.typeCastExpression.expression);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.typeQueryExpression.operand);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.conditionalExpression.test);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.conditionalExpression.consequent);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.conditionalExpression.alternate);
            break;
        case ZR_AST_IF_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.ifExpression.condition);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.ifExpression.thenExpr);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.ifExpression.elseExpr);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.primaryExpression.property);
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.primaryExpression.members);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.memberExpression.property);
            break;
        case ZR_AST_FUNCTION_CALL:
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.functionCall.args);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.constructExpression.target);
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.constructExpression.args);
            break;
        case ZR_AST_ARRAY_LITERAL:
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.arrayLiteral.elements);
            break;
        case ZR_AST_OBJECT_LITERAL:
            zr_debug_semantic_register_summary_list(
                    agent, frameId, compilerState, node->data.objectLiteral.properties);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.keyValuePair.key);
            zr_debug_semantic_register_summary_node(
                    agent, frameId, compilerState, node->data.keyValuePair.value);
            break;
        default:
            break;
    }
}

static void zr_debug_semantic_type_ref_to_inferred(SZrCompilerState *compilerState,
                                                   const SZrFunctionTypedTypeRef *typeRef,
                                                   SZrInferredType *result) {
    if (compilerState == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (typeRef == ZR_NULL) {
        ZrParser_InferredType_Init(compilerState->state, result, ZR_VALUE_TYPE_OBJECT);
        return;
    }

    if (typeRef->isArray) {
        SZrInferredType elementType;

        ZrParser_InferredType_Init(compilerState->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = typeRef->ownershipQualifier;
        result->isNullable = typeRef->isNullable;
        ZrCore_Array_Init(compilerState->state, &result->elementTypes, sizeof(SZrInferredType), 1);
        ZrParser_InferredType_InitFull(compilerState->state,
                                       &elementType,
                                       typeRef->elementBaseType,
                                       ZR_FALSE,
                                       typeRef->elementTypeName);
        ZrCore_Array_Push(compilerState->state, &result->elementTypes, &elementType);
        return;
    }

    if (typeRef->typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(compilerState->state,
                                       result,
                                       typeRef->baseType,
                                       typeRef->isNullable,
                                       typeRef->typeName);
    } else {
        ZrParser_InferredType_Init(compilerState->state, result, typeRef->baseType);
        result->isNullable = typeRef->isNullable;
    }
    result->ownershipQualifier = typeRef->ownershipQualifier;
}

static const SZrFunctionTypedLocalBinding *zr_debug_semantic_find_typed_local_binding(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedLocalBindingLength; ++index) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];

        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool zr_debug_semantic_register_canonical_frame_binding(
        SZrCompilerState *compilerState,
        const SZrFunction *function,
        const SZrDebugFrameBinding *frameBinding,
        SZrString *activeName,
        const SZrTypeValue *activeValue) {
    const SZrFunctionTypedLocalBinding *typedBinding;
    SZrInferredType inferredType;
    SZrFileRange declarationRange;
    TZrBool registered;

    if (compilerState == ZR_NULL ||
        compilerState->state == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        function == ZR_NULL ||
        frameBinding == ZR_NULL ||
        activeName == ZR_NULL) {
        return ZR_FALSE;
    }

    typedBinding = zr_debug_semantic_find_typed_local_binding(function, frameBinding->stackSlot);
    if (typedBinding == ZR_NULL ||
        typedBinding->name == ZR_NULL ||
        !ZrCore_String_Equal(typedBinding->name, activeName) ||
        frameBinding->symbolId == ZR_SEMANTIC_ID_INVALID ||
        frameBinding->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&declarationRange, 0, sizeof(declarationRange));
    declarationRange.source = function->sourceCodeList;
    declarationRange.start.line = (TZrInt32)frameBinding->declarationStartLine;
    declarationRange.start.column = (TZrInt32)frameBinding->declarationStartColumn;
    declarationRange.end.line = (TZrInt32)frameBinding->declarationEndLine;
    declarationRange.end.column = (TZrInt32)frameBinding->declarationEndColumn;
    zr_debug_semantic_type_ref_to_inferred(compilerState, &typedBinding->type, &inferredType);
    zr_debug_semantic_apply_exact_value_range(activeValue, &inferredType);
    registered = ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(compilerState->state,
                                                                     compilerState->typeEnv,
                                                                     typedBinding->name,
                                                                     &inferredType,
                                                                     frameBinding->symbolId,
                                                                     frameBinding->typeId,
                                                                     frameBinding->placeId,
                                                                     declarationRange);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return registered;
}

static void zr_debug_semantic_register_entry_typed_locals(ZrDebugAgent *agent,
                                                          SZrCompilerState *compilerState) {
    SZrFunction *entryFunction;
    TZrUInt32 index;

    if (agent == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        agent->entryFunction == ZR_NULL) {
        return;
    }

    entryFunction = agent->entryFunction;
    for (index = 0; index < entryFunction->typedLocalBindingLength; ++index) {
        const SZrFunctionTypedLocalBinding *binding = &entryFunction->typedLocalBindings[index];
        SZrInferredType inferredType;
        SZrFileRange declarationRange;
        TZrBool hasCanonicalIdentity;

        if (binding->name == ZR_NULL ||
            ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, binding->name) != ZR_NULL) {
            continue;
        }

        memset(&declarationRange, 0, sizeof(declarationRange));
        declarationRange.source = entryFunction->sourceCodeList;
        declarationRange.start.line = (TZrInt32)binding->declarationStartLine;
        declarationRange.start.column = (TZrInt32)binding->declarationStartColumn;
        declarationRange.end.line = (TZrInt32)binding->declarationEndLine;
        declarationRange.end.column = (TZrInt32)binding->declarationEndColumn;
        hasCanonicalIdentity = binding->symbolId != ZR_SEMANTIC_ID_INVALID &&
                               binding->typeId != ZR_SEMANTIC_ID_INVALID &&
                               declarationRange.source != ZR_NULL;
        zr_debug_semantic_type_ref_to_inferred(compilerState, &binding->type, &inferredType);
        if (hasCanonicalIdentity) {
            (void)ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
                    compilerState->state,
                    compilerState->typeEnv,
                    binding->name,
                    &inferredType,
                    binding->symbolId,
                    binding->typeId,
                    binding->placeId,
                    declarationRange);
        }
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
    }
}

static void zr_debug_semantic_free_inferred_type_array(SZrCompilerState *compilerState, SZrArray *types) {
    TZrSize index;

    if (compilerState == ZR_NULL || types == ZR_NULL || !types->isValid) {
        return;
    }

    for (index = 0; index < types->length; ++index) {
        SZrInferredType *type = (SZrInferredType *)ZrCore_Array_Get(types, index);
        if (type != ZR_NULL) {
            ZrParser_InferredType_Free(compilerState->state, type);
        }
    }
    if (types->head != ZR_NULL) {
        ZrCore_Array_Free(compilerState->state, types);
    }
    ZrCore_Array_Construct(types);
}

static TZrBool zr_debug_semantic_collect_callable_parameter_types(SZrCompilerState *compilerState,
                                                                  const SZrFunction *callable,
                                                                  SZrArray *paramTypes,
                                                                  SZrArray *parameterPassingModes) {
    TZrUInt32 index;

    if (compilerState == ZR_NULL || callable == ZR_NULL || paramTypes == ZR_NULL ||
        parameterPassingModes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(compilerState->state,
                      paramTypes,
                      sizeof(SZrInferredType),
                      callable->parameterMetadataCount > 0 ? callable->parameterMetadataCount : 1u);
    ZrCore_Array_Init(compilerState->state,
                      parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      callable->parameterMetadataCount > 0 ? callable->parameterMetadataCount : 1u);

    for (index = 0; index < callable->parameterMetadataCount; ++index) {
        SZrInferredType paramType;
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

        zr_debug_semantic_type_ref_to_inferred(compilerState, &callable->parameterMetadata[index].type, &paramType);
        ZrCore_Array_Push(compilerState->state, paramTypes, &paramType);
        ZrCore_Array_Push(compilerState->state, parameterPassingModes, &passingMode);
    }

    return ZR_TRUE;
}

static void zr_debug_semantic_register_entry_callables(ZrDebugAgent *agent, SZrCompilerState *compilerState) {
    SZrFunction *entryFunction;
    TZrUInt32 index;

    if (agent == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        agent->entryFunction == ZR_NULL) {
        return;
    }

    entryFunction = agent->entryFunction;
    for (index = 0; index < entryFunction->topLevelCallableBindingLength; ++index) {
        const SZrFunctionTopLevelCallableBinding *binding = &entryFunction->topLevelCallableBindings[index];
        const SZrFunction *callable;
        SZrInferredType returnType;
        SZrArray paramTypes;
        SZrArray parameterPassingModes;
        TZrBool returnTypeInitialized = ZR_FALSE;
        TZrBool paramTypesInitialized = ZR_FALSE;
        TZrBool parameterPassingModesInitialized = ZR_FALSE;

        if (binding->name == ZR_NULL ||
            binding->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            binding->callableChildIndex >= entryFunction->childFunctionLength) {
            continue;
        }

        callable = &entryFunction->childFunctionList[binding->callableChildIndex];
        if (!callable->hasCallableReturnType) {
            continue;
        }

        ZrCore_Array_Construct(&paramTypes);
        ZrCore_Array_Construct(&parameterPassingModes);
        zr_debug_semantic_type_ref_to_inferred(compilerState, &callable->callableReturnType, &returnType);
        returnTypeInitialized = ZR_TRUE;

        if (!zr_debug_semantic_collect_callable_parameter_types(compilerState,
                                                               callable,
                                                               &paramTypes,
                                                               &parameterPassingModes)) {
            goto cleanup;
        }
        paramTypesInitialized = ZR_TRUE;
        parameterPassingModesInitialized = ZR_TRUE;

        (void)ZrParser_TypeEnvironment_RegisterFunctionEx(compilerState->state,
                                                          compilerState->typeEnv,
                                                          binding->name,
                                                          &returnType,
                                                          &paramTypes,
                                                          ZR_NULL,
                                                          &parameterPassingModes,
                                                          ZR_NULL);

cleanup:
        if (returnTypeInitialized) {
            ZrParser_InferredType_Free(compilerState->state, &returnType);
        }
        if (paramTypesInitialized) {
            zr_debug_semantic_free_inferred_type_array(compilerState, &paramTypes);
        }
        if (parameterPassingModesInitialized && parameterPassingModes.head != ZR_NULL) {
            ZrCore_Array_Free(compilerState->state, &parameterPassingModes);
        }
    }
}

static EZrDebugEvaluationContextStatus zr_debug_semantic_register_frame_variables(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState) {
    SZrDebugEvaluationContext context;
    SZrDebugFrameBinding frameBinding;
    EZrDebugEvaluationContextStatus status;
    TZrUInt32 bindingIndex;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    status = ZrCore_Debug_GetEvaluationContext(agent->state,
                                               frameId == 0 ? 0u : frameId - 1u,
                                               &context);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }

    if (context.activation.function == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    for (bindingIndex = 0; bindingIndex < context.activeBindingCount; ++bindingIndex) {
        SZrString *activeName;
        const SZrTypeValue *activeValue;

        memset(&frameBinding, 0, sizeof(frameBinding));
        status = ZrCore_Debug_EvaluationContext_GetBinding(agent->state,
                                                           &context,
                                                           bindingIndex,
                                                           &frameBinding);
        if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return status;
        }

        activeName = ZrCore_Function_GetLocalVariableName(context.activation.function,
                                                           frameBinding.stackSlot,
                                                           context.instructionOffset);
        activeValue = zr_debug_frame_value_slot(agent->state,
                                                context.activation.function,
                                                context.activation.callInfo,
                                                frameBinding.stackSlot);
        if (!zr_debug_semantic_register_canonical_frame_binding(compilerState,
                                                                context.activation.function,
                                                                &frameBinding,
                                                                activeName,
                                                                activeValue)) {
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }
    }

    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}

static EZrDebugEvaluationContextStatus zr_debug_semantic_register_closure_captures(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        SZrCompilerState *compilerState) {
    SZrDebugEvaluationContext context;
    EZrDebugEvaluationContextStatus status;
    TZrUInt32 captureIndex;

    if (agent == ZR_NULL || agent->state == ZR_NULL || compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    status = ZrCore_Debug_GetEvaluationContext(
            agent->state, frameId == 0u ? 0u : frameId - 1u, &context);
    if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        return status;
    }
    if (context.activation.function == ZR_NULL || context.activation.function->closureValueLength == 0u) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
    }
    if (context.activation.function->closureValueList == ZR_NULL) {
        return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
    }

    for (captureIndex = 0u;
         captureIndex < context.activation.function->closureValueLength;
         ++captureIndex) {
        SZrDebugClosureCaptureBinding capture;
        SZrString *name;
        const SZrTypeBinding *existing;
        SZrInferredType inferredType;
        SZrFileRange declarationRange;

        memset(&capture, 0, sizeof(capture));
        status = ZrCore_Debug_EvaluationContext_GetClosureCapture(
                agent->state, &context, captureIndex, &capture);
        if (status != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return status;
        }
        name = context.activation.function->closureValueList[captureIndex].name;
        if (name == ZR_NULL) {
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }

        existing = ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name);
        if (existing != ZR_NULL) {
            if (existing->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION) {
                continue;
            }
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }

        memset(&declarationRange, 0, sizeof(declarationRange));
        declarationRange.source = context.activation.function->sourceCodeList;
        declarationRange.start.line = (TZrInt32)capture.declarationStartLine;
        declarationRange.start.column = (TZrInt32)capture.declarationStartColumn;
        declarationRange.end.line = (TZrInt32)capture.declarationEndLine;
        declarationRange.end.column = (TZrInt32)capture.declarationEndColumn;
        zr_debug_semantic_type_ref_to_inferred(compilerState, capture.type, &inferredType);
        if (!ZrParser_TypeEnvironment_RegisterClosureCapture(
                    compilerState->state,
                    compilerState->typeEnv,
                    name,
                    &inferredType,
                    capture.symbolId,
                    capture.typeId,
                    declarationRange,
                    capture.captureIndex,
                    capture.token)) {
            ZrParser_InferredType_Free(compilerState->state, &inferredType);
            return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE;
        }
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
    }

    return ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK;
}

TZrBool zr_debug_semantic_register_bindings(ZrDebugAgent *agent,
                                            TZrUInt32 frameId,
                                            SZrCompilerState *compilerState) {
    EZrDebugEvaluationContextStatus frameStatus;

    frameStatus = zr_debug_semantic_register_frame_variables(agent, frameId, compilerState);
    if (frameStatus != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK &&
        frameStatus != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT) {
        return ZR_FALSE;
    }

    if (frameStatus == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT) {
        zr_debug_semantic_register_entry_typed_locals(agent, compilerState);
    }
    if (frameStatus == ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
        frameStatus = zr_debug_semantic_register_closure_captures(agent, frameId, compilerState);
        if (frameStatus != ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK) {
            return ZR_FALSE;
        }
    }
    if (!zr_debug_semantic_register_runtime_root(agent, frameId, compilerState)) {
        return ZR_FALSE;
    }
    if (agent->entryFunction != ZR_NULL &&
        !ZrParser_TypeInference_RegisterRuntimePrototypes(
                compilerState,
                agent->entryFunction)) {
        return ZR_FALSE;
    }
    zr_debug_semantic_register_entry_callables(agent, compilerState);
    return ZR_TRUE;
}

TZrBool zr_debug_semantic_register_summary_bindings(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *expression) {
    TZrBool canonicalBindingsAvailable =
            zr_debug_semantic_register_bindings(agent, frameId, compilerState);

    zr_debug_semantic_register_summary_node(agent, frameId, compilerState, expression);
    return (TZrBool)(canonicalBindingsAvailable || expression != ZR_NULL);
}
