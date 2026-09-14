/*
 * Focused contract tests for the frame-safe async wait/budget boundary.
 * This is intentionally a plain C test so the state machine can be checked
 * without constructing a complete VM state or scheduler.
 */
#include <assert.h>
#include <string.h>

#include "zr_vm_core/async_frame_budget.h"

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

    assert(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 2u, ZR_FALSE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_RUNNING);
    /* Exhaustion in the middle of an instruction remains pending. */
    assert(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 2u, ZR_FALSE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_RUNNING);
    assert(frame.status == ZR_ASYNC_FRAME_STATUS_RUNNING);
    /* The next proven state-map boundary is the only legal suspension point. */
    assert(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 0u, ZR_TRUE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_SUSPENDED);
    assert(frame.status == ZR_ASYNC_FRAME_STATUS_SUSPENDED);
    assert(frame.suspendReason == ZR_ASYNC_FRAME_SUSPEND_BUDGET);
    assert(frame.suspensionCount == 1u);
}

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
    assert(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    assert(!ZrCore_AsyncFrameBudget_CanSuspend(
                   &frame, ZR_TRUE, ZR_FALSE, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND);

    frame.flags &= ~ZR_ASYNC_FRAME_FLAG_HAS_BORROW;
    assert(ZrCore_AsyncFrameBudget_Pin(&frame, &diagnostic));
    assert(frame.pinCount == 1u);
    assert(ZrCore_AsyncFrameBudget_Unpin(&frame, &diagnostic));
    assert(frame.pinCount == 0u);
    assert(ZrCore_AsyncFrameBudget_RequestCancel(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 0u, ZR_TRUE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_CANCELLED);
    assert(frame.status == ZR_ASYNC_FRAME_STATUS_CANCELLED);
    assert(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));
    assert(frame.status == ZR_ASYNC_FRAME_STATUS_TORN_DOWN);
    assert(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));
}

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
    assert(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Pin(&frame, &diagnostic));
    assert(!ZrCore_AsyncFrameBudget_Complete(&frame, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN);
    assert(ZrCore_AsyncFrameBudget_Unpin(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Suspend(
                   &frame, ZR_ASYNC_FRAME_SUSPEND_WAIT, ZR_TRUE, ZR_FALSE,
                   &diagnostic));
    assert(frame.status == ZR_ASYNC_FRAME_STATUS_SUSPENDED);
    assert(ZrCore_AsyncFrameBudget_Resume(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Complete(&frame, &diagnostic));
    assert(frame.status == ZR_ASYNC_FRAME_STATUS_COMPLETED);
    assert(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));
}

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
    assert(ZrCore_AsyncFrameBudget_RequestCancel(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Begin(&frame, &diagnostic));
    assert(ZrCore_AsyncFrameBudget_Poll(
                   &frame, 0u, ZR_TRUE, ZR_FALSE, &diagnostic) ==
           ZR_ASYNC_FRAME_POLL_CANCELLED);
    assert(ZrCore_AsyncFrameBudget_Teardown(&frame, &diagnostic));
}

static void test_wait_registration_recheck_and_wake_resume_once(void) {
    SZrAsyncWaitSlot slots[2];
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;

    assert(ZrCore_AsyncWaitRegistry_Init(&registry, slots, 2u, &diagnostic));
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 21u;
    request.generation = 7u;
    request.allowSuspend = ZR_TRUE;
    assert(ZrCore_Execution_BeginAsyncWait(&request, &diagnostic));
    assert(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_WAITING);

    /* Wake in the register/recheck window: recheck must not lose it. */
    assert(ZrCore_AsyncWait_Wake(&handle, &diagnostic));
    assert(ZrCore_AsyncWait_Recheck(&handle, ZR_TRUE, &diagnostic));
    assert(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_READY);
    assert(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    assert(!ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    assert(ZrCore_AsyncWait_ResumeCount(&handle) == 1u);
    assert(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    ZrCore_AsyncWaitRegistry_Deinit(&registry);
}

static void test_wait_cancel_and_timeout_are_single_winners(void) {
    SZrAsyncWaitSlot slot;
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;

    assert(ZrCore_AsyncWaitRegistry_Init(&registry, &slot, 1u, &diagnostic));
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 22u;
    request.generation = 8u;
    request.allowSuspend = ZR_TRUE;
    assert(ZrCore_Execution_BeginAsyncWait(&request, &diagnostic));
    assert(ZrCore_AsyncWait_Cancel(&handle, &diagnostic));
    assert(!ZrCore_AsyncWait_Timeout(&handle, &diagnostic));
    assert(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_CANCELLED);
    assert(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    assert(ZrCore_AsyncWait_Release(&handle, &diagnostic));

    assert(ZrCore_Execution_BeginAsyncWait(&request, &diagnostic));
    assert(ZrCore_AsyncWait_Timeout(&handle, &diagnostic));
    assert(!ZrCore_AsyncWait_Cancel(&handle, &diagnostic));
    assert(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_TIMED_OUT);
    assert(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    assert(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    ZrCore_AsyncWaitRegistry_Deinit(&registry);
}

static void test_wait_rejects_non_suspendable_resources(void) {
    SZrAsyncWaitSlot slot;
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;

    assert(ZrCore_AsyncWaitRegistry_Init(&registry, &slot, 1u, &diagnostic));
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 23u;
    request.generation = 9u;
    request.allowSuspend = ZR_TRUE;
    request.holdsStackAlias = ZR_TRUE;
    assert(!ZrCore_Execution_BeginAsyncWait(&request, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND);
    ZrCore_AsyncWaitRegistry_Deinit(&registry);
}

static void test_compile_queue_copies_snapshot_and_discards_stale_result(void) {
    SZrCompileJobRecord records[2];
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrByte source[] = {1u, 2u, 3u, 4u};
    const TZrByte *snapshot = ZR_NULL;
    TZrSize snapshotLength = 0u;

    assert(ZrCore_CompileQueue_Init(&queue, records, 2u, 64u, &diagnostic));
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
    assert(ZrCore_Execution_QueueCompilation(&request, &diagnostic));
    assert(ZrCore_CompileQueue_IsWarmup(&handle));
    source[0] = 99u;
    assert(ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic));
    assert(ZrCore_CompileQueue_GetSnapshot(
                   &handle, &snapshot, &snapshotLength, &diagnostic));
    assert(snapshotLength == sizeof(source));
    assert(snapshot[0] == 1u);
    assert(ZrCore_CompileQueue_Complete(
                   &handle, 12u, 101u, 202u, 303u, 404u, &diagnostic) ==
           ZR_FALSE);
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION);
    assert(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_DISCARDED);
    assert(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    ZrCore_CompileQueue_Deinit(&queue);
}

static void test_compile_queue_cancel_and_reject_bad_snapshot(void) {
    SZrCompileJobRecord record;
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;

    assert(ZrCore_CompileQueue_Init(&queue, &record, 1u, 4u, &diagnostic));
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &queue;
    request.outHandle = &handle;
    request.irLength = 1u;
    request.requestedGeneration = 1u;
    request.moduleHash = 1u;
    request.signatureHash = 2u;
    request.layoutHash = 3u;
    assert(!ZrCore_Execution_QueueCompilation(&request, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT);
    request.irSnapshot = (const TZrByte *)"x";
    assert(ZrCore_Execution_QueueCompilation(&request, &diagnostic));
    assert(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_QUEUED);
    assert(ZrCore_CompileQueue_Cancel(&handle, &diagnostic));
    assert(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_CANCELLED);
    assert(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    ZrCore_CompileQueue_Deinit(&queue);
}

static void test_compile_queue_running_cancel_keeps_snapshot_until_ack(void) {
    SZrCompileJobRecord record;
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrByte source[] = {7u, 8u, 9u};
    const TZrByte *snapshot = ZR_NULL;
    TZrSize snapshotLength = 0u;

    assert(ZrCore_CompileQueue_Init(&queue, &record, 1u, 16u, &diagnostic));
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
    assert(ZrCore_Execution_QueueCompilation(&request, &diagnostic));
    assert(ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic));
    assert(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_RUNNING);

    /* A running worker owns the immutable snapshot until Complete acknowledges
     * cancellation.  Cancel must not publish a releasable state. */
    assert(ZrCore_CompileQueue_Cancel(&handle, &diagnostic));
    assert(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_RUNNING);
    assert(!ZrCore_CompileQueue_Release(&handle, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE);
    assert(ZrCore_CompileQueue_GetSnapshot(
                   &handle, &snapshot, &snapshotLength, &diagnostic));
    assert(snapshotLength == sizeof(source));
    assert(snapshot[0] == source[0]);

    /* Complete is the worker acknowledgement and transitions to DISCARDED;
     * only then may the owner release the copied bytes. */
    assert(!ZrCore_CompileQueue_Complete(
                   &handle, 31u, 41u, 42u, 43u, 44u, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED);
    assert(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_DISCARDED);
    assert(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    ZrCore_CompileQueue_Deinit(&queue);
}

static void test_compile_queue_rejects_request_identity_mismatch(void) {
    SZrCompileJobRecord record;
    SZrCompileJobRecord otherRecord;
    SZrCompileQueue queue;
    SZrCompileQueue otherQueue;
    SZrCompileJobHandle handle;
    SZrCompileJobHandle otherHandle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;

    assert(ZrCore_CompileQueue_Init(&queue, &record, 1u, 4u, &diagnostic));
    assert(ZrCore_CompileQueue_Init(
                   &otherQueue, &otherRecord, 1u, 4u, &diagnostic));
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &otherQueue;
    request.outHandle = &handle;
    request.requestedGeneration = 51u;
    request.moduleHash = 61u;
    request.signatureHash = 62u;
    request.layoutHash = 63u;
    assert(!ZrCore_CompileQueue_Queue(
                   &queue, &request, &handle, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);

    request.queue = &queue;
    request.outHandle = &otherHandle;
    assert(!ZrCore_CompileQueue_Queue(
                   &queue, &request, &handle, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);

    request.queue = ZR_NULL;
    request.outHandle = &handle;
    assert(!ZrCore_Execution_QueueCompilation(&request, &diagnostic));
    assert(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);

    ZrCore_CompileQueue_Deinit(&otherQueue);
    ZrCore_CompileQueue_Deinit(&queue);
}

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
    return 0;
}
