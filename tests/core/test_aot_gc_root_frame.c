#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static SZrAotGcRootMap make_single_slot_root_map(SZrAotGcRootSlot *slot) {
    SZrAotGcRootMap map;

    slot->stackSlot = 0u;
    slot->frameByteOffset = 0u;
    slot->typeLayoutId = 7u;
    slot->fieldByteOffset = 0u;
    slot->locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET;
    slot->reserved0 = 0u;
    slot->reserved1 = 0u;

    map.rootCount = 1u;
    map.roots = slot;
    return map;
}

static void test_aot_root_frame_push_pop_balances_state_stack(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_slot_root_map(&slot);
    SZrAotGcRootFrame firstFrame;
    SZrAotGcRootFrame secondFrame;
    TZrStackValuePointer frameBase;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);
    frameBase = state->stackBase.valuePointer;

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
    TEST_ASSERT_NULL(state->aotGcRootFrameStack);

    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &firstFrame, frameBase, &map));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Gc_AotRootFrameDepth(state));
    TEST_ASSERT_EQUAL_PTR(&firstFrame, state->aotGcRootFrameStack);
    TEST_ASSERT_EQUAL_PTR(&map, firstFrame.rootMap);
    TEST_ASSERT_EQUAL_PTR(frameBase, firstFrame.frameBase);
    TEST_ASSERT_NULL(firstFrame.previous);

    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &secondFrame, frameBase + 1, &map));
    TEST_ASSERT_EQUAL_UINT32(2u, ZrCore_Gc_AotRootFrameDepth(state));
    TEST_ASSERT_EQUAL_PTR(&secondFrame, state->aotGcRootFrameStack);
    TEST_ASSERT_EQUAL_PTR(&firstFrame, secondFrame.previous);

    TEST_ASSERT_FALSE(ZrCore_Gc_AotRootFramePop(state, &firstFrame));
    TEST_ASSERT_EQUAL_UINT32(2u, ZrCore_Gc_AotRootFrameDepth(state));
    TEST_ASSERT_EQUAL_PTR(&secondFrame, state->aotGcRootFrameStack);

    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePop(state, &secondFrame));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Gc_AotRootFrameDepth(state));
    TEST_ASSERT_EQUAL_PTR(&firstFrame, state->aotGcRootFrameStack);
    TEST_ASSERT_NULL(secondFrame.rootMap);
    TEST_ASSERT_NULL(secondFrame.frameBase);
    TEST_ASSERT_NULL(secondFrame.previous);

    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePop(state, &firstFrame));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
    TEST_ASSERT_NULL(state->aotGcRootFrameStack);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_root_frame_keeps_young_value_above_stack_top_live(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_slot_root_map(&slot);
    SZrAotGcRootFrame rootFrame;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);
    TEST_ASSERT_NOT_NULL(state->stackTop.valuePointer);

    {
        SZrGarbageCollector *collector = state->global->garbageCollector;
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        TZrStackValuePointer frameBase = state->stackTop.valuePointer + 4;
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        SZrTypeValue *rootValue = &frameBase->value;
        SZrRawObject *newObject;

        TEST_ASSERT_NOT_NULL(object);
        TEST_ASSERT_TRUE(frameBase < state->stackTail.valuePointer);
        TEST_ASSERT_TRUE(frameBase >= state->stackTop.valuePointer);

        collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_Value_InitAsRawObject(state, rootValue, oldObject);
        TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &rootFrame, frameBase, &map));

        collector->gcDebtSize = 4096;
        collector->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        TEST_ASSERT_TRUE(rootValue->isGarbageCollectable);
        newObject = rootValue->value.object;
        TEST_ASSERT_TRUE(newObject == oldObject || oldObject->garbageCollectMark.forwardingAddress == newObject);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
                                 newObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 newObject->garbageCollectMark.storageKind);

        TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePop(state, &rootFrame));
    }

    ZrTests_Runtime_State_Destroy(state);
}

