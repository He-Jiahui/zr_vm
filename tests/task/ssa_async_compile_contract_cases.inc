/* Same-generation completion must still match every compile contract field. */
static void test_compile_queue_rejects_contract_changes_and_reuses_slot(void) {
    const TZrUInt64 generation = 21u;
    const TZrUInt64 moduleHash = 101u;
    const TZrUInt64 signatureHash = 202u;
    const TZrUInt64 layoutHash = 303u;
    const TZrUInt64 resultHash = 404u;
    SZrCompileJobRecord record;
    SZrCompileQueue queue;
    SZrCompileJobHandle handle;
    SZrCompileQueueRequest request;
    SZrAsyncFrameDiagnostic diagnostic;
    TZrByte source[] = {5u, 6u};
    TZrBool queueInitialized = ZR_FALSE;
    TZrBool handleActive = ZR_FALSE;
    TZrBool workerOwnsSnapshot = ZR_FALSE;
    TZrUInt32 caseIndex;
    TZrUInt64 previousJobId = 0u;

    queueInitialized =
            ZrCore_CompileQueue_Init(&queue, &record, 1u, sizeof(source), &diagnostic);
    TEST_CHECK(queueInitialized);
    memset(&request, 0, sizeof(request));
    request.schemaVersion = ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION;
    request.queue = &queue;
    request.outHandle = &handle;
    request.irSnapshot = source;
    request.irLength = sizeof(source);
    request.requestedGeneration = generation;
    request.moduleHash = moduleHash;
    request.signatureHash = signatureHash;
    request.layoutHash = layoutHash;

    /* Four independent failures, then a successful publication in the same slot. */
    for (caseIndex = 0u; caseIndex < 5u; caseIndex++) {
        TZrBool published;
        TZrUInt64 completedModuleHash = moduleHash;
        TZrUInt64 completedSignatureHash = signatureHash;
        TZrUInt64 completedLayoutHash = layoutHash;
        TZrUInt64 completedResultHash = resultHash;

        switch (caseIndex) {
            case 0u: completedModuleHash++; break;
            case 1u: completedSignatureHash++; break;
            case 2u: completedLayoutHash++; break;
            case 3u: completedResultHash = 0u; break;
            default: break;
        }
        handleActive = ZrCore_Execution_QueueCompilation(&request, &diagnostic);
        TEST_CHECK(handleActive);
        TEST_CHECK(handle.slotIndex == 0u);
        TEST_CHECK(handle.jobId > previousJobId);
        previousJobId = handle.jobId;
        TEST_CHECK(queue.activeCount == 1u);
        TEST_CHECK(record.resultHash == 0u);
        workerOwnsSnapshot =
                ZrCore_CompileQueue_ClaimNext(&queue, &handle, &diagnostic);
        TEST_CHECK(workerOwnsSnapshot);
        published = ZrCore_CompileQueue_Complete(
                &handle, generation, completedModuleHash, completedSignatureHash,
                completedLayoutHash, completedResultHash, &diagnostic);
        workerOwnsSnapshot = ZR_FALSE;
        if (caseIndex < 4u) {
            TEST_CHECK(!published);
            TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH);
            TEST_CHECK(diagnostic.generation == generation);
            TEST_CHECK(diagnostic.state == ZR_COMPILE_JOB_DISCARDED);
            TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_DISCARDED);
            TEST_CHECK(record.resultHash == 0u);
        } else {
            TEST_CHECK(published);
            TEST_CHECK(diagnostic.code == ZR_ASYNC_FRAME_DIAGNOSTIC_NONE);
            TEST_CHECK(ZrCore_CompileQueue_State(&handle) == ZR_COMPILE_JOB_COMPLETED);
            TEST_CHECK(record.resultHash == resultHash);
        }
        TEST_CHECK(queue.activeCount == 1u);
        TEST_CHECK(record.snapshot != ZR_NULL);
        TEST_CHECK(record.snapshotLength == sizeof(source));
        TEST_CHECK(ZrCore_CompileQueue_Release(&handle, &diagnostic));
        handleActive = ZR_FALSE;
        TEST_CHECK(queue.activeCount == 0u);
        TEST_CHECK(record.state == ZR_COMPILE_JOB_FREE);
        TEST_CHECK(record.snapshot == ZR_NULL);
        TEST_CHECK(record.snapshotLength == 0u);
        TEST_CHECK(record.resultHash == 0u);
    }

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
