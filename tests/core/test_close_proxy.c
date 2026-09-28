#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct SZrCloseProxyProbe {
    SZrRawObject *sourceObject;
    SZrRawObject *olderObject;
    TZrMemoryOffset sourceOffset;
    TZrMemoryOffset physicalOffset;
    TZrUInt32 sourceCalls;
    TZrUInt32 olderCalls;
    TZrBool sourceClearedAtCallback;
    TZrBool physicalClearedAtCallback;
    TZrBool errorPassedToCallback;
    TZrBool receiverSurvivedStackGrowth;
    TZrBool receiverSurvivedFullGc;
    TZrBool growStackInCallback;
    TZrBool collectInCallback;
    TZrBool throwReplacementInCallback;
    TZrBool pushAotRootBeforeThrow;
} SZrCloseProxyProbe;

static SZrCloseProxyProbe *gProbe;

static const SZrAotGcRootSlot close_proxy_throw_root_slot = {
    0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u
};
static const SZrAotGcRootMap close_proxy_throw_root_map = {
    1u, &close_proxy_throw_root_slot
};

typedef struct SZrCloseProxyDropProbe {
    TZrMemoryOffset sourceOffset;
    TZrMemoryOffset physicalOffset;
    TZrUInt32 calls;
    TZrBool sawBothSlotsCleared;
    TZrBool grewStack;
} SZrCloseProxyDropProbe;

static SZrCloseProxyDropProbe *gDropProbe;

void setUp(void) {
    gProbe = ZR_NULL;
    gDropProbe = ZR_NULL;
}

void tearDown(void) {
    gProbe = ZR_NULL;
    gDropProbe = ZR_NULL;
}

static TZrInt64 close_proxy_drop_callback(SZrState *state) {
    SZrTypeValue *source = ZrCore_Stack_GetValue(
            ZrCore_Stack_LoadOffsetToPointer(state, gDropProbe->sourceOffset));
    SZrTypeValue *physical = (SZrTypeValue *)ZrCore_Stack_LoadByteOffsetToAddress(
            state, gDropProbe->physicalOffset);
    TZrSize grownSize =
            (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer) * 2u;

    ++gDropProbe->calls;
    gDropProbe->sawBothSlotsCleared =
            ZR_VALUE_IS_TYPE_NULL(source->type) && ZR_VALUE_IS_TYPE_NULL(physical->type);
    gDropProbe->grewStack = ZrCore_Stack_GrowTo(state, grownSize, ZR_TRUE);
    return 0;
}

static TZrInt64 close_proxy_probe_callback(SZrState *state) {
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *receiver = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *error = ZrCore_Stack_GetValue(base + 2);
    SZrRawObject *object = receiver->value.object;

    if (object == gProbe->olderObject) {
        ++gProbe->olderCalls;
    } else if (object == gProbe->sourceObject ||
               (gProbe->collectInCallback && receiver->type == ZR_VALUE_TYPE_OBJECT)) {
        TZrStackValuePointer source = ZrCore_Stack_LoadOffsetToPointer(state, gProbe->sourceOffset);
        ++gProbe->sourceCalls;
        gProbe->sourceClearedAtCallback =
                ZR_VALUE_IS_TYPE_NULL(ZrCore_Stack_GetValue(source)->type);
        gProbe->errorPassedToCallback = !ZR_VALUE_IS_TYPE_NULL(error->type);
        if (gProbe->physicalOffset != 0) {
            SZrTypeValue *physical = (SZrTypeValue *)ZrCore_Stack_LoadByteOffsetToAddress(
                    state, gProbe->physicalOffset);
            gProbe->physicalClearedAtCallback = ZR_VALUE_IS_TYPE_NULL(physical->type);
        }
        if (gProbe->growStackInCallback) {
            TZrSize grownSize =
                    (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer) * 2u;
            TZrBool grew = ZrCore_Stack_GrowTo(state, grownSize, ZR_TRUE);
            base = state->callInfoList->functionBase.valuePointer;
            receiver = ZrCore_Stack_GetValue(base + 1);
            gProbe->receiverSurvivedStackGrowth =
                    grew && receiver->type == ZR_VALUE_TYPE_OBJECT &&
                    receiver->value.object == gProbe->sourceObject;
        }
        if (gProbe->collectInCallback) {
            ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
            base = state->callInfoList->functionBase.valuePointer;
            receiver = ZrCore_Stack_GetValue(base + 1);
            gProbe->receiverSurvivedFullGc =
                    receiver->type == ZR_VALUE_TYPE_OBJECT &&
                    ZrCore_Value_GetMeta(state, receiver, ZR_META_CLOSE) != ZR_NULL;
        }
        if (gProbe->throwReplacementInCallback) {
            SZrAotGcRootFrame callbackRootFrame;
            SZrRawObject *callbackRoot = object;
            SZrString *message = ZrCore_String_CreateFromNative(state, "replacement");
            SZrTypeValue payload;
            if (gProbe->pushAotRootBeforeThrow &&
                !ZrCore_Gc_AotRootFramePush(
                        state, &callbackRootFrame,
                        (TZrStackValuePointer)&callbackRoot,
                        &close_proxy_throw_root_map)) {
                ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
            }
            if (message == ZR_NULL) {
                ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
            }
            ZrCore_Value_InitAsRawObject(
                    state, &payload, ZR_CAST_RAW_OBJECT_AS_SUPER(message));
            payload.type = ZR_VALUE_TYPE_STRING;
            payload.isGarbageCollectable = ZR_TRUE;
            payload.isNative = ZR_FALSE;
            if (!ZrCore_Exception_NormalizeThrownValue(
                        state, &payload, state->callInfoList,
                        ZR_THREAD_STATUS_RUNTIME_ERROR)) {
                ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
            }
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
        }
    }
    return 0;
}

