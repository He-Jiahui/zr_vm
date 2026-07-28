#include "zr_vm_core/reflection.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include "reflection_bound_runtime_native_internal.h"
#include "reflection_generic_method_native_internal.h"

static TZrInt64 reflection_make_generic_method_native_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

TZrInt64 ZrCore_Reflection_MakeGenericMethodNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *definitionValue;
    SZrTypeValue *argumentsValue;
    SZrMetadataRuntime *runtime;
    SZrObject *constructedMethod;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL || state->stackTop.valuePointer != functionBase + 3) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    definitionValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentsValue = ZrCore_Stack_GetValue(functionBase + 2);
    if (definitionValue == ZR_NULL || definitionValue->type != ZR_VALUE_TYPE_OBJECT ||
        definitionValue->value.object == ZR_NULL ||
        definitionValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        argumentsValue == ZR_NULL || argumentsValue->type != ZR_VALUE_TYPE_ARRAY ||
        argumentsValue->value.object == ZR_NULL ||
        argumentsValue->value.object->type != ZR_RAW_OBJECT_TYPE_ARRAY) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    runtime = ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
            state, ZrCore_Reflection_MakeGenericMethodNativeEntry);
    if (runtime == ZR_NULL) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }
    constructedMethod = ZrCore_Reflection_MakeGenericMethodFromObjects(
            state,
            runtime,
            (SZrObject *)definitionValue->value.object,
            (SZrObject *)argumentsValue->value.object);
    if (constructedMethod == ZR_NULL) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(functionBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(constructedMethod));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_MakeGenericMethodNativeEntry,
            pinRuntimeModule);
}

SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(
        SZrState *state,
        SZrMetadataRuntime *runtime) {
    return ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
            state, runtime, ZR_TRUE);
}
