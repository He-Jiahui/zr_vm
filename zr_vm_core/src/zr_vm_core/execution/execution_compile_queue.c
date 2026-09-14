#include "zr_vm_core/async_frame_budget.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <windows.h>
#endif

enum {
    COMPILE_STAGE_VALIDATE = 1u,
    COMPILE_STAGE_QUEUE = 2u,
    COMPILE_STAGE_CLAIM = 3u,
    COMPILE_STAGE_COMPLETE = 4u
};

static TZrUInt32 compile_atomic_load(const volatile TZrUInt32 *value) {
#if defined(_MSC_VER)
    return (TZrUInt32)InterlockedCompareExchange(
            (volatile LONG *)value, 0L, 0L);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void compile_atomic_store(volatile TZrUInt32 *value, TZrUInt32 next) {
#if defined(_MSC_VER)
    (void)InterlockedExchange((volatile LONG *)value, (LONG)next);
#else
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#endif
}

static TZrBool compile_atomic_cas(volatile TZrUInt32 *value,
                                  TZrUInt32 *expected,
                                  TZrUInt32 desired) {
#if defined(_MSC_VER)
    LONG old = InterlockedCompareExchange((volatile LONG *)value,
                                           (LONG)desired,
                                           (LONG)*expected);
    if ((TZrUInt32)old == *expected) {
        return ZR_TRUE;
    }
    *expected = (TZrUInt32)old;
    return ZR_FALSE;
#else
    return (TZrBool)__atomic_compare_exchange_n(
            value, expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

static void compile_lock(volatile TZrUInt32 *lock) {
    TZrUInt32 expected;

    for (;;) {
        expected = 0u;
        if (compile_atomic_cas(lock, &expected, 1u)) {
            return;
        }
    }
}

static void compile_unlock(volatile TZrUInt32 *lock) {
    compile_atomic_store(lock, 0u);
}

static void compile_diag_clear(SZrAsyncFrameDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrBool compile_fail(SZrAsyncFrameDiagnostic *diagnostic,
                            EZrAsyncFrameDiagnosticCode code,
                            TZrUInt32 stage,
                            TZrUInt32 state,
                            TZrUInt64 expected,
                            TZrUInt64 actual,
                            TZrUInt64 generation) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->stage = stage;
        diagnostic->state = state;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
        diagnostic->generation = generation;
    }
    return ZR_FALSE;
}

static TZrBool compile_queue_valid(const SZrCompileQueue *queue) {
    return (TZrBool)(queue != ZR_NULL && queue->records != ZR_NULL &&
                     queue->capacity != 0u);
}

/* The caller must hold handle->queue->lock.  Keeping identity validation
 * under the queue lock prevents Release from clearing a record between the
 * handle check and the operation that consumes the snapshot. */
static SZrCompileJobRecord *compile_record_from_handle_locked(
        const SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrCompileJobRecord *record;

    if (handle == ZR_NULL || !compile_queue_valid(handle->queue) ||
        handle->slotIndex >= handle->queue->capacity) {
        compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                     COMPILE_STAGE_VALIDATE, ZR_COMPILE_JOB_FREE,
                     1u, 0u, 0u);
        return ZR_NULL;
    }
    record = &handle->queue->records[handle->slotIndex];
    if (record->jobId != handle->jobId || handle->jobId == 0u ||
        compile_atomic_load(&record->state) == ZR_COMPILE_JOB_FREE) {
        compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND,
                     COMPILE_STAGE_VALIDATE, compile_atomic_load(&record->state),
                     handle->jobId, record->jobId, record->jobId);
        return ZR_NULL;
    }
    return record;
}

TZrBool ZrCore_CompileQueue_Init(
        SZrCompileQueue *queue,
        SZrCompileJobRecord *records,
        TZrUInt32 capacity,
        TZrSize maxSnapshotBytes,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrUInt32 index;

    compile_diag_clear(diagnostic);
    if (queue == ZR_NULL || records == ZR_NULL || capacity == 0u) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_VALIDATE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    memset(queue, 0, sizeof(*queue));
    queue->records = records;
    queue->capacity = capacity;
    queue->maxSnapshotBytes = maxSnapshotBytes;
    queue->nextJobId = 1u;
    for (index = 0u; index < capacity; index++) {
        memset(&records[index], 0, sizeof(records[index]));
        compile_atomic_store(&records[index].state, ZR_COMPILE_JOB_FREE);
    }
    return ZR_TRUE;
}

