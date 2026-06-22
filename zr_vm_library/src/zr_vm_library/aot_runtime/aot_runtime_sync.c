#include "zr_vm_library/aot_runtime.h"

#include "aot_runtime_internal.h"

#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

static SZrLibraryAotRuntimeState *aot_runtime_sync_runtime_state(SZrState *state) {
    return state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
}

static TZrStackValuePointer aot_runtime_sync_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL || slotIndex >= frame->generatedFrameSlotCount) {
        return ZR_NULL;
    }

    return frame->slotBase + slotIndex;
}

static TZrBool aot_runtime_sync_local_source(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 sourceSlot,
                                             const SZrTypeValue **sourceValueOut) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_sync_runtime_state(state);
    TZrStackValuePointer sourcePointer = aot_runtime_sync_frame_slot(frame, sourceSlot);
    const SZrTypeValue *sourceValue;

    if (sourceValueOut == ZR_NULL || state == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT local sync");
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT local sync");
        return ZR_FALSE;
    }

    *sourceValueOut = sourceValue;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SyncSignedIntLocal(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 sourceSlot,
                                                TZrInt64 *outValue) {
    const SZrTypeValue *sourceValue;

    if (outValue == ZR_NULL || !aot_runtime_sync_local_source(state, frame, sourceSlot, &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        *outValue = sourceValue->value.nativeObject.nativeInt64;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SyncUnsignedIntLocal(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt64 *outValue) {
    const SZrTypeValue *sourceValue;

    if (outValue == ZR_NULL || !aot_runtime_sync_local_source(state, frame, sourceSlot, &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        *outValue = sourceValue->value.nativeObject.nativeUInt64;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SyncFloatLocal(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 sourceSlot,
                                            TZrFloat64 *outValue) {
    const SZrTypeValue *sourceValue;

    if (outValue == ZR_NULL || !aot_runtime_sync_local_source(state, frame, sourceSlot, &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        *outValue = sourceValue->value.nativeObject.nativeDouble;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_SyncBoolLocal(SZrState *state,
                                           ZrAotGeneratedFrame *frame,
                                           TZrUInt32 sourceSlot,
                                           TZrBool *outValue) {
    const SZrTypeValue *sourceValue;

    if (outValue == ZR_NULL || !aot_runtime_sync_local_source(state, frame, sourceSlot, &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        *outValue = (TZrBool)(sourceValue->value.nativeObject.nativeBool != 0u);
    }

    return ZR_TRUE;
}
