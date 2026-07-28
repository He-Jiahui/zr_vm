#include "reflection_type_resolve_native_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include "reflection_bound_runtime_native_internal.h"

static TZrInt64 reflection_resolve_type_id_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

TZrInt64 ZrCore_Reflection_ResolveTypeIdNativeEntryInternal(SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *typeIdValue;
    SZrMetadataRuntime *runtime;
    SZrObject *descriptor;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL || state->stackTop.valuePointer != functionBase + 2) {
        return reflection_resolve_type_id_return_null(state, functionBase);
    }
    runtime = ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
            state, ZrCore_Reflection_ResolveTypeIdNativeEntryInternal);
    if (runtime == ZR_NULL) {
        return reflection_resolve_type_id_return_null(state, functionBase);
    }

    typeIdValue = ZrCore_Stack_GetValue(functionBase + 1);
    if (typeIdValue == ZR_NULL || typeIdValue->type != ZR_VALUE_TYPE_OBJECT ||
        typeIdValue->value.object == ZR_NULL ||
        typeIdValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return reflection_resolve_type_id_return_null(state, functionBase);
    }
    descriptor = ZrCore_Reflection_ResolveTypeIdObject(
            state, (SZrObject *)typeIdValue->value.object);
    if (descriptor == ZR_NULL) {
        return reflection_resolve_type_id_return_null(state, functionBase);
    }

    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(functionBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

SZrClosureNative *ZrCore_Reflection_CreateResolveTypeIdNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_ResolveTypeIdNativeEntryInternal,
            pinRuntimeModule);
}
