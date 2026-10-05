/* 本文件由 Unity main 逐例驱动直接 task-frame API；不装配调度器 wake/cancel。
 * layout、pool 与回调上下文均由用例栈借出，必须先 Free task 再销毁 pool。
 * TODO：Unity 断言长跳转会绕过局部 Task/Pool_Free；tearDown 仅销毁 VM，需另核宿主 calloc 帧的失败收尾。 */
#include "unity.h"
#include "test_support.h"

#include <string.h>

#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/task_frame_runtime.h"
#include "zr_vm_core/value.h"

/* Unity 每例重建 VM 状态；poll 与 drop/finally 回调在该状态存活期间执行。 */
/* 仅供本翻译单元串行用例借用；tearDown 后任何 poll/drop/finally 均不得继续使用。 */
static SZrState *g_state;

/* 在根表扩容期间按需拒绝一次 ARRAY 分配；上下文保持到 Unity tearDown。 */
typedef struct SZrTaskFrameAllocatorFailureContext {
    /* 保存安装拒绝器前的宿主函数；拒绝条件不满足时必须仍可调用。 */
    FZrAllocator upstreamAllocator;
    /* 与上游分配器配套的借用参数；保持到 VM 销毁结束。 */
    TZrPtr upstreamAllocationArguments;
    /* 一次性开关：命中指定分配后立即清除，避免故障清理继续受拒绝影响。 */
    TZrBool rejectNextArrayAllocation;
    /* 累计实际拒绝次数，区分填表阶段与 slot 注册阶段的两个失败。 */
    TZrUInt32 rejectedArrayAllocationCount;
} SZrTaskFrameAllocatorFailureContext;

/* 全局存储保证替换分配器的 userData 在局部用例返回及 tearDown 中仍有效。 */
static SZrTaskFrameAllocatorFailureContext g_taskFrameAllocatorFailure;

/* 拒绝一次新的非零 ARRAY 分配；此用例以根表扩容触发，标签本身不限于根表。 */
/* 安装点先保存非空上游函数并传入本全局上下文；这里的空检查只保护拒绝分支。 */
static TZrPtr task_frame_fail_next_array_allocation(TZrPtr userData,
                                                    TZrPtr pointer,
                                                    TZrSize originalSize,
                                                    TZrSize newSize,
                                                    TZrInt64 flag) {
    SZrTaskFrameAllocatorFailureContext *context =
            (SZrTaskFrameAllocatorFailureContext *)userData;

    if (context != ZR_NULL && context->rejectNextArrayAllocation &&
        pointer == ZR_NULL && newSize != 0U && flag == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        context->rejectNextArrayAllocation = ZR_FALSE;
        context->rejectedArrayAllocationCount++;
        return ZR_NULL;
    }
    return context->upstreamAllocator(
            context->upstreamAllocationArguments, pointer, originalSize, newSize, flag);
}

