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

/* 各用例借用此局部 probe 观测关闭协议；对象地址用于未搬迁场景识别，栈相对偏移用于扩栈后重取 source/mirror。
 * 开关控制具体回调动作，观测 flag 只代表本用例实际走过并断言的路径。 */
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

/* 进程级回调入口借用当前测试的 C-local probe；setUp/tearDown 清空，不延长 probe 或 VM 生命周期。 */
static SZrCloseProxyProbe *gProbe;

/* Throw 场景把 callbackRoot 的本地对象指针作为唯一 LOCAL_ADDRESS 根；map 借用此永久静态描述。 */
static const SZrAotGcRootSlot close_proxy_throw_root_slot = {
    0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u
};
static const SZrAotGcRootMap close_proxy_throw_root_map = {
    1u, &close_proxy_throw_root_slot
};

/* 资源析构专用观测，与普通 @close probe 分开；两种 source 偏移用于验证 Drop 前先清别名，再请求扩栈。 */
typedef struct SZrCloseProxyDropProbe {
    TZrMemoryOffset sourceOffset;
    TZrMemoryOffset physicalOffset;
    TZrUInt32 calls;
    TZrBool sawBothSlotsCleared;
    TZrBool grewStack;
} SZrCloseProxyDropProbe;

/* 析构回调借用最后一个资源测试的局部 drop probe，生命周期由测试和 Unity hooks 限定。 */
static SZrCloseProxyDropProbe *gDropProbe;

void setUp(void) {
    gProbe = ZR_NULL;
    gDropProbe = ZR_NULL;
}

void tearDown(void) {
    gProbe = ZR_NULL;
    gDropProbe = ZR_NULL;
}

/* 资源析构回调在两种 source 都已清空时记录次数并请求栈扩容，防止直接 owner 别名被重复 Drop。
 * 只在最后一个资源测试中注册；不检查 @close 计数；增长成功不是地址一定搬迁。 */
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

/* 按 source/older 接收者分别观察清理顺序；开关选择扩栈、请求 full GC 或抛 replacement 的具体场景。
 * gProbe 借用当前测试局部对象；GC 场景重取 receiver，原始对象地址不作为移动后身份保证；开关组合不全覆盖。 */
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
        /* 扩容可能改变栈基址；成功后从当前 call-info 重取 receiver，再检查未搬迁对象的身份。 */
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
        /* full GC 请求之后重取栈槽；只按类型和 close meta 观察 receiver，不沿用可能转发的 object 裸地址。 */
        if (gProbe->collectInCallback) {
            ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
            base = state->callInfoList->functionBase.valuePointer;
            receiver = ZrCore_Stack_GetValue(base + 1);
            gProbe->receiverSurvivedFullGc =
                    receiver->type == ZR_VALUE_TYPE_OBJECT &&
                    ZrCore_Value_GetMeta(state, receiver, ZR_META_CLOSE) != ZR_NULL;
        }
        /* 此分支规范化 replacement 后 Throw；根帧用例故意不 Pop，让受保护异常退栈恢复入口根链，避免扫描已离开的 C-local frame。 */
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

/* 创建带原生 @close 的永久原型，让各场景共享相同回调协议。
 * closer 和 prototype 永久标记仅限此 VM；失败由 Unity 断言截断，非失败清理证明。 */
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

/* 创建并初始化永久普通对象，用于无需验证对象可回收性的清理场景。
 * GC 存活用例自行构造非永久 source，不经过本 helper。 */
static SZrObject *close_proxy_new_object(SZrState *state, SZrObjectPrototype *prototype) {
    SZrObject *object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Object_Init(state, object);
    return object;
}

/* 建立人工活动栈与初始 call-info，给高槽代理和回调 scratch 留空间。
 * 返回 state 由调用测试成功尾部销毁；默认 baseCallInfo 仍是 native，只有测试显式置 NONE 才启用 VM layout。 */
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

/* 将普通对象放入指定 logical/physical 栈槽，供关闭链读取 @close 元方法。
 * 不注册关闭节点，也不创建 Unique/Shared owner。 */
