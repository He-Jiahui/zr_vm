#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_library/task_runtime.h"

/* Compile the private implementation unchanged to exercise rejection and OOM
 * boundaries without exposing test entry points in the product ABI. */
static const TZrChar *kTaskQueueField = "__zr_task_queue";
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
#include "../../zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc"

static SZrGcRootHandle queue_reuse_saved_task;
static TZrBool queue_reuse_task_saved;
static SZrGcRootHandle queue_reuse_saved_job;
static SZrGcRootHandle queue_reuse_saved_scheduler;
static TZrBool queue_reuse_job_saved;
static TZrBool queue_reuse_scheduler_saved;
static TZrBool queue_reuse_inner_was_queued;

static TZrBool queue_reuse_capture_job(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *job = ZR_NULL;
    SZrObject *scheduler = ZR_NULL;
    if (!ZrLib_CallContext_ReadObject(context, 0u, &job) ||
        !ZrLib_CallContext_ReadObject(context, 1u, &scheduler)) return ZR_FALSE;
    queue_reuse_job_saved = ZrCore_GcRootHandle_Create(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(job), &queue_reuse_saved_job);
    queue_reuse_scheduler_saved = queue_reuse_job_saved &&
            ZrCore_GcRootHandle_Create(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(scheduler), &queue_reuse_saved_scheduler);
    ZrLib_Value_SetInt(context->state, result, 1);
    return queue_reuse_scheduler_saved;
}

static TZrBool queue_reuse_capture_task(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *task = ZR_NULL;
    if (queue_reuse_task_saved || !ZrLib_CallContext_ReadObject(context, 0u, &task) ||
        !ZrCore_GcRootHandle_Create(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(task), &queue_reuse_saved_task)) {
        return ZR_FALSE;
    }
    queue_reuse_task_saved = ZR_TRUE;
    ZrLib_Value_SetInt(context->state, result, 1);
    return ZR_TRUE;
}

static TZrBool queue_reuse_get_task(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrRawObject *task = ZR_NULL;
    if (!queue_reuse_task_saved ||
        !ZrCore_GcRootHandle_Resolve(context->state, &queue_reuse_saved_task, &task)) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, ZR_CAST_OBJECT(context->state, task), ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

static TZrInt64 queue_reuse_saved_status(SZrState *state);

static TZrBool queue_reuse_enqueue_inner(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrRawObject *job = ZR_NULL;
    SZrRawObject *scheduler = ZR_NULL;
    SZrTypeValue task;
    if (!ZrCore_GcRootHandle_Resolve(context->state, &queue_reuse_saved_job, &job) ||
        !ZrCore_GcRootHandle_Resolve(context->state, &queue_reuse_saved_scheduler, &scheduler) ||
        !ZrLibrary_TaskRuntime_ScheduleJob(context->state, ZR_CAST_OBJECT(context->state, scheduler),
                                         ZR_CAST_OBJECT(context->state, job), &task)) return ZR_FALSE;
    queue_reuse_task_saved = ZrCore_GcRootHandle_Create(context->state, task.value.object, &queue_reuse_saved_task);
    queue_reuse_inner_was_queued = queue_reuse_task_saved && queue_reuse_saved_status(context->state) == ZR_VM_TASK_STATUS_QUEUED;
    ZrLib_Value_SetInt(context->state, result, 13);
    return queue_reuse_task_saved;
}

static const ZrLibParameterDescriptor queue_reuse_capture_parameters[] = {
        {.name = "task", .typeName = "object"},
};
static const ZrLibParameterDescriptor queue_reuse_job_parameters[] = {
        {.name = "job", .typeName = "zr.task.Job<int>"},
        {.name = "scheduler", .typeName = "zr.task.Scheduler"},
};
static const ZrLibFunctionDescriptor queue_reuse_host_functions[] = {
        {.name = "captureTask", .minArgumentCount = 1u, .maxArgumentCount = 1u,
         .callback = queue_reuse_capture_task, .returnTypeName = "int",
         .parameters = queue_reuse_capture_parameters, .parameterCount = 1u},
        {.name = "savedTask", .minArgumentCount = 0u, .maxArgumentCount = 0u,
         .callback = queue_reuse_get_task, .returnTypeName = "zr.task.Task<int>"},
        {.name = "captureJob", .minArgumentCount = 2u, .maxArgumentCount = 2u,
         .callback = queue_reuse_capture_job, .returnTypeName = "int",
         .parameters = queue_reuse_job_parameters, .parameterCount = 2u},
        {.name = "enqueueInner", .minArgumentCount = 0u, .maxArgumentCount = 0u,
         .callback = queue_reuse_enqueue_inner, .returnTypeName = "int"},
};
static const ZrLibModuleDescriptor queue_reuse_host_module = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "task.queue.test",
        .functions = queue_reuse_host_functions, .functionCount = 4u,
        .moduleVersion = "1.0.0", .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

static TZrInt64 queue_reuse_saved_status(SZrState *state) {
    SZrRawObject *raw = ZR_NULL;
    SZrObject *task;
    TZrSize bucket;
    if (!queue_reuse_task_saved || !ZrCore_GcRootHandle_Resolve(state, &queue_reuse_saved_task, &raw)) return -1;
    task = ZR_CAST_OBJECT(state, raw);
    for (bucket = 0; bucket < task->nodeMap.capacity; ++bucket) {
        SZrHashKeyValuePair *pair;
        for (pair = task->nodeMap.buckets[bucket]; pair != ZR_NULL; pair = pair->next) {
            const SZrString *key;
            if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) continue;
            key = (const SZrString *)pair->key.value.object;
            if (ZrCore_String_GetByteLength(key) == strlen("__zr_task_status") &&
                memcmp(ZrCore_String_GetNativeString(key), "__zr_task_status", strlen("__zr_task_status")) == 0 &&
                ZR_VALUE_IS_TYPE_INT(pair->value.type)) return pair->value.value.nativeObject.nativeInt64;
        }
    }
    return -1;
}

