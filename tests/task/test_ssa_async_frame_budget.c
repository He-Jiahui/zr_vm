/* 不构造完整 VM 或 scheduler，直接检查 async frame budget、wait 与 compile queue。 */
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/async_frame_budget.h"

static unsigned int g_testFailureCount;

static void test_record_failure(const char *expression,
                                const char *file,
                                int line) {
    g_testFailureCount++;
    (void)fprintf(stderr, "%s:%d: test check failed: %s\n",
                  file, line, expression);
}

/* This check always evaluates its expression under NDEBUG. */
#define TEST_CHECK(expression) \
    do { \
        if (!(expression)) { \
            test_record_failure(#expression, __FILE__, __LINE__); \
            goto cleanup; \
        } \
    } while (0)

static void test_cleanup_wait_handle_if_active(
        SZrAsyncWaitHandle *handle,
        TZrBool *handleActive,
        SZrAsyncFrameDiagnostic *diagnostic) {
    EZrAsyncWaitState state;

    if (handleActive == ZR_NULL || !*handleActive) {
        return;
    }

    state = ZrCore_AsyncWait_State(handle);
    if (state == ZR_ASYNC_WAIT_STATE_REGISTERING ||
        state == ZR_ASYNC_WAIT_STATE_WAITING) {
        (void)ZrCore_AsyncWait_Cancel(handle, diagnostic);
        state = ZrCore_AsyncWait_State(handle);
    }
    if (state == ZR_ASYNC_WAIT_STATE_READY ||
        state == ZR_ASYNC_WAIT_STATE_CANCELLED ||
        state == ZR_ASYNC_WAIT_STATE_TIMED_OUT) {
        (void)ZrCore_AsyncWait_Resume(handle, diagnostic);
        state = ZrCore_AsyncWait_State(handle);
    }
    if (state == ZR_ASYNC_WAIT_STATE_RESUMED &&
        ZrCore_AsyncWait_Release(handle, diagnostic)) {
        *handleActive = ZR_FALSE;
        return;
    }

    test_record_failure("cleanup must release an active wait handle",
                        __FILE__, __LINE__);
}

static void test_cleanup_compile_handle_if_active(
        SZrCompileJobHandle *handle,
        TZrBool *handleActive,
        TZrBool *workerOwnsSnapshot,
        const SZrCompileQueueRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic) {
    EZrCompileJobState state;

    if (handleActive == ZR_NULL || !*handleActive) {
        return;
    }

    state = ZrCore_CompileQueue_State(handle);
    if (state == ZR_COMPILE_JOB_QUEUED) {
        (void)ZrCore_CompileQueue_Cancel(handle, diagnostic);
        state = ZrCore_CompileQueue_State(handle);
    }
    if (state == ZR_COMPILE_JOB_RUNNING) {
        if (workerOwnsSnapshot == ZR_NULL || !*workerOwnsSnapshot ||
            request == ZR_NULL) {
            test_record_failure("running compile job must retain its worker lease",
                                __FILE__, __LINE__);
            return;
        }
        (void)ZrCore_CompileQueue_Complete(
                handle, request->requestedGeneration, request->moduleHash,
                request->signatureHash, request->layoutHash, 1u, diagnostic);
        *workerOwnsSnapshot = ZR_FALSE;
        state = ZrCore_CompileQueue_State(handle);
    }
    if (state == ZR_COMPILE_JOB_COMPLETED ||
        state == ZR_COMPILE_JOB_CANCELLED ||
        state == ZR_COMPILE_JOB_DISCARDED) {
        if (workerOwnsSnapshot != ZR_NULL) {
            *workerOwnsSnapshot = ZR_FALSE;
        }
    }
    if ((state == ZR_COMPILE_JOB_COMPLETED ||
         state == ZR_COMPILE_JOB_CANCELLED ||
         state == ZR_COMPILE_JOB_DISCARDED) &&
        ZrCore_CompileQueue_Release(handle, diagnostic)) {
        *handleActive = ZR_FALSE;
        return;
    }

    test_record_failure("cleanup must release an active compile handle",
                        __FILE__, __LINE__);
}

/* 工作量耗尽只能在有效 state map 边界暂停，不能切断指令。 */
static void test_frame_budget_only_suspends_at_a_coherent_boundary(void) {
    SZrAsyncFrameBudget frame;
    SZrAsyncFrameDiagnostic diagnostic;

    ZrCore_AsyncFrameBudget_Init(&frame);
    frame.frameId = 17u;
    frame.generation = 4u;
    frame.stateMapId = 9u;
    frame.stateMapGeneration = 4u;
    frame.maxWorkUnits = 3u;
    frame.flags = ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT |
                  ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND |
                  ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID;

    TEST_CHECK(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 2u, ZR_FALSE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_RUNNING);
    /* Exhaustion in the middle of an instruction remains pending. */
    TEST_CHECK(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 2u, ZR_FALSE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_RUNNING);
    TEST_CHECK(frame.status == ZR_ASYNC_FRAME_STATUS_RUNNING);
    /* The next proven state-map boundary is the only legal suspension point. */
    TEST_CHECK(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 0u, ZR_TRUE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_SUSPENDED);
    TEST_CHECK(frame.status == ZR_ASYNC_FRAME_STATUS_SUSPENDED);
    TEST_CHECK(frame.suspendReason == ZR_ASYNC_FRAME_SUSPEND_BUDGET);
    TEST_CHECK(frame.suspensionCount == 1u);

cleanup:
    return;
}

/* 活跃借用阻止暂停，pin 释放后取消和 teardown 可安全完成。 */
static void test_frame_budget_rejects_unsafe_suspend_and_balances_pin(void) {
    SZrAsyncFrameBudget frame;
    SZrAsyncFrameDiagnostic diagnostic;

    ZrCore_AsyncFrameBudget_Init(&frame);
    frame.frameId = 18u;
    frame.generation = 5u;
    frame.stateMapId = 1u;
    frame.stateMapGeneration = 5u;
    frame.maxWorkUnits = 1u;
    frame.flags = ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT |
                  ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND |
                  ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID |
                  ZR_ASYNC_FRAME_FLAG_HAS_BORROW;
    TEST_CHECK(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    TEST_CHECK(!ZrCore_AsyncFrameBudget_CanSuspend(
                   &frame, ZR_TRUE, ZR_FALSE, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND);

    frame.flags &= ~ZR_ASYNC_FRAME_FLAG_HAS_BORROW;
    TEST_CHECK(ZrCore_AsyncFrameBudget_Pin(&frame, &diagnostic));
    TEST_CHECK(frame.pinCount == 1u);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Unpin(&frame, &diagnostic));
    TEST_CHECK(frame.pinCount == 0u);
    TEST_CHECK(ZrCore_AsyncFrameBudget_RequestCancel(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 0u, ZR_TRUE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_CANCELLED);
    TEST_CHECK(frame.status == ZR_ASYNC_FRAME_STATUS_CANCELLED);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));
    TEST_CHECK(frame.status == ZR_ASYNC_FRAME_STATUS_TORN_DOWN);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));