static void test_gc_safepoint_advances_pending_collection_debt(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrGarbageCollector *collector;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    collector = state->global->garbageCollector;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 4096;
    collector->gcLastStepWork = 0;

    ZrCore_Gc_SafePoint(state);

    TEST_ASSERT_GREATER_THAN_UINT64(0u, collector->gcLastStepWork);
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR,
                             collector->statsSnapshot.lastCollectionKind);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_gc_write_barrier_records_old_to_young_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrGarbageCollector *collector;
    SZrObject *parent;
    SZrObject *child;
    SZrRawObject *parentRaw;
    SZrRawObject *childRaw;
    SZrTypeValue childValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    collector = state->global->garbageCollector;
    parent = ZrCore_Object_New(state, ZR_NULL);
    child = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_NOT_NULL(child);

    parentRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(parent);
    childRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(child);
    ZrCore_RawObject_MarkAsReferenced(parentRaw);
    ZrCore_RawObject_MarkAsInit(state, childRaw);
    ZrCore_RawObject_SetStorageKind(parentRaw, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
    ZrCore_RawObject_SetRegionKind(parentRaw, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);
    ZrCore_RawObject_SetStorageKind(childRaw, ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE);
    ZrCore_RawObject_SetRegionKind(childRaw, ZR_GARBAGE_COLLECT_REGION_KIND_EDEN);
    childRaw->garbageCollectMark.anchorScopeDepth = 3u;
    ZrCore_Value_InitAsRawObject(state, &childValue, childRaw);

    TEST_ASSERT_EQUAL_UINT32(0u, collector->rememberedObjectCount);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, parentRaw));

    ZrCore_Gc_WriteBarrier(state, parentRaw, &childValue);

    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, parentRaw));
    TEST_ASSERT_EQUAL_UINT32(1u, collector->rememberedObjectCount);
    TEST_ASSERT_TRUE(parentRaw->garbageCollectMark.rememberedRegistryIndex < collector->rememberedObjectCount);
    TEST_ASSERT_EQUAL_PTR(parentRaw, collector->rememberedObjects[parentRaw->garbageCollectMark.rememberedRegistryIndex]);
    TEST_ASSERT_TRUE((childRaw->garbageCollectMark.escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE,
                             childRaw->garbageCollectMark.promotionReason);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_gc_native_call_pin_value_marks_and_releases_temporary_pin(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrObject *alreadyIgnoredObject;
    SZrRawObject *rawObject;
    SZrRawObject *alreadyIgnoredRawObject;
    SZrTypeValue value;
    SZrTypeValue alreadyIgnoredValue;
    SZrGcNativeCallPin pin;
    SZrGcNativeCallPin alreadyIgnoredPin;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    ZrCore_Value_InitAsRawObject(state, &value, rawObject);

    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, rawObject));
    TEST_ASSERT_EQUAL_UINT32(0u, rawObject->garbageCollectMark.pinFlags);

    TEST_ASSERT_TRUE(ZrCore_Gc_NativeCallPinValue(state, &value, &pin));

    TEST_ASSERT_EQUAL_PTR(rawObject, pin.object);
    TEST_ASSERT_TRUE(pin.ignoredAddedByCaller);
    TEST_ASSERT_TRUE(pin.pinKindAddedByCaller);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, rawObject));
    TEST_ASSERT_TRUE((rawObject->garbageCollectMark.pinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PINNED, rawObject->garbageCollectMark.regionKind);

    ZrCore_Gc_NativeCallUnpin(state->global, &pin);

    TEST_ASSERT_NULL(pin.object);
    TEST_ASSERT_FALSE(pin.ignoredAddedByCaller);
    TEST_ASSERT_FALSE(pin.pinKindAddedByCaller);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, rawObject));
    TEST_ASSERT_TRUE((rawObject->garbageCollectMark.pinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0u);

    alreadyIgnoredObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(alreadyIgnoredObject);
    alreadyIgnoredRawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(alreadyIgnoredObject);
    ZrCore_Value_InitAsRawObject(state, &alreadyIgnoredValue, alreadyIgnoredRawObject);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, alreadyIgnoredRawObject));

    TEST_ASSERT_TRUE(ZrCore_Gc_NativeCallPinValue(state, &alreadyIgnoredValue, &alreadyIgnoredPin));
    TEST_ASSERT_FALSE(alreadyIgnoredPin.ignoredAddedByCaller);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, alreadyIgnoredRawObject));
    TEST_ASSERT_TRUE((alreadyIgnoredRawObject->garbageCollectMark.pinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0u);

    ZrCore_Gc_NativeCallUnpin(state->global, &alreadyIgnoredPin);

    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, alreadyIgnoredRawObject));
    TEST_ASSERT_TRUE((alreadyIgnoredRawObject->garbageCollectMark.pinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0u);
    ZrCore_GarbageCollector_UnignoreObject(state->global, alreadyIgnoredRawObject);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_root_frame_push_pop_balances_state_stack);
    RUN_TEST(test_aot_root_frame_keeps_young_value_above_stack_top_live);
    RUN_TEST(test_gc_safepoint_advances_pending_collection_debt);
    RUN_TEST(test_gc_write_barrier_records_old_to_young_value);
    RUN_TEST(test_gc_native_call_pin_value_marks_and_releases_temporary_pin);
    return UNITY_END();
}