/* UnityDefaultTestRun 在每例之前调用；重置注入统计并创建独立 VM 域。 */
void setUp(void) {
    memset(&g_taskFrameAllocatorFailure, 0, sizeof(g_taskFrameAllocatorFailure));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* Unity 在正常返回或断言保护跳转后调用；先撤销拒绝开关再释放 VM。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        g_taskFrameAllocatorFailure.rejectNextArrayAllocation = ZR_FALSE;
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 创建可经 GC 移动或标为 resource 的对象，供结果/slot 所有权用例使用。 */
/* 返回借用裸指针；调用方在主动 GC 前须写入带 root 的 slot/header 或建立 unique 所有权。 */
static SZrObject *task_frame_create_object(const TZrChar *name, TZrBool resource) {
    SZrString *typeName = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    if (resource) {
        prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    }
    object = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    return object;
}

/* 首次 poll 直接完成，不请求暂停帧。 */
/* Start 同步派发；outResult 由 runtime 预置为空，本回调只写普通整数，不留借用状态。 */
static EZrCoreTaskFramePollOutcome task_frame_sync_complete(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    ZR_UNUSED_PARAMETER(task);
    ZR_UNUSED_PARAMETER(userData);
    ZrCore_Value_InitAsInt(state, outResult, 42);
    return ZR_CORE_TASK_FRAME_POLL_COMPLETE;
}

/* poll 调用次数代表恢复进度；observedValue 捕获跨暂停 slot 的值。 */
typedef struct SZrTaskFrameSuspendContext {
    /* 跨多次 poll 的外部进度；由 Start 借用 context，须覆盖最后一次 Resume。 */
    TZrUInt32 invocationCount;
    /* 保存首次暂停 slot 的整数读回值，供用例断言而非 runtime 状态索引。 */
    TZrInt64 observedValue;
} SZrTaskFrameSuspendContext;

/* 两次保存/恢复同一 slot，模拟编译器生成的多状态 async 函数。 */
/* Start 保存本函数及 context，Resume 再派发；stateId 1/2 都在三状态 layout 范围内。 */
static EZrCoreTaskFramePollOutcome task_frame_multi_suspend(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    SZrTaskFrameSuspendContext *context = (SZrTaskFrameSuspendContext *)userData;
    SZrTypeValue value;

    if (context == ZR_NULL) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }

    if (context->invocationCount == 0U) {
        context->invocationCount++;
        ZrCore_Value_InitAsInt(state, &value, 20);
        if (!ZrCore_TaskFrameTask_Suspend(state, task, 1U) ||
            !ZrCore_TaskFrameTask_StoreSlot(state, task, 0U, &value)) {
            return ZR_CORE_TASK_FRAME_POLL_FAULT;
        }
        return ZR_CORE_TASK_FRAME_POLL_SUSPEND;
    }

    if (context->invocationCount == 1U) {
        context->invocationCount++;
        /* BUG：本次 poll 的 value 未初始化；LoadSlot 经 Value_Copy 的覆盖准备读取其 ownershipKind。
         * 前一次 poll 的 InitAsInt 不初始化本次自动对象；此处须先 ResetAsNull，当前只记录不改行为。 */
        if (!ZrCore_TaskFrameTask_LoadSlot(state, task, 0U, &value)) {
            return ZR_CORE_TASK_FRAME_POLL_FAULT;
        }
        context->observedValue = value.value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, &value, 21);
        if (!ZrCore_TaskFrameTask_Suspend(state, task, 2U) ||
            !ZrCore_TaskFrameTask_StoreSlot(state, task, 0U, &value)) {
            return ZR_CORE_TASK_FRAME_POLL_FAULT;
        }
        return ZR_CORE_TASK_FRAME_POLL_SUSPEND;
    }

    context->invocationCount++;
    ZrCore_Value_InitAsInt(state, outResult, 22);
    return ZR_CORE_TASK_FRAME_POLL_COMPLETE;
}

/* 区分 finally 执行、slot drop 与 finally 期间 slot 是否仍可读取。 */
typedef struct SZrTaskFrameFaultContext {
    /* 决定首次暂停与下一次显式 Fault 的外部进度。 */
    TZrUInt32 invocationCount;
    /* 已初始化 slot 的 drop 调用数，包括覆盖旧值与终态清理。 */
    TZrUInt32 dropCount;
    /* finally 的实际调用数；Free 后再次断言以防终态重复执行。 */
    TZrUInt32 finallyCount;
    /* finally 中 LoadSlot 的成功结果；不代表读取了全部 slot。 */
    TZrBool finallyObservedLiveSlot;
} SZrTaskFrameFaultContext;