cleanup:
    return;
}

/* 活跃 pin 阻止完成；解除后可按 wait 暂停、恢复并完成。 */
static void test_frame_budget_supports_wait_suspend_and_completion_gates(void) {
    SZrAsyncFrameBudget frame;
    SZrAsyncFrameDiagnostic diagnostic;

    ZrCore_AsyncFrameBudget_Init(&frame);
    frame.frameId = 19u;
    frame.generation = 6u;
    frame.stateMapId = 2u;
    frame.stateMapGeneration = 6u;
    frame.flags = ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT |
                  ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND |
                  ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID;
    TEST_CHECK(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Pin(&frame, &diagnostic));
    TEST_CHECK(!ZrCore_AsyncFrameBudget_Complete(&frame, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Unpin(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Suspend(
                   &frame, ZR_ASYNC_FRAME_SUSPEND_WAIT, ZR_TRUE, ZR_FALSE,
                   &diagnostic));
    TEST_CHECK(frame.status == ZR_ASYNC_FRAME_STATUS_SUSPENDED);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Resume(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Complete(&frame, &diagnostic));
    TEST_CHECK(frame.status == ZR_ASYNC_FRAME_STATUS_COMPLETED);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));

cleanup:
    return;
}

/* Start 前收到的取消请求不得被 Begin 重置。 */
static void test_frame_budget_preserves_prestart_cancellation(void) {
    SZrAsyncFrameBudget frame;
    SZrAsyncFrameDiagnostic diagnostic;

    ZrCore_AsyncFrameBudget_Init(&frame);
    frame.frameId = 20u;
    frame.generation = 6u;
    frame.stateMapId = 3u;
    frame.stateMapGeneration = 6u;
    frame.flags = ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT |
                  ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND |
                  ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID;
    TEST_CHECK(ZrCore_AsyncFrameBudget_RequestCancel(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    TEST_CHECK(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 0u, ZR_TRUE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_CANCELLED);
    TEST_CHECK(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));

cleanup:
    return;
}

/* 注册与 recheck 之间的唤醒不得丢失，且只恢复一次。 */
static void test_wait_registration_recheck_and_wake_resume_once(void) {
    SZrAsyncWaitSlot slots[2];
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrBool registryInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;

    registryInitialized =
            ZrCore_AsyncWaitRegistry_Init(&registry, slots, 2u, &diagnostic);
    TEST_CHECK(registryInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 21u;
    request.generation = 7u;
    request.allowSuspend = ZR_TRUE;
    handleActive = ZrCore_Execution_BeginAsyncWait(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_WAITING);

    /* Wake in the register/recheck window: recheck must not lose it. */
    TEST_CHECK(ZrCore_AsyncWait_Wake(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_Recheck(&handle, ZR_TRUE, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_READY);
    TEST_CHECK(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    TEST_CHECK(!ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&handle) == 1u);
    TEST_CHECK(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;

cleanup:
    if (handleActive) {
        test_cleanup_wait_handle_if_active(&handle, &handleActive, &diagnostic);
    }
    if (registryInitialized) {
        if (handleActive) {
            test_record_failure("wait registry deinit requires released handles",
                                __FILE__, __LINE__);
        } else {
            ZrCore_AsyncWaitRegistry_Deinit(&registry);
            registryInitialized = ZR_FALSE;
        }
    }
}

/* 取消和超时竞态只能有一个终态胜出，之后释放槽位。 */
static void test_wait_cancel_and_timeout_are_single_winners(void) {
    SZrAsyncWaitSlot slot;
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrBool registryInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;

    registryInitialized =
            ZrCore_AsyncWaitRegistry_Init(&registry, &slot, 1u, &diagnostic);
    TEST_CHECK(registryInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 22u;
    request.generation = 8u;
    request.allowSuspend = ZR_TRUE;
    handleActive = ZrCore_Execution_BeginAsyncWait(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(ZrCore_AsyncWait_Cancel(&handle, &diagnostic));
    TEST_CHECK(!ZrCore_AsyncWait_Timeout(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_CANCELLED);
    TEST_CHECK(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;

    handleActive = ZrCore_Execution_BeginAsyncWait(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(ZrCore_AsyncWait_Timeout(&handle, &diagnostic));
    TEST_CHECK(!ZrCore_AsyncWait_Cancel(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_TIMED_OUT);
    TEST_CHECK(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;

cleanup:
    if (handleActive) {
        test_cleanup_wait_handle_if_active(&handle, &handleActive, &diagnostic);
    }
    if (registryInitialized) {
        if (handleActive) {
            test_record_failure("wait registry deinit requires released handles",
                                __FILE__, __LINE__);
        } else {
            ZrCore_AsyncWaitRegistry_Deinit(&registry);
            registryInitialized = ZR_FALSE;
        }
    }
}

/* 栈别名在暂停后会失效，BeginAsyncWait 必须拒绝此请求。 */
static void test_wait_rejects_non_suspendable_resources(void) {
    SZrAsyncWaitSlot slot;
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrBool registryInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;

    registryInitialized =
            ZrCore_AsyncWaitRegistry_Init(&registry, &slot, 1u, &diagnostic);
    TEST_CHECK(registryInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 23u;
    request.generation = 9u;
    request.allowSuspend = ZR_TRUE;
    request.holdsStackAlias = ZR_TRUE;
    handleActive = ZrCore_Execution_BeginAsyncWait(&request, &diagnostic);
    TEST_CHECK(!handleActive);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND);

cleanup:
    if (handleActive) {
        test_cleanup_wait_handle_if_active(&handle, &handleActive, &diagnostic);
    }
    if (registryInitialized) {
        if (handleActive) {
            test_record_failure("wait registry deinit requires released handles",
                                __FILE__, __LINE__);
        } else {
            ZrCore_AsyncWaitRegistry_Deinit(&registry);
            registryInitialized = ZR_FALSE;
        }
    }
}

/* 队列持有 IR 快照副本；过期代际的编译结果不可发布。 */
static void test_compile_queue_copies_snapshot_and_discards_stale_result(void) {
    SZrCompileJobRecord records[2];
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrByte source[] = {1u, 2u, 3u, 4u};
    const TZrByte *snapshot = ZR_NULL;
    TZrSize snapshotLength = 0u;
    TZrBool queueInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;
    TZrBool workerOwnsSnapshot = ZR_FALSE;
    TZrBool resultPublished = ZR_FALSE;

    queueInitialized =
            ZrCore_CompileQueue_Init(&queue, records, 2u, 64u, &diagnostic);
    TEST_CHECK(queueInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &queue;
    request.outHandle = &handle;
    request.irSnapshot = source;
    request.irLength = sizeof(source);
    request.requestedGeneration = 11u;
    request.moduleHash = 101u;
    request.signatureHash = 202u;
    request.layoutHash = 303u;
    request.flags = ZR_COMPILE_QUEUE_REQUEST_WARMUP;
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(ZrCore_CompileQueue_IsWarmup(&handle));
    source[0] = 99u;
    workerOwnsSnapshot =
            ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic);
    TEST_CHECK(workerOwnsSnapshot);
    TEST_CHECK(ZrCore_CompileQueue_GetSnapshot(
                   &handle, &snapshot, &snapshotLength, &diagnostic));
    TEST_CHECK(snapshotLength == sizeof(source));
    TEST_CHECK(snapshot[0] == 1u);
    resultPublished = ZrCore_CompileQueue_Complete(
            &handle, 12u, 101u, 202u, 303u, 404u, &diagnostic);
    workerOwnsSnapshot = ZR_FALSE;
    TEST_CHECK(!resultPublished);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_DISCARDED);
    TEST_CHECK(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;

cleanup:
    test_cleanup_compile_handle_if_active(
            &handle, &handleActive, &workerOwnsSnapshot, &request, &diagnostic);
    if (queueInitialized) {
        if (handleActive || workerOwnsSnapshot) {
            test_record_failure("queue deinit requires no live handle or worker",
                                __FILE__, __LINE__);
        } else {
            ZrCore_CompileQueue_Deinit(&queue);
            queueInitialized = ZR_FALSE;
        }
    }
}

/* 缺失快照被拒绝；有效排队任务取消后可释放槽位。 */
static void test_compile_queue_cancel_and_reject_bad_snapshot(void) {
    SZrCompileJobRecord record;
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrBool queueInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;

    queueInitialized =
            ZrCore_CompileQueue_Init(&queue, &record, 1u, 4u, &diagnostic);
    TEST_CHECK(queueInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &queue;
    request.outHandle = &handle;
    request.irLength = 1u;
    request.requestedGeneration = 1u;
    request.moduleHash = 1u;
    request.signatureHash = 2u;
    request.layoutHash = 3u;
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(!handleActive);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT);
    request.irSnapshot = (const TZrByte *)"x";
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_QUEUED);
    TEST_CHECK(ZrCore_CompileQueue_Cancel(&handle, &diagnostic));
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_CANCELLED);
    TEST_CHECK(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;

cleanup:
    test_cleanup_compile_handle_if_active(
            &handle, &handleActive, ZR_NULL, &request, &diagnostic);
    if (queueInitialized) {
        if (handleActive) {
            test_record_failure("queue deinit requires released handles",
                                __FILE__, __LINE__);
        } else {
            ZrCore_CompileQueue_Deinit(&queue);
            queueInitialized = ZR_FALSE;
        }
    }
}

/* 运行中取消须等待 worker 完成确认后才能释放快照。 */
static void test_compile_queue_running_cancel_keeps_snapshot_until_ack(void) {
    SZrCompileJobRecord record;
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrByte source[] = {7u, 8u, 9u};
    const TZrByte *snapshot = ZR_NULL;
    TZrSize snapshotLength = 0u;
    TZrBool queueInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;
    TZrBool workerOwnsSnapshot = ZR_FALSE;
    TZrBool resultPublished = ZR_FALSE;

    queueInitialized =
            ZrCore_CompileQueue_Init(&queue, &record, 1u, 16u, &diagnostic);
    TEST_CHECK(queueInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &queue;
    request.outHandle = &handle;
    request.irSnapshot = source;
    request.irLength = sizeof(source);
    request.requestedGeneration = 31u;
    request.moduleHash = 41u;
    request.signatureHash = 42u;
    request.layoutHash = 43u;
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(handleActive);
    workerOwnsSnapshot =
            ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic);
    TEST_CHECK(workerOwnsSnapshot);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_RUNNING);

    /* A running worker owns the immutable snapshot until Complete acknowledges
     * cancellation.  Cancel must not publish a releasable state. */
    TEST_CHECK(ZrCore_CompileQueue_Cancel(&handle, &diagnostic));
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_RUNNING);
    TEST_CHECK(!ZrCore_CompileQueue_Release(&handle, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE);
    TEST_CHECK(ZrCore_CompileQueue_GetSnapshot(
                   &handle, &snapshot, &snapshotLength, &diagnostic));
    TEST_CHECK(snapshotLength == sizeof(source));
    TEST_CHECK(snapshot[0] == source[0]);

    /* Complete is the worker acknowledgement and transitions to DISCARDED;
     * only then may the owner release the copied bytes. */
    resultPublished = ZrCore_CompileQueue_Complete(
            &handle, 31u, 41u, 42u, 43u, 44u, &diagnostic);
    workerOwnsSnapshot = ZR_FALSE;
    TEST_CHECK(!resultPublished);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_DISCARDED);
    TEST_CHECK(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;

cleanup:
    test_cleanup_compile_handle_if_active(
            &handle, &handleActive, &workerOwnsSnapshot, &request, &diagnostic);
    if (queueInitialized) {
        if (handleActive || workerOwnsSnapshot) {
            test_record_failure("queue deinit requires no live handle or worker",
                                __FILE__, __LINE__);
        } else {
            ZrCore_CompileQueue_Deinit(&queue);
            queueInitialized = ZR_FALSE;
        }
    }
}

/* 请求中的 queue/outHandle 必须与调用入口实参指向同一任务。 */
static void test_compile_queue_rejects_request_identity_mismatch(void) {
    SZrCompileJobRecord record;
    SZrCompileJobRecord otherRecord;
    SZrCompileQueue queue;
    SZrCompileQueue otherQueue;
    SZrCompileJobHandle handle;
    SZrCompileJobHandle otherHandle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrBool queueInitialized = ZR_FALSE;
    TZrBool otherQueueInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;
    TZrBool otherHandleActive = ZR_FALSE;
    TZrBool workerOwnsSnapshot = ZR_FALSE;

    queueInitialized =
            ZrCore_CompileQueue_Init(&queue, &record, 1u, 4u, &diagnostic);
    TEST_CHECK(queueInitialized);
    otherQueueInitialized =
            ZrCore_CompileQueue_Init(&otherQueue, &otherRecord, 1u, 4u,
                                     &diagnostic);
    TEST_CHECK(otherQueueInitialized);
    memset(&handle, 0, sizeof(handle));
    memset(&otherHandle, 0, sizeof(otherHandle));
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &otherQueue;
    request.outHandle = &handle;
    request.requestedGeneration = 51u;
    request.moduleHash = 61u;
    request.signatureHash = 62u;
    request.layoutHash = 63u;
    handleActive = ZrCore_CompileQueue_Queue(
            &queue, &request, &handle, &diagnostic);
    TEST_CHECK(!handleActive);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);

    request.queue = &queue;
    request.outHandle = &otherHandle;
    handleActive = ZrCore_CompileQueue_Queue(
            &queue, &request, &handle, &diagnostic);
    otherHandleActive = otherHandle.queue != ZR_NULL;
    TEST_CHECK(!handleActive && !otherHandleActive);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);

    request.queue = ZR_NULL;
    request.outHandle = &handle;
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(!handleActive);
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);

cleanup:
    test_cleanup_compile_handle_if_active(
            &handle, &handleActive, &workerOwnsSnapshot, &request, &diagnostic);
    test_cleanup_compile_handle_if_active(
            &otherHandle, &otherHandleActive, &workerOwnsSnapshot, &request,
            &diagnostic);
    if (queueInitialized) {
        if (handleActive || otherHandleActive || workerOwnsSnapshot) {
            test_record_failure("queue deinit requires no live handle or worker",
                                __FILE__, __LINE__);
        } else {
            ZrCore_CompileQueue_Deinit(&queue);
            queueInitialized = ZR_FALSE;
        }
    }
    if (otherQueueInitialized) {
        if (handleActive || otherHandleActive || workerOwnsSnapshot) {
            test_record_failure("other queue deinit requires no live handle or worker",
                                __FILE__, __LINE__);
        } else {
            ZrCore_CompileQueue_Deinit(&otherQueue);
            otherQueueInitialized = ZR_FALSE;
        }
    }
}

#include "ssa_async_compile_contract_cases.inc"
#include "ssa_async_stale_handle_cases.inc"

int main(void) {
    test_frame_budget_only_suspends_at_a_coherent_boundary();
    test_frame_budget_rejects_unsafe_suspend_and_balances_pin();
    test_frame_budget_supports_wait_suspend_and_completion_gates();
    test_frame_budget_preserves_prestart_cancellation();
    test_wait_registration_recheck_and_wake_resume_once();
    test_wait_cancel_and_timeout_are_single_winners();
    test_wait_rejects_non_suspendable_resources();
    test_compile_queue_copies_snapshot_and_discards_stale_result();
    test_compile_queue_cancel_and_reject_bad_snapshot();
    test_compile_queue_running_cancel_keeps_snapshot_until_ack();
    test_compile_queue_rejects_request_identity_mismatch();
    test_compile_queue_rejects_contract_changes_and_reuses_slot();
    test_wait_stale_handle_cannot_change_reused_slot();
    test_compile_stale_handle_cannot_change_reused_job();
    return g_testFailureCount == 0u ? 0 : 1;
}
