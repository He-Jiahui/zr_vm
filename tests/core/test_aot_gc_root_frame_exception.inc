typedef struct SZrAotRootFrameThrowContext {
    TZrBool rootFramePushSucceeded;
    EZrThreadStatus throwStatus;
} SZrAotRootFrameThrowContext;

typedef struct SZrAotRootFrameNestedTryRunContext {
    SZrAotRootFrameThrowContext throwContext;
    EZrThreadStatus innerStatus;
    TZrBool innerTopMatchedCaller;
    TZrBool innerDepthMatchedCaller;
} SZrAotRootFrameNestedTryRunContext;

typedef struct SZrAotRootFramePushPopContext {
    TZrBool rootFramePushSucceeded;
    TZrBool rootFramePopSucceeded;
} SZrAotRootFramePushPopContext;

typedef struct SZrAotRootFrameGrowAndThrowContext {
    TZrSize requiredStackSize;
    uintptr_t stackBaseAfterGrowthAddress;
    TZrBool callbackRootPushSucceeded;
    TZrBool stackGrowthSucceeded;
    EZrThreadStatus throwStatus;
} SZrAotRootFrameGrowAndThrowContext;

typedef struct SZrAotRootFrameMovingAllocatorContext {
    TZrUInt32 moveCount;
} SZrAotRootFrameMovingAllocatorContext;

static TZrPtr aot_root_frame_moving_allocator(TZrPtr userData,
                                               TZrPtr pointer,
                                               TZrSize originalSize,
                                               TZrSize newSize,
                                               TZrInt64 flag) {
    SZrAotRootFrameMovingAllocatorContext *context =
            (SZrAotRootFrameMovingAllocatorContext *)userData;
    uintptr_t pointerAddress = (uintptr_t)(void *)pointer;
    TZrPtr newPointer;

    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0u) {
        if (pointer != ZR_NULL && pointerAddress >= (uintptr_t)0x1000u) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL || pointerAddress < (uintptr_t)0x1000u) {
        newPointer = malloc(newSize);
        if (newPointer != ZR_NULL) {
            memset(newPointer, 0xCD, newSize);
        }
        return newPointer;
    }

    newPointer = malloc(newSize);
    if (newPointer == ZR_NULL) {
        return ZR_NULL;
    }
    memset(newPointer, 0xCD, newSize);
    if (originalSize > 0u) {
        memcpy(newPointer, pointer, originalSize < newSize ? originalSize : newSize);
    }
    free(pointer);
    if (context != ZR_NULL) {
        context->moveCount++;
    }
    return newPointer;
}

static SZrState *aot_root_frame_create_state_with_moving_allocator(
        SZrAotRootFrameMovingAllocatorContext *allocatorContext) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            aot_root_frame_moving_allocator, allocatorContext, 12345, &callbacks);
    SZrState *state;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    state = global->mainThreadState;
    if (state != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(state, global);
    }
    return state;
}

/* This node and its map, descriptor, and slot live in the callback's C frame.
 * The test intentionally throws across that frame using the real TryRun API. */
static void aot_root_frame_push_then_throw(SZrState *state, TZrPtr arguments) {
    SZrAotRootFrameThrowContext *context = (SZrAotRootFrameThrowContext *)arguments;
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame frame;
    SZrRawObject *localRoot = ZR_NULL;
    TZrStackValuePointer frameBase = (TZrStackValuePointer)(void *)&localRoot;

    context->rootFramePushSucceeded =
            ZrCore_Gc_AotRootFramePush(state, &frame, frameBase, &map);
    ZrCore_Exception_Throw(state, context->throwStatus);
}

static void aot_root_frame_push_pop_normally(SZrState *state, TZrPtr arguments) {
    SZrAotRootFramePushPopContext *context = (SZrAotRootFramePushPopContext *)arguments;
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame frame;
    SZrRawObject *localRoot = ZR_NULL;
    TZrStackValuePointer frameBase = (TZrStackValuePointer)(void *)&localRoot;

    context->rootFramePushSucceeded =
            ZrCore_Gc_AotRootFramePush(state, &frame, frameBase, &map);
    if (context->rootFramePushSucceeded) {
        context->rootFramePopSucceeded = ZrCore_Gc_AotRootFramePop(state, &frame);
    }
}

/* Keep an outer VM-stack root active while this callback pushes a C-local root,
 * forces a moving stack allocation, and exits non-locally. */
