#include "zr_vm_common/zr_aot_abi.h"

#include <stdint.h>

/* 活动 AOT 根帧可同时借用 VM 栈槽和 C 局部地址；扩栈只迁移前者。 */
static void test_stack_relocation_preserves_active_aot_frame_byte_offset_root(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrGarbageCollector *collector;
    SZrObject *stackObject;
    SZrObject *secondStackObject;
    SZrObject *localObject;
    SZrRawObject *oldStackObject;
    SZrRawObject *oldSecondStackObject;
    SZrRawObject *localObjectRoot;
    SZrRawObject *oldLocalObject;
    SZrAotGcRootSlot stackRootSlot = {0};
    SZrAotGcRootSlot localRootSlot = {0};
    SZrAotGcRootMap stackRootMap;
    SZrAotGcRootMap localRootMap;
    SZrAotGcRootFrame stackRootFrame;
    SZrAotGcRootFrame secondStackRootFrame;
    SZrAotGcRootFrame localRootFrame;
    TZrStackValuePointer stackFrameBase;
    TZrStackValuePointer secondStackFrameBase;
    TZrStackValuePointer originalLocalFrameBase;
    TZrMemoryOffset stackFrameBaseOffset;
    TZrMemoryOffset secondStackFrameBaseOffset;
    uintptr_t oldStackBaseAddress;
    TZrStackValuePointer expectedRelocatedStackFrameBase;
    TZrStackValuePointer expectedRelocatedSecondStackFrameBase;
    TZrSize oldStackSize;
    uintptr_t oldStackObjectAddress;
    uintptr_t oldSecondStackObjectAddress;
    uintptr_t oldLocalObjectAddress;
    SZrTypeValue *relocatedStackRoot;
    SZrTypeValue *relocatedSecondStackRoot;
    SZrAotGcRootFrame *savedRootFrameTop;
    TZrUInt32 savedRootFrameDepth;
    TZrBool previousStopGcFlag;
    TZrBool failedGrowthWasReported;
    TZrBool failedGrowthRestoredRootFrameState;
    TZrBool failedGrowthRestoredStopGcFlag;
    TZrBool retryGrowthMovedStack;
    TZrBool successfulGrowthMigratedRootFrame;
    TZrBool restoredRootFrameChainWasValid;
    TZrBool stackObjectWasYoungMovableBeforeCollection;
    TZrBool secondStackObjectWasYoungMovableBeforeCollection;
    TZrBool localObjectWasYoungMovableBeforeCollection;
    TZrBool stackRootSlotWasInStackRangeBeforeCollection;
    TZrBool secondStackRootSlotWasInStackRangeBeforeCollection;
    TZrBool stackRootObjectRemainsRegistered;
    TZrBool secondStackRootObjectRemainsRegistered;
    TZrBool localRootObjectRemainsRegistered;
    TZrBool stackRootStillPointsAtSurvivor;
    TZrBool secondStackRootStillPointsAtSurvivor;
    TZrBool localRootStillPointsAtSurvivor;
    TZrBool stackRootReachedSurvivor;
    TZrBool secondStackRootReachedSurvivor;
    TZrBool localRootReachedSurvivor;
    TZrBool collectionWasMinor;
    TZrBool localRootFramePopped;
    TZrBool secondStackRootFramePopped;
    TZrBool stackRootFramePopped;
    TZrUInt32 rootFrameDepthAfterPop;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    stackObject = ZrCore_Object_New(state, ZR_NULL);
    secondStackObject = ZrCore_Object_New(state, ZR_NULL);
    localObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(stackObject);
    TEST_ASSERT_NOT_NULL(secondStackObject);
    TEST_ASSERT_NOT_NULL(localObject);
    oldStackObject = ZR_CAST_RAW_OBJECT_AS_SUPER(stackObject);
    oldSecondStackObject = ZR_CAST_RAW_OBJECT_AS_SUPER(secondStackObject);
    oldLocalObject = ZR_CAST_RAW_OBJECT_AS_SUPER(localObject);
    oldStackObjectAddress = (uintptr_t)(void *)oldStackObject;
    oldSecondStackObjectAddress = (uintptr_t)(void *)oldSecondStackObject;
    oldLocalObjectAddress = (uintptr_t)(void *)oldLocalObject;
    localObjectRoot = oldLocalObject;

    stackFrameBase = state->stackTop.valuePointer + 4u;
    TEST_ASSERT_TRUE(stackFrameBase < state->stackTail.valuePointer);
    secondStackFrameBase = stackFrameBase + 1u;
    TEST_ASSERT_TRUE(secondStackFrameBase < state->stackTail.valuePointer);
    stackFrameBaseOffset = ZrCore_Stack_SavePointerAsOffset(state, stackFrameBase);
    secondStackFrameBaseOffset = ZrCore_Stack_SavePointerAsOffset(state, secondStackFrameBase);
    ZrCore_Value_InitAsRawObject(state, &stackFrameBase->value, oldStackObject);
    ZrCore_Value_InitAsRawObject(state, &secondStackFrameBase->value, oldSecondStackObject);

    stackRootSlot.locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET;
    stackRootMap.rootCount = 1u;
    stackRootMap.roots = &stackRootSlot;
    localRootSlot.locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    localRootMap.rootCount = 1u;
    localRootMap.roots = &localRootSlot;

    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &stackRootFrame, stackFrameBase, &stackRootMap));
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(
            state, &secondStackRootFrame, secondStackFrameBase, &stackRootMap));
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(
            state, &localRootFrame, (TZrStackValuePointer)(void *)&localObjectRoot, &localRootMap));

    collector = state->global->garbageCollector;
    originalLocalFrameBase = localRootFrame.frameBase;
    savedRootFrameTop = state->aotGcRootFrameStack;
    savedRootFrameDepth = ZrCore_Gc_AotRootFrameDepth(state);
    previousStopGcFlag = collector->stopGcFlag;
    oldStackBaseAddress = (uintptr_t)state->stackBase.valuePointer;
    oldStackSize = ZrCore_State_StackGetSize(state);
    stackObjectWasYoungMovableBeforeCollection =
            (TZrBool)(oldStackObject->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_EDEN &&
                      oldStackObject->garbageCollectMark.storageKind ==
                              ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                      oldStackObject->garbageCollectMark.heapGenerationKind ==
                              ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG);
    secondStackObjectWasYoungMovableBeforeCollection =
            (TZrBool)(oldSecondStackObject->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_EDEN &&
                      oldSecondStackObject->garbageCollectMark.storageKind ==
                              ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                      oldSecondStackObject->garbageCollectMark.heapGenerationKind ==
                              ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG);
    localObjectWasYoungMovableBeforeCollection =
            (TZrBool)(oldLocalObject->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_EDEN &&
                      oldLocalObject->garbageCollectMark.storageKind ==
                              ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                      oldLocalObject->garbageCollectMark.heapGenerationKind ==
                              ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG);
    allocatorContext.moveCount = 0u;
    allocatorContext.failNextMove = ZR_TRUE;
    failedGrowthWasReported = (TZrBool)!ZrCore_Stack_GrowTo(state, oldStackSize + 1u, ZR_FALSE);
    failedGrowthRestoredRootFrameState =
            (TZrBool)(state->aotGcRootFrameStack == savedRootFrameTop &&
                      ZrCore_Gc_AotRootFrameDepth(state) == savedRootFrameDepth &&
                      stackRootFrame.frameBase == stackFrameBase &&
                      stackRootFrame.previous == ZR_NULL &&
                      secondStackRootFrame.frameBase == secondStackFrameBase &&
                      secondStackRootFrame.previous == &stackRootFrame &&
                      localRootFrame.frameBase == originalLocalFrameBase &&
                      localRootFrame.previous == &secondStackRootFrame);
    failedGrowthRestoredStopGcFlag = (TZrBool)(collector->stopGcFlag == previousStopGcFlag);
    retryGrowthMovedStack = ZrCore_Stack_GrowTo(state, oldStackSize + 1u, ZR_TRUE);
    retryGrowthMovedStack = (TZrBool)(retryGrowthMovedStack && allocatorContext.moveCount > 0u &&
                                       (uintptr_t)state->stackBase.valuePointer != oldStackBaseAddress);
    expectedRelocatedStackFrameBase = ZrCore_Stack_LoadOffsetToPointer(state, stackFrameBaseOffset);
    expectedRelocatedSecondStackFrameBase = ZrCore_Stack_LoadOffsetToPointer(state, secondStackFrameBaseOffset);
    successfulGrowthMigratedRootFrame =
            (TZrBool)(retryGrowthMovedStack && stackRootFrame.frameBase == expectedRelocatedStackFrameBase &&
                      secondStackRootFrame.frameBase == expectedRelocatedSecondStackFrameBase &&
                      localRootFrame.frameBase == originalLocalFrameBase);
    restoredRootFrameChainWasValid =
            (TZrBool)(localRootFrame.previous == &secondStackRootFrame &&
                      secondStackRootFrame.previous == &stackRootFrame &&
                      stackRootFrame.previous == ZR_NULL);
    stackRootSlotWasInStackRangeBeforeCollection =
            (TZrBool)(expectedRelocatedStackFrameBase >= state->stackBase.valuePointer &&
                      expectedRelocatedStackFrameBase < state->stackTail.valuePointer &&
                      expectedRelocatedStackFrameBase->value.isGarbageCollectable &&
                      expectedRelocatedStackFrameBase->value.value.object == oldStackObject);
    secondStackRootSlotWasInStackRangeBeforeCollection =
            (TZrBool)(expectedRelocatedSecondStackFrameBase >= state->stackBase.valuePointer &&
                      expectedRelocatedSecondStackFrameBase < state->stackTail.valuePointer &&
                      expectedRelocatedSecondStackFrameBase->value.isGarbageCollectable &&
                      expectedRelocatedSecondStackFrameBase->value.value.object == oldSecondStackObject);
    relocatedStackRoot = &expectedRelocatedStackFrameBase->value;
    relocatedSecondStackRoot = &expectedRelocatedSecondStackFrameBase->value;

    stackRootObjectRemainsRegistered = ZR_FALSE;
    secondStackRootObjectRemainsRegistered = ZR_FALSE;
    localRootObjectRemainsRegistered = ZR_FALSE;
    stackRootStillPointsAtSurvivor = ZR_FALSE;
    secondStackRootStillPointsAtSurvivor = ZR_FALSE;
    localRootStillPointsAtSurvivor = ZR_FALSE;
    stackRootReachedSurvivor = ZR_FALSE;
    secondStackRootReachedSurvivor = ZR_FALSE;
    localRootReachedSurvivor = ZR_FALSE;
    collectionWasMinor = ZR_FALSE;
    if (successfulGrowthMigratedRootFrame && restoredRootFrameChainWasValid) {
        collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        collector->gcDebtSize = 4096u;
        collector->gcLastStepWork = 0u;
        ZrCore_GarbageCollector_GcStep(state);

        for (SZrRawObject *object = collector->gcObjectList; object != ZR_NULL; object = object->next) {
            if ((uintptr_t)(void *)object == oldStackObjectAddress) {
                stackRootObjectRemainsRegistered = ZR_TRUE;
            }
            if ((uintptr_t)(void *)object == oldSecondStackObjectAddress) {
                secondStackRootObjectRemainsRegistered = ZR_TRUE;
            }
            if ((uintptr_t)(void *)object == oldLocalObjectAddress) {
                localRootObjectRemainsRegistered = ZR_TRUE;
            }
        }
        stackRootStillPointsAtSurvivor =
                (TZrBool)(stackRootObjectRemainsRegistered && relocatedStackRoot->value.object == oldStackObject);
        secondStackRootStillPointsAtSurvivor =
                (TZrBool)(secondStackRootObjectRemainsRegistered &&
                          relocatedSecondStackRoot->value.object == oldSecondStackObject);
        localRootStillPointsAtSurvivor =
                (TZrBool)(localRootObjectRemainsRegistered && localObjectRoot == oldLocalObject);
        stackRootReachedSurvivor =
                (TZrBool)(stackRootObjectRemainsRegistered &&
                          oldStackObject->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR &&
                          oldStackObject->garbageCollectMark.storageKind ==
                                  ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                          oldStackObject->garbageCollectMark.survivalAge > 0u);
        secondStackRootReachedSurvivor =
                (TZrBool)(secondStackRootObjectRemainsRegistered &&
                          oldSecondStackObject->garbageCollectMark.regionKind ==
                                  ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR &&
                          oldSecondStackObject->garbageCollectMark.storageKind ==
                                  ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                          oldSecondStackObject->garbageCollectMark.survivalAge > 0u);
        localRootReachedSurvivor =
                (TZrBool)(localRootObjectRemainsRegistered &&
                          oldLocalObject->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR &&
                          oldLocalObject->garbageCollectMark.storageKind ==
                                  ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                          oldLocalObject->garbageCollectMark.survivalAge > 0u);
        collectionWasMinor = (TZrBool)(collector->statsSnapshot.lastCollectionKind ==
                                       ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    }

    if (state->aotGcRootFrameStack != &localRootFrame ||
        localRootFrame.previous != &secondStackRootFrame ||
        secondStackRootFrame.previous != &stackRootFrame ||
        stackRootFrame.previous != ZR_NULL) {
        state->aotGcRootFrameStack = &localRootFrame;
        state->aotGcRootFrameDepth = savedRootFrameDepth;
        localRootFrame.previous = &secondStackRootFrame;
        secondStackRootFrame.previous = &stackRootFrame;
        stackRootFrame.previous = ZR_NULL;
    }

    localRootFramePopped = ZrCore_Gc_AotRootFramePop(state, &localRootFrame);
    secondStackRootFramePopped = ZrCore_Gc_AotRootFramePop(state, &secondStackRootFrame);
    stackRootFramePopped = ZrCore_Gc_AotRootFramePop(state, &stackRootFrame);
    rootFrameDepthAfterPop = ZrCore_Gc_AotRootFrameDepth(state);
    ZrCore_GlobalState_Free(state->global);

    TEST_ASSERT_TRUE_MESSAGE(failedGrowthWasReported, "the allocator failure must be reported without throwing");
    TEST_ASSERT_TRUE_MESSAGE(failedGrowthRestoredRootFrameState,
                             "failed stack growth must restore the root chain and original frame bases");
    TEST_ASSERT_TRUE_MESSAGE(failedGrowthRestoredStopGcFlag,
                             "failed stack growth must restore the collector stop flag after root restoration");
    TEST_ASSERT_TRUE_MESSAGE(retryGrowthMovedStack, "the subsequent growth must relocate the stack allocation");
    TEST_ASSERT_TRUE_MESSAGE(successfulGrowthMigratedRootFrame,
                             "successful stack growth must relocate both VM-stack root bases and preserve the C-local base");
    TEST_ASSERT_TRUE_MESSAGE(restoredRootFrameChainWasValid,
                             "successful stack growth must restore the nonnull VM-frame previous link");
    TEST_ASSERT_TRUE_MESSAGE(stackObjectWasYoungMovableBeforeCollection &&
                                     secondStackObjectWasYoungMovableBeforeCollection &&
                                     localObjectWasYoungMovableBeforeCollection,
                             "all fixture objects must enter minor GC as young movable Eden objects");
    TEST_ASSERT_TRUE_MESSAGE(stackRootSlotWasInStackRangeBeforeCollection &&
                                     secondStackRootSlotWasInStackRangeBeforeCollection,
                             "each FRAME_BYTE_OFFSET descriptor must name an initialized slot in the VM stack range");
    TEST_ASSERT_TRUE_MESSAGE(collectionWasMinor, "the fixture must execute a generational minor collection");
    TEST_ASSERT_TRUE_MESSAGE(stackRootObjectRemainsRegistered && stackRootStillPointsAtSurvivor,
                             "FRAME_BYTE_OFFSET root must keep its object registered across the minor collection");
    TEST_ASSERT_TRUE_MESSAGE(stackRootReachedSurvivor,
                             "FRAME_BYTE_OFFSET root must keep its young object in the survivor region");
    TEST_ASSERT_TRUE_MESSAGE(secondStackRootObjectRemainsRegistered && secondStackRootStillPointsAtSurvivor,
                             "the second FRAME_BYTE_OFFSET root must remain linked and keep its object registered");
    TEST_ASSERT_TRUE_MESSAGE(secondStackRootReachedSurvivor,
                             "the second FRAME_BYTE_OFFSET root must keep its young object in the survivor region");
    TEST_ASSERT_TRUE_MESSAGE(localRootObjectRemainsRegistered && localRootStillPointsAtSurvivor,
                             "stack relocation must leave an active LOCAL_ADDRESS root usable by minor GC");
    TEST_ASSERT_TRUE_MESSAGE(localRootReachedSurvivor,
                             "LOCAL_ADDRESS root must keep its young object in the survivor region");
    TEST_ASSERT_TRUE(localRootFramePopped);
    TEST_ASSERT_TRUE(secondStackRootFramePopped);
    TEST_ASSERT_TRUE(stackRootFramePopped);
    TEST_ASSERT_EQUAL_UINT32(0u, rootFrameDepthAfterPop);
}