static void test_scheduler_reuse_preserves_completed_job_result(void) {
    static const char *firstSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
            "var firstJob = init task.Job<int>(fn() => { return 7; });\n"
            "var first = task.currentScheduler.schedule<int>(firstJob);\n"
            "var value = first.result();\n"
            "var captured = host.captureTask(first);\n"
            "return value;\n";
    static const char *secondSource =
            "var task = import(\"zr.task\");\n"
            "var secondJob = init task.Job<int>(fn() => { return 9; });\n"
            "var second = task.currentScheduler.schedule<int>(secondJob);\n"
            "return second.result();\n";
    static const char *lastSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
            "var first = host.savedTask();\n"
            "return first.result();\n";
    SZrState *state = create_task_job_scheduler_test_state();
    SZrFunction *function;
    TZrBool created = state != ZR_NULL;
    TZrBool firstReady = ZR_FALSE;
    TZrBool secondReady = ZR_FALSE;
    TZrBool afterRead = ZR_FALSE;
    TZrInt64 before = 0;
    TZrInt64 secondResult = 0;
    TZrInt64 after = 0;
    TZrInt64 beforeStatus = -1;
    TZrInt64 afterStatus = -1;
    TZrBool registered = ZR_FALSE;

    memset(&queue_reuse_saved_task, 0, sizeof(queue_reuse_saved_task));
    queue_reuse_task_saved = ZR_FALSE;

    if (created) {
        registered = ZrLibrary_NativeRegistry_RegisterModule(state->global, &queue_reuse_host_module);
        function = compile_task_job_scheduler_source(state, firstSource, "task_scheduler_first_job.zr");
        firstReady = registered && function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &before);
        if (firstReady) {
            beforeStatus = queue_reuse_saved_status(state);
            function = compile_task_job_scheduler_source(state, secondSource, "task_scheduler_second_job.zr");
            secondReady = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &secondResult);
            afterStatus = queue_reuse_saved_status(state);
            if (secondReady && afterStatus == ZR_VM_TASK_STATUS_COMPLETED) {
                function = compile_task_job_scheduler_source(state, lastSource, "task_scheduler_saved_job.zr");
                afterRead = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &after);
            }
        }
        if (queue_reuse_task_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_task);
        queue_reuse_task_saved = ZR_FALSE;
    }
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE(created);
    TEST_ASSERT_TRUE(registered && firstReady);
    TEST_ASSERT_EQUAL_INT64(7, before);
    TEST_ASSERT_EQUAL_INT64(ZR_VM_TASK_STATUS_COMPLETED, beforeStatus);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(ZR_VM_TASK_STATUS_COMPLETED, afterStatus, "new scheduling replayed the old completed Task");
    TEST_ASSERT_TRUE(secondReady && afterRead);
    TEST_ASSERT_EQUAL_INT64(9, secondResult);
    TEST_ASSERT_EQUAL_INT64(7, after);
}