static SZrObjectPrototype *close_proxy_new_prototype(SZrState *state) {
    SZrClosureNative *closer = ZrCore_ClosureNative_New(state, 0u);
    SZrString *name;
    SZrObjectPrototype *prototype;

    TEST_ASSERT_NOT_NULL(closer);
    closer->nativeFunction = close_proxy_probe_callback;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closer));
    name = ZrCore_String_CreateFromNative(state, "CloseProxyProbe");
    TEST_ASSERT_NOT_NULL(name);
    prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    ZrCore_ObjectPrototype_AddMeta(state,
                                   prototype,
                                   ZR_META_CLOSE,
                                   ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closer)));
    return prototype;
}

static SZrObject *close_proxy_new_object(SZrState *state, SZrObjectPrototype *prototype) {
    SZrObject *object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Object_Init(state, object);
    return object;
}

static SZrState *close_proxy_new_state(TZrStackValuePointer *frameBase) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state, 128u, ZR_TRUE));
    *frameBase = state->stackBase.valuePointer + 8u;
    state->baseCallInfo.functionBase.valuePointer = *frameBase - 1u;
    state->baseCallInfo.functionTop.valuePointer = *frameBase + 5u;
    state->baseCallInfo.metadataFunction = ZR_NULL;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = *frameBase + 24u;
    return state;
}

static void close_proxy_put_object(SZrState *state,
                                   TZrStackValuePointer slot,
                                   SZrObject *object) {
    ZrCore_Value_InitAsRawObject(state,
                                 ZrCore_Stack_GetValue(slot),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Stack_GetValue(slot)->type = ZR_VALUE_TYPE_OBJECT;
}

static void close_proxy_seed_current_error(SZrState *state, const TZrChar *messageText) {
    SZrString *message = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)messageText);
    SZrTypeValue payload;

    TEST_ASSERT_NOT_NULL(message);
    ZrCore_Value_InitAsRawObject(state, &payload, ZR_CAST_RAW_OBJECT_AS_SUPER(message));
    payload.type = ZR_VALUE_TYPE_STRING;
    payload.isGarbageCollectable = ZR_TRUE;
    payload.isNative = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCore_Exception_NormalizeThrownValue(
            state, &payload, state->callInfoList, ZR_THREAD_STATUS_RUNTIME_ERROR));
}

