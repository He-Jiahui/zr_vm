#include "reflection_construction_native_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include "reflection_bound_runtime_native_internal.h"

static TZrInt64 construction_native_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static SZrObject *construction_native_descriptor(
        SZrState *state,
        TZrStackValuePointer valuePointer) {
    SZrTypeValue *value;

    if (state == ZR_NULL || valuePointer == ZR_NULL) {
        return ZR_NULL;
    }
    value = ZrCore_Stack_GetValue(valuePointer);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL ||
        value->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

TZrInt64 ZrCore_Reflection_RequireConstructibleNativeEntryInternal(
        SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrObject *descriptor;
    EZrReflectionConstructionStatus status;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL ||
        state->stackTop.valuePointer != functionBase + 2 ||
        ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
                state,
                ZrCore_Reflection_RequireConstructibleNativeEntryInternal) ==
                ZR_NULL) {
        return construction_native_return_null(state, functionBase);
    }
    descriptor = construction_native_descriptor(state, functionBase + 1);
    if (descriptor == ZR_NULL ||
        !ZrCore_Reflection_RequireConstructible(
                state, descriptor, &status)) {
        return construction_native_return_null(state, functionBase);
    }
    *ZrCore_Stack_GetValue(functionBase) =
            *ZrCore_Stack_GetValue(functionBase + 1);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

TZrInt64 ZrCore_Reflection_CreateInstanceNativeEntryInternal(
        SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrObject *descriptor;
    TZrSize argumentCount;
    SZrTypeValue result;
    EZrReflectionConstructionStatus status;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL ||
        state->stackTop.valuePointer < functionBase + 2 ||
        ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
                state,
                ZrCore_Reflection_CreateInstanceNativeEntryInternal) == ZR_NULL) {
        return construction_native_return_null(state, functionBase);
    }
    descriptor = construction_native_descriptor(state, functionBase + 1);
    argumentCount = (TZrSize)(state->stackTop.valuePointer - functionBase - 2);
    ZrCore_Value_ResetAsNull(&result);
    if (descriptor == ZR_NULL ||
        !ZrCore_Reflection_CreateInstance(
                state,
                descriptor,
                argumentCount > 0u
                        ? ZrCore_Stack_GetValue(functionBase + 2)
                        : ZR_NULL,
                argumentCount,
                &result,
                &status)) {
        return construction_native_return_null(state, functionBase);
    }
    *ZrCore_Stack_GetValue(functionBase) = result;
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

SZrClosureNative *
ZrCore_Reflection_CreateRequireConstructibleNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_RequireConstructibleNativeEntryInternal,
            pinRuntimeModule);
}

SZrClosureNative *ZrCore_Reflection_CreateInstanceNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_CreateInstanceNativeEntryInternal,
            pinRuntimeModule);
}