static void aot_root_frame_grow_stack_then_throw(SZrState *state, TZrPtr arguments) {
    SZrAotRootFrameGrowAndThrowContext *context =
            (SZrAotRootFrameGrowAndThrowContext *)arguments;
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame frame;
    SZrRawObject *localRoot = ZR_NULL;

    context->callbackRootPushSucceeded = ZrCore_Gc_AotRootFramePush(
            state, &frame, (TZrStackValuePointer)(void *)&localRoot, &map);
    context->stackGrowthSucceeded = ZrCore_Stack_GrowTo(
            state, context->requiredStackSize, ZR_TRUE);
    context->stackBaseAfterGrowthAddress = (uintptr_t)(void *)state->stackBase.valuePointer;
    ZrCore_Exception_Throw(state, context->throwStatus);
}

/* A caught inner throw must restore the still-live outer chain before returning
 * through this callback.  The equality check reads only the state field's byte
 * representation; on the failing implementation it can contain a pointer to
 * the callback frame that the longjmp has already ended. */
static void aot_root_frame_nested_tryrun_callback(SZrState *state, TZrPtr arguments) {
    SZrAotRootFrameNestedTryRunContext *context =
            (SZrAotRootFrameNestedTryRunContext *)arguments;
    SZrAotGcRootFrame *savedCallerTop = state->aotGcRootFrameStack;
    TZrUInt32 savedCallerDepth = state->aotGcRootFrameDepth;

    context->innerStatus = ZrCore_Exception_TryRun(
            state, aot_root_frame_push_then_throw, &context->throwContext);
    context->innerTopMatchedCaller =
            memcmp(&state->aotGcRootFrameStack, &savedCallerTop, sizeof(savedCallerTop)) == 0;
    context->innerDepthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
    state->aotGcRootFrameStack = savedCallerTop;
    state->aotGcRootFrameDepth = savedCallerDepth;
}

static TZrBool aot_root_frame_run_following_minor_gc(SZrState *state) {
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 4096;
    collector->gcLastStepWork = 0;
    ZrCore_GarbageCollector_GcStep(state);
    return collector->statsSnapshot.lastCollectionKind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
}

static void test_aot_root_frame_tryrun_throw_restores_empty_chain(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotRootFrameThrowContext context = {ZR_FALSE, ZR_THREAD_STATUS_RUNTIME_ERROR};
    SZrAotGcRootFrame *savedCallerTop;
    SZrAotGcRootFrame *emptyTop = ZR_NULL;
    TZrUInt32 savedCallerDepth;
    TZrBool initialChainWasEmpty;
    TZrBool topMatchedCaller;
    TZrBool depthMatchedCaller;
    TZrBool followingMinorGcRan = ZR_FALSE;
    EZrThreadStatus status = ZR_THREAD_STATUS_INVALID;

    if (state == ZR_NULL) {
        TEST_ASSERT_NOT_NULL(state);
        return;
    }

    savedCallerTop = state->aotGcRootFrameStack;
    savedCallerDepth = state->aotGcRootFrameDepth;
    initialChainWasEmpty =
            memcmp(&savedCallerTop, &emptyTop, sizeof(savedCallerTop)) == 0 && savedCallerDepth == 0u;

    status = ZrCore_Exception_TryRun(state, aot_root_frame_push_then_throw, &context);
    topMatchedCaller =
            memcmp(&state->aotGcRootFrameStack, &savedCallerTop, sizeof(savedCallerTop)) == 0;
    depthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
    state->aotGcRootFrameStack = savedCallerTop;
    state->aotGcRootFrameDepth = savedCallerDepth;

    if (initialChainWasEmpty && topMatchedCaller && depthMatchedCaller) {
        followingMinorGcRan = aot_root_frame_run_following_minor_gc(state);
    }

    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(initialChainWasEmpty);
    TEST_ASSERT_TRUE(context.rootFramePushSucceeded);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    TEST_ASSERT_TRUE(topMatchedCaller);
    TEST_ASSERT_TRUE(depthMatchedCaller);
    TEST_ASSERT_TRUE(followingMinorGcRan);
}