static const TZrChar *close_proxy_current_error_message(SZrState *state) {
    SZrString *fieldName;
    SZrTypeValue key;
    const SZrTypeValue *field;

    if (!state->hasCurrentException ||
        state->currentException.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    fieldName = ZrCore_String_CreateFromNative(state, "message");
    if (fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    field = ZrCore_Object_GetValue(
            state, ZR_CAST_OBJECT(state, state->currentException.value.object), &key);
    return field != ZR_NULL && field->type == ZR_VALUE_TYPE_STRING
                   ? ZrCore_String_GetNativeString(
                             ZR_CAST_STRING(state, field->value.object))
                   : ZR_NULL;
}

static void test_proxy_closes_source_once_and_preserves_older_markers(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *older = close_proxy_new_object(state, prototype);
    SZrObject *source = close_proxy_new_object(state, prototype);
    TZrMemoryOffset olderOffset = ZrCore_Stack_SavePointerAsOffset(state, frame);

    gProbe = &probe;
    probe.olderObject = ZR_CAST_RAW_OBJECT_AS_SUPER(older);
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    probe.growStackInCallback = ZR_TRUE;
    close_proxy_put_object(state, frame, older);
    close_proxy_put_object(state, frame + 1u, source);
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame);
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame + 1u);

    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_FALSE(probe.errorPassedToCallback);
    TEST_ASSERT_TRUE(probe.receiverSurvivedStackGrowth);
    TEST_ASSERT_EQUAL_UINT32(0u, probe.olderCalls);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, probe.sourceOffset),
                          state->toBeClosedValueList.valuePointer);

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, olderOffset),
                          state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.olderCalls);
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                          state->toBeClosedValueList.valuePointer);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_closes_unmarked_source_without_a_using_body(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(ZrCore_Stack_GetValue(
            ZrCore_Stack_LoadOffsetToPointer(state, probe.sourceOffset))->type));
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                          state->toBeClosedValueList.valuePointer);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_preserves_plain_local_without_close_meta(void) {
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(frame), 73);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 2u, frame));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_Stack_GetValue(frame)->type));
    TEST_ASSERT_EQUAL_INT64(73, ZrCore_Stack_GetValue(frame)->value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_preserves_plain_local_and_distinct_physical_mirror(void) {
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    const TZrUInt32 physicalByteOffset = (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    SZrTypeValue *physical = (SZrTypeValue *)((TZrByte *)frame + physicalByteOffset);

    layout.stackSlot = 0u;
    layout.byteOffset = physicalByteOffset;
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = physicalByteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(frame), 73);
    ZrCore_Value_InitAsInt(state, physical, 73);

    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 2u, frame));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_Stack_GetValue(frame)->type));
    TEST_ASSERT_EQUAL_INT64(73, ZrCore_Stack_GetValue(frame)->value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(physical->type));
    TEST_ASSERT_EQUAL_INT64(73, physical->value.nativeObject.nativeInt64);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_resets_borrowed_local_without_calling_close_meta(void) {
    SZrCloseProxyProbe probe = {0};
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *object = close_proxy_new_object(state, prototype);
    const TZrUInt32 physicalByteOffset =
            (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    SZrTypeValue *owner = ZrCore_Stack_GetValue(frame);
    SZrTypeValue *borrowed = ZrCore_Stack_GetValue(frame + 1u);
    SZrTypeValue *physical = (SZrTypeValue *)((TZrByte *)frame + physicalByteOffset);

    layout.stackSlot = 1u;
    layout.byteOffset = physicalByteOffset;
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 2u;
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = physicalByteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            state, owner, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_TRUE(ZrCore_Ownership_BorrowValue(state, borrowed, owner));
    *physical = *borrowed;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    gProbe = &probe;

    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(0u, probe.sourceCalls);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(borrowed->type));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(physical->type));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_UNIQUE, owner->ownershipKind);
    ZrCore_Ownership_ReleaseValue(state, owner);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_exception_boundary_closes_proxy_before_older_source_marker(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    close_proxy_put_object(state, frame + 1u, source);
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame + 1u);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));

    /* A handler entered after the outer MARK has this slot as its boundary. */
    ZrCore_Value_InitAsInt(state, &state->currentException, 37);
    state->hasCurrentException = ZR_TRUE;
    state->currentExceptionStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    ZrCore_Closure_CloseClosure(state, frame + 2u, ZR_THREAD_STATUS_RUNTIME_ERROR, ZR_FALSE);
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_TRUE(probe.errorPassedToCallback);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(state, probe.sourceOffset),
                          state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_nested_proxies_tombstone_once(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 4u, frame + 1u));
    TEST_ASSERT_EQUAL_UINT64(2u, ZrCore_Closure_CloseRegisteredValues(
            state, 2u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                          state->toBeClosedValueList.valuePointer);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_clears_distinct_physical_value_before_callback(void) {
    SZrCloseProxyProbe probe = {0};
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);
    const TZrUInt32 physicalByteOffset =
            (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    SZrTypeValue *physical =
            (SZrTypeValue *)((TZrByte *)frame + physicalByteOffset);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame);
    probe.physicalOffset = ZrCore_Stack_SaveByteAddressAsOffset(state, physical);
    layout.stackSlot = 0u;
    layout.byteOffset = physicalByteOffset;
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 5u;
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = physicalByteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    close_proxy_put_object(state, frame, source);
    ZrCore_Value_Copy(state, physical, ZrCore_Stack_GetValue(frame));
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame));

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_TRUE(probe.physicalClearedAtCallback);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(physical->type));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_and_receiver_survive_full_gc(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = ZrCore_Object_New(state, prototype);

    TEST_ASSERT_NOT_NULL(source);
    ZrCore_Object_Init(state, source);
    gProbe = &probe;
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    probe.collectInCallback = ZR_TRUE;
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));

    /* The only runtime root for the private proxy token is its hidden stack slot. */
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    probe.sourceObject = ZrCore_Stack_GetValue(
            ZrCore_Stack_LoadOffsetToPointer(state, probe.sourceOffset))->value.object;
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_TRUE(probe.receiverSurvivedFullGc);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_original_error_is_rooted_across_full_gc_in_close_callback(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    probe.collectInCallback = ZR_TRUE;
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    close_proxy_seed_current_error(state, "original");

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_RUNTIME_ERROR, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.receiverSurvivedFullGc);
    TEST_ASSERT_EQUAL_STRING("original", close_proxy_current_error_message(state));
    ZrTests_Runtime_State_Destroy(state);
}

