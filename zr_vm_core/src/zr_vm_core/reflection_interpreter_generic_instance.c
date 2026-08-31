//
// Interpreter object context for reflection-created generic reference and boxed value instances.
//

#include "zr_vm_core/reflection.h"

#include "object/object_call_internal.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static const TZrChar *kInterpreterGenericTypeObjectField = "__zr_genericTypeInfo";
static const TZrChar *kGenericBaseTokenField = "genericBaseToken";
static const TZrChar *kGenericMethodTokenField = "genericMethodToken";
static const TZrChar *kGenericArgumentCountField = "genericArgumentCount";
static const TZrChar *kGenericArgumentsField = "genericArguments";
static const TZrChar *kMetadataRuntimeField = "metadataRuntime";

static TZrBool reflection_interpreter_generic_pin(
        SZrState *state,
        SZrRawObject *object,
        TZrBool *addedByCaller) {
    return ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            state != ZR_NULL ? state->global : ZR_NULL, state, object, addedByCaller);
}

static void reflection_interpreter_generic_unpin(
        SZrGlobalState *global,
        SZrRawObject *object,
        TZrBool addedByCaller) {
    if (addedByCaller && global != ZR_NULL && object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

static TZrBool reflection_interpreter_generic_is_supported_instance(
        const SZrObject *object) {
    return (TZrBool)(object != ZR_NULL &&
                     (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT ||
                      object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT));
}

static const SZrTypeValue *reflection_interpreter_generic_get_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue fieldKey;
    const SZrTypeValue *fieldValue = ZR_NULL;
    TZrBool fieldStringPinned = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString != ZR_NULL &&
        reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &fieldStringPinned)) {
        ZrCore_Value_InitAsRawObject(
                state, &fieldKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
        fieldKey.type = ZR_VALUE_TYPE_STRING;
        fieldValue = ZrCore_Object_GetValue(state, object, &fieldKey);
    }
    reflection_interpreter_generic_unpin(state->global,
                                         fieldString != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)
                                                 : ZR_NULL,
                                         fieldStringPinned);
    return fieldValue;
}

SZrObject *ZrCore_Reflection_NewInterpreterGenericInstanceObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        SZrObjectPrototype *openPrototype) {
    SZrReflectionDynamicGenericTypeInstance resolved;
    SZrObject *typeObject;
    SZrObject *object = ZR_NULL;
    SZrObject *result = ZR_NULL;
    SZrString *fieldName = ZR_NULL;
    SZrTypeValue fieldKey;
    SZrTypeValue fieldValue;
    const SZrTypeValue *storedValue;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool objectPinned = ZR_FALSE;
    TZrBool fieldNamePinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || instance == ZR_NULL || openPrototype == ZR_NULL ||
        (openPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
         openPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) ||
        !ZrCore_Reflection_RevalidateDynamicGenericTypeInstance(runtime, instance, &resolved) ||
        resolved.route != ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT) {
        return ZR_NULL;
    }

    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &resolved);
    if (typeObject == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject), &typeObjectPinned)) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(state, openPrototype);
    if (object == ZR_NULL) {
        goto cleanup;
    }
    if (openPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        object->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
    }
    ZrCore_Object_Init(state, object);
    if (!reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinned)) {
        goto cleanup;
    }

    fieldName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)kInterpreterGenericTypeObjectField);
    if (fieldName == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName), &fieldNamePinned)) {
        goto cleanup;
    }

    ZrCore_Value_InitAsRawObject(state, &fieldKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    fieldKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &fieldValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject));
    fieldValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, object, &fieldKey, &fieldValue);
    storedValue = ZrCore_Object_GetValue(state, object, &fieldKey);
    if (storedValue == ZR_NULL || storedValue->type != ZR_VALUE_TYPE_OBJECT ||
        storedValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject)) {
        goto cleanup;
    }
    result = object;

cleanup:
    reflection_interpreter_generic_unpin(state->global,
                                         fieldName != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName)
                                                 : ZR_NULL,
                                         fieldNamePinned);
    reflection_interpreter_generic_unpin(state->global,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                                         objectPinned);
    reflection_interpreter_generic_unpin(state->global,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject),
                                         typeObjectPinned);
    return result;
}