/* slot layout 的 drop 回调，用于检查已初始化 slot 的清理次数。 */
/* slots 的 drop 字段注册本函数；cleanup_slot 在释放值/root 之前派发，context 借用到 Task_Free。 */
static void task_frame_count_drop(SZrState *state, SZrTypeValue *value, TZrPtr userData) {
    SZrTaskFrameFaultContext *context = (SZrTaskFrameFaultContext *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(value);
    if (context != ZR_NULL) {
        context->dropCount++;
    }
}

/* finally 必须先于 slot 清理运行，所以此处仍能读取暂存值。 */
/* layout 的 finally 字段注册本函数；读取目标 slot 前将输出值初始化为空。 */
static void task_frame_count_finally(SZrState *state,
                                     SZrCoreTaskFrameTask *task,
                                     TZrPtr userData) {
    SZrTaskFrameFaultContext *context = (SZrTaskFrameFaultContext *)userData;
    SZrTypeValue value;

    if (context == ZR_NULL) {
        return;
    }
    context->finallyCount++;
    ZrCore_Value_ResetAsNull(&value);
    context->finallyObservedLiveSlot = ZrCore_TaskFrameTask_LoadSlot(
            state, task, 0U, &value);
}

/* 暂停后以值故障，覆盖 slot 重写、finally 和 drop 的联合路径。 */
/* Start 的第一次 poll 写 7 后重写为 8；Resume 再 Fault(99)，未写入的 slot 1 不应 drop。 */
static EZrCoreTaskFramePollOutcome task_frame_fault_after_suspend(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    SZrTaskFrameFaultContext *context = (SZrTaskFrameFaultContext *)userData;
    SZrTypeValue value;

    ZR_UNUSED_PARAMETER(outResult);
    if (context == ZR_NULL) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }
    if (context->invocationCount++ == 0U) {
        ZrCore_Value_InitAsInt(state, &value, 7);
        if (!ZrCore_TaskFrameTask_Suspend(state, task, 1U) ||
            !ZrCore_TaskFrameTask_StoreSlot(state, task, 0U, &value)) {
            return ZR_CORE_TASK_FRAME_POLL_FAULT;
        }
        ZrCore_Value_InitAsInt(state, &value, 8);
        if (!ZrCore_TaskFrameTask_StoreSlot(state, task, 0U, &value)) {
            return ZR_CORE_TASK_FRAME_POLL_FAULT;
        }
        return ZR_CORE_TASK_FRAME_POLL_SUSPEND;
    }

    ZrCore_Value_InitAsInt(state, &value, 99);
    if (!ZrCore_TaskFrameTask_Fault(state, task, &value)) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }
    return ZR_CORE_TASK_FRAME_POLL_FAULT;
}

/* 裸对象指针只负责初次发布；暂停期间存活由 slot GC map 保证。 */
typedef struct SZrTaskFrameGcContext {
    /* 首次写 slot 的借用对象；GC 可移动它，恢复阶段不解引用此旧裸地址。 */
    SZrObject *object;
    /* 首次调用发布对象，之后完成；生命周期覆盖两次直接派发。 */
    TZrUInt32 invocationCount;
} SZrTaskFrameGcContext;

/* 根注册失败时，drop 仍需收到 StoreSlot 已复制的值，且只调用一次。 */
typedef struct SZrTaskFrameRootFailureContext {
    /* 失败回滚比较用的借用地址；本注入序列没有在复制与 drop 之间主动 GC。 */
    SZrObject *object;
    /* 检查注册失败回滚只 drop 一次，重复 Free 不增加它。 */
    TZrUInt32 dropCount;
    /* 记录 drop 收到的是已复制的 GC 值，而非清空后的 slot。 */
    TZrBool dropObservedCopiedObject;
} SZrTaskFrameRootFailureContext;