void ZrCore_CompileQueue_Deinit(SZrCompileQueue *queue) {
    TZrUInt32 index;

    if (queue == ZR_NULL) {
        return;
    }
    compile_lock(&queue->lock);
    if (queue->records != ZR_NULL) {
        for (index = 0u; index < queue->capacity; index++) {
            free(queue->records[index].snapshot);
            queue->records[index].snapshot = ZR_NULL;
            queue->records[index].snapshotLength = 0u;
            compile_atomic_store(&queue->records[index].state,
                                  ZR_COMPILE_JOB_FREE);
        }
    }
    queue->records = ZR_NULL;
    queue->capacity = 0u;
    queue->activeCount = 0u;
    queue->nextJobId = 0u;
    compile_unlock(&queue->lock);
}

TZrBool ZrCore_CompileQueue_Queue(
        SZrCompileQueue *queue,
        const SZrCompileQueueRequest *request,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrByte *copy = ZR_NULL;
    TZrUInt32 index;
    TZrUInt64 jobId;
    SZrCompileJobRecord *record = ZR_NULL;

    compile_diag_clear(diagnostic);
    if (!compile_queue_valid(queue) || request == ZR_NULL ||
        outHandle == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    /* The explicit and wrapper forms share one request contract: the queue
     * and output handle embedded in the request must identify the exact
     * arguments being used.  Allowing a NULL or stale identity here would let
     * callers publish a job while retaining an unrelated handle. */
    if (request->queue != queue || request->outHandle != outHandle) {
        return compile_fail(diagnostic,
                            ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, request->requestedGeneration);
    }
    if (request->schemaVersion != ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION,
                            request->schemaVersion,
                            request->requestedGeneration);
    }
    if ((request->flags & ~ZR_COMPILE_QUEUE_REQUEST_KNOWN_MASK) != 0u ||
        request->reserved0 || request->reserved1 || request->reserved2 ||
        request->reserved3 || request->requestedGeneration == 0u ||
        request->moduleHash == 0u || request->signatureHash == 0u ||
        request->layoutHash == 0u) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, request->requestedGeneration);
    }
    if (request->irLength > queue->maxSnapshotBytes ||
        (request->irLength != 0u && request->irSnapshot == ZR_NULL)) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            queue->maxSnapshotBytes, request->irLength,
                            request->requestedGeneration);
    }
    if (request->irLength != 0u) {
        copy = (TZrByte *)malloc(request->irLength);
        if (copy == ZR_NULL) {
            return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_OUT_OF_MEMORY,
                                COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                                request->irLength, 0u,
                                request->requestedGeneration);
        }
        memcpy(copy, request->irSnapshot, request->irLength);
    }

    compile_lock(&queue->lock);
    for (index = 0u; index < queue->capacity; index++) {
        if (compile_atomic_load(&queue->records[index].state) ==
            ZR_COMPILE_JOB_FREE) {
            record = &queue->records[index];
            break;
        }
    }
    if (record == ZR_NULL) {
        compile_unlock(&queue->lock);
        free(copy);
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CAPACITY,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            queue->capacity, queue->capacity,
                            request->requestedGeneration);
    }
    jobId = queue->nextJobId;
    if (jobId == 0u) {
        compile_unlock(&queue->lock);
        free(copy);
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            UINT64_MAX, queue->nextJobId,
                            request->requestedGeneration);
    }
    if (queue->activeCount == UINT32_MAX) {
        /* This is unreachable for a practical fixed-capacity queue, but keep
         * the scalar counter fail-closed rather than wrapping.  Check before
         * publishing any record fields so a failed enqueue is transactionally
         * invisible to workers and future callers. */
        compile_unlock(&queue->lock);
        free(copy);
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            UINT32_MAX, queue->activeCount,
                            request->requestedGeneration);
    }
    queue->nextJobId = jobId == UINT64_MAX ? 0u : jobId + 1u;
    record->jobId = jobId;
    record->requestedGeneration = request->requestedGeneration;
    record->moduleHash = request->moduleHash;
    record->signatureHash = request->signatureHash;
    record->layoutHash = request->layoutHash;
    record->resultHash = 0u;
    record->snapshot = copy;
    record->snapshotLength = request->irLength;
    record->flags = request->flags;
    record->reserved = 0u;
    compile_atomic_store(&record->cancellationRequested, 0u);
    compile_atomic_store(&record->state, ZR_COMPILE_JOB_QUEUED);
    queue->activeCount++;
    outHandle->queue = queue;
    outHandle->slotIndex = index;
    outHandle->jobId = jobId;
    compile_unlock(&queue->lock);
    return ZR_TRUE;
}