/* Root-frame chain nodes must remain address-stable until Pop, including across VM stack growth. */
static void test_aot_root_frame_push_rejects_node_inside_vm_stack_allocation(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrAotGcRootSlot rootSlot = {0};
    SZrAotGcRootMap rootMap;
    SZrAotGcRootFrame activeRootFrame;
    SZrAotGcRootFrame *stackResidentRootFrame;
    SZrAotGcRootFrame *savedRootFrameTop;
    SZrTypeValue localRoot = {0};
    TZrUInt32 savedRootFrameDepth;
    TZrBool stackResidentFrameWasRejected;
    TZrBool rootChainWasUnchanged;
    TZrBool activeRootFrameWasPopped;

    TEST_ASSERT_NOT_NULL(state);
    rootSlot.locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    rootMap.rootCount = 1u;
    rootMap.roots = &rootSlot;
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(
            state, &activeRootFrame, (TZrStackValuePointer)(void *)&localRoot, &rootMap));

    savedRootFrameTop = state->aotGcRootFrameStack;
    savedRootFrameDepth = ZrCore_Gc_AotRootFrameDepth(state);
    stackResidentRootFrame = (SZrAotGcRootFrame *)(void *)state->stackTail.valuePointer;
    stackResidentFrameWasRejected = (TZrBool)!ZrCore_Gc_AotRootFramePush(
            state, stackResidentRootFrame, (TZrStackValuePointer)(void *)&localRoot, &rootMap);
    rootChainWasUnchanged =
            (TZrBool)(state->aotGcRootFrameStack == savedRootFrameTop &&
                      ZrCore_Gc_AotRootFrameDepth(state) == savedRootFrameDepth &&
                      activeRootFrame.previous == ZR_NULL);

    if (state->aotGcRootFrameStack == stackResidentRootFrame) {
        (void)ZrCore_Gc_AotRootFramePop(state, stackResidentRootFrame);
    }
    if (state->aotGcRootFrameStack != savedRootFrameTop) {
        state->aotGcRootFrameStack = savedRootFrameTop;
        state->aotGcRootFrameDepth = savedRootFrameDepth;
    }
    activeRootFrameWasPopped = ZrCore_Gc_AotRootFramePop(state, &activeRootFrame);
    ZrCore_GlobalState_Free(state->global);

    TEST_ASSERT_TRUE_MESSAGE(stackResidentFrameWasRejected,
                             "root-frame nodes inside the movable VM stack allocation must be rejected");
    TEST_ASSERT_TRUE_MESSAGE(rootChainWasUnchanged,
                             "rejecting an unstable root-frame node must leave the active chain unchanged");
    TEST_ASSERT_TRUE(activeRootFrameWasPopped);
}