/* 把对象写入有 root 标记的 slot，随后由测试触发 compact GC。 */
/* 首次调用把借用对象地址复制到 slot；恢复以整数完成，存活对象由 root/LoadSlot 解析。 */
static EZrCoreTaskFramePollOutcome task_frame_suspend_gc_value(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    SZrTaskFrameGcContext *context = (SZrTaskFrameGcContext *)userData;
    SZrTypeValue value;

    if (context == ZR_NULL || context->object == ZR_NULL) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }
    if (context->invocationCount++ == 0U) {
        ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(context->object));
        if (!ZrCore_TaskFrameTask_Suspend(state, task, 1U) ||
            !ZrCore_TaskFrameTask_StoreSlot(state, task, 0U, &value)) {
            return ZR_CORE_TASK_FRAME_POLL_FAULT;
        }
        return ZR_CORE_TASK_FRAME_POLL_SUSPEND;
    }

    ZrCore_Value_InitAsInt(state, outResult, 1);
    return ZR_CORE_TASK_FRAME_POLL_COMPLETE;
}

/* 填满域根表后，保存 GC slot 的 root 注册会被测试分配器拒绝。 */
/* Start 借用注入上下文；Suspend 的宿主帧分配不走 VM ARRAY 拒绝器，StoreSlot 失败返回 FAULT。 */
static EZrCoreTaskFramePollOutcome task_frame_store_gc_value_after_root_table_full(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    SZrTaskFrameRootFailureContext *context =
            (SZrTaskFrameRootFailureContext *)userData;
    SZrTypeValue value;

    ZR_UNUSED_PARAMETER(outResult);
    if (context == ZR_NULL || context->object == ZR_NULL ||
        !ZrCore_TaskFrameTask_Suspend(state, task, 1U)) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(context->object));
    return ZrCore_TaskFrameTask_StoreSlot(state, task, 0U, &value)
                   ? ZR_CORE_TASK_FRAME_POLL_SUSPEND
                   : ZR_CORE_TASK_FRAME_POLL_FAULT;
}

/* 记录失败 StoreSlot 的 drop 是否仍能观察到该 slot 的原始对象。 */
/* slotLayout 注册的回滚观察器；只比较地址，不接管值/root，也不释放 context.object。 */
static void task_frame_count_root_failure_drop(
        SZrState *state, SZrTypeValue *value, TZrPtr userData) {
    SZrTaskFrameRootFailureContext *context =
            (SZrTaskFrameRootFailureContext *)userData;

    ZR_UNUSED_PARAMETER(state);
    if (context != ZR_NULL) {
        context->dropCount++;
        context->dropObservedCopiedObject =
                value != ZR_NULL && value->isGarbageCollectable &&
                value->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(context->object);
    }
}

/* unique 结果的原始资源由 task header 接管，Await 后转移给调用方。 */
typedef struct SZrTaskFrameOwnerContext {
    /* 借用资源指针用于创建 unique 值；成功后所有权经 header 移至 Await 输出。 */
    SZrObject *resource;
} SZrTaskFrameOwnerContext;

/* 普通 GC 结果需要在完成到 Await 之间由 task header 保活。 */
typedef struct SZrTaskFrameResultGcContext {
    /* 首次 poll 发布的借用普通对象；完成到 Free 之间由 header 的结果 root 保活。 */
    SZrObject *object;
} SZrTaskFrameResultGcContext;

/* 产出不可复制的 unique 值，验证 Await 仅允许一次所有权转移。 */
/* Start 同步派发；InitUniqueValue 失败不宣布完成，成功产出的 owner 由 runtime 物化到 header。 */
static EZrCoreTaskFramePollOutcome task_frame_complete_unique_result(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    SZrTaskFrameOwnerContext *context = (SZrTaskFrameOwnerContext *)userData;

    ZR_UNUSED_PARAMETER(task);
    if (context == ZR_NULL || context->resource == ZR_NULL ||
        !ZrCore_Ownership_InitUniqueValue(
                state, outResult, ZR_CAST_RAW_OBJECT_AS_SUPER(context->resource))) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }
    return ZR_CORE_TASK_FRAME_POLL_COMPLETE;
}

