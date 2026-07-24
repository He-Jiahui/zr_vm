#include "unity.h"
#include "test_support.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/task_frame_runtime.h"
#include "zr_vm_core/value.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

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

typedef struct SZrTaskFrameSuspendContext {
    TZrUInt32 invocationCount;
    TZrInt64 observedValue;
} SZrTaskFrameSuspendContext;

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

typedef struct SZrTaskFrameFaultContext {
    TZrUInt32 invocationCount;
    TZrUInt32 dropCount;
    TZrUInt32 finallyCount;
    TZrBool finallyObservedLiveSlot;
} SZrTaskFrameFaultContext;

static void task_frame_count_drop(SZrState *state, SZrTypeValue *value, TZrPtr userData) {
    SZrTaskFrameFaultContext *context = (SZrTaskFrameFaultContext *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(value);
    if (context != ZR_NULL) {
        context->dropCount++;
    }
}

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

typedef struct SZrTaskFrameGcContext {
    SZrObject *object;
    TZrUInt32 invocationCount;
} SZrTaskFrameGcContext;

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

typedef struct SZrTaskFrameOwnerContext {
    SZrObject *resource;
} SZrTaskFrameOwnerContext;

typedef struct SZrTaskFrameResultGcContext {
    SZrObject *object;
} SZrTaskFrameResultGcContext;

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sync_completion_does_not_allocate_a_frame);
    RUN_TEST(test_pending_task_promotes_once_and_resumes_multiple_states);
    RUN_TEST(test_fault_cleans_only_initialized_drop_slots);
    RUN_TEST(test_gc_map_roots_suspended_values_and_reuses_frame_pool);
    RUN_TEST(test_non_copy_result_transfers_once);
    RUN_TEST(test_completed_task_header_roots_gc_result_until_await);
    return UNITY_END();
}
