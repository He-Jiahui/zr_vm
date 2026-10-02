/* Sequential slot reuse keeps the registry/queue alive throughout each test. */
static void test_wait_stale_handle_cannot_change_reused_slot(void) {
    typedef TZrBool (*TZrWaitWinner)(SZrAsyncWaitHandle *, SZrAsyncFrameDiagnostic *);
    const TZrWaitWinner staleWinners[] = {
        ZrCore_AsyncWait_Wake, ZrCore_AsyncWait_Cancel, ZrCore_AsyncWait_Timeout
    };
    SZrAsyncWaitSlot slot;
    SZrAsyncWaitRegistry registry;
    SZrAsyncWaitHandle handle;
    SZrAsyncWaitHandle stale;
    SZrAsyncWaitRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrBool registryInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;
    TZrUInt64 currentToken;
    TZrUInt32 index;

    registryInitialized = ZrCore_AsyncWaitRegistry_Init(&registry, &slot, 1u, &diagnostic);
    TEST_CHECK(registryInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.registry = &registry;
    request.outHandle = &handle;
    request.frameId = 71u;
    request.generation = 31u;
    request.allowSuspend = ZR_TRUE;
    handleActive = ZrCore_Execution_BeginAsyncWait(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(ZrCore_AsyncWait_Wake(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&handle) == 1u);
    stale = handle;
    TEST_CHECK(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;
    TEST_CHECK(slot.state == ZR_ASYNC_WAIT_STATE_FREE);

    /* Reuse the slot with the same generation, so the token must reject A. */
    handleActive = ZrCore_Execution_BeginAsyncWait(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(handle.slotIndex == stale.slotIndex);
    TEST_CHECK(handle.generation == stale.generation);
    TEST_CHECK(handle.token > stale.token);
    currentToken = handle.token;
    for (index = 0u; index < sizeof(staleWinners) / sizeof(staleWinners[0]); index++) {
        TEST_CHECK(!staleWinners[index](&stale, &diagnostic));
        TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
        TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_WAITING);
        TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&handle) == 0u);
        TEST_CHECK(slot.token == currentToken);
    }
    TEST_CHECK(!ZrCore_AsyncWait_Recheck(&stale, ZR_TRUE, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_WAITING);
    TEST_CHECK(slot.token == currentToken);
    TEST_CHECK(ZrCore_AsyncWait_Wake(&handle, &diagnostic));
    TEST_CHECK(!ZrCore_AsyncWait_Resume(&stale, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_READY);
    TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&handle) == 0u);
    TEST_CHECK(ZrCore_AsyncWait_Resume(&handle, &diagnostic));
    TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&handle) == 1u);
    TEST_CHECK(!ZrCore_AsyncWait_Release(&stale, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(ZrCore_AsyncWait_State(&handle) == ZR_ASYNC_WAIT_STATE_RESUMED);
    TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&handle) == 1u);
    TEST_CHECK(slot.token == currentToken);
    TEST_CHECK(ZrCore_AsyncWait_State(&stale) == ZR_ASYNC_WAIT_STATE_FREE);
    TEST_CHECK(ZrCore_AsyncWait_ResumeCount(&stale) == 0u);
    TEST_CHECK(ZrCore_AsyncWait_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;
    TEST_CHECK(slot.state == ZR_ASYNC_WAIT_STATE_FREE);
    TEST_CHECK(slot.token == 0u && slot.generation == 0u);

cleanup:
    test_cleanup_wait_handle_if_active(&handle, &handleActive, &diagnostic);
    if (registryInitialized) {
        if (handleActive) {
            test_record_failure("wait registry deinit requires released handles",
                                __FILE__, __LINE__);
        } else {
            ZrCore_AsyncWaitRegistry_Deinit(&registry);
        }
    }
}

