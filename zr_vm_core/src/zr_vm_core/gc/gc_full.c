/* Full collection owns one mutation-lock, STW and execution-budget layer.
 * Its protected body may abandon marking, but must not abandon those layers. */
#include "gc/gc_internal.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution_budget.h"

/* This object belongs to the caller of TryRun, not the setjmp-containing
 * callee. Its fields remain defined when the protected body exits nonlocally. */
typedef struct SZrGcFullContext {
    TZrUInt64 startedUs;
    TZrSize work;
    EZrGarbageCollectGeneration generation;
    TZrBool isImmediate;
    TZrBool previousImmediate;
    TZrBool completed;
} SZrGcFullContext;

static void garbage_collector_full_body(SZrState *state, TZrPtr argument) {
    SZrGcFullContext *context = (SZrGcFullContext *)argument;
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;

    garbage_collector_concurrent_major_cancel(state);
    context->startedUs = garbage_collector_now_us();
    collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;

    ZR_ASSERT(!collector->isImmediateGcFlag);
    collector->isImmediateGcFlag = context->isImmediate;
    if (collector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL) {
        context->work += garbage_collector_run_generational_full(state);
    } else {
        context->work += garbage_collector_prepare_major_collection(state);
        garbage_collector_full_inc(state, global);
        context->work += collector->gcLastStepWork;
    }
    collector->isImmediateGcFlag = context->previousImmediate;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->gcLastStepWork = context->work > 0 ? context->work : 1;
    garbage_collector_record_step_telemetry(collector, context->startedUs);
    if (collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    }
    context->completed = ZR_TRUE;
}

/* The rooted NATIVE_DATA scanner can throw before atomic changes generation.
 * At that point no object has been unlinked or freed by this full collection.
 * Do not apply this restart to sweep/finalizer/compaction failures: those may
 * already have consumed storage and cannot be rolled back by resetting marks. */
static void garbage_collector_full_abandon_mark(
        SZrState *state, const SZrGcFullContext *context) {
    SZrGarbageCollector *collector = state->global->garbageCollector;

    if (collector->gcGeneration != context->generation) return;
    switch (collector->gcRunningStatus) {
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED:
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION:
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC:
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC:
            break;
        default:
            return;
    }

    /* This reset neither allocates nor calls a scanner. Clearing only heads
     * would leave referenced objects whose unscanned children get skipped on
     * retry. Preserve live object/region/root ownership; discard mark work. */
    garbage_collector_concurrent_major_cancel(state);
    (void)garbage_collector_prepare_major_collection(state);
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;
    collector->gcLastStepWork = 0;
    collector->statsSnapshot.lastStepWork = 0u;
    /* Keep the unfulfilled explicit request; do not credit debt or record a
     * completed collection. The next collection starts with a fresh root scan. */
}

/* Used by host collection, close and allocation-pressure paths. */
void ZrCore_GarbageCollector_GcFull(SZrState *state, TZrBool isImmediate) {
    SZrGarbageCollector *collector;
    SZrGcFullContext context = {0};
    EZrThreadStatus status;
    EZrThreadStatus exceptionStatus;
    TZrBool hadException;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) return;
    collector = state->global->garbageCollector;
    context.isImmediate = isImmediate;
    ZrCore_ExecutionBudget_GcBegin(state);
    if (!ZrCore_GcDomain_StopTheWorldBegin(
                state, ZR_GC_DOMAIN_PAUSE_TIMEOUT_MILLISECONDS, ZR_NULL)) {
        ZrCore_ExecutionBudget_GcEnd(state);
        return;
    }
    ZrCore_GcDomain_MutationLock(state->gcDomain);
    context.previousImmediate = collector->isImmediateGcFlag;
    context.generation = collector->gcGeneration;
    status = ZrCore_Exception_TryRun(state, garbage_collector_full_body, &context);
    if (!context.completed) {
        garbage_collector_full_abandon_mark(state, &context);
    }
    collector->isImmediateGcFlag = context.previousImmediate;
    /* Release only the layers acquired above, preserving an outer pause and
     * recursive lock owner. Restore the scanner Error after GcEnd's Poll, which
     * may latch cancellation and clear hasCurrentException without changing its
     * value slot. No copy/release of that ownership-bearing slot is necessary. */
    hadException = state->hasCurrentException;
    exceptionStatus = state->currentExceptionStatus;
    ZrCore_GcDomain_MutationUnlock(state->gcDomain);
    ZrCore_GcDomain_StopTheWorldEnd(state);
    ZrCore_ExecutionBudget_GcEnd(state);
    if (!context.completed) {
        state->hasCurrentException = hadException;
        state->currentExceptionStatus = exceptionStatus;
        ZrCore_Exception_Throw(state, status);
    }
}