typedef struct SZrCloseProxyCloseResult {
    TZrSize closedCount;
} SZrCloseProxyCloseResult;

static void close_proxy_close_with_error_in_try(SZrState *state, TZrPtr argument) {
    SZrCloseProxyCloseResult *result = (SZrCloseProxyCloseResult *)argument;
    result->closedCount = ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_RUNTIME_ERROR, ZR_FALSE);
}

static void test_native_close_error_replaces_original_without_leaking_frame(void) {
    SZrCloseProxyProbe probe = {0};
    SZrCloseProxyCloseResult result = {0};
    SZrExecutionBudget budget = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    TZrMemoryOffset initialStackTopOffset =
            ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);

    gProbe = &probe;
    state->executionBudget = &budget;
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    probe.throwReplacementInCallback = ZR_TRUE;
    for (TZrUInt32 attempt = 0u; attempt < 3u; ++attempt) {
        SZrObject *source;
        SZrCallInfo *cursor;
        TZrUInt32 reachableCallInfos = 0u;
        EZrThreadStatus status;

        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        source = close_proxy_new_object(state, prototype);
        probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
        close_proxy_put_object(state, frame + 1u, source);
        TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
        close_proxy_seed_current_error(state, "original");
        result.closedCount = 0u;

        status = ZrCore_Exception_TryRun(state, close_proxy_close_with_error_in_try, &result);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
        TEST_ASSERT_EQUAL_UINT64(1u, result.closedCount);
        TEST_ASSERT_EQUAL_UINT32(attempt + 1u, probe.sourceCalls);
        TEST_ASSERT_EQUAL_STRING("replacement", close_proxy_current_error_message(state));
        TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
        TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                              state->toBeClosedValueList.valuePointer);
        TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
        TEST_ASSERT_EQUAL_UINT32(0u, state->nestedNativeCallYieldFlag);
        TEST_ASSERT_NULL(budget.countedNativeFrame);
        TEST_ASSERT_EQUAL_UINT64(initialStackTopOffset,
                ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer));
        for (cursor = state->baseCallInfo.next; cursor != ZR_NULL;
             cursor = cursor->next) {
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(state->callInfoListLength,
                                             reachableCallInfos + 1u);
            ++reachableCallInfos;
        }
        TEST_ASSERT_EQUAL_UINT32(state->callInfoListLength, reachableCallInfos);
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_close_throw_discards_unwound_aot_root_frame(void) {
    SZrCloseProxyProbe probe = {0};
    SZrCloseProxyCloseResult result = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);
    EZrThreadStatus status;

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    probe.throwReplacementInCallback = ZR_TRUE;
    probe.pushAotRootBeforeThrow = ZR_TRUE;
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    close_proxy_seed_current_error(state, "original");

    status = ZrCore_Exception_TryRun(state, close_proxy_close_with_error_in_try, &result);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_EQUAL_UINT64(1u, result.closedCount);
    TEST_ASSERT_EQUAL_STRING("replacement", close_proxy_current_error_message(state));
    TEST_ASSERT_NULL(state->aotGcRootFrameStack);
    TEST_ASSERT_EQUAL_UINT32(0u, state->aotGcRootFrameDepth);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_EQUAL_STRING("replacement", close_proxy_current_error_message(state));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_rejects_slot_below_current_marker_without_changing_chain(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    close_proxy_put_object(state, frame + 1u, source);
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame + 1u);
    TEST_ASSERT_FALSE(ZrCore_Closure_MarkCloseProxy(state, frame, frame + 1u));
    TEST_ASSERT_EQUAL_PTR(frame + 1u, state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(ZrCore_Stack_GetValue(frame)->type));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_physical_proxy_follows_physical_source_marker(void) {
    SZrCloseProxyProbe probe = {0};
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layouts[2] = {{0}};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);
    const TZrUInt32 sourceByteOffset =
            (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    const TZrUInt32 proxyByteOffset =
            (TZrUInt32)(17u * sizeof(SZrTypeValueOnStack));
    TZrStackValuePointer physicalSource =
            (TZrStackValuePointer)((TZrByte *)frame + sourceByteOffset);
    TZrStackValuePointer physicalProxy =
            (TZrStackValuePointer)((TZrByte *)frame + proxyByteOffset);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame);
    probe.physicalOffset = ZrCore_Stack_SaveByteAddressAsOffset(state, physicalSource);
    for (TZrUInt32 slot = 0u; slot < 2u; ++slot) {
        layouts[slot].stackSlot = slot;
        layouts[slot].byteOffset = slot == 0u ? sourceByteOffset : proxyByteOffset;
        layouts[slot].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        layouts[slot].byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        layouts[slot].typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        layouts[slot].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    }
    function.stackSize = 2u;
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    function.frameByteSize = proxyByteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    close_proxy_put_object(state, frame, source);
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(physicalSource),
                      ZrCore_Stack_GetValue(frame));
    ZrCore_Closure_ToBeClosedValueClosureNew(state, physicalSource);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, physicalProxy, frame));

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_TRUE(probe.sourceClearedAtCallback);
    TEST_ASSERT_TRUE(probe.physicalClearedAtCallback);
    TEST_ASSERT_EQUAL_PTR(physicalSource, state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_frame_does_not_clear_an_inactive_physical_layout(void) {
    SZrCloseProxyProbe probe = {0};
    SZrFunction staleFunction = {0};
    SZrFunctionFrameSlotLayout staleLayout = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);
    SZrObject *unrelated = close_proxy_new_object(state, prototype);
    const TZrUInt32 physicalByteOffset =
            (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    SZrTypeValue *unrelatedPhysical =
            (SZrTypeValue *)((TZrByte *)frame + physicalByteOffset);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame);
    staleLayout.stackSlot = 0u;
    staleLayout.byteOffset = physicalByteOffset;
    staleLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    staleLayout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    staleLayout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    staleLayout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    staleFunction.stackSize = 1u;
    staleFunction.frameSlotLayouts = &staleLayout;
    staleFunction.frameSlotLayoutLength = 1u;
    staleFunction.frameByteSize = physicalByteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    staleFunction.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    /* Native call frames have no active VM layout even if a stale cache remains. */
    TEST_ASSERT_FALSE(ZR_CALL_INFO_IS_VM(&state->baseCallInfo));
    state->baseCallInfo.metadataFunction = &staleFunction;
    close_proxy_put_object(state, frame, source);
    ZrCore_Value_InitAsRawObject(state, unrelatedPhysical,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(unrelated));
    unrelatedPhysical->type = ZR_VALUE_TYPE_OBJECT;
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame));

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, unrelatedPhysical->type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(unrelated),
                          unrelatedPhysical->value.object);
    state->baseCallInfo.metadataFunction = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_copied_proxy_token_cannot_register_at_another_slot(void) {
    SZrCloseProxyProbe probe = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);

    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    ZrCore_Stack_CopyValue(state, frame + 4u, ZrCore_Stack_GetValue(frame + 3u));
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame + 4u);
    TEST_ASSERT_EQUAL_PTR(frame + 3u, state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.sourceCalls);
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                          state->toBeClosedValueList.valuePointer);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_releases_retained_owner_mirror_without_invalidating_stage(void) {
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *object = ZrCore_Object_New(state, prototype);
    const TZrUInt32 byteOffset =
            (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    SZrTypeValue *source = ZrCore_Stack_GetValue(frame);
    SZrTypeValue *physical = (SZrTypeValue *)((TZrByte *)frame + byteOffset);

    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    layout.stackSlot = 0u;
    layout.byteOffset = byteOffset;
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = byteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            state, source, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    ZrCore_Value_Copy(state, physical, source);
    TEST_ASSERT_NOT_NULL(source->ownershipControl);
    TEST_ASSERT_EQUAL_PTR(source->ownershipControl, physical->ownershipControl);
    TEST_ASSERT_EQUAL_UINT32(2u, ZrCore_Ownership_GetStrongRefCount(
            ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame));

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source->type));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(physical->type));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(
            ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_proxy_drops_direct_owner_alias_once_after_tombstoning(void) {
    SZrCloseProxyDropProbe drop = {0};
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(state, 0u);
    SZrObject *object;
    const TZrUInt32 byteOffset =
            (TZrUInt32)(16u * sizeof(SZrTypeValueOnStack));
    SZrTypeValue *source = ZrCore_Stack_GetValue(frame);
    SZrTypeValue *physical = (SZrTypeValue *)((TZrByte *)frame + byteOffset);

    TEST_ASSERT_NOT_NULL(destructor);
    destructor->nativeFunction = close_proxy_drop_callback;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    ZrCore_ObjectPrototype_AddMeta(state, prototype, ZR_META_DESTRUCTOR,
                                   ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor)));
    object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    layout.stackSlot = 0u;
    layout.byteOffset = byteOffset;
    layout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout.byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    function.stackSize = 1u;
    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = byteOffset + (TZrUInt32)sizeof(SZrTypeValue);
    function.frameByteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->baseCallInfo.metadataFunction = &function;
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            state, source, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
    TEST_ASSERT_NULL(source->ownershipControl);
    ZrCore_Value_Copy(state, physical, source);
    drop.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame);
    drop.physicalOffset = ZrCore_Stack_SaveByteAddressAsOffset(state, physical);
    gDropProbe = &drop;
    ZrCore_Closure_ToBeClosedValueClosureNew(state, frame);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame));

    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, drop.calls);
    TEST_ASSERT_TRUE(drop.sawBothSlotsCleared);
    TEST_ASSERT_TRUE(drop.grewStack);
    TEST_ASSERT_EQUAL_UINT64(1u, ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_INVALID, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32(1u, drop.calls);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_proxy_closes_source_once_and_preserves_older_markers);
    RUN_TEST(test_proxy_closes_unmarked_source_without_a_using_body);
    RUN_TEST(test_proxy_preserves_plain_local_without_close_meta);
    RUN_TEST(test_proxy_preserves_plain_local_and_distinct_physical_mirror);
    RUN_TEST(test_proxy_resets_borrowed_local_without_calling_close_meta);
    RUN_TEST(test_exception_boundary_closes_proxy_before_older_source_marker);
    RUN_TEST(test_nested_proxies_tombstone_once);
    RUN_TEST(test_proxy_clears_distinct_physical_value_before_callback);
    RUN_TEST(test_proxy_and_receiver_survive_full_gc);
    RUN_TEST(test_original_error_is_rooted_across_full_gc_in_close_callback);
    RUN_TEST(test_native_close_error_replaces_original_without_leaking_frame);
    RUN_TEST(test_native_close_throw_discards_unwound_aot_root_frame);
    RUN_TEST(test_proxy_rejects_slot_below_current_marker_without_changing_chain);
    RUN_TEST(test_physical_proxy_follows_physical_source_marker);
    RUN_TEST(test_native_frame_does_not_clear_an_inactive_physical_layout);
    RUN_TEST(test_copied_proxy_token_cannot_register_at_another_slot);
    RUN_TEST(test_proxy_releases_retained_owner_mirror_without_invalidating_stage);
    RUN_TEST(test_proxy_drops_direct_owner_alias_once_after_tombstoning);
    return UNITY_END();
}