static void close_proxy_put_object(SZrState *state,
                                   TZrStackValuePointer slot,
                                   SZrObject *object) {
    ZrCore_Value_InitAsRawObject(state,
                                 ZrCore_Stack_GetValue(slot),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Stack_GetValue(slot)->type = ZR_VALUE_TYPE_OBJECT;
}

/* 把文本规范化成 ambient Error，供 pending-error 关闭守卫保存并传给回调。
 * 此 helper 不 Throw；NormalizeThrownValue 成功与实际异常跳转分别检查。 */
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

/* 读取当前 Error 的 message，区分保留 original 与替换为 replacement。
 * 返回借用字符串地址供紧邻断言；缺异常、字段名分配失败或字段类型不符返回 null。 */
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

/* 代理先消费 source 并在回调扩栈后保持 receiver；随后空 source 登记与更老对象按次摘链。
 * 分别检查 source 一次、older 延后一次与 sentinel；请求扩栈成功不等于必然搬迁。 */
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

/* 只登记高 proxy 而不登记 source，确认一次关闭仍消费原 local 并回到 sentinel。
 * 人工 core API 场景，不执行 parser 的 body-free using 语法。 */
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

/* 无 close meta 的整数 73 经代理清理后仍可读取，区分摘登记与消费值。
 * 只检查 logical source；不构造 ownership owner。 */
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

/* 活动 VM layout 把 logical 整数映射到远处 VALUE mirror，关闭代理后两处 73 均保留。
 * 普通 layout 查找允许偏移16；不是 AOT DIRECT_VALUE fast-layout 夹具。 */
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

/* 关闭 borrowed logical/mirror 只清视图，不调用对象 @close；Unique owner 仍由测试显式释放。
 * owner kind 保持 Unique，未对完整所有权/refcount 状态作额外断言。 */
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

/* 用显式关闭阈值模拟 handler 边界，pending error 传入 @close，而较低 source 登记保留到下一次摘链。
 * 没有建立真实 handler 或脚本 catch；errorPassed 只断言非 null，不断言具体 Error 内容。 */
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

/* 两个 proxy 指向同一 source，关闭两个节点只有首个回调，后一个看到已清空 local。
 * 只检查一次回调与最终 sentinel，不覆盖嵌套脚本语义。 */
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

/* 对同一普通对象的 logical source 和独立 VM VALUE mirror，回调进入前都应为 null。
 * 旧 source 登记稍后摘除无重复回调；非所有权控制块场景。 */
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

/* 非永久 source 与栈内 proxy 先请求 full GC，再在关闭回调中请求一次，检查 receiver 仍有对象类型和 close meta。
 * 回调后只检查可访问性；未断言收集统计、地址搬迁或所有对象释放。 */
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

/* ambient original Error 在关闭回调请求 full GC 后仍可读 message，防止 guard 保存的异常失根。
 * source 是永久对象；Error 存活与 receiver 可访问性分别断言。 */
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

/* 外层 TryRun 的结果载体：只有关闭请求正常返回时才写 closedCount，不存 callback 抛错状态。 */
typedef struct SZrCloseProxyCloseResult {
    TZrSize closedCount;
} SZrCloseProxyCloseResult;

/* 作为 TryRun 回调请求关闭一个登记，并把正常返回的关闭数写入外层结果。
 * result 属于测试局部变量；内部 close guard 消化 callback throw，因此外层 TryRun 可以返回 FINE。 */
static void close_proxy_close_with_error_in_try(SZrState *state, TZrPtr argument) {
    SZrCloseProxyCloseResult *result = (SZrCloseProxyCloseResult *)argument;
    result->closedCount = ZrCore_Closure_CloseRegisteredValues(
            state, 1u, ZR_THREAD_STATUS_RUNTIME_ERROR, ZR_FALSE);
}

/* 重复三次让 native @close 抛 replacement，检查原 Error 被替换及可复用 call-info 链、栈顶和预算标记恢复。
 * 外层 TryRun FINE 表示内层 guard 已处理跳转，不表示 callback 未抛错；未检测所有内存泄漏。 */
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
    /* 每轮重新登记并替换 Error，检查活动帧已退回 base、next 链仍完整及栈顶/预算标记恢复；不是堆泄漏检测。 */
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

/* native 回调登记 C-local AOT root 后 Throw，返回 guard 时根链/深度必须清空，再请求 full GC 后仍读 replacement。
 * 人工根帧异常退栈夹具，不执行生成 AOT function。 */
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

/* 已取消预算应阻止 @close 调用并保留 CANCELLED/EXECUTION_TERMINATED，而不恢复旧 ambient Error。
 * 先保存观测再解绑/free cancel token 与 VM，断言不访问已释放 state。 */
static void test_pending_error_close_preserves_budget_termination(void) {
    SZrCloseProxyProbe probe = {0};
    SZrCloseProxyCloseResult result = {0};
    SZrExecutionBudget budget = {0};
    SZrExecutionCancelToken *cancelToken = ZrCore_ExecutionCancelToken_New();
    TZrStackValuePointer frame;
    SZrState *state = close_proxy_new_state(&frame);
    SZrObjectPrototype *prototype = close_proxy_new_prototype(state);
    SZrObject *source = close_proxy_new_object(state, prototype);
    EZrThreadStatus tryStatus;
    EZrThreadStatus threadStatus;
    EZrExecutionTermination termination;
    TZrBool hasCurrentException;

    TEST_ASSERT_NOT_NULL(cancelToken);
    gProbe = &probe;
    probe.sourceObject = ZR_CAST_RAW_OBJECT_AS_SUPER(source);
    probe.sourceOffset = ZrCore_Stack_SavePointerAsOffset(state, frame + 1u);
    close_proxy_put_object(state, frame + 1u, source);
    TEST_ASSERT_TRUE(ZrCore_Closure_MarkCloseProxy(state, frame + 3u, frame + 1u));
    close_proxy_seed_current_error(state, "original");
    budget.cancelToken = cancelToken;
    state->executionBudget = &budget;
    ZrCore_ExecutionCancelToken_Cancel(cancelToken);

    /* 先保存需要断言的状态，再解绑栈内 budget 并释放 token/VM；后续断言只读取保存的标量与 probe。 */
    tryStatus = ZrCore_Exception_TryRun(
            state, close_proxy_close_with_error_in_try, &result);
    threadStatus = state->threadStatus;
    termination = budget.termination;
    hasCurrentException = state->hasCurrentException;
    state->executionBudget = ZR_NULL;
    ZrCore_ExecutionCancelToken_Free(cancelToken);
    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, tryStatus);
    TEST_ASSERT_EQUAL_UINT64(1u, result.closedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, probe.sourceCalls);
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_TERMINATION_CANCELLED, termination);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXECUTION_TERMINATED, threadStatus);
    TEST_ASSERT_FALSE(hasCurrentException);
}