/* 直接完成并返回 GC 对象，验证没有暂停帧时的结果 root。 */
/* Start 同步派发；不创建 frame，raw GC 输出的保活由 runtime 完成路径注册 root。 */
static EZrCoreTaskFramePollOutcome task_frame_complete_gc_result(
        SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult) {
    SZrTaskFrameResultGcContext *context = (SZrTaskFrameResultGcContext *)userData;

    ZR_UNUSED_PARAMETER(task);
    if (context == ZR_NULL || context->object == ZR_NULL) {
        return ZR_CORE_TASK_FRAME_POLL_FAULT;
    }
    ZrCore_Value_InitAsRawObject(state, outResult, ZR_CAST_RAW_OBJECT_AS_SUPER(context->object));
    return ZR_CORE_TASK_FRAME_POLL_COMPLETE;
}

/* 同步完成走 task header，不能为未发生的暂停分配 frame。 */
/* main 的 RUN_TEST 注册；零 layout 只测试直接完成，不对 Suspend 的合法性作推断。 */
static void test_sync_completion_does_not_allocate_a_frame(void) {
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask task;
    SZrCoreTaskFrameLayout layout = {0};
    SZrTypeValue result;

    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&task);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &task, &pool, &layout, task_frame_sync_complete, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_STATUS_COMPLETED,
                          ZrCore_TaskFrameTask_Status(&task));
    TEST_ASSERT_EQUAL_UINT32(0U, pool.frameAllocationCount);
    TEST_ASSERT_EQUAL_UINT32(0U, pool.activeFrameCount);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_READY,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &result, ZR_NULL));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    ZrCore_TaskFrameTask_Free(g_state, &task);
    ZrCore_TaskFramePool_Free(g_state, &pool);
}

/* 首次暂停才提升到堆帧，后续恢复复用它并稳定返回可复制结果。 */
/* main 的 RUN_TEST 注册；栈 layout/context 活到 Task_Free，仅检验手动 Resume、单次提升及普通整数重复 Await。 */
static void test_pending_task_promotes_once_and_resumes_multiple_states(void) {
    SZrCoreTaskFrameSlotLayout slotLayout = {ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL};
    SZrCoreTaskFrameLayout layout = {3U, 1U, &slotLayout};
    SZrTaskFrameSuspendContext context = {0};
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask task;
    SZrTypeValue result;

    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&task);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &task, &pool, &layout, task_frame_multi_suspend, &context));
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_STATUS_SUSPENDED,
                          ZrCore_TaskFrameTask_Status(&task));
    TEST_ASSERT_EQUAL_UINT32(1U, pool.frameAllocationCount);
    TEST_ASSERT_EQUAL_UINT32(1U, pool.activeFrameCount);
    TEST_ASSERT_EQUAL_UINT32(1U, ZrCore_TaskFrameTask_State(&task));
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_PENDING,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &result, ZR_NULL));

    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Resume(g_state, &task));
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_STATUS_SUSPENDED,
                          ZrCore_TaskFrameTask_Status(&task));
    TEST_ASSERT_EQUAL_UINT32(2U, ZrCore_TaskFrameTask_State(&task));
    TEST_ASSERT_EQUAL_INT64(20, context.observedValue);
    TEST_ASSERT_EQUAL_UINT32(1U, pool.frameAllocationCount);

    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Resume(g_state, &task));
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_STATUS_COMPLETED,
                          ZrCore_TaskFrameTask_Status(&task));
    TEST_ASSERT_EQUAL_UINT32(0U, pool.activeFrameCount);
    TEST_ASSERT_EQUAL_UINT32(1U, pool.pooledFrameCount);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_READY,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &result, ZR_NULL));
    TEST_ASSERT_EQUAL_INT64(22, result.value.nativeObject.nativeInt64);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_READY,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &result, ZR_NULL));
    TEST_ASSERT_EQUAL_INT64(22, result.value.nativeObject.nativeInt64);
    ZrCore_TaskFrameTask_Free(g_state, &task);
    ZrCore_TaskFramePool_Free(g_state, &pool);
}