SZrObject *ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(
        SZrState *state,
        SZrObject *instanceObject) {
    const SZrTypeValue *fieldValue;
    SZrObject *typeObject = ZR_NULL;
    TZrBool objectPinned = ZR_FALSE;

    if (state == ZR_NULL ||
        !reflection_interpreter_generic_is_supported_instance(instanceObject) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), &objectPinned)) {
        return ZR_NULL;
    }

    fieldValue = reflection_interpreter_generic_get_field(
            state, instanceObject, kInterpreterGenericTypeObjectField);
    if (fieldValue != ZR_NULL && fieldValue->type == ZR_VALUE_TYPE_OBJECT &&
        fieldValue->value.object != ZR_NULL) {
        typeObject = ZR_CAST_OBJECT(state, fieldValue->value.object);
        if (!ZrCore_Reflection_IsReflectionObject(state, typeObject)) {
            typeObject = ZR_NULL;
        }
    }

    reflection_interpreter_generic_unpin(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), objectPinned);
    return typeObject;
}

static SZrObject *reflection_interpreter_generic_resolve_parameter_type_object(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *typeObject,
        const TZrChar *genericOwnerField,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex) {
    SZrReflectionResolvedGenericParameter parameter;
    SZrObject *argumentsObject = ZR_NULL;
    SZrObject *result = ZR_NULL;
    const SZrTypeValue *fieldValue;
    const SZrTypeValue *argumentValue;
    SZrTypeValue argumentKey;
    TZrUInt32 argumentCount;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool argumentsPinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || typeObject == ZR_NULL ||
        genericOwnerField == ZR_NULL ||
        !ZrCore_Reflection_IsReflectionObject(state, typeObject) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject), &typeObjectPinned) ||
        !ZrCore_Reflection_ResolveGenericParameter(
                runtime, genericOwnerToken, parameterIndex, &parameter)) {
        goto cleanup;
    }

    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, kMetadataRuntimeField);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        fieldValue->value.nativeObject.nativePointer != runtime) {
        goto cleanup;
    }
    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, genericOwnerField);
    if (fieldValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(fieldValue->type) ||
        fieldValue->value.nativeObject.nativeInt64 < 0 ||
        (TZrUInt64)fieldValue->value.nativeObject.nativeInt64 != parameter.ownerToken) {
        goto cleanup;
    }
    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, kGenericArgumentCountField);
    if (fieldValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(fieldValue->type) ||
        fieldValue->value.nativeObject.nativeInt64 <= 0 ||
        (TZrUInt64)fieldValue->value.nativeObject.nativeInt64 > (TZrUInt64)(~(TZrUInt32)0u)) {
        goto cleanup;
    }
    argumentCount = (TZrUInt32)fieldValue->value.nativeObject.nativeInt64;
    if (parameter.parameterIndex >= argumentCount) {
        goto cleanup;
    }

    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, kGenericArgumentsField);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_ARRAY ||
        fieldValue->value.object == ZR_NULL) {
        goto cleanup;
    }
    argumentsObject = ZR_CAST_OBJECT(state, fieldValue->value.object);
    if (argumentsObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, argumentsObject) ||
        argumentsObject->nodeMap.elementCount != argumentCount ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsObject), &argumentsPinned)) {
        goto cleanup;
    }

    ZrCore_Value_InitAsInt(state, &argumentKey, parameter.parameterIndex);
    argumentValue = ZrCore_Object_GetValue(state, argumentsObject, &argumentKey);
    if (argumentValue != ZR_NULL && argumentValue->type == ZR_VALUE_TYPE_OBJECT &&
        argumentValue->value.object != ZR_NULL) {
        SZrObject *candidate = ZR_CAST_OBJECT(state, argumentValue->value.object);
        if (ZrCore_Reflection_IsReflectionObject(state, candidate)) {
            result = candidate;
        }
    }

cleanup:
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         argumentsObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsObject)
                                                 : ZR_NULL,
                                         argumentsPinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         typeObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject)
                                                 : ZR_NULL,
                                         typeObjectPinned);
    return result;
}

SZrObject *ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex) {
    SZrObject *typeObject;
    SZrObject *result = ZR_NULL;
    TZrBool instancePinned = ZR_FALSE;

    if (state == ZR_NULL ||
        !reflection_interpreter_generic_is_supported_instance(instanceObject) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), &instancePinned)) {
        return ZR_NULL;
    }
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, instanceObject);
    if (typeObject != ZR_NULL) {
        result = reflection_interpreter_generic_resolve_parameter_type_object(
                state,
                runtime,
                typeObject,
                kGenericBaseTokenField,
                genericOwnerToken,
                parameterIndex);
    }
    reflection_interpreter_generic_unpin(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), instancePinned);
    return result;
}