static void test_scheduler_keeps_reentrant_enqueue_until_outer_job_returns(void) {
    static const char *innerSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
            "var job = init task.Job<int>(fn() => { return 19; });\n"
            "return host.captureJob(job, task.currentScheduler);\n";
    static const char *outerSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
            "var outerJob = init task.Job<int>(fn() => {\n"
            "  return host.enqueueInner();\n"
            "});\n"
            "var outer = task.currentScheduler.schedule<int>(outerJob);\n"
            "return outer.result();\n";
    static const char *resultSource =
            "var host = import(\"task.queue.test\");\n"
            "var inner = host.savedTask();\n"
            "return inner.result();\n";
    SZrState *state = create_task_job_scheduler_test_state();
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;
    TZrBool executed = ZR_FALSE;
    TZrBool outerExecuted = ZR_FALSE;
    TZrInt64 outerResult = 0;
    TZrBool queued = ZR_FALSE;
    queue_reuse_task_saved = ZR_FALSE;
    queue_reuse_job_saved = queue_reuse_scheduler_saved = ZR_FALSE;
    queue_reuse_inner_was_queued = ZR_FALSE;
    if (state != ZR_NULL && ZrLibrary_NativeRegistry_RegisterModule(state->global, &queue_reuse_host_module)) {
        function = compile_task_job_scheduler_source(state, innerSource, "task_scheduler_reentrant_inner.zr");
        executed = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &result);
        if (executed) {
            function = compile_task_job_scheduler_source(state, outerSource, "task_scheduler_reentrant_outer.zr");
            outerExecuted = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &outerResult);
            queued = queue_reuse_inner_was_queued;
            if (outerExecuted) {
                function = compile_task_job_scheduler_source(state, resultSource, "task_scheduler_reentrant_result.zr");
                executed = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &result);
            }
        }
        if (queue_reuse_task_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_task);
        if (queue_reuse_job_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_job);
        if (queue_reuse_scheduler_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_scheduler);
    }
    queue_reuse_task_saved = ZR_FALSE;
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE_MESSAGE(executed, "reentrant scheduler source failed");
    TEST_ASSERT_TRUE(outerExecuted && queued);
    TEST_ASSERT_EQUAL_INT64(13, outerResult);
    TEST_ASSERT_EQUAL_INT64(19, result);
}

static void test_scheduler_reuses_queue_for_cooperative_turns(void) {
    static const char *firstSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var first = task.currentScheduler.schedule<int>(job);\n"
            "var value = first.result();\n"
            "var captured = host.captureTask(first);\n"
            "return value;\n";
    static const char *yieldSource =
            "var task = import(\"zr.task\");\n"
            "var yielded = task.yieldNow();\n"
            "yielded.result();\n"
            "return 23;\n";
    SZrState *state = create_task_job_scheduler_test_state();
    TZrBool ready = ZR_FALSE;
    TZrBool yielded = ZR_FALSE;
    TZrBool delayed = ZR_FALSE;
    TZrInt64 first = 0;
    TZrInt64 yieldResult = 0;
    TZrInt64 status = -1;
    queue_reuse_task_saved = ZR_FALSE;
    if (state != ZR_NULL && ZrLibrary_NativeRegistry_RegisterModule(state->global, &queue_reuse_host_module)) {
        SZrFunction *function = compile_task_job_scheduler_source(state, firstSource, "task_scheduler_before_cooperative.zr");
        ready = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &first);
        if (ready) {
            const ZrLibModuleDescriptor *module;
            const ZrLibFunctionDescriptor *delay;
            const ZrLibMethodDescriptor *resultMethod;
            ZrLibCallContext context = {0};
            SZrTypeValue turns;
            SZrTypeValue task;
            SZrTypeValue result;
            function = compile_task_job_scheduler_source(state, yieldSource, "task_scheduler_repeated_yield.zr");
            yielded = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &yieldResult);
            module = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
            delay = find_function_descriptor(module, "delay");
            resultMethod = find_method_descriptor(find_type_descriptor(module, "Task"), "result");
            ZrLib_Value_SetInt(state, &turns, 3);
            context.state = state;
            context.argumentValues = &turns;
            context.argumentCount = 1u;
            if (yielded && delay != ZR_NULL && resultMethod != ZR_NULL && delay->callback(&context, &task)) {
                context.selfValue = &task;
                context.argumentValues = ZR_NULL;
                context.argumentCount = 0u;
                delayed = resultMethod->callback(&context, &result) && result.type == ZR_VALUE_TYPE_NULL;
            }
            status = queue_reuse_saved_status(state);
        }
        if (queue_reuse_task_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_task);
    }
    queue_reuse_task_saved = ZR_FALSE;
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE(ready && yielded && delayed);
    TEST_ASSERT_EQUAL_INT64(7, first);
    TEST_ASSERT_EQUAL_INT64(23, yieldResult);
    TEST_ASSERT_EQUAL_INT64(ZR_VM_TASK_STATUS_COMPLETED, status);
}