static void test_aot_root_frame_tryrun_throw_fine_restores_empty_chain(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotRootFrameThrowContext context = {ZR_FALSE, ZR_THREAD_STATUS_FINE};
    SZrAotGcRootFrame *savedCallerTop;
    TZrUInt32 savedCallerDepth;
    TZrBool topMatchedCaller;
    TZrBool depthMatchedCaller;
    TZrBool followingMinorGcRan = ZR_FALSE;
    EZrThreadStatus status = ZR_THREAD_STATUS_INVALID;

    if (state == ZR_NULL) {
        TEST_ASSERT_NOT_NULL(state);
        return;
    }

    savedCallerTop = state->aotGcRootFrameStack;
    savedCallerDepth = state->aotGcRootFrameDepth;
    status = ZrCore_Exception_TryRun(state, aot_root_frame_push_then_throw, &context);
    topMatchedCaller =
            memcmp(&state->aotGcRootFrameStack, &savedCallerTop, sizeof(savedCallerTop)) == 0;
    depthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
    state->aotGcRootFrameStack = savedCallerTop;
    state->aotGcRootFrameDepth = savedCallerDepth;

    if (topMatchedCaller && depthMatchedCaller) {
        followingMinorGcRan = aot_root_frame_run_following_minor_gc(state);
    }

    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(context.rootFramePushSucceeded);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_TRUE(topMatchedCaller);
    TEST_ASSERT_TRUE(depthMatchedCaller);
    TEST_ASSERT_TRUE(followingMinorGcRan);
}

static void test_aot_root_frame_tryrun_throw_preserves_outer_chain(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object = ZR_NULL;
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame outerFrame;
    SZrRawObject *outerRoot = ZR_NULL;
    TZrStackValuePointer outerFrameBase = (TZrStackValuePointer)(void *)&outerRoot;
    SZrAotRootFrameThrowContext context = {ZR_FALSE, ZR_THREAD_STATUS_RUNTIME_ERROR};
    SZrAotGcRootFrame *savedCallerTop;
    TZrUInt32 savedCallerDepth;
    TZrBool outerObjectCreated = ZR_FALSE;
    TZrBool outerObjectSurvivedMinorGc = ZR_FALSE;
    TZrBool outerPushSucceeded = ZR_FALSE;
    TZrBool topMatchedCaller = ZR_FALSE;
    TZrBool depthMatchedCaller = ZR_FALSE;
    TZrBool followingMinorGcRan = ZR_FALSE;
    TZrBool outerPopSucceeded = ZR_FALSE;
    EZrThreadStatus status = ZR_THREAD_STATUS_INVALID;

    if (state == ZR_NULL) {
        TEST_ASSERT_NOT_NULL(state);
        return;
    }

    savedCallerTop = &outerFrame;
    savedCallerDepth = 1u;
    if (state->global != ZR_NULL && state->global->garbageCollector != ZR_NULL) {
        object = ZrCore_Object_New(state, ZR_NULL);
        if (object != ZR_NULL) {
            outerRoot = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
            outerObjectCreated = ZR_TRUE;
        }
    }
    outerPushSucceeded = ZrCore_Gc_AotRootFramePush(state, &outerFrame, outerFrameBase, &map);
    if (outerPushSucceeded) {
        status = ZrCore_Exception_TryRun(state, aot_root_frame_push_then_throw, &context);
        topMatchedCaller =
                memcmp(&state->aotGcRootFrameStack, &savedCallerTop, sizeof(savedCallerTop)) == 0;
        depthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
        state->aotGcRootFrameStack = savedCallerTop;
        state->aotGcRootFrameDepth = savedCallerDepth;

        if (topMatchedCaller && depthMatchedCaller) {
            followingMinorGcRan = aot_root_frame_run_following_minor_gc(state);
            if (followingMinorGcRan && outerRoot != ZR_NULL) {
                outerObjectSurvivedMinorGc =
                        outerRoot->garbageCollectMark.regionKind == ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR &&
                        outerRoot->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
            }
        }

        outerPopSucceeded = ZrCore_Gc_AotRootFramePop(state, &outerFrame);
    }

    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(outerPushSucceeded);
    TEST_ASSERT_TRUE(context.rootFramePushSucceeded);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    TEST_ASSERT_TRUE(outerObjectCreated);
    TEST_ASSERT_TRUE(topMatchedCaller);
    TEST_ASSERT_TRUE(depthMatchedCaller);
    TEST_ASSERT_TRUE(followingMinorGcRan);
    TEST_ASSERT_TRUE(outerObjectSurvivedMinorGc);
    TEST_ASSERT_TRUE(outerPopSucceeded);
}