/* 故障先执行 finally 再清理已初始化 slot，且重写 slot 只额外 drop 一次。 */
/* main 的 RUN_TEST 注册；slot 0 被重写、slot 1 未初始化，finally 观察发生在终态 drop 之前。 */
static void test_fault_cleans_only_initialized_drop_slots(void) {
    SZrTaskFrameFaultContext context = {0};
    SZrCoreTaskFrameSlotLayout slots[2] = {
            {ZR_FALSE, ZR_TRUE, task_frame_count_drop, &context},
            {ZR_FALSE, ZR_TRUE, task_frame_count_drop, &context}};
    SZrCoreTaskFrameLayout layout = {
            2U, 2U, slots, task_frame_count_finally, &context};
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask task;
    SZrTypeValue result;
    SZrTypeValue error;

    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&task);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &task, &pool, &layout, task_frame_fault_after_suspend, &context));
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Resume(g_state, &task));
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_STATUS_FAULTED,
                          ZrCore_TaskFrameTask_Status(&task));
    TEST_ASSERT_EQUAL_UINT32(1U, context.finallyCount);
    TEST_ASSERT_TRUE(context.finallyObservedLiveSlot);
    TEST_ASSERT_EQUAL_UINT32(2U, context.dropCount);
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_Value_ResetAsNull(&error);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_FAULTED,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &result, &error));
    TEST_ASSERT_EQUAL_INT64(99, error.value.nativeObject.nativeInt64);
    ZrCore_TaskFrameTask_Free(g_state, &task);
    TEST_ASSERT_EQUAL_UINT32(1U, context.finallyCount);
    ZrCore_TaskFramePool_Free(g_state, &pool);
}

/* compact GC 后仍能读取暂停 slot；归还的 frame 在下个任务中复用。 */
/* main 的 RUN_TEST 注册；LoadSlot 解析根句柄后检查非空，未断言对象移动距离或完整字段内容。 */
static void test_gc_map_roots_suspended_values_and_reuses_frame_pool(void) {
    SZrCoreTaskFrameSlotLayout slotLayout = {ZR_TRUE, ZR_FALSE, ZR_NULL, ZR_NULL};
    SZrCoreTaskFrameLayout layout = {2U, 1U, &slotLayout};
    SZrTaskFrameGcContext firstContext = {task_frame_create_object("FrameRoot", ZR_FALSE), 0U};
    SZrTaskFrameGcContext secondContext = {ZR_NULL, 0U};
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask first;
    SZrCoreTaskFrameTask second;
    SZrTypeValue rooted;

    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&first);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &first, &pool, &layout, task_frame_suspend_gc_value, &firstContext));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&rooted);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_LoadSlot(g_state, &first, 0U, &rooted));
    TEST_ASSERT_TRUE(rooted.isGarbageCollectable);
    TEST_ASSERT_NOT_NULL(rooted.value.object);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Resume(g_state, &first));
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_TaskFrameTask_Free(g_state, &first);

    secondContext.object = task_frame_create_object("FrameRootReuse", ZR_FALSE);
    TEST_ASSERT_NOT_NULL(secondContext.object);
    ZrCore_TaskFrameTask_Init(&second);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &second, &pool, &layout, task_frame_suspend_gc_value, &secondContext));
    TEST_ASSERT_EQUAL_UINT32(1U, pool.frameAllocationCount);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Resume(g_state, &second));
    ZrCore_TaskFrameTask_Free(g_state, &second);
    ZrCore_TaskFramePool_Free(g_state, &pool);
}