/* proxy 低于 source 和现有链头时拒绝登记，原关闭链和值槽保持，随后正常关闭 source。
 * 一个输入同时违反两个顺序条件，不能单独隔离每个拒绝分支。 */
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

/* 人工 VM layout 的 source/proxy 位于物理偏移16/17，较高 proxy 清空 logical source 与 mirror 后留下外层物理登记。
 * 直接调用 core API；不是调用 AOT runtime helper 或 generated entry。 */
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

/* native baseCallInfo 虽缓存旧 layout，也不能把无关物理对象当作 logical source mirror 清除。
 * 保留 unrelated 的类型和对象地址；测试显式移除旧 metadata 再 Destroy。 */
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

/* 复制 token 到另一高槽后，槽身份校验拒绝其加入链；原 proxy 仍只关闭 source 一次。
 * 调用 ToBeClosedValueClosureNew 无返回值，链头不变是拒绝证据；不调用 MarkCloseProxy 的 occupied 分支。 */
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

/* 普通对象 Unique 值复制后两槽持有同一 control 的两份强引用，代理消费两份引用并清空两槽。
 * 强计数断言 2→0；不读取已 Drop 对象字段，也不据此断言析构回调次数。 */
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

/* 资源 direct owner 无 control，复制 mirror 只是别名；两源槽先清空再析构扩栈，外层登记后续摘除不重复 Drop。
 * 析构回调次数为1；普通 @close 回调不作为该 Drop 观测。 */
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
    RUN_TEST(test_pending_error_close_preserves_budget_termination);
    RUN_TEST(test_proxy_rejects_slot_below_current_marker_without_changing_chain);
    RUN_TEST(test_physical_proxy_follows_physical_source_marker);
    RUN_TEST(test_native_frame_does_not_clear_an_inactive_physical_layout);
    RUN_TEST(test_copied_proxy_token_cannot_register_at_another_slot);
    RUN_TEST(test_proxy_releases_retained_owner_mirror_without_invalidating_stage);
    RUN_TEST(test_proxy_drops_direct_owner_alias_once_after_tombstoning);
    return UNITY_END();
}