static void test_aot_root_frame_push_rejects_duplicate_active_node(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrAotGcRootSlot rootSlot = {0};
    SZrAotGcRootMap rootMap;
    SZrAotGcRootFrame rootFrame;
    SZrAotGcRootFrame *savedRootFrameTop;
    const SZrAotGcRootMap *savedRootMap;
    TZrStackValuePointer savedFrameBase;
    TZrUInt32 savedRootFrameDepth;
    SZrRawObject *localRoot = ZR_NULL;
    TZrBool duplicatePushWasRejected;
    TZrBool rootChainAndFrameWereUnchanged;
    TZrBool rootFrameWasPopped;

    TEST_ASSERT_NOT_NULL(state);
    rootSlot.locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    rootMap.rootCount = 1u;
    rootMap.roots = &rootSlot;
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(
            state, &rootFrame, (TZrStackValuePointer)(void *)&localRoot, &rootMap));

    savedRootFrameTop = state->aotGcRootFrameStack;
    savedRootFrameDepth = ZrCore_Gc_AotRootFrameDepth(state);
    savedRootMap = rootFrame.rootMap;
    savedFrameBase = rootFrame.frameBase;
    duplicatePushWasRejected = (TZrBool)!ZrCore_Gc_AotRootFramePush(
            state, &rootFrame, (TZrStackValuePointer)(void *)&localRoot, &rootMap);
    rootChainAndFrameWereUnchanged =
            (TZrBool)(state->aotGcRootFrameStack == savedRootFrameTop &&
                      ZrCore_Gc_AotRootFrameDepth(state) == savedRootFrameDepth &&
                      rootFrame.rootMap == savedRootMap && rootFrame.frameBase == savedFrameBase &&
                      rootFrame.previous == ZR_NULL);

    if (!rootChainAndFrameWereUnchanged) {
        state->aotGcRootFrameStack = &rootFrame;
        state->aotGcRootFrameDepth = 1u;
        rootFrame.previous = ZR_NULL;
    }
    rootFrameWasPopped = ZrCore_Gc_AotRootFramePop(state, &rootFrame);
    ZrCore_GlobalState_Free(state->global);

    TEST_ASSERT_TRUE_MESSAGE(duplicatePushWasRejected,
                             "pushing a node already present in the active root chain must fail");
    TEST_ASSERT_TRUE_MESSAGE(rootChainAndFrameWereUnchanged,
                             "rejecting a duplicate node must preserve its links, base, map, top, and depth");
    TEST_ASSERT_TRUE(rootFrameWasPopped);
}
