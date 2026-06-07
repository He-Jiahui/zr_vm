#include "debug_internal.h"

#include <stdint.h>

#include "zr_vm_parser/compiler.h"
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

static void zr_debug_semantic_register_value_binding(SZrCompilerState *compilerState,
                                                     const TZrChar *nameText,
                                                     const SZrTypeValue *value,
                                                     TZrBool replaceExisting) {
    SZrString *name;
    SZrInferredType inferredType;
    SZrFileRange emptyRange;

    if (compilerState == ZR_NULL ||
        compilerState->state == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL ||
        nameText == ZR_NULL) {
        return;
    }

    name = ZrCore_String_Create(compilerState->state, (TZrNativeString)nameText, strlen(nameText));
    if (name == ZR_NULL) {
        return;
    }
    if (!replaceExisting && ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, name) != ZR_NULL) {
        return;
    }

    memset(&emptyRange, 0, sizeof(emptyRange));
    ZrParser_InferredType_Init(compilerState->state,
                               &inferredType,
                               value != ZR_NULL ? value->type : ZR_VALUE_TYPE_OBJECT);
    zr_debug_semantic_apply_exact_value_range(value, &inferredType);
    ZrParser_TypeEnvironment_RegisterVariableEx(compilerState->state,
                                                compilerState->typeEnv,
                                                name,
                                                &inferredType,
                                                ZR_NULL,
                                                emptyRange);
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
}

static void zr_debug_semantic_register_globals(ZrDebugAgent *agent, SZrCompilerState *compilerState) {
    if (agent == ZR_NULL || agent->state == ZR_NULL || agent->state->global == ZR_NULL) {
        return;
    }

    zr_debug_semantic_register_value_binding(
            compilerState, "loadedModules", &agent->state->global->loadedModulesRegistry, ZR_FALSE);
    zr_debug_semantic_register_value_binding(compilerState, "zr", &agent->state->global->zrObject, ZR_FALSE);
    if (agent->state->hasCurrentException) {
        zr_debug_semantic_register_value_binding(compilerState, "error", &agent->state->currentException, ZR_FALSE);
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
        SZrFileRange emptyRange;

        if (binding->name == ZR_NULL ||
            ZrParser_TypeEnvironment_FindVariableBinding(compilerState->typeEnv, binding->name) != ZR_NULL) {
            continue;
        }

        memset(&emptyRange, 0, sizeof(emptyRange));
        zr_debug_semantic_type_ref_to_inferred(compilerState, &binding->type, &inferredType);
        (void)ZrParser_TypeEnvironment_RegisterVariableEx(compilerState->state,
                                                          compilerState->typeEnv,
                                                          binding->name,
                                                          &inferredType,
                                                          ZR_NULL,
                                                          emptyRange);
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

static void zr_debug_semantic_register_frame_variables(ZrDebugAgent *agent,
                                                       TZrUInt32 frameId,
                                                       SZrCompilerState *compilerState) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    TZrUInt32 pc;
    TZrUInt32 slotIndex;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        compilerState == ZR_NULL ||
        compilerState->typeEnv == ZR_NULL) {
        return;
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId == 0 ? 1u : frameId, &function);
    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return;
    }

    pc = zr_debug_instruction_offset(callInfo, function);
    for (slotIndex = 0; slotIndex < function->stackSize; ++slotIndex) {
        SZrString *name = ZrCore_Function_GetLocalVariableName(function, slotIndex, pc);
        const SZrTypeValue *slotValue;

        if (name == ZR_NULL) {
            continue;
        }

        slotValue = zr_debug_frame_value_slot(agent->state, function, callInfo, slotIndex);
        zr_debug_semantic_register_value_binding(compilerState, zr_debug_string_native(name), slotValue, ZR_TRUE);
    }

    for (slotIndex = 0; slotIndex < function->closureValueLength; ++slotIndex) {
        SZrString *name = function->closureValueList[slotIndex].name;
        const TZrChar *nameText = zr_debug_string_native(name);
        const SZrTypeValue *captureValue = zr_debug_closure_capture_value(agent, function, callInfo, nameText);

        if (nameText == ZR_NULL || nameText[0] == '\0' || captureValue == ZR_NULL) {
            continue;
        }

        zr_debug_semantic_register_value_binding(compilerState, nameText, captureValue, ZR_TRUE);
    }
}

void zr_debug_semantic_register_bindings(ZrDebugAgent *agent,
                                         TZrUInt32 frameId,
                                         SZrCompilerState *compilerState) {
    zr_debug_semantic_register_frame_variables(agent, frameId, compilerState);
    zr_debug_semantic_register_globals(agent, compilerState);
    zr_debug_semantic_register_entry_typed_locals(agent, compilerState);
    zr_debug_semantic_register_entry_callables(agent, compilerState);
}