static void test_aot_root_frame_nested_tryrun_throw_restores_outer_chain(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame outerFrame;
    SZrRawObject *outerRoot = ZR_NULL;
    TZrStackValuePointer outerFrameBase = (TZrStackValuePointer)(void *)&outerRoot;
    SZrAotRootFrameNestedTryRunContext context = {
            {ZR_FALSE, ZR_THREAD_STATUS_RUNTIME_ERROR}, ZR_THREAD_STATUS_INVALID, ZR_FALSE, ZR_FALSE};
    SZrAotGcRootFrame *savedCallerTop;
    TZrUInt32 savedCallerDepth;
    TZrBool outerPushSucceeded = ZR_FALSE;
    TZrBool outerTopMatchedCaller = ZR_FALSE;
    TZrBool outerDepthMatchedCaller = ZR_FALSE;
    TZrBool followingMinorGcRan = ZR_FALSE;
    TZrBool outerPopSucceeded = ZR_FALSE;
    EZrThreadStatus outerStatus = ZR_THREAD_STATUS_INVALID;

    if (state == ZR_NULL) {
        TEST_ASSERT_NOT_NULL(state);
        return;
    }

    savedCallerTop = &outerFrame;
    savedCallerDepth = 1u;
    outerPushSucceeded = ZrCore_Gc_AotRootFramePush(state, &outerFrame, outerFrameBase, &map);
    if (outerPushSucceeded) {
        outerStatus = ZrCore_Exception_TryRun(state, aot_root_frame_nested_tryrun_callback, &context);
        outerTopMatchedCaller =
                memcmp(&state->aotGcRootFrameStack, &savedCallerTop, sizeof(savedCallerTop)) == 0;
        outerDepthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
        state->aotGcRootFrameStack = savedCallerTop;
        state->aotGcRootFrameDepth = savedCallerDepth;

        if (context.innerTopMatchedCaller && context.innerDepthMatchedCaller &&
            outerTopMatchedCaller && outerDepthMatchedCaller) {
            followingMinorGcRan = aot_root_frame_run_following_minor_gc(state);
        }

        outerPopSucceeded = ZrCore_Gc_AotRootFramePop(state, &outerFrame);
    }

    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(outerPushSucceeded);
    TEST_ASSERT_TRUE(context.throwContext.rootFramePushSucceeded);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_RUNTIME_ERROR, context.innerStatus);
    TEST_ASSERT_TRUE(context.innerTopMatchedCaller);
    TEST_ASSERT_TRUE(context.innerDepthMatchedCaller);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_TRUE(outerTopMatchedCaller);
    TEST_ASSERT_TRUE(outerDepthMatchedCaller);
    TEST_ASSERT_TRUE(followingMinorGcRan);
    TEST_ASSERT_TRUE(outerPopSucceeded);
}

static void test_aot_root_frame_tryrun_normal_push_pop_keeps_outer_chain(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame outerFrame;
    SZrRawObject *outerRoot = ZR_NULL;
    TZrStackValuePointer outerFrameBase = (TZrStackValuePointer)(void *)&outerRoot;
    SZrAotRootFramePushPopContext context = {ZR_FALSE, ZR_FALSE};
    SZrAotGcRootFrame *savedCallerTop;
    TZrUInt32 savedCallerDepth;
    TZrBool outerPushSucceeded = ZR_FALSE;
    TZrBool topMatchedCaller = ZR_FALSE;
    TZrBool depthMatchedCaller = ZR_FALSE;
    TZrBool followingMinorGcRan = ZR_FALSE;
    TZrBool outerPopSucceeded = ZR_FALSE;
    EZrThreadStatus status = ZR_THREAD_STATUS_INVALID;

    if (state == ZR_NULL) {
        TEST_ASSERT_NOT_NULL(state);
        return;
    }

    savedCallerTop = &outerFrame;
    savedCallerDepth = 1u;
    outerPushSucceeded = ZrCore_Gc_AotRootFramePush(state, &outerFrame, outerFrameBase, &map);
    if (outerPushSucceeded) {
        status = ZrCore_Exception_TryRun(state, aot_root_frame_push_pop_normally, &context);
        topMatchedCaller =
                memcmp(&state->aotGcRootFrameStack, &savedCallerTop, sizeof(savedCallerTop)) == 0;
        depthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
        state->aotGcRootFrameStack = savedCallerTop;
        state->aotGcRootFrameDepth = savedCallerDepth;

        if (context.rootFramePushSucceeded && context.rootFramePopSucceeded &&
            topMatchedCaller && depthMatchedCaller) {
            followingMinorGcRan = aot_root_frame_run_following_minor_gc(state);
        }

        outerPopSucceeded = ZrCore_Gc_AotRootFramePop(state, &outerFrame);
    }

    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(outerPushSucceeded);
    TEST_ASSERT_TRUE(context.rootFramePushSucceeded);
    TEST_ASSERT_TRUE(context.rootFramePopSucceeded);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_TRUE(topMatchedCaller);
    TEST_ASSERT_TRUE(depthMatchedCaller);
    TEST_ASSERT_TRUE(followingMinorGcRan);
    TEST_ASSERT_TRUE(outerPopSucceeded);
}