TZrBool ZrCore_Execution_QueueCompilation(
        const SZrCompileQueueRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic) {
    if (request == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    if (request->queue == ZR_NULL || request->outHandle == ZR_NULL) {
        return compile_fail(diagnostic,
                            ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, request->requestedGeneration);
    }
    return ZrCore_CompileQueue_Queue(request->queue, request,
                                     request->outHandle, diagnostic);
}

TZrBool ZrCore_CompileQueue_QueueWarmup(
        SZrCompileQueue *queue,
        const SZrCompileQueueRequest *request,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrCompileQueueRequest warmup;

    if (!compile_queue_valid(queue) || request == ZR_NULL ||
        outHandle == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    if (request->queue != queue || request->outHandle != outHandle) {
        return compile_fail(diagnostic,
                            ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH,
                            COMPILE_STAGE_QUEUE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, request->requestedGeneration);
    }
    warmup = *request;
    warmup.flags |= ZR_COMPILE_QUEUE_REQUEST_WARMUP;
    return ZrCore_CompileQueue_Queue(queue, &warmup, outHandle, diagnostic);
}

TZrBool ZrCore_CompileQueue_ClaimNext(
        SZrCompileQueue *queue,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    TZrUInt32 index;
    SZrCompileJobRecord *record = ZR_NULL;

    compile_diag_clear(diagnostic);
    if (!compile_queue_valid(queue) || outHandle == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_CLAIM, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    compile_lock(&queue->lock);
    for (index = 0u; index < queue->capacity; index++) {
        if (compile_atomic_load(&queue->records[index].state) ==
            ZR_COMPILE_JOB_QUEUED) {
            record = &queue->records[index];
            if (compile_atomic_load(&record->cancellationRequested) != 0u) {
                compile_atomic_store(&record->state, ZR_COMPILE_JOB_CANCELLED);
                record = ZR_NULL;
                continue;
            }
            compile_atomic_store(&record->state, ZR_COMPILE_JOB_RUNNING);
            break;
        }
    }
    if (record == ZR_NULL) {
        compile_unlock(&queue->lock);
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
                            COMPILE_STAGE_CLAIM, ZR_COMPILE_JOB_FREE,
                            ZR_COMPILE_JOB_QUEUED, ZR_COMPILE_JOB_FREE, 0u);
    }
    outHandle->queue = queue;
    outHandle->slotIndex = index;
    outHandle->jobId = record->jobId;
    compile_unlock(&queue->lock);
    return ZR_TRUE;
}

TZrBool ZrCore_CompileQueue_Cancel(
        SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrCompileJobRecord *record;
    SZrCompileQueue *queue;
    TZrUInt32 state;

    compile_diag_clear(diagnostic);
    if (handle == ZR_NULL || handle->queue == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_CLAIM, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    queue = handle->queue;
    if (!compile_queue_valid(queue) || handle->slotIndex >= queue->capacity) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_CLAIM, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    compile_lock(&queue->lock);
    record = compile_record_from_handle_locked(handle, diagnostic);
    if (record == ZR_NULL) {
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    state = compile_atomic_load(&record->state);
    if (state == ZR_COMPILE_JOB_QUEUED) {
        /* No worker owns a queued snapshot yet, so it can become terminal
         * immediately and be released. */
        compile_atomic_store(&record->cancellationRequested, 1u);
        compile_atomic_store(&record->state, ZR_COMPILE_JOB_CANCELLED);
        compile_unlock(&queue->lock);
        return ZR_TRUE;
    }
    if (state == ZR_COMPILE_JOB_RUNNING) {
        /* A worker may currently hold record->snapshot.  Keep RUNNING until
         * Complete acknowledges cancellation; Release consequently cannot
         * free the bytes underneath that worker. */
        compile_atomic_store(&record->cancellationRequested, 1u);
        compile_unlock(&queue->lock);
        return ZR_TRUE;
    }
    if (state == ZR_COMPILE_JOB_CANCELLED ||
        state == ZR_COMPILE_JOB_DISCARDED) {
        compile_unlock(&queue->lock);
        return ZR_TRUE;
    }
    compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                 COMPILE_STAGE_CLAIM, state,
                 ZR_COMPILE_JOB_RUNNING, state,
                 record->requestedGeneration);
    compile_unlock(&queue->lock);
    return ZR_FALSE;
}

TZrBool ZrCore_CompileQueue_GetSnapshot(
        const SZrCompileJobHandle *handle,
        const TZrByte **outSnapshot,
        TZrSize *outLength,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrCompileJobRecord *record;
    SZrCompileQueue *queue;
    TZrUInt32 state;
    TZrUInt64 generation;

    compile_diag_clear(diagnostic);
    if (outSnapshot == ZR_NULL || outLength == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_VALIDATE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    if (handle == ZR_NULL || handle->queue == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_VALIDATE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    queue = handle->queue;
    if (!compile_queue_valid(queue) || handle->slotIndex >= queue->capacity) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_VALIDATE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    compile_lock(&queue->lock);
    record = compile_record_from_handle_locked(handle, diagnostic);
    if (record == ZR_NULL) {
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    state = compile_atomic_load(&record->state);
    generation = record->requestedGeneration;
    if (state != ZR_COMPILE_JOB_QUEUED && state != ZR_COMPILE_JOB_RUNNING) {
        compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                     COMPILE_STAGE_VALIDATE, state,
                     ZR_COMPILE_JOB_RUNNING, state, generation);
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    *outSnapshot = record->snapshot;
    *outLength = record->snapshotLength;
    compile_unlock(&queue->lock);
    return ZR_TRUE;
}

TZrBool ZrCore_CompileQueue_Complete(
        SZrCompileJobHandle *handle,
        TZrUInt64 currentGeneration,
        TZrUInt64 moduleHash,
        TZrUInt64 signatureHash,
        TZrUInt64 layoutHash,
        TZrUInt64 resultHash,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrCompileJobRecord *record;
    SZrCompileQueue *queue;
    TZrUInt32 state;
    TZrBool valid;

    compile_diag_clear(diagnostic);
    if (handle == ZR_NULL || handle->queue == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_COMPLETE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    queue = handle->queue;
    if (!compile_queue_valid(queue) || handle->slotIndex >= queue->capacity) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_COMPLETE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    compile_lock(&queue->lock);
    record = compile_record_from_handle_locked(handle, diagnostic);
    if (record == ZR_NULL) {
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    state = compile_atomic_load(&record->state);
    if (state != ZR_COMPILE_JOB_RUNNING) {
        EZrAsyncFrameDiagnosticCode code =
                compile_atomic_load(&record->cancellationRequested) != 0u
                    ? ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED
                    : ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE;
        TZrUInt64 generation = record->requestedGeneration;
        compile_fail(diagnostic, code, COMPILE_STAGE_COMPLETE, state,
                     ZR_COMPILE_JOB_RUNNING, state, generation);
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    if (compile_atomic_load(&record->cancellationRequested) != 0u) {
        compile_atomic_store(&record->state, ZR_COMPILE_JOB_DISCARDED);
        compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED,
                     COMPILE_STAGE_COMPLETE, ZR_COMPILE_JOB_DISCARDED,
                     0u, 1u, record->requestedGeneration);
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    valid = currentGeneration == record->requestedGeneration &&
            moduleHash == record->moduleHash &&
            signatureHash == record->signatureHash &&
            layoutHash == record->layoutHash && resultHash != 0u;
    if (!valid) {
        EZrAsyncFrameDiagnosticCode code =
                currentGeneration != record->requestedGeneration
                    ? ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION
                    : ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH;
        compile_atomic_store(&record->state, ZR_COMPILE_JOB_DISCARDED);
        compile_fail(diagnostic, code, COMPILE_STAGE_COMPLETE,
                     ZR_COMPILE_JOB_DISCARDED,
                     record->requestedGeneration, currentGeneration,
                     record->requestedGeneration);
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    record->resultHash = resultHash;
    compile_atomic_store(&record->state, ZR_COMPILE_JOB_COMPLETED);
    compile_unlock(&queue->lock);
    return ZR_TRUE;
}

TZrBool ZrCore_CompileQueue_Release(
        SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic) {
    SZrCompileJobRecord *record;
    SZrCompileQueue *queue;
    TZrUInt32 state;

    compile_diag_clear(diagnostic);
    if (handle == ZR_NULL || handle->queue == ZR_NULL) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_COMPLETE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    queue = handle->queue;
    if (!compile_queue_valid(queue) || handle->slotIndex >= queue->capacity) {
        return compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
                            COMPILE_STAGE_COMPLETE, ZR_COMPILE_JOB_FREE,
                            1u, 0u, 0u);
    }
    compile_lock(&queue->lock);
    record = compile_record_from_handle_locked(handle, diagnostic);
    if (record == ZR_NULL) {
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    state = compile_atomic_load(&record->state);
    if (state != ZR_COMPILE_JOB_COMPLETED &&
        state != ZR_COMPILE_JOB_CANCELLED &&
        state != ZR_COMPILE_JOB_DISCARDED) {
        compile_fail(diagnostic, ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
                     COMPILE_STAGE_COMPLETE, state,
                     ZR_COMPILE_JOB_COMPLETED, state,
                     record->requestedGeneration);
        compile_unlock(&queue->lock);
        return ZR_FALSE;
    }
    free(record->snapshot);
    record->snapshot = ZR_NULL;
    record->snapshotLength = 0u;
    record->resultHash = 0u;
    record->jobId = 0u;
    record->requestedGeneration = 0u;
    record->moduleHash = 0u;
    record->signatureHash = 0u;
    record->layoutHash = 0u;
    record->flags = 0u;
    record->reserved = 0u;
    compile_atomic_store(&record->cancellationRequested, 0u);
    compile_atomic_store(&record->state, ZR_COMPILE_JOB_FREE);
    if (queue->activeCount != 0u) {
        queue->activeCount--;
    }
    compile_unlock(&queue->lock);
    memset(handle, 0, sizeof(*handle));
    return ZR_TRUE;
}

EZrCompileJobState ZrCore_CompileQueue_State(
        const SZrCompileJobHandle *handle) {
    SZrCompileJobRecord *record;
    SZrCompileQueue *queue;
    EZrCompileJobState state = ZR_COMPILE_JOB_FREE;

    if (handle == ZR_NULL || handle->queue == ZR_NULL) {
        return state;
    }
    queue = handle->queue;
    if (!compile_queue_valid(queue) || handle->slotIndex >= queue->capacity) {
        return state;
    }
    compile_lock(&queue->lock);
    record = compile_record_from_handle_locked(handle, ZR_NULL);
    if (record != ZR_NULL) {
        state = (EZrCompileJobState)compile_atomic_load(&record->state);
    }
    compile_unlock(&queue->lock);
    return state;
}

TZrBool ZrCore_CompileQueue_IsWarmup(
        const SZrCompileJobHandle *handle) {
    SZrCompileJobRecord *record;
    SZrCompileQueue *queue;
    TZrBool warmup = ZR_FALSE;

    if (handle == ZR_NULL || handle->queue == ZR_NULL) {
        return ZR_FALSE;
    }
    queue = handle->queue;
    if (!compile_queue_valid(queue) || handle->slotIndex >= queue->capacity) {
        return ZR_FALSE;
    }
    compile_lock(&queue->lock);
    record = compile_record_from_handle_locked(handle, ZR_NULL);
    if (record != ZR_NULL) {
        warmup = (TZrBool)((record->flags & ZR_COMPILE_QUEUE_REQUEST_WARMUP) !=
                           0u);
    }
    compile_unlock(&queue->lock);
    return warmup;
}
