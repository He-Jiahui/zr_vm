#ifndef ZR_VM_CORE_ASYNC_FRAME_BUDGET_H
#define ZR_VM_CORE_ASYNC_FRAME_BUDGET_H

/*
 * Frame-safe asynchronous execution contract.
 *
 * The contract is deliberately independent of the concrete VM state and
 * scheduler.  It gives the scheduler a small, scalar state machine for
 * suspended frames, waiters, and background compilation jobs.  Runtime
 * adapters may embed these records in their own objects, but must not bypass
 * the state transitions below.
 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_ASYNC_FRAME_BUDGET_MAGIC ((TZrUInt32)0x31464241u) /* ABF1 */

typedef enum EZrAsyncFrameDiagnosticCode {
    ZR_ASYNC_FRAME_DIAGNOSTIC_NONE = 0,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
    ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_LOCK_GUARD_ACROSS_SUSPEND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_NATIVE_CRITICAL,
    ZR_ASYNC_FRAME_DIAGNOSTIC_STATE_MAP_REQUIRED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_BUDGET_EXHAUSTED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN,
    ZR_ASYNC_FRAME_DIAGNOSTIC_PIN_UNDERFLOW,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_CAPACITY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_ALREADY_RESUMED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CAPACITY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT,
    ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION,
    ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH,
    ZR_ASYNC_FRAME_DIAGNOSTIC_OUT_OF_MEMORY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW,
    ZR_ASYNC_FRAME_DIAGNOSTIC_COUNT
} EZrAsyncFrameDiagnosticCode;

/* Stable scalar diagnostic; it is safe to copy across a scheduler boundary. */
typedef struct SZrAsyncFrameDiagnostic {
    EZrAsyncFrameDiagnosticCode code;
    TZrUInt32 stage;
    TZrUInt32 state;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt64 expected;
    TZrUInt64 actual;
    TZrUInt64 generation;
} SZrAsyncFrameDiagnostic;

typedef enum EZrAsyncFrameStatus {
    ZR_ASYNC_FRAME_STATUS_IDLE = 0,
    ZR_ASYNC_FRAME_STATUS_RUNNING,
    ZR_ASYNC_FRAME_STATUS_SUSPENDED,
    ZR_ASYNC_FRAME_STATUS_RESUMING,
    ZR_ASYNC_FRAME_STATUS_COMPLETED,
    ZR_ASYNC_FRAME_STATUS_CANCELLED,
    ZR_ASYNC_FRAME_STATUS_FAULTED,
    ZR_ASYNC_FRAME_STATUS_TORN_DOWN,
    ZR_ASYNC_FRAME_STATUS_COUNT
} EZrAsyncFrameStatus;

/* Compatibility spelling used by scheduler adapters. */
#define ZR_ASYNC_FRAME_STATUS_TEARDOWN ZR_ASYNC_FRAME_STATUS_TORN_DOWN

typedef enum EZrAsyncFrameSuspendReason {
    ZR_ASYNC_FRAME_SUSPEND_NONE = 0,
    ZR_ASYNC_FRAME_SUSPEND_WAIT,
    ZR_ASYNC_FRAME_SUSPEND_BUDGET,
    ZR_ASYNC_FRAME_SUSPEND_COMPILE,
    ZR_ASYNC_FRAME_SUSPEND_COUNT
} EZrAsyncFrameSuspendReason;

typedef enum EZrAsyncFramePollOutcome {
    ZR_ASYNC_FRAME_POLL_RUNNING = 0,
    ZR_ASYNC_FRAME_POLL_SUSPENDED,
    ZR_ASYNC_FRAME_POLL_CANCELLED,
    ZR_ASYNC_FRAME_POLL_COMPLETED,
    ZR_ASYNC_FRAME_POLL_FAULTED
} EZrAsyncFramePollOutcome;

/* An async contract is explicit.  Synchronous callers must not infer it. */
#define ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT ((TZrUInt32)1u << 0u)
#define ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND ((TZrUInt32)1u << 1u)
#define ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID ((TZrUInt32)1u << 2u)
#define ZR_ASYNC_FRAME_FLAG_HAS_BORROW ((TZrUInt32)1u << 3u)
#define ZR_ASYNC_FRAME_FLAG_HAS_STACK_ALIAS ((TZrUInt32)1u << 4u)
#define ZR_ASYNC_FRAME_FLAG_HAS_LOCK_GUARD ((TZrUInt32)1u << 5u)
#define ZR_ASYNC_FRAME_FLAG_NATIVE_CRITICAL ((TZrUInt32)1u << 6u)
#define ZR_ASYNC_FRAME_FLAG_KNOWN_MASK \
    (ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT | ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND | \
     ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID | ZR_ASYNC_FRAME_FLAG_HAS_BORROW | \
     ZR_ASYNC_FRAME_FLAG_HAS_STACK_ALIAS | ZR_ASYNC_FRAME_FLAG_HAS_LOCK_GUARD | \
     ZR_ASYNC_FRAME_FLAG_NATIVE_CRITICAL)

