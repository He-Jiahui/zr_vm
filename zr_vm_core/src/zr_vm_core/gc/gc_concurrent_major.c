// Host-driven concurrent major collection lifecycle.

#include "gc/gc_internal.h"
#include "gc/gc_domain_internal.h"

static TZrUInt64 concurrent_major_elapsed_us(TZrUInt64 startedUs) {
    TZrUInt64 finishedUs = garbage_collector_now_us();
    TZrUInt64 durationUs = finishedUs >= startedUs
                                   ? finishedUs - startedUs
                                   : 0u;
    return durationUs > 0u ? durationUs : 1u;
}

TZrSize garbage_collector_concurrent_major_begin(SZrState *state,
                                                  TZrBool forceCompact) {
    SZrGarbageCollector *collector;
    TZrSize work;
    TZrUInt64 startedUs;
    TZrUInt64 durationUs;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return 0u;
    }

    collector = state->global->garbageCollector;
    if (collector->concurrentMajorActive) {
        return 0u;
    }

    startedUs = garbage_collector_now_us();
    work = garbage_collector_prepare_major_collection(state);
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->concurrentMajorActive = ZR_TRUE;
    collector->concurrentMajorForceCompact = forceCompact;
    collector->concurrentMajorMarkDrained = ZR_FALSE;
    collector->concurrentMajorCycleId++;
    if (collector->concurrentMajorCycleId == 0u) {
        collector->concurrentMajorCycleId = 1u;
    }
    collector->concurrentMajorWork = work;
    collector->collectionPhase =
            ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;

    ZrGarbageCollectorRestartCollection(state);
    work += garbage_collector_snapshot_concurrent_thread_roots(state);
    collector->concurrentMajorWork = work;
    durationUs = concurrent_major_elapsed_us(startedUs);
    collector->statsSnapshot.concurrentMajorCycleCount++;
    collector->statsSnapshot.concurrentMajorInitialPauseCount++;
    collector->statsSnapshot.concurrentMajorInitialPauseTotalUs += durationUs;
    if (collector->statsSnapshot.concurrentMajorInitialPauseMaxUs < durationUs) {
        collector->statsSnapshot.concurrentMajorInitialPauseMaxUs = durationUs;
    }
    collector->statsSnapshot.concurrentMajorActive = ZR_TRUE;
    return work > 0u ? work : 1u;
}

TZrSize garbage_collector_concurrent_major_mark_slice(
        SZrState *state,
        TZrSize objectBudget) {
    SZrGarbageCollector *collector;
    TZrSize work = 0u;
    TZrUInt64 startedUs;
    TZrUInt64 durationUs;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return 0u;
    }
    collector = state->global->garbageCollector;
    if (!collector->concurrentMajorActive ||
        collector->concurrentMajorMarkDrained) {
        return 0u;
    }
    if (objectBudget == 0u) {
        objectBudget = 1u;
    }

    startedUs = garbage_collector_now_us();
    ZrCore_GcDomain_MutationLock(state->gcDomain);
    while (objectBudget-- > 0u) {
        if (collector->waitToScanObjectList == ZR_NULL) {
            if (collector->waitToScanAgainObjectList != ZR_NULL) {
                collector->waitToScanObjectList =
                        collector->waitToScanAgainObjectList;
                collector->waitToScanAgainObjectList = ZR_NULL;
            } else {
                collector->concurrentMajorMarkDrained = ZR_TRUE;
                break;
            }
        }
        work += ZrGarbageCollectorPropagateMark(state);
    }
    ZrCore_GcDomain_MutationUnlock(state->gcDomain);

    collector->concurrentMajorWork += work;
    durationUs = concurrent_major_elapsed_us(startedUs);
    collector->statsSnapshot.concurrentMajorMarkSliceCount++;
    collector->statsSnapshot.concurrentMajorMarkTotalUs += durationUs;
    if (collector->statsSnapshot.concurrentMajorMarkMaxUs < durationUs) {
        collector->statsSnapshot.concurrentMajorMarkMaxUs = durationUs;
    }
    return work;
}

TZrSize garbage_collector_concurrent_major_finish(
        SZrState *state,
        TZrBool *outDidCompact) {
    SZrGarbageCollector *collector;
    TZrSize work;

    if (outDidCompact != ZR_NULL) {
        *outDidCompact = ZR_FALSE;
    }
    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return 0u;
    }
    collector = state->global->garbageCollector;
    if (!collector->concurrentMajorActive ||
        !collector->concurrentMajorMarkDrained) {
        return 0u;
    }

    work = garbage_collector_finish_generational_major_collection(
            state, collector->concurrentMajorForceCompact, outDidCompact);
    collector->concurrentMajorWork += work;
    collector->concurrentMajorActive = ZR_FALSE;
    collector->concurrentMajorForceCompact = ZR_FALSE;
    collector->concurrentMajorMarkDrained = ZR_FALSE;
    collector->scheduledCollectionKind =
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->statsSnapshot.concurrentMajorActive = ZR_FALSE;
    collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    return work > 0u ? work : 1u;
}

void garbage_collector_concurrent_major_cancel(SZrState *state) {
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return;
    }

    collector = state->global->garbageCollector;
    collector->concurrentMajorActive = ZR_FALSE;
    collector->concurrentMajorForceCompact = ZR_FALSE;
    collector->concurrentMajorMarkDrained = ZR_FALSE;
    collector->concurrentMajorWork = 0u;
    collector->statsSnapshot.concurrentMajorActive = ZR_FALSE;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
}
