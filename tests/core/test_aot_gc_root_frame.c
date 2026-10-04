/* TODO: Windows Clang 下 Unity 的 stdnoreturn 宏与后续 UCRT 头的兼容性，
 * 需沿既有 zr_vm_aot_gc_root_frame_test 核查头文件顺序；当前 before/after
 * 编译均在 __declspec(noreturn) 处失败，尚无编译或运行通过信用。 */
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

/** @brief 满足 Unity 每用例初始化钩子；state 由各场景独立创建。
 * @note 本目标用手写 C 模拟 AOT 根帧协议，不装载或执行生成的 AOT 模块。 */
void setUp(void) {}

/** @brief 满足 Unity 每用例收尾钩子；本夹具没有共享 state 可在此回收。
 * TODO: 本文件前六个场景在 Destroy 前执行断言；若 Unity 中断，局部 state
 * 无法由此空钩子回收。inc 的正常观察路径则先 Destroy 再断言，不能一概称为
 * 泄漏；下一步核对 UnityDefaultTestRun 与具体失败入口的 state 回收责任。 */
void tearDown(void) {}

/* map 借用单个描述符，零字节偏移选取 frameBase 处的 SZrTypeValue。
 * slot、map、链节点须活到 Pop；夹具的 typeLayoutId 不覆盖布局注册或解析。 */
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

/* 用 LOCAL_ADDRESS 解释 C 局部原始对象指针，不把该地址当作 VM value。
 * 返回 map 仍借用调用方的 slot；回调非局部退出时必须移除这条借用链。 */
static SZrAotGcRootMap make_single_local_address_root_map(SZrAotGcRootSlot *slot) {
    SZrAotGcRootMap map;

    slot->stackSlot = 0u;
    slot->frameByteOffset = 0u;
    slot->typeLayoutId = 7u;
    slot->fieldByteOffset = 0u;
    slot->locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    slot->reserved0 = 0u;
    slot->reserved1 = 0u;

    map.rootCount = 1u;
    map.roots = slot;
    return map;
}

/* 同一 map 可供两个独立宿主节点借用；乱序 Pop 必须失败且保留链顶和深度，
 * 正常逐层 Pop 后节点中的借用字段清空，不把失败当作部分弹出。 */
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

/* 手工登记 stackTop 之外的 value 根，检查年轻对象到 survivor 的存活及引用
 * 写回；允许对象未搬移，不能据此宣称每次 minor 都移动地址。 */
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

        /* 模拟生成代码的临时 frame slot：它在常规 VM stackTop 之外，
         * 若 AOT root map 未参与扫描，minor GC 会失去这条引用。 */
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

/* 与 value 根场景成对：扫描器读取并改写 C 局部 raw pointer。
 * 这里不建立常规 VM 栈根，map 和 frame 均须在 C 作用域结束前弹出。 */
static void test_aot_root_frame_local_address_keeps_young_raw_object_live(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGcRootSlot slot;
    SZrAotGcRootMap map = make_single_local_address_root_map(&slot);
    SZrAotGcRootFrame rootFrame;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

    {
        SZrGarbageCollector *collector = state->global->garbageCollector;
        SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
        SZrRawObject *oldObject = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        SZrRawObject *localRoot = oldObject;
        TZrStackValuePointer rootBase = (TZrStackValuePointer)(void *)&localRoot;
        SZrRawObject *newObject;

        TEST_ASSERT_NOT_NULL(object);
        TEST_ASSERT_NOT_NULL(localRoot);

        /* LOCAL_ADDRESS 指向 C 局部指针而非 SZrTypeValue；收集器需改写
         * 该地址中的移动后对象指针。 */
        collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &rootFrame, rootBase, &map));

        collector->gcDebtSize = 4096;
        collector->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        TEST_ASSERT_NOT_NULL(localRoot);
        newObject = localRoot;
        TEST_ASSERT_TRUE(newObject == oldObject || oldObject->garbageCollectMark.forwardingAddress == newObject);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
                                 newObject->garbageCollectMark.regionKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 newObject->garbageCollectMark.storageKind);

        TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePop(state, &rootFrame));
    }

    ZrTests_Runtime_State_Destroy(state);
}

/* 人为设置正债务，断言安全点记录非零工作及 minor 类型；
 * 没有对象搬迁、完整周期终态或跨线程暂停的断言。 */
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

/* 人为标记老 owner 和年轻 child，验证屏障登记记忆集和逸出/晋升原因；
 * 不通过实际长寿对象收集得到老代，也不验证后续晋升完成。 */
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

    /* 用人为设定的代际标记验证写屏障的跨代记忆集契约。 */
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

/* 直接测试 pin 凭据的新增标记/ignore 所有权；没有调用外部 native 函数。
 * 原有 ignore 不归本次 unpin 撤销，最终由原拥有者显式归还。 */
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

    /* 原先已由调用方忽略的对象，unpin 只能撤销本次加的 pin，
     * 不能替外层调用方撤销原有的 ignore 所有权。 */
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

#include "tests/core/test_aot_gc_root_frame_exception.inc"

/** @brief 注册六个基础 GC 场景和文本包含的六个异常场景，返回 Unity 失败计数。
 * @note CTest 通过可执行套件脚本启动此手写 C 入口，无生成 AOT entry thunk。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_root_frame_push_pop_balances_state_stack);
    RUN_TEST(test_aot_root_frame_keeps_young_value_above_stack_top_live);
    RUN_TEST(test_aot_root_frame_local_address_keeps_young_raw_object_live);
    RUN_TEST(test_gc_safepoint_advances_pending_collection_debt);
    RUN_TEST(test_gc_write_barrier_records_old_to_young_value);
    RUN_TEST(test_gc_native_call_pin_value_marks_and_releases_temporary_pin);
    RUN_TEST(test_aot_root_frame_tryrun_throw_restores_empty_chain);
    RUN_TEST(test_aot_root_frame_tryrun_throw_fine_restores_empty_chain);
    RUN_TEST(test_aot_root_frame_tryrun_throw_preserves_outer_chain);
    RUN_TEST(test_aot_root_frame_nested_tryrun_throw_restores_outer_chain);
    RUN_TEST(test_aot_root_frame_tryrun_normal_push_pop_keeps_outer_chain);
    RUN_TEST(test_aot_root_frame_tryrun_throw_preserves_relocated_outer_frame_base);
    return UNITY_END();
}