/* 根表扩容失败后必须回滚 slot 并恰好调用一次其已注册的 drop。 */
/* main 的 RUN_TEST 注册；在已有根槽全部占用后拒绝扩容，成功填入的根必须逐个 Release。 */
static void test_gc_root_registration_failure_drops_copied_slot_once(void) {
    /* 有界填表上限，达到它仍未拒绝即由断言失败，不能当作注入成功。 */
    enum { TASK_FRAME_ROOT_FILLER_CAPACITY = 4096 };
    /* 静态句柄数组只存本例已成功注册的根；以 fillerCount 为有效范围，不拥有独立对象。 */
    static SZrGcRootHandle fillerRoots[TASK_FRAME_ROOT_FILLER_CAPACITY];
    SZrTaskFrameRootFailureContext context = {
            task_frame_create_object("FrameRootFailure", ZR_FALSE), 0U, ZR_FALSE};
    SZrCoreTaskFrameSlotLayout slotLayout = {
            ZR_TRUE, ZR_TRUE, task_frame_count_root_failure_drop, &context};
    SZrCoreTaskFrameLayout layout = {2U, 1U, &slotLayout};
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask task;
    TZrUInt32 fillerCount = 0U;
    TZrUInt32 rootCountBefore = (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state);

    TEST_ASSERT_NOT_NULL(context.object);
    /* 保存配套函数/参数后才替换；wrapper 留到 tearDown，避免其 userData 指向已退出的用例栈。 */
    g_taskFrameAllocatorFailure.upstreamAllocator = g_state->global->upstreamAllocator;
    g_taskFrameAllocatorFailure.upstreamAllocationArguments =
            g_state->global->upstreamAllocationArguments;
    g_state->global->upstreamAllocator = task_frame_fail_next_array_allocation;
    g_state->global->upstreamAllocationArguments = &g_taskFrameAllocatorFailure;

    /* 第一次拒绝发生于 Create 的根表扩容，故失败后所有现存槽正好填满。 */
    g_taskFrameAllocatorFailure.rejectNextArrayAllocation = ZR_TRUE;
    while (fillerCount < TASK_FRAME_ROOT_FILLER_CAPACITY &&
           ZrCore_GcRootHandle_Create(
                   g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(context.object),
                   &fillerRoots[fillerCount])) {
        fillerCount++;
    }
    TEST_ASSERT_TRUE(fillerCount < TASK_FRAME_ROOT_FILLER_CAPACITY);
    TEST_ASSERT_EQUAL_UINT32(1U,
                             g_taskFrameAllocatorFailure.rejectedArrayAllocationCount);
    TEST_ASSERT_EQUAL_UINT32(rootCountBefore + fillerCount,
                             (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));

    /* 此处新建 frame 使用宿主 calloc；随后 StoreSlot 的根表扩容触发已装配的拒绝器。 */
    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&task);
    g_taskFrameAllocatorFailure.rejectNextArrayAllocation = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &task, &pool, &layout,
            task_frame_store_gc_value_after_root_table_full, &context));
    TEST_ASSERT_EQUAL_UINT32(2U,
                             g_taskFrameAllocatorFailure.rejectedArrayAllocationCount);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_STATUS_FAULTED,
                          ZrCore_TaskFrameTask_Status(&task));
    TEST_ASSERT_EQUAL_UINT32(1U, context.dropCount);
    TEST_ASSERT_TRUE(context.dropObservedCopiedObject);

    /* Fault、重复 Free 和池销毁都不能再次运行已经完成的 drop。 */
    ZrCore_TaskFrameTask_Free(g_state, &task);
    TEST_ASSERT_EQUAL_UINT32(1U, context.dropCount);
    ZrCore_TaskFrameTask_Free(g_state, &task);
    ZrCore_TaskFramePool_Free(g_state, &pool);
    TEST_ASSERT_EQUAL_UINT32(1U, context.dropCount);
    ZrCore_TaskFramePool_Free(g_state, &pool);
    TEST_ASSERT_EQUAL_UINT32(1U, context.dropCount);

    /* 撤销拒绝开关后逆序释放成功注册的 filler 根，恢复入例前根计数。 */
    g_taskFrameAllocatorFailure.rejectNextArrayAllocation = ZR_FALSE;
    while (fillerCount > 0U) {
        fillerCount--;
        ZrCore_GcRootHandle_Release(g_state, &fillerRoots[fillerCount]);
    }
    TEST_ASSERT_EQUAL_UINT32(rootCountBefore,
                             (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
}

/* unique 结果第一次 Await 转移所有权，第二次须报告已消费。 */
/* main 的 RUN_TEST 注册；unique 第一次 Await 的输出属于用例，必须先 ReleaseValue 再释放 task。 */
static void test_non_copy_result_transfers_once(void) {
    SZrTaskFrameOwnerContext context = {task_frame_create_object("FrameUnique", ZR_TRUE)};
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask task;
    SZrCoreTaskFrameLayout layout = {0};
    SZrTypeValue firstResult;
    SZrTypeValue secondResult;

    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&task);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &task, &pool, &layout, task_frame_complete_unique_result, &context));
    ZrCore_Value_ResetAsNull(&firstResult);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_READY,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &firstResult, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_UNIQUE, firstResult.ownershipKind);
    ZrCore_Value_ResetAsNull(&secondResult);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_RESULT_CONSUMED,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &secondResult, ZR_NULL));
    ZrCore_Ownership_ReleaseValue(g_state, &firstResult);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)ZrCore_GcDomain_GetOwnershipRootCount(g_state));
    ZrCore_TaskFrameTask_Free(g_state, &task);
    ZrCore_TaskFramePool_Free(g_state, &pool);
}