static void test_compile_stale_handle_cannot_change_reused_job(void) {
    SZrCompileJobRecord record;
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileJobHandle stale;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrByte source[] = {81u, 82u};
    const TZrByte *snapshot = ZR_NULL;
    const TZrByte *staleSnapshot = ZR_NULL;
    TZrSize snapshotLength = 0u;
    TZrSize staleLength = 0u;
    TZrBool queueInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;
    TZrBool workerOwnsSnapshot = ZR_FALSE;
    TZrBool published;
    TZrUInt64 currentJobId;

    queueInitialized = ZrCore_CompileQueue_Init(&queue, &record, 1u, sizeof(source), &diagnostic);
    TEST_CHECK(queueInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &queue;
    request.outHandle = &handle;
    request.irSnapshot = source;
    request.irLength = sizeof(source);
    request.requestedGeneration = 41u;
    request.moduleHash = 501u;
    request.signatureHash = 502u;
    request.layoutHash = 503u;
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(handleActive);
    workerOwnsSnapshot = ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic);
    TEST_CHECK(workerOwnsSnapshot);
    published = ZrCore_CompileQueue_Complete(&handle, 41u, 501u, 502u, 503u, 601u, &diagnostic);
    workerOwnsSnapshot = ZR_FALSE;
    TEST_CHECK(published);
    stale = handle;
    TEST_CHECK(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;
    TEST_CHECK(queue.activeCount == 0u);

    source[0] = 91u;
    request.flags = ZR_COMPILE_QUEUE_REQUEST_WARMUP;
    handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
    TEST_CHECK(handleActive);
    TEST_CHECK(handle.slotIndex == stale.slotIndex);
    TEST_CHECK(handle.jobId > stale.jobId);
    TEST_CHECK(record.requestedGeneration == request.requestedGeneration);
    currentJobId = handle.jobId;
    TEST_CHECK(ZrCore_CompileQueue_State(&stale) == ZR_COMPILE_JOB_FREE);
    TEST_CHECK(!ZrCore_CompileQueue_IsWarmup(&stale));
    TEST_CHECK(ZrCore_CompileQueue_IsWarmup(&handle));
    TEST_CHECK(!ZrCore_CompileQueue_GetSnapshot(&stale, &staleSnapshot, &staleLength, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(staleSnapshot == ZR_NULL && staleLength == 0u);
    TEST_CHECK(!ZrCore_CompileQueue_Cancel(&stale, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_QUEUED);
    TEST_CHECK(record.cancellationRequested == 0u);
    TEST_CHECK(record.jobId == currentJobId && queue.activeCount == 1u);

    workerOwnsSnapshot = ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic);
    TEST_CHECK(workerOwnsSnapshot);
    TEST_CHECK(ZrCore_CompileQueue_GetSnapshot(&handle, &snapshot, &snapshotLength, &diagnostic));
    TEST_CHECK(snapshotLength == sizeof(source) && snapshot[0] == 91u && snapshot[1] == 82u);
    TEST_CHECK(!ZrCore_CompileQueue_Complete(&stale, 41u, 501u, 502u, 503u, 602u, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_RUNNING);
    TEST_CHECK(record.resultHash == 0u);
    TEST_CHECK(!ZrCore_CompileQueue_Cancel(&stale, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(record.cancellationRequested == 0u);
    TEST_CHECK(record.snapshot == snapshot && snapshot[0] == 91u);
    published = ZrCore_CompileQueue_Complete(&handle, 41u, 501u, 502u, 503u, 603u, &diagnostic);
    workerOwnsSnapshot = ZR_FALSE;
    TEST_CHECK(published);
    TEST_CHECK(!ZrCore_CompileQueue_Release(&stale, &diagnostic));
    TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND);
    TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_COMPLETED);
    TEST_CHECK(record.resultHash == 603u);
    TEST_CHECK(record.jobId == currentJobId && queue.activeCount == 1u);
    TEST_CHECK(record.snapshot == snapshot && snapshotLength == record.snapshotLength);
    /* The worker no longer borrows the bytes after Complete; inspect record ownership only. */
    TEST_CHECK(ZrCore_CompileQueue_Release(&handle, &diagnostic));
    handleActive = ZR_FALSE;
    TEST_CHECK(queue.activeCount == 0u && record.state == ZR_COMPILE_JOB_FREE);
    TEST_CHECK(record.snapshot == ZR_NULL && record.snapshotLength == 0u);
    TEST_CHECK(record.resultHash == 0u);

cleanup:
    test_cleanup_compile_handle_if_active(
            &handle, &handleActive, &workerOwnsSnapshot, &request, &diagnostic);
    if (queueInitialized) {
        if (handleActive || workerOwnsSnapshot) {
            test_record_failure("queue deinit requires no live handle or worker",
                                __FILE__, __LINE__);
        } else {
            ZrCore_CompileQueue_Deinit(&queue);
        }
    }
}