/* These objects use real hash pairs, member descriptors and core setters. */
static SZrObject *queue_reuse_private_scheduler(SZrState *state, const char *readonlyField,
                                               TZrBool withQueue, SZrGcRootHandle *root,
                                               TZrBool *rootCreated) {
    SZrObject *scheduler = ZrCore_Object_New(state, ZR_NULL);
    SZrObjectPrototype *prototype;
    SZrTypeValue value;
    SZrMemberDescriptor descriptor = {0};
    *rootCreated = scheduler != ZR_NULL && ZrCore_GcRootHandle_Create(state, ZR_CAST_RAW_OBJECT_AS_SUPER(scheduler), root);
    if (!*rootCreated) return ZR_NULL;
    ZrLib_Value_SetInt(state, &value, 5);
    ZrLib_Object_SetFieldCString(state, scheduler, kTaskQueueHeadField, &value);
    ZrLib_Value_SetNull(&value);
    if (withQueue) {
        SZrObject *queue = ZrLib_Array_New(state);
        if (queue == ZR_NULL) return ZR_NULL;
        ZrLib_Value_SetObject(state, &value, queue, ZR_VALUE_TYPE_ARRAY);
    }
    ZrLib_Object_SetFieldCString(state, scheduler, kTaskQueueField, &value);
    if (readonlyField != ZR_NULL) {
        SZrString *name = ZrCore_String_Create(state, "queue-test", strlen("queue-test"));
        prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        descriptor.name = ZrCore_String_Create(state, (TZrNativeString)readonlyField, strlen(readonlyField));
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        if (prototype == ZR_NULL || descriptor.name == ZR_NULL ||
            !ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor)) return ZR_NULL;
        scheduler->prototype = prototype;
    }
    return scheduler;
}

static void test_scheduler_checked_fields_reject_writes_without_replaying(void) {
    TZrUInt32 mode;
    TZrBool outcomes[4] = {ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE};
    for (mode = 0; mode < 4u; ++mode) {
        SZrState *state = create_task_job_scheduler_test_state();
        SZrGcRootHandle root;
        TZrBool rooted = ZR_FALSE;
        SZrObject *scheduler = state == ZR_NULL ? ZR_NULL : queue_reuse_private_scheduler(
                state, mode == 0u ? kTaskQueueHeadField : mode < 3u ? kTaskQueueField : ZR_NULL,
                mode == 1u, &root, &rooted);
        if (scheduler != ZR_NULL) {
            SZrHashKeyValuePair *head = task_runtime_scheduler_pair(scheduler, kTaskQueueHeadField);
            SZrHashKeyValuePair *pair = task_runtime_scheduler_pair(scheduler, kTaskQueueField);
            TZrUInt32 version = scheduler->memberVersion;
            TZrUInt32 pins = scheduler->super.garbageCollectMark.pinFlags;
            TZrBool ignored = ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super);
            TZrSize ignoredCount = state->global->garbageCollector->ignoredObjectCount;
            if (mode == 1u) {
                outcomes[mode] = !task_runtime_scheduler_detach_queue(state, scheduler, ZR_CAST_OBJECT(state, pair->value.value.object)) &&
                        pair->value.type == ZR_VALUE_TYPE_ARRAY && head->value.value.nativeObject.nativeInt64 == 5 &&
                        scheduler->memberVersion == version;
            } else {
                SZrObject *queue = task_runtime_scheduler_queue(state, scheduler);
                outcomes[mode] = mode == 3u ? queue != ZR_NULL && pair->value.value.object == &queue->super :
                        queue == ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NULL;
                outcomes[mode] = outcomes[mode] && head->value.value.nativeObject.nativeInt64 == (mode == 0u ? 5 : 0) &&
                        scheduler->super.garbageCollectMark.pinFlags == pins &&
                        ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super) == ignored &&
                        state->global->garbageCollector->ignoredObjectCount == ignoredCount &&
                        scheduler->memberVersion == version + (mode == 0u ? 0u : mode == 2u ? 1u : 2u);
            }
        }
        if (rooted) ZrCore_GcRootHandle_Release(state, &root);
        ZrTests_State_Destroy(state);
    }
    TEST_ASSERT_TRUE_MESSAGE(outcomes[0], "rejected head reset changed fields");
    TEST_ASSERT_TRUE_MESSAGE(outcomes[1], "rejected detach lost queue or reset head");
    TEST_ASSERT_TRUE_MESSAGE(outcomes[2], "rejected attach published queue or leaked scheduler pin");
    TEST_ASSERT_TRUE_MESSAGE(outcomes[3], "successful attach did not update versions or release pin");
}

