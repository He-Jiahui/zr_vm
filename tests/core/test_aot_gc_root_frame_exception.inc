/* 调用方持有的异常回调结果槽：记录 Push 是否成功及本次 Throw 状态。
 * 它跨 TryRun 存活，但不拥有回调内的 frame、map 或根指针。 */
typedef struct SZrAotRootFrameThrowContext {
    TZrBool rootFramePushSucceeded;
    EZrThreadStatus throwStatus;
} SZrAotRootFrameThrowContext;

/* 外层回调带入内层 Throw 配置，并在返回前记录内层恢复状态；
 * 布尔值供外部场景在清理 state 后断言，不延长内层 C 节点生命期。 */
typedef struct SZrAotRootFrameNestedTryRunContext {
    SZrAotRootFrameThrowContext throwContext;
    EZrThreadStatus innerStatus;
    TZrBool innerTopMatchedCaller;
    TZrBool innerDepthMatchedCaller;
} SZrAotRootFrameNestedTryRunContext;

/* 正常返回对照只记录成对 Push/Pop 的结果；节点始终留在回调 C 栈中。 */
typedef struct SZrAotRootFramePushPopContext {
    TZrBool rootFramePushSucceeded;
    TZrBool rootFramePopSucceeded;
} SZrAotRootFramePushPopContext;

/* 跨 TryRun 观察扩容与抛出：整数地址只比较搬移前后身份，不可解引用；
 * requiredStackSize 由外层场景设置，旧栈地址不能在扩容后继续作为根基址。 */
typedef struct SZrAotRootFrameGrowAndThrowContext {
    TZrSize requiredStackSize;
    uintptr_t stackBaseAfterGrowthAddress;
    TZrBool callbackRootPushSucceeded;
    TZrBool stackGrowthSucceeded;
    EZrThreadStatus throwStatus;
} SZrAotRootFrameGrowAndThrowContext;

/* 计数所有成功的旧块搬移；场景同时核对栈基址改变，不能单靠此计数
 * 认定发生了 VM 栈搬移。此上下文须活到全局 state 销毁结束。 */
typedef struct SZrAotRootFrameMovingAllocatorContext {
    TZrUInt32 moveCount;
} SZrAotRootFrameMovingAllocatorContext;

/* 成功扩容总在旧块仍存活时另行 malloc，保证地址不同；失败保留旧块。
 * 0xCD 填充让未初始化区域显露出来，随后仅复制新旧容量的交集；
 * 低地址沿测试分配器约定当作占位输入，调用方不得传真实的低地址分配。 */
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

/* 仅为搬移场景把测试分配器和其栈上计数上下文装进 global。
 * 返回主 state 借用 global；正常收尾仍通过 Runtime_State_Destroy 释放整个 global。 */
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

/* frame/map/描述符和局部根均属于此回调的 C 作用域；Push 后经真实 Throw
 * 非局部退出，不在回调中 Pop，专门检验 TryRun 移除已结束节点的责任。 */
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

/* 正常返回对照显式配对 Pop；TryRun 不应依靠异常恢复替代此常规清理。
 * 只有 Push 成功才 Pop，两个结果写回外层持有的观察上下文。 */
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

/* 外层 VM 槽根继续有效；本回调另建 C 局部根再扩容并 Throw。
 * 扩容应重定位外层 frameBase，却不能搬移本回调的宿主节点或局部指针地址。 */
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

/* 内层 Throw 捕获后应恢复仍存活的外层链；只比较链顶字段的字节，
 * 不遍历可能已结束的回调节点。先保存观察结果再显式修复测试现场，
 * 因此修复后的外层正常返回不能替代 innerTop/innerDepth 的断言。 */
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

/* 用正债务请求一次后续分代步骤；返回值仅按 lastCollectionKind 判断。
 * TODO: 该快照初始即可能是 MINOR，GcStep 也可能早退；核查 gc.c 的
 * gcLastStepWork 或周期计数，确认要证明本次收集执行/完成应增加何种观察。 */
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

/* 空链入口跨过 callback 局部节点的异常退出；先记录实际链顶/深度，
 * 再修复现场，仅匹配时请求后续 GC，Destroy 后统一断言观察结果。 */
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

/* Throw(FINE) 的状态值与正常返回相同，仍必须走非局部退出的根链恢复；
 * 此场景区分退出方式与状态值，不据 FINE 假定回调显式 Pop。 */
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

/* 内层 C 节点退出只移除内层借用；外层 LOCAL_ADDRESS 根仍须可被后续
 * minor 扫描，并通过 survivor 标记证明外层对象存活。清理完成后再断言。 */
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

/* 内层 Throw 被回调捕获，外层 TryRun 正常返回；分别断言两层恢复，
 * 避免内层主动修复现场掩盖内层失衡。外层空根只检验链恢复，不证明对象存活。 */
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

/* 无异常对照：回调自行 Pop 后正常返回，外层空根链应保持不变。
 * 不把正常路径的配对清理归因于 TryRun 的异常快照回退。 */
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

/* 同时检查 C 局部节点被抛出移除、外层节点仍活着及其 VM 槽基址重定位。
 * 只有链、搬移、当前槽和年轻对象观察均成立才做后续 GC；保存所有布尔值，
 * 先 Pop/恢复入口链并 Destroy，再让 Unity 断言报告失败。 */
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
                    /* 先记录未修复结果再恢复活链；回归失败时也不遍历已结束的回调节点。 */
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

                /* 回调局部节点已结束；只 Pop 仍存活的外层节点，再恢复场景入口链。 */
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
