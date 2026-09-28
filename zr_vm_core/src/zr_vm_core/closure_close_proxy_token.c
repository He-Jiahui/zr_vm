#include "closure_close_proxy_token.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/native.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

/* Runtime-only identity. Bytecode and artifacts contain stack slots, never this address. */
static TZrUInt8 gCloseProxyIdentity;

TZrBool ZrCore_ClosureProxyToken_Install(struct SZrState *state,
                                        TZrStackValuePointer proxySlot,
                                        TZrStackValuePointer sourceSlot) {
    TZrMemoryOffset proxyOffset;
    TZrMemoryOffset sourceOffset;
    struct SZrNativeData *proxy;

    proxyOffset = ZrCore_Stack_SavePointerAsOffset(state, proxySlot);
    sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, sourceSlot);
    proxy = (struct SZrNativeData *)ZrCore_RawObject_New(
            state, ZR_VALUE_TYPE_NATIVE_DATA,
            sizeof(struct SZrNativeData) + 2u * sizeof(SZrTypeValue), ZR_FALSE);
    if (proxy == ZR_NULL) {
        return ZR_FALSE;
    }
    proxy->valueLength = 3u;
    ZrCore_Value_InitAsNativePointer(state, &proxy->valueExtend[0], (TZrPtr)&gCloseProxyIdentity);
    ZrCore_Value_InitAsInt(state, &proxy->valueExtend[1], (TZrInt64)sourceOffset);
    ZrCore_Value_InitAsInt(state, &proxy->valueExtend[2], (TZrInt64)proxyOffset);
    proxySlot = ZrCore_Stack_LoadOffsetToPointer(state, proxyOffset);
    ZrCore_Stack_SetRawObjectValue(state, proxySlot, ZR_CAST_RAW_OBJECT_AS_SUPER(proxy));
    return ZR_TRUE;
}

TZrBool ZrCore_ClosureProxyToken_GetSourceOffset(struct SZrState *state,
                                                TZrStackValuePointer proxySlot,
                                                TZrMemoryOffset *outSourceOffset) {
    SZrTypeValue *value;
    struct SZrNativeData *proxy;
    TZrInt64 sourceOffset;

    if (state == ZR_NULL || proxySlot == ZR_NULL || outSourceOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    value = ZrCore_Stack_GetValueNoProfile(proxySlot);
    if (value->type != ZR_VALUE_TYPE_NATIVE_DATA ||
        !value->isGarbageCollectable || value->value.object == ZR_NULL ||
        value->value.object->type != ZR_RAW_OBJECT_TYPE_NATIVE_DATA) {
        return ZR_FALSE;
    }
    proxy = (struct SZrNativeData *)value->value.object;
    if (proxy->valueLength != 3u ||
        proxy->valueExtend[0].type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        proxy->valueExtend[0].value.nativeObject.nativePointer != (TZrPtr)&gCloseProxyIdentity ||
        proxy->valueExtend[1].type != ZR_VALUE_TYPE_INT64 ||
        proxy->valueExtend[2].type != ZR_VALUE_TYPE_INT64 ||
        proxy->valueExtend[2].value.nativeObject.nativeInt64 !=
                (TZrInt64)ZrCore_Stack_SavePointerAsOffset(state, proxySlot)) {
        return ZR_FALSE;
    }
    sourceOffset = proxy->valueExtend[1].value.nativeObject.nativeInt64;
    if (sourceOffset < 0 ||
        sourceOffset >= (TZrInt64)ZrCore_Stack_SavePointerAsOffset(
                                state, state->stackTop.valuePointer) ||
        sourceOffset % (TZrInt64)sizeof(SZrTypeValueOnStack) != 0) {
        return ZR_FALSE;
    }
    *outSourceOffset = (TZrMemoryOffset)sourceOffset;
    return ZR_TRUE;
}