typedef struct SZrQueueReuseAllocationFailure {
    FZrAllocator upstream;
    TZrPtr argument;
    TZrUInt32 failures;
    TZrBool reject;
} SZrQueueReuseAllocationFailure;

static TZrPtr queue_reuse_fail_queue_allocation(TZrPtr argument, TZrPtr pointer,
                                               TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    SZrQueueReuseAllocationFailure *failure = (SZrQueueReuseAllocationFailure *)argument;
    /* Reject the request and its GC retry; allow Error normalization to allocate. */
    if (failure->reject && failure->failures < 2u && pointer == ZR_NULL &&
        newSize == sizeof(SZrObject) && flag == ZR_MEMORY_NATIVE_TYPE_OBJECT) {
        ++failure->failures;
        return ZR_NULL;
    }
    return failure->upstream(failure->argument, pointer, originalSize, newSize, flag);
}

static void test_scheduler_queue_oom_releases_pin_and_can_retry(void) {
    SZrState *state = create_task_job_scheduler_test_state();
    SZrGcRootHandle root;
    TZrBool rooted = ZR_FALSE;
    SZrObject *scheduler = state == ZR_NULL ? ZR_NULL : queue_reuse_private_scheduler(state, ZR_NULL, ZR_FALSE, &root, &rooted);
    SZrQueueReuseAllocationFailure failure = {0};
    TZrBool rejected = ZR_FALSE;
    TZrBool cleaned = ZR_FALSE;
    TZrBool retried = ZR_FALSE;
    if (scheduler != ZR_NULL) {
        TZrUInt32 pins = scheduler->super.garbageCollectMark.pinFlags;
        TZrBool ignored = ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super);
        failure.upstream = state->global->upstreamAllocator;
        failure.argument = state->global->upstreamAllocationArguments;
        failure.reject = ZR_TRUE;
        state->global->upstreamAllocator = queue_reuse_fail_queue_allocation;
        state->global->upstreamAllocationArguments = &failure;
        rejected = task_runtime_scheduler_queue(state, scheduler) == ZR_NULL &&
                failure.failures == 2u && state->threadStatus == ZR_THREAD_STATUS_MEMORY_ERROR;
        failure.reject = ZR_FALSE;
        state->global->upstreamAllocator = failure.upstream;
        state->global->upstreamAllocationArguments = failure.argument;
        cleaned = scheduler->super.garbageCollectMark.pinFlags == pins &&
                ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super) == ignored &&
                task_runtime_scheduler_pair(scheduler, kTaskQueueField)->value.type == ZR_VALUE_TYPE_NULL;
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        retried = task_runtime_scheduler_queue(state, scheduler) != ZR_NULL;
    }
    if (rooted) ZrCore_GcRootHandle_Release(state, &root);
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE_MESSAGE(rejected, "queue allocation OOM was not observed");
    TEST_ASSERT_TRUE_MESSAGE(cleaned, "queue OOM leaked pin or published queue");
    TEST_ASSERT_TRUE_MESSAGE(retried, "queue creation could not retry after OOM");
}