TZrBool ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo(
        SZrState *state,
        SZrCallInfo *callInfo,
        SZrObject *instanceObject) {
    SZrObject *typeObject;

    if (callInfo != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericContext);
    }
    if (state == ZR_NULL || callInfo == ZR_NULL || instanceObject == ZR_NULL ||
        ZrCore_CallInfo_IsNative(callInfo)) {
        return ZR_FALSE;
    }
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, instanceObject);
    if (typeObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state,
            &callInfo->interpreterGenericContext,
            ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject));
    callInfo->interpreterGenericContext.type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

SZrObject *ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
        SZrState *state,
        SZrCallInfo *callInfo) {
    SZrObject *typeObject;

    if (state == ZR_NULL || callInfo == ZR_NULL || ZrCore_CallInfo_IsNative(callInfo) ||
        callInfo->interpreterGenericContext.type != ZR_VALUE_TYPE_OBJECT ||
        callInfo->interpreterGenericContext.value.object == ZR_NULL) {
        return ZR_NULL;
    }
    typeObject = ZR_CAST_OBJECT(
            state, callInfo->interpreterGenericContext.value.object);
    return ZrCore_Reflection_IsReflectionObject(state, typeObject) ? typeObject : ZR_NULL;
}

SZrObject *ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrCallInfo *callInfo,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex) {
    SZrObject *typeObject = ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
            state, callInfo);

    if (typeObject == ZR_NULL) {
        return ZR_NULL;
    }
    return reflection_interpreter_generic_resolve_parameter_type_object(
            state,
            runtime,
            typeObject,
            kGenericBaseTokenField,
            genericOwnerToken,
            parameterIndex);
}

TZrBool ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrCallInfo *callInfo,
        TZrMetadataToken methodSpecToken) {
    SZrObject *contextObject;

    if (callInfo != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericMethodContext);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || callInfo == ZR_NULL ||
        ZrCore_CallInfo_IsNative(callInfo)) {
        return ZR_FALSE;
    }
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, methodSpecToken);
    if (contextObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state,
            &callInfo->interpreterGenericMethodContext,
            ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject));
    callInfo->interpreterGenericMethodContext.type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

SZrObject *ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
        SZrState *state,
        SZrCallInfo *callInfo) {
    SZrObject *contextObject;

    if (state == ZR_NULL || callInfo == ZR_NULL || ZrCore_CallInfo_IsNative(callInfo) ||
        callInfo->interpreterGenericMethodContext.type != ZR_VALUE_TYPE_OBJECT ||
        callInfo->interpreterGenericMethodContext.value.object == ZR_NULL) {
        return ZR_NULL;
    }
    contextObject = ZR_CAST_OBJECT(
            state, callInfo->interpreterGenericMethodContext.value.object);
    return ZrCore_Reflection_IsReflectionObject(state, contextObject)
                   ? contextObject
                   : ZR_NULL;
}

SZrObject *ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrCallInfo *callInfo,
        TZrMetadataToken genericMethodToken,
        TZrUInt32 parameterIndex) {
    SZrObject *contextObject =
            ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
                    state, callInfo);

    if (contextObject == ZR_NULL) {
        return ZR_NULL;
    }
    return reflection_interpreter_generic_resolve_parameter_type_object(
            state,
            runtime,
            contextObject,
            kGenericMethodTokenField,
            genericMethodToken,
            parameterIndex);
}

TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        SZrFunction *function,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result) {
    SZrMetadataRuntimeMethodSpecSignatureView signatureView;
    SZrReflectionResolvedGenericParameter genericParameter;
    SZrObject *contextObject = ZR_NULL;
    SZrTypeValue contextValue;
    TZrBool functionPinned = ZR_FALSE;
    TZrBool contextPinned = ZR_FALSE;
    TZrBool invoked = ZR_FALSE;
    TZrUInt32 genericArgumentIndex;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || function == ZR_NULL ||
        result == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        (argumentCount > 0u && arguments == ZR_NULL) ||
        function->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION || function->super.isNative ||
        function->instructionsList == ZR_NULL || function->hasVariableArguments ||
        function->parameterCount != argumentCount ||
        !ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
                runtime, methodSpecToken, &signatureView) ||
        signatureView.argumentCount == 0u) {
        goto cleanup;
    }
    for (genericArgumentIndex = 0u;
         genericArgumentIndex < signatureView.argumentCount;
         ++genericArgumentIndex) {
        if (!ZrCore_Reflection_ResolveGenericParameter(
                    runtime,
                    signatureView.methodToken,
                    genericArgumentIndex,
                    &genericParameter)) {
            goto cleanup;
        }
    }
    if (ZrCore_Reflection_ResolveGenericParameter(
                runtime,
                signatureView.methodToken,
                signatureView.argumentCount,
                &genericParameter) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), &functionPinned)) {
        goto cleanup;
    }

    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, methodSpecToken);
    if (contextObject == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject), &contextPinned)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            state, &contextValue, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject));
    contextValue.type = ZR_VALUE_TYPE_OBJECT;
    invoked = ZrCore_Object_CallFunctionWithReceiverAndInterpreterGenericContexts(
            state,
            function,
            ZR_NULL,
            arguments,
            argumentCount,
            ZR_NULL,
            &contextValue,
            result);

cleanup:
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         contextObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject)
                                                 : ZR_NULL,
                                         contextPinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         function != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(function)
                                                 : ZR_NULL,
                                         functionPinned);
    return invoked;
}

TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result) {
    SZrMetadataRuntimeMethodSpecSignatureView signatureView;
    SZrMetadataRuntimeInterpreterMethodBindingView methodBindingView;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || result == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
                runtime, methodSpecToken, &signatureView) ||
        !ZrCore_MetadataRuntime_ReadInterpreterMethodBindingView(
                state, runtime, signatureView.methodToken, &methodBindingView)) {
        return ZR_FALSE;
    }
    return ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction(
            state,
            runtime,
            methodSpecToken,
            methodBindingView.function,
            arguments,
            argumentCount,
            result);
}

TZrBool ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        SZrFunction *function,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result) {
    SZrObject *typeObject = ZR_NULL;
    const SZrTypeValue *runtimeValue;
    const SZrTypeValue *ownerValue;
    SZrTypeValue receiverValue;
    SZrTypeValue contextValue;
    TZrBool instancePinned = ZR_FALSE;
    TZrBool functionPinned = ZR_FALSE;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool invoked = ZR_FALSE;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || instanceObject == ZR_NULL ||
        function == ZR_NULL || result == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        (argumentCount > 0u && arguments == ZR_NULL) ||
        function->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION || function->super.isNative ||
        function->instructionsList == ZR_NULL || function->hasVariableArguments ||
        function->parameterCount != argumentCount + 1u ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), &functionPinned) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), &instancePinned)) {
        goto cleanup;
    }

    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(
            state, instanceObject);
    if (typeObject == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject), &typeObjectPinned)) {
        goto cleanup;
    }
    runtimeValue = reflection_interpreter_generic_get_field(
            state, typeObject, kMetadataRuntimeField);
    if (runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        runtimeValue->value.nativeObject.nativePointer != runtime) {
        goto cleanup;
    }
    ownerValue = reflection_interpreter_generic_get_field(
            state, typeObject, kGenericBaseTokenField);
    if (ownerValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(ownerValue->type) ||
        ownerValue->value.nativeObject.nativeInt64 < 0 ||
        (TZrUInt64)ownerValue->value.nativeObject.nativeInt64 != genericOwnerToken) {
        goto cleanup;
    }

    ZrCore_Value_InitAsRawObject(
            state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(
            state, &contextValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject));
    contextValue.type = ZR_VALUE_TYPE_OBJECT;
    invoked = ZrCore_Object_CallFunctionWithReceiverAndInterpreterGenericContext(
            state,
            function,
            &receiverValue,
            arguments,
            argumentCount,
            &contextValue,
            result);

cleanup:
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         typeObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject)
                                                 : ZR_NULL,
                                         typeObjectPinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         instanceObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject)
                                                 : ZR_NULL,
                                         instancePinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         function != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(function)
                                                 : ZR_NULL,
                                         functionPinned);
    return invoked;
}