/* 完成但未 Await 的结果也必须跨 compact GC 保留有效对象。 */
/* main 的 RUN_TEST 注册；仅断言结果非空及域归属，并在 Free 后检查 header root 已释放。 */
static void test_completed_task_header_roots_gc_result_until_await(void) {
    SZrTaskFrameResultGcContext context = {
            task_frame_create_object("CompletedTaskResult", ZR_FALSE)};
    SZrCoreTaskFramePool pool;
    SZrCoreTaskFrameTask task;
    SZrCoreTaskFrameLayout layout = {0};
    SZrTypeValue result;

    ZrCore_TaskFramePool_Init(&pool);
    ZrCore_TaskFrameTask_Init(&task);
    TEST_ASSERT_TRUE(ZrCore_TaskFrameTask_Start(
            g_state, &task, &pool, &layout, task_frame_complete_gc_result, &context));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    /* TODO：核对移动 GC 是否实际迁移本对象及 Await 对 header.result 的地址更新；
     * 当前断言只有非空和域戳，不足以证明移动后全部对象内容仍有效。 */
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_EQUAL_INT(ZR_CORE_TASK_FRAME_AWAIT_READY,
                          ZrCore_TaskFrameTask_Await(g_state, &task, &result, ZR_NULL));
    TEST_ASSERT_TRUE(result.isGarbageCollectable);
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(g_state, result.value.object));
    ZrCore_TaskFrameTask_Free(g_state, &task);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_TaskFramePool_Free(g_state, &pool);
}

/* CMake 单独编译本入口，Unity 按注册顺序串行运行七例并由 UNITY_END 返回断言汇总。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sync_completion_does_not_allocate_a_frame);
    RUN_TEST(test_pending_task_promotes_once_and_resumes_multiple_states);
    RUN_TEST(test_fault_cleans_only_initialized_drop_slots);
    RUN_TEST(test_gc_map_roots_suspended_values_and_reuses_frame_pool);
    RUN_TEST(test_gc_root_registration_failure_drops_copied_slot_once);
    RUN_TEST(test_non_copy_result_transfers_once);
    RUN_TEST(test_completed_task_header_roots_gc_result_until_await);
    return UNITY_END();
}