static void test_aot_root_frame_tryrun_throw_preserves_relocated_outer_frame_base(void) {
    SZrAotRootFrameMovingAllocatorContext allocatorContext = {0};
    SZrState *state = aot_root_frame_create_state_with_moving_allocator(&allocatorContext);
    SZrGarbageCollector *collector = ZR_NULL;
    SZrAotGcRootSlot stackRootSlot = {0};
    SZrAotGcRootMap stackRootMap = make_single_slot_root_map(&stackRootSlot);
    SZrAotGcRootFrame outerFrame;
    SZrObject *object = ZR_NULL;
    SZrRawObject *oldObject = ZR_NULL;
    SZrTypeValueOnStack *stackFrameBase = ZR_NULL;
    SZrTypeValueOnStack *relocatedFrameBase = ZR_NULL;
    SZrAotGcRootFrame *savedCallerTop = ZR_NULL;
    SZrAotGcRootFrame *savedOuterCallerTop = ZR_NULL;
    TZrUInt32 savedCallerDepth = 0u;
    TZrUInt32 savedOuterCallerDepth = 0u;
    TZrSize requiredStackSize = 0u;
    TZrMemoryOffset frameBaseOffset = 0;
    uintptr_t oldStackBaseAddress = (uintptr_t)0u;
    TZrUInt32 movesBeforeGrowth = 0u;
    SZrAotRootFrameGrowAndThrowContext throwContext = {
            0u, (uintptr_t)0u, ZR_FALSE, ZR_FALSE, ZR_THREAD_STATUS_RUNTIME_ERROR};
    EZrThreadStatus status = ZR_THREAD_STATUS_INVALID;
    TZrBool outerPushSucceeded = ZR_FALSE;
    TZrBool objectCreated = ZR_FALSE;
    TZrBool topMatchedCaller = ZR_FALSE;
    TZrBool depthMatchedCaller = ZR_FALSE;
    TZrBool stackActuallyMoved = ZR_FALSE;
    TZrBool outerFrameBaseWasRebased = ZR_FALSE;
    TZrBool rootSlotWasInCurrentStack = ZR_FALSE;
    TZrBool rootSlotStillHeldYoungObject = ZR_FALSE;
    TZrBool followingMinorGcRan = ZR_FALSE;
    TZrBool rootReachedSurvivor = ZR_FALSE;
    TZrBool outerPopSucceeded = ZR_FALSE;

    if (state == ZR_NULL) {
        TEST_ASSERT_NOT_NULL(state);
        return;
    }
    if (state->global != ZR_NULL) {
        collector = state->global->garbageCollector;
    }
    if (collector != ZR_NULL && state->stackTop.valuePointer != ZR_NULL &&
        state->stackTail.valuePointer != ZR_NULL) {
        collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        if (state->stackTail.valuePointer - state->stackTop.valuePointer > 4u) {
            stackFrameBase = state->stackTop.valuePointer + 4u;
            frameBaseOffset = ZrCore_Stack_SavePointerAsOffset(state, stackFrameBase);
            ZrCore_Value_ResetAsNull(&stackFrameBase->value);
            savedOuterCallerTop = state->aotGcRootFrameStack;
            savedOuterCallerDepth = state->aotGcRootFrameDepth;
            outerPushSucceeded = ZrCore_Gc_AotRootFramePush(
                    state, &outerFrame, stackFrameBase, &stackRootMap);
            if (outerPushSucceeded) {
                savedCallerTop = state->aotGcRootFrameStack;
                savedCallerDepth = state->aotGcRootFrameDepth;
                object = ZrCore_Object_New(state, ZR_NULL);
                if (object != ZR_NULL) {
                    objectCreated = ZR_TRUE;
                    oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
                    ZrCore_Value_InitAsRawObject(state, &stackFrameBase->value, oldObject);

                    requiredStackSize = ZrCore_State_StackGetSize(state) + 1u;
                    throwContext.requiredStackSize = requiredStackSize;
                    oldStackBaseAddress = (uintptr_t)(void *)state->stackBase.valuePointer;
                    movesBeforeGrowth = allocatorContext.moveCount;
                    status = ZrCore_Exception_TryRun(
                            state, aot_root_frame_grow_stack_then_throw, &throwContext);

                    topMatchedCaller =
                            memcmp(&state->aotGcRootFrameStack,
                                   &savedCallerTop,
                                   sizeof(savedCallerTop)) == 0;
                    depthMatchedCaller = state->aotGcRootFrameDepth == savedCallerDepth;
                    /* Repair before any GC or Pop, including the expected RED build. */
                    state->aotGcRootFrameStack = savedCallerTop;
                    state->aotGcRootFrameDepth = savedCallerDepth;

                    stackActuallyMoved =
                            throwContext.stackGrowthSucceeded &&
                            throwContext.stackBaseAfterGrowthAddress != oldStackBaseAddress &&
                            allocatorContext.moveCount > movesBeforeGrowth;
                    if (stackActuallyMoved) {
                        relocatedFrameBase = ZrCore_Stack_LoadOffsetToPointer(state, frameBaseOffset);
                        outerFrameBaseWasRebased =
                                memcmp(&outerFrame.frameBase,
                                       &relocatedFrameBase,
                                       sizeof(relocatedFrameBase)) == 0;
                    }
                    if (stackActuallyMoved && outerFrameBaseWasRebased) {
                        rootSlotWasInCurrentStack =
                                relocatedFrameBase >= state->stackBase.valuePointer &&
                                relocatedFrameBase < state->stackTail.valuePointer;
                        if (rootSlotWasInCurrentStack) {
                            rootSlotStillHeldYoungObject =
                                    relocatedFrameBase->value.isGarbageCollectable &&
                                    relocatedFrameBase->value.value.object == oldObject &&
                                    oldObject->garbageCollectMark.regionKind ==
                                            ZR_GARBAGE_COLLECT_REGION_KIND_EDEN &&
                                    oldObject->garbageCollectMark.storageKind ==
                                            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE &&
                                    oldObject->garbageCollectMark.heapGenerationKind ==
                                            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG;
                        }
                    }

                    if (topMatchedCaller && depthMatchedCaller && stackActuallyMoved &&
                        outerFrameBaseWasRebased && rootSlotWasInCurrentStack &&
                        rootSlotStillHeldYoungObject &&
                        status == ZR_THREAD_STATUS_RUNTIME_ERROR) {
                        followingMinorGcRan = aot_root_frame_run_following_minor_gc(state);
                        if (followingMinorGcRan) {
                            SZrRawObject *survivor = relocatedFrameBase->value.value.object;
                            rootReachedSurvivor =
                                    relocatedFrameBase->value.isGarbageCollectable &&
                                    survivor != ZR_NULL &&
                                    survivor->garbageCollectMark.regionKind ==
                                            ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR &&
                                    survivor->garbageCollectMark.storageKind ==
                                            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
                        }
                    }
                }

                /* TryRun's callback-local frame has ended; Pop only the live outer node. */
                state->aotGcRootFrameStack = savedCallerTop;
                state->aotGcRootFrameDepth = savedCallerDepth;
                outerPopSucceeded = ZrCore_Gc_AotRootFramePop(state, &outerFrame);
                state->aotGcRootFrameStack = savedOuterCallerTop;
                state->aotGcRootFrameDepth = savedOuterCallerDepth;
            }
        }
    }

    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(outerPushSucceeded);
    TEST_ASSERT_TRUE(objectCreated);
    TEST_ASSERT_TRUE(throwContext.callbackRootPushSucceeded);
    TEST_ASSERT_TRUE(throwContext.stackGrowthSucceeded);
    TEST_ASSERT_EQUAL_UINT32(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    TEST_ASSERT_TRUE(topMatchedCaller);
    TEST_ASSERT_TRUE(depthMatchedCaller);
    TEST_ASSERT_TRUE(stackActuallyMoved);
    TEST_ASSERT_TRUE(outerFrameBaseWasRebased);
    TEST_ASSERT_TRUE(rootSlotWasInCurrentStack);
    TEST_ASSERT_TRUE(rootSlotStillHeldYoungObject);
    TEST_ASSERT_TRUE(followingMinorGcRan);
    TEST_ASSERT_TRUE(rootReachedSurvivor);
    TEST_ASSERT_TRUE(outerPopSucceeded);
}