#define ZR_ASYNC_FRAME_PENDING_BUDGET ((TZrUInt32)1u << 0u)
#define ZR_ASYNC_FRAME_PENDING_CANCEL ((TZrUInt32)1u << 1u)
#define ZR_ASYNC_FRAME_PENDING_WAIT ((TZrUInt32)1u << 2u)

typedef struct SZrAsyncFrameBudget {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    EZrAsyncFrameStatus status;
    EZrAsyncFrameSuspendReason suspendReason;
    TZrUInt32 pendingFlags;
    TZrUInt32 stateMapId;
    TZrUInt32 pinCount;
    TZrUInt64 frameId;
    TZrUInt64 generation;
    TZrUInt64 stateMapGeneration;
    TZrUInt64 maxWorkUnits;
    TZrUInt64 consumedWorkUnits;
    TZrUInt64 pollCount;
    TZrUInt64 suspensionCount;
    volatile TZrUInt32 cancellationRequested;
    TZrUInt32 reserved;
} SZrAsyncFrameBudget;

ZR_CORE_API void ZrCore_AsyncFrameBudget_DiagnosticClear(
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_AsyncFrameBudget_DiagnosticName(
        EZrAsyncFrameDiagnosticCode code);
ZR_CORE_API const TZrChar *ZrCore_AsyncFrameBudget_StatusName(
        EZrAsyncFrameStatus status);
ZR_CORE_API void ZrCore_AsyncFrameBudget_Init(SZrAsyncFrameBudget *frame);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Validate(
        const SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Begin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_CanSuspend(
        const SZrAsyncFrameBudget *frame,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API EZrAsyncFramePollOutcome ZrCore_AsyncFrameBudget_Poll(
        SZrAsyncFrameBudget *frame,
        TZrUInt64 workUnits,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Suspend(
        SZrAsyncFrameBudget *frame,
        EZrAsyncFrameSuspendReason reason,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Complete(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Fault(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Pin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Unpin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_RequestCancel(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Resume(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Teardown(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);

typedef enum EZrAsyncWaitState {
    ZR_ASYNC_WAIT_STATE_FREE = 0,
    ZR_ASYNC_WAIT_STATE_REGISTERING,
    ZR_ASYNC_WAIT_STATE_WAITING,
    ZR_ASYNC_WAIT_STATE_READY,
    ZR_ASYNC_WAIT_STATE_CANCELLED,
    ZR_ASYNC_WAIT_STATE_TIMED_OUT,
    ZR_ASYNC_WAIT_STATE_RESUMED,
    ZR_ASYNC_WAIT_STATE_COUNT
} EZrAsyncWaitState;

typedef struct SZrAsyncWaitSlot {
    volatile TZrUInt32 state;
    volatile TZrUInt32 resumeCount;
    TZrUInt64 token;
    TZrUInt64 frameId;
    TZrUInt64 generation;
    TZrUInt64 deadlineMicros;
} SZrAsyncWaitSlot;

typedef struct SZrAsyncWaitRegistry {
    volatile TZrUInt32 lock;
    TZrUInt64 nextToken;
    SZrAsyncWaitSlot *slots;
    TZrUInt32 capacity;
    TZrUInt32 reserved;
} SZrAsyncWaitRegistry;

typedef struct SZrAsyncWaitHandle {
    SZrAsyncWaitRegistry *registry;
    TZrUInt32 slotIndex;
    TZrUInt64 token;
    TZrUInt64 generation;
} SZrAsyncWaitHandle;

typedef struct SZrAsyncWaitRequest {
    TZrUInt32 schemaVersion;
    SZrAsyncWaitRegistry *registry;
    SZrAsyncWaitHandle *outHandle;
    TZrUInt64 frameId;
    TZrUInt64 generation;
    TZrUInt64 deadlineMicros;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrBool conditionReady;
    TZrBool allowSuspend;
    TZrBool holdsBorrow;
    TZrBool holdsStackAlias;
    TZrBool holdsLockGuard;
    TZrBool inNativeCritical;
    TZrBool reserved0;
} SZrAsyncWaitRequest;

ZR_CORE_API TZrBool ZrCore_AsyncWaitRegistry_Init(
        SZrAsyncWaitRegistry *registry,
        SZrAsyncWaitSlot *slots,
        TZrUInt32 capacity,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_AsyncWaitRegistry_Deinit(
        SZrAsyncWaitRegistry *registry);

/* The request carries registry/outHandle so this matches the plan's two-arg
 * execution entry while retaining a convenient explicit form below. */
ZR_CORE_API TZrBool ZrCore_Execution_BeginAsyncWait(
        const SZrAsyncWaitRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Begin(
        SZrAsyncWaitRegistry *registry,
        const SZrAsyncWaitRequest *request,
        SZrAsyncWaitHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Recheck(
        SZrAsyncWaitHandle *handle,
        TZrBool conditionReady,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Wake(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Cancel(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Timeout(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Resume(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_AsyncWait_Release(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API EZrAsyncWaitState ZrCore_AsyncWait_State(
        const SZrAsyncWaitHandle *handle);
ZR_CORE_API TZrUInt32 ZrCore_AsyncWait_ResumeCount(
        const SZrAsyncWaitHandle *handle);

typedef enum EZrCompileJobState {
    ZR_COMPILE_JOB_FREE = 0,
    ZR_COMPILE_JOB_QUEUED,
    ZR_COMPILE_JOB_RUNNING,
    ZR_COMPILE_JOB_COMPLETED,
    ZR_COMPILE_JOB_CANCELLED,
    ZR_COMPILE_JOB_DISCARDED,
    ZR_COMPILE_JOB_COUNT
} EZrCompileJobState;

#define ZR_COMPILE_QUEUE_REQUEST_WARMUP ((TZrUInt32)1u << 0u)
#define ZR_COMPILE_QUEUE_REQUEST_KNOWN_MASK ZR_COMPILE_QUEUE_REQUEST_WARMUP

typedef struct SZrCompileJobRecord {
    volatile TZrUInt32 state;
    volatile TZrUInt32 cancellationRequested;
    TZrUInt64 jobId;
    TZrUInt64 requestedGeneration;
    TZrUInt64 moduleHash;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 resultHash;
    TZrByte *snapshot;
    TZrSize snapshotLength;
    TZrUInt32 flags;
    TZrUInt32 reserved;
} SZrCompileJobRecord;

typedef struct SZrCompileQueue {
    volatile TZrUInt32 lock;
    TZrUInt64 nextJobId;
    SZrCompileJobRecord *records;
    TZrUInt32 capacity;
    TZrUInt32 activeCount;
    TZrSize maxSnapshotBytes;
} SZrCompileQueue;

typedef struct SZrCompileJobHandle {
    SZrCompileQueue *queue;
    TZrUInt32 slotIndex;
    TZrUInt64 jobId;
} SZrCompileJobHandle;

typedef struct SZrCompileQueueRequest {
    TZrUInt32 schemaVersion;
    SZrCompileQueue *queue;
    SZrCompileJobHandle *outHandle;
    const TZrByte *irSnapshot;
    TZrSize irLength;
    TZrUInt64 requestedGeneration;
    TZrUInt64 moduleHash;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt32 flags;
    TZrBool reserved0;
    TZrBool reserved1;
    TZrBool reserved2;
    TZrBool reserved3;
} SZrCompileQueueRequest;

/*
 * Queue/request identity and worker lifetime contract:
 *
 * - request->queue and request->outHandle are required to be non-NULL and
 *   must equal the queue/output arguments supplied to Queue/QueueWarmup.
 *   The wrapper QueueCompilation uses those same fields, so it cannot publish
 *   a job through an unrelated queue or handle.
 * - Queue record transitions are serialized by the queue lock.  A handle
 *   value may be copied for a worker, but one handle object must not be
 *   concurrently mutated (for example, Release must not race a call that
 *   reads that same object).  The queue must remain initialized until every
 *   handle is terminal and released; Deinit is a quiescent operation.
 * - GetSnapshot returns a borrowed immutable pointer.  The worker must finish
 *   reading it before Complete; after Complete acknowledges cancellation or
 *   publishes/discards a result, Release may free the snapshot.
 * - Cancel transitions QUEUED jobs to CANCELLED.  For RUNNING jobs it only
 *   sets cancellationRequested and deliberately keeps RUNNING until the
 *   worker calls Complete.  Release rejects RUNNING, preventing a snapshot
 *   from being freed while the worker may still read it.
 */

ZR_CORE_API TZrBool ZrCore_CompileQueue_Init(
        SZrCompileQueue *queue,
        SZrCompileJobRecord *records,
        TZrUInt32 capacity,
        TZrSize maxSnapshotBytes,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_CompileQueue_Deinit(SZrCompileQueue *queue);
ZR_CORE_API TZrBool ZrCore_Execution_QueueCompilation(
        const SZrCompileQueueRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_Queue(
        SZrCompileQueue *queue,
        const SZrCompileQueueRequest *request,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_QueueWarmup(
        SZrCompileQueue *queue,
        const SZrCompileQueueRequest *request,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_ClaimNext(
        SZrCompileQueue *queue,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_Cancel(
        SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_GetSnapshot(
        const SZrCompileJobHandle *handle,
        const TZrByte **outSnapshot,
        TZrSize *outLength,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_Complete(
        SZrCompileJobHandle *handle,
        TZrUInt64 currentGeneration,
        TZrUInt64 moduleHash,
        TZrUInt64 signatureHash,
        TZrUInt64 layoutHash,
        TZrUInt64 resultHash,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompileQueue_Release(
        SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
ZR_CORE_API EZrCompileJobState ZrCore_CompileQueue_State(
        const SZrCompileJobHandle *handle);
ZR_CORE_API TZrBool ZrCore_CompileQueue_IsWarmup(
        const SZrCompileJobHandle *handle);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_ASYNC_FRAME_BUDGET_H */
