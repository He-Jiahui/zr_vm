//
// Public GC lifecycle and scheduling entry points.
//

#include "gc/gc_internal.h"

#if defined(ZR_PLATFORM_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

static TZrUInt64 garbage_collector_now_us(void) {
#if defined(ZR_PLATFORM_WIN)
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return frequency.QuadPart > 0 ? (TZrUInt64)((counter.QuadPart * 1000000ULL) / frequency.QuadPart) : 0u;
#else
    struct timespec now;

    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return 0u;
    }
    return (TZrUInt64)now.tv_sec * 1000000ULL + (TZrUInt64)(now.tv_nsec / 1000);
#endif
}

static void garbage_collector_refresh_cumulative_snapshot(SZrGarbageCollector *collector) {
    if (collector == ZR_NULL) {
        return;
    }

    collector->statsSnapshot.minorCollectionCount =
            collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->statsSnapshot.majorCollectionCount =
            collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    collector->statsSnapshot.fullCollectionCount =
            collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL];
    collector->statsSnapshot.minorCollectionTotalDurationUs =
            collector->collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->statsSnapshot.majorCollectionTotalDurationUs =
            collector->collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    collector->statsSnapshot.fullCollectionTotalDurationUs =
            collector->collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL];
    collector->statsSnapshot.minorCollectionMaxDurationUs =
            collector->collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->statsSnapshot.majorCollectionMaxDurationUs =
            collector->collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    collector->statsSnapshot.fullCollectionMaxDurationUs =
            collector->collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL];
}

static void garbage_collector_refresh_pressure_snapshot(SZrGarbageCollector *collector) {
    TZrUInt32 regionCount = 0u;
    TZrUInt32 edenRegionCount = 0u;
    TZrUInt32 survivorRegionCount = 0u;
    TZrUInt32 oldRegionCount = 0u;
    TZrUInt32 pinnedRegionCount = 0u;
    TZrUInt32 largeRegionCount = 0u;
    TZrUInt32 permanentRegionCount = 0u;
    TZrUInt64 edenUsedBytes = 0u;
    TZrUInt64 survivorUsedBytes = 0u;
    TZrUInt64 oldUsedBytes = 0u;
    TZrUInt64 pinnedUsedBytes = 0u;
    TZrUInt64 largeUsedBytes = 0u;
    TZrUInt64 permanentUsedBytes = 0u;
    TZrUInt64 edenLiveBytes = 0u;
    TZrUInt64 survivorLiveBytes = 0u;
    TZrUInt64 oldLiveBytes = 0u;
    TZrUInt64 pinnedLiveBytes = 0u;
    TZrUInt64 largeLiveBytes = 0u;
    TZrUInt64 permanentLiveBytes = 0u;

    if (collector == ZR_NULL) {
        return;
    }

    collector->statsSnapshot.managedMemoryBytes = (TZrUInt64)collector->managedMemories;
    collector->statsSnapshot.gcDebtBytes = (TZrInt64)collector->gcDebtSize;
    collector->statsSnapshot.ignoredObjectCount = (TZrUInt32)collector->ignoredObjectCount;

    for (TZrSize index = 0; index < collector->regionCount; index++) {
        const SZrGarbageCollectRegionDescriptor *region = &collector->regions[index];

        if (region->liveObjectCount == 0u) {
            continue;
        }

        regionCount++;
        switch (region->kind) {
            case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
                edenRegionCount++;
                edenUsedBytes += region->usedBytes;
                edenLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
                survivorRegionCount++;
                survivorUsedBytes += region->usedBytes;
                survivorLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
                oldRegionCount++;
                oldUsedBytes += region->usedBytes;
                oldLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_PINNED:
                pinnedRegionCount++;
                pinnedUsedBytes += region->usedBytes;
                pinnedLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_LARGE:
                largeRegionCount++;
                largeUsedBytes += region->usedBytes;
                largeLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT:
                permanentRegionCount++;
                permanentUsedBytes += region->usedBytes;
                permanentLiveBytes += region->liveBytes;
                break;
            default:
                break;
        }
    }

    collector->statsSnapshot.regionCount = regionCount;
    collector->statsSnapshot.edenRegionCount = edenRegionCount;
    collector->statsSnapshot.survivorRegionCount = survivorRegionCount;
    collector->statsSnapshot.oldRegionCount = oldRegionCount;
    collector->statsSnapshot.pinnedRegionCount = pinnedRegionCount;
    collector->statsSnapshot.largeRegionCount = largeRegionCount;
    collector->statsSnapshot.permanentRegionCount = permanentRegionCount;
    collector->statsSnapshot.edenUsedBytes = edenUsedBytes;
    collector->statsSnapshot.survivorUsedBytes = survivorUsedBytes;
    collector->statsSnapshot.oldUsedBytes = oldUsedBytes;
    collector->statsSnapshot.pinnedUsedBytes = pinnedUsedBytes;
    collector->statsSnapshot.largeUsedBytes = largeUsedBytes;
    collector->statsSnapshot.permanentUsedBytes = permanentUsedBytes;
    collector->statsSnapshot.edenLiveBytes = edenLiveBytes;
    collector->statsSnapshot.survivorLiveBytes = survivorLiveBytes;
    collector->statsSnapshot.oldLiveBytes = oldLiveBytes;
    collector->statsSnapshot.pinnedLiveBytes = pinnedLiveBytes;
    collector->statsSnapshot.largeLiveBytes = largeLiveBytes;
    collector->statsSnapshot.permanentLiveBytes = permanentLiveBytes;
}

static void garbage_collector_record_step_telemetry(SZrGarbageCollector *collector, TZrUInt64 startedUs) {
    TZrUInt64 finishedUs;
    TZrUInt64 durationUs;
    TZrUInt32 kindIndex;

    if (collector == ZR_NULL) {
        return;
    }

    finishedUs = garbage_collector_now_us();
    durationUs = finishedUs >= startedUs ? finishedUs - startedUs : 0u;
    if (durationUs == 0u && collector->gcLastStepWork > 0) {
        durationUs = 1u;
    }

    collector->statsSnapshot.lastStepDurationUs = durationUs;
    collector->statsSnapshot.lastStepWork = (TZrUInt64)collector->gcLastStepWork;

    kindIndex = (TZrUInt32)collector->statsSnapshot.lastCollectionKind;
    if (kindIndex < ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX &&
        (collector->gcLastStepWork > 0 || durationUs > 0u)) {
        collector->collectionCounts[kindIndex] += 1u;
        collector->collectionTotalDurationUs[kindIndex] += durationUs;
        if (collector->collectionMaxDurationUs[kindIndex] < durationUs) {
            collector->collectionMaxDurationUs[kindIndex] = durationUs;
        }
    }

    garbage_collector_refresh_cumulative_snapshot(collector);
}

static TZrUInt32 garbage_collector_merge_scope_depth(TZrUInt32 currentScopeDepth, TZrUInt32 incomingScopeDepth) {
    if (currentScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return incomingScopeDepth;
    }
    if (incomingScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return currentScopeDepth;
    }
    return currentScopeDepth < incomingScopeDepth ? currentScopeDepth : incomingScopeDepth;
}

static TZrBool garbage_collector_collection_is_marking_roots(const SZrGlobalState *global,
                                                             const SZrGarbageCollector *collector) {
    if (global == ZR_NULL || collector == ZR_NULL) {
        return ZR_FALSE;
    }

    if (collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE ||
        collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_SWEEP ||
        collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_COMPACT) {
        return ZR_FALSE;
    }

    return ZrCore_GarbageCollector_IsInvariant((SZrGlobalState *)global) ||
           collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK ||
           collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE ||
           collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT ||
           collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK;
}

static void garbage_collector_mark_ignored_root_if_needed(SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || object == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    if (!garbage_collector_collection_is_marking_roots(global, collector)) {
        return;
    }

    if (object->type >= ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX ||
        object->type == ZR_RAW_OBJECT_TYPE_INVALID ||
        ZrCore_RawObject_IsReleased(object)) {
        return;
    }

    garbage_collector_mark_object(state, object);
}

static EZrGarbageCollectPromotionReason garbage_collector_promotion_reason_from_escape_flags(
        TZrUInt32 escapeFlags,
        EZrGarbageCollectPromotionReason promotionReason) {
    if (promotionReason != ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE) {
        return promotionReason;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT;
    }
    if ((escapeFlags & (ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE |
                        ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE)) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_HOST_HANDLE;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
    }
    if (escapeFlags != ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE;
    }
    return ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
}

static void garbage_collector_mark_raw_object_escaped_internal(SZrState *state,
                                                               SZrRawObject *object,
                                                               TZrUInt32 escapeFlags,
                                                               TZrUInt32 scopeDepth,
                                                               EZrGarbageCollectPromotionReason promotionReason,
                                                               TZrBool propagateCaptures) {
    EZrGarbageCollectPromotionReason resolvedPromotionReason;
    TZrUInt32 previousEscapeFlags;
    EZrGarbageCollectPromotionReason previousPromotionReason;
    TZrBool escapeInfoChanged = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL ||
        escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, object)) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, object);
    }

    previousEscapeFlags = object->garbageCollectMark.escapeFlags;
    previousPromotionReason = object->garbageCollectMark.promotionReason;
    object->garbageCollectMark.escapeFlags |= escapeFlags;
    object->garbageCollectMark.anchorScopeDepth =
            garbage_collector_merge_scope_depth(object->garbageCollectMark.anchorScopeDepth, scopeDepth);

    resolvedPromotionReason =
            garbage_collector_promotion_reason_from_escape_flags(escapeFlags, promotionReason);
    if (resolvedPromotionReason != ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE &&
        (object->garbageCollectMark.promotionReason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE ||
         object->garbageCollectMark.promotionReason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL)) {
        object->garbageCollectMark.promotionReason = resolvedPromotionReason;
    }
    escapeInfoChanged = object->garbageCollectMark.escapeFlags != previousEscapeFlags ||
                        object->garbageCollectMark.promotionReason != previousPromotionReason;

    if (propagateCaptures && escapeInfoChanged) {
        ZrCore_Closure_PropagateEscapeFromObject(state, object, escapeFlags, resolvedPromotionReason);
    }
}

static void garbage_collector_schedule_collection_internal(SZrGlobalState *global,
                                                           EZrGarbageCollectCollectionKind kind,
                                                           TZrBool isExplicitRequest) {
    SZrGarbageCollector *collector;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    collector = global->garbageCollector;
    collector->scheduledCollectionKind = kind;
    collector->statsSnapshot.lastRequestedCollectionKind = kind;
    if (isExplicitRequest) {
        collector->gcFlags |= ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    } else {
        collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    }
    if (collector->gcDebtSize <= 0) {
        ZrCore_GarbageCollector_AddDebtSpace(global, ZR_GC_DEBT_CREDIT_BYTES);
    }
}

void ZrCore_GarbageCollector_New(SZrGlobalState *global) {
    SZrGarbageCollector *gc =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrGarbageCollector), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    SZrState *state;

    global->garbageCollector = gc;
    state = global->mainThreadState;

    gc->managedMemories = sizeof(SZrGlobalState) + sizeof(SZrState);
    gc->gcDebtSize = 0;
    gc->atomicMemories = 0;
    gc->aliveMemories = 0;
    gc->ignoredObjectCount = 0;
    gc->ignoredObjectCapacity = 0;
    gc->ignoredObjects = ZR_NULL;
    gc->rememberedObjects = ZR_NULL;
    gc->rememberedObjectCount = 0;
    gc->rememberedObjectCapacity = 0;
    gc->nextRegionId = 1u;
    gc->currentEdenRegionId = 0u;
    gc->currentSurvivorRegionId = 0u;
    gc->currentOldRegionId = 0u;
    gc->currentEdenRegionUsedBytes = 0u;
    gc->currentSurvivorRegionUsedBytes = 0u;
    gc->currentOldRegionUsedBytes = 0u;
    gc->regions = ZR_NULL;
    gc->regionCount = 0;
    gc->regionCapacity = 0;
    gc->gcPauseBudget = ZR_GC_DEFAULT_PAUSE_BUDGET;
    gc->gcSweepSliceBudget = ZR_GC_DEFAULT_SWEEP_SLICE_BUDGET;
    gc->gcLastStepWork = 0;
    gc->gcLastCompletedRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;

    gc->gcObjectList = ZR_CAST_RAW_OBJECT_AS_SUPER(state);

    gc->gcMajorGenerationMultiplier = ZR_GARBAGE_COLLECT_MAJOR_MULTIPLIER;
    gc->gcMinorGenerationMultiplier = ZR_GARBAGE_COLLECT_MINOR_MULTIPLIER;
    gc->gcStepMultiplierPercent = ZR_GARBAGE_COLLECT_STEP_MULTIPLIER_PERCENT;
    gc->gcStepSizeLog2 = ZR_GARBAGE_COLLECT_STEP_LOG2_SIZE;
    gc->gcPauseThresholdPercent = ZR_GARBAGE_COLLECT_PAUSE_THRESHOLD_PERCENT;

    gc->gcMode = ZR_GARBAGE_COLLECT_MODE_INCREMENTAL;
    gc->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
    gc->gcInitializeObjectStatus = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    gc->gcGeneration = ZR_GARBAGE_COLLECT_GENERATION_A;
    gc->stopGcFlag = ZR_FALSE;
    gc->stopImmediateGcFlag = ZR_FALSE;
    gc->isImmediateGcFlag = ZR_FALSE;

    gc->permanentObjectList = ZR_NULL;
    gc->waitToScanObjectList = ZR_NULL;
    gc->waitToScanAgainObjectList = ZR_NULL;
    gc->waitToReleaseObjectList = ZR_NULL;
    gc->releasedObjectList = ZR_NULL;

    gc->heapLimitBytes = 0;
    gc->youngRegionSize = 256u * 1024u;
    gc->youngRegionCountTarget = 4u;
    gc->survivorAgeThreshold = 2u;
    gc->pauseBudgetUs = 2000u;
    gc->remarkBudgetUs = 1000u;
    gc->workerCount = 1u;
    gc->fragmentationCompactThreshold = 35u;
    gc->gcFlags = 0u;
    gc->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    gc->minorCollectionEpoch = 0u;
    gc->statsSnapshot.heapLimitBytes = gc->heapLimitBytes;
    gc->statsSnapshot.managedMemoryBytes = 0u;
    gc->statsSnapshot.gcDebtBytes = 0;
    gc->statsSnapshot.pauseBudgetUs = gc->pauseBudgetUs;
    gc->statsSnapshot.remarkBudgetUs = gc->remarkBudgetUs;
    gc->statsSnapshot.workerCount = gc->workerCount;
    gc->statsSnapshot.ignoredObjectCount = 0u;
    gc->statsSnapshot.rememberedObjectCount = 0u;
    gc->statsSnapshot.regionCount = 0u;
    gc->statsSnapshot.edenRegionCount = 0u;
    gc->statsSnapshot.survivorRegionCount = 0u;
    gc->statsSnapshot.oldRegionCount = 0u;
    gc->statsSnapshot.pinnedRegionCount = 0u;
    gc->statsSnapshot.largeRegionCount = 0u;
    gc->statsSnapshot.permanentRegionCount = 0u;
    gc->statsSnapshot.edenUsedBytes = 0u;
    gc->statsSnapshot.survivorUsedBytes = 0u;
    gc->statsSnapshot.oldUsedBytes = 0u;
    gc->statsSnapshot.pinnedUsedBytes = 0u;
    gc->statsSnapshot.largeUsedBytes = 0u;
    gc->statsSnapshot.permanentUsedBytes = 0u;
    gc->statsSnapshot.edenLiveBytes = 0u;
    gc->statsSnapshot.survivorLiveBytes = 0u;
    gc->statsSnapshot.oldLiveBytes = 0u;
    gc->statsSnapshot.pinnedLiveBytes = 0u;
    gc->statsSnapshot.largeLiveBytes = 0u;
    gc->statsSnapshot.permanentLiveBytes = 0u;
    gc->statsSnapshot.lastStepDurationUs = 0u;
    gc->statsSnapshot.lastStepWork = 0u;
    gc->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->statsSnapshot.collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    memset(gc->collectionCounts, 0, sizeof(gc->collectionCounts));
    memset(gc->collectionTotalDurationUs, 0, sizeof(gc->collectionTotalDurationUs));
    memset(gc->collectionMaxDurationUs, 0, sizeof(gc->collectionMaxDurationUs));
    garbage_collector_refresh_cumulative_snapshot(gc);
}

void ZrCore_GarbageCollector_Free(SZrGlobalState *global, SZrGarbageCollector *collector) {
    TZrSize ignoredBytes;
    TZrSize regionBytes;

    if (global == ZR_NULL || collector == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    collector->ignoredObjectCount = 0;

    if (global->mainThreadState != ZR_NULL) {
        SZrState *state = global->mainThreadState;
        SZrRawObject *stateObject = ZR_CAST_RAW_OBJECT_AS_SUPER(state);

        if (stateObject != ZR_NULL &&
            stateObject->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
            stateObject->type != ZR_RAW_OBJECT_TYPE_INVALID) {
            TZrSize maxIterations = ZR_GC_SHUTDOWN_FULL_COLLECTION_ITERATION_LIMIT;
            TZrSize iterationCount = 0;

            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            global->garbageCollector->waitToScanAgainObjectList = ZR_NULL;
            global->garbageCollector->waitToReleaseObjectList = ZR_NULL;

            ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
            while (global->garbageCollector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
                if (++iterationCount > maxIterations) {
                    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
                    break;
                }

                garbage_collector_single_step(state);
                if (global->garbageCollector->stopGcFlag) {
                    break;
                }
            }
        }
    }

    collector->gcObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->permanentObjectList = ZR_NULL;
    collector->aliveObjectList = ZR_NULL;
    collector->circleMoreObjectList = ZR_NULL;
    collector->circleOnceObjectList = ZR_NULL;
    collector->aliveObjectWithReleaseFunctionList = ZR_NULL;
    collector->circleMoreObjectWithReleaseFunctionList = ZR_NULL;
    collector->circleOnceObjectWithReleaseFunctionList = ZR_NULL;

    if (collector->ignoredObjects != ZR_NULL) {
        ignoredBytes = collector->ignoredObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(
                global, collector->ignoredObjects, ignoredBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        collector->ignoredObjects = ZR_NULL;
    }
    collector->ignoredObjectCapacity = 0;
    if (collector->rememberedObjects != ZR_NULL) {
        TZrSize rememberedBytes = collector->rememberedObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(
                global, collector->rememberedObjects, rememberedBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        collector->rememberedObjects = ZR_NULL;
    }
    collector->rememberedObjectCapacity = 0;
    collector->rememberedObjectCount = 0;
    if (collector->regions != ZR_NULL) {
        regionBytes = collector->regionCapacity * sizeof(SZrGarbageCollectRegionDescriptor);
        ZrCore_Memory_RawFreeWithType(global, collector->regions, regionBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        collector->regions = ZR_NULL;
    }
    collector->regionCount = 0;
    collector->regionCapacity = 0;

    ZrCore_Memory_RawFreeWithType(global, collector, sizeof(SZrGarbageCollector), ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

void ZrCore_GarbageCollector_AddDebtSpace(SZrGlobalState *global, TZrMemoryOffset size) {
    TZrMemoryOffset currentDebt = global->garbageCollector->gcDebtSize;

    if (size > 0) {
        if (currentDebt > ZR_MAX_MEMORY_OFFSET - size) {
            global->garbageCollector->gcDebtSize = ZR_MAX_MEMORY_OFFSET;
        } else {
            global->garbageCollector->gcDebtSize = currentDebt + size;
        }
    } else if (size < 0) {
        TZrMemoryOffset newDebt = currentDebt + size;
        if (newDebt < 0 || newDebt > currentDebt) {
            global->garbageCollector->gcDebtSize = 0;
        } else {
            global->garbageCollector->gcDebtSize = newDebt;
        }
    }
}

TZrBool ZrCore_GarbageCollector_IgnoreObject(SZrState *state, SZrRawObject *object) {
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    if (collector == ZR_NULL) {
        return ZR_FALSE;
    }

    if (garbage_collector_ignore_registry_contains(collector, object)) {
        garbage_collector_mark_ignored_root_if_needed(state, object);
        return ZR_TRUE;
    }

    if (!garbage_collector_ensure_ignore_registry_capacity(state->global, collector->ignoredObjectCount + 1)) {
        return ZR_FALSE;
    }

    collector->ignoredObjects[collector->ignoredObjectCount++] = object;
    garbage_collector_mark_ignored_root_if_needed(state, object);
    return ZR_TRUE;
}

TZrBool ZrCore_GarbageCollector_UnignoreObject(SZrGlobalState *global, SZrRawObject *object) {
    SZrGarbageCollector *collector;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    for (TZrSize i = 0; i < collector->ignoredObjectCount; i++) {
        if (collector->ignoredObjects[i] == object) {
            for (TZrSize move = i + 1; move < collector->ignoredObjectCount; move++) {
                collector->ignoredObjects[move - 1] = collector->ignoredObjects[move];
            }
            collector->ignoredObjectCount--;
            if (collector->ignoredObjects != ZR_NULL) {
                collector->ignoredObjects[collector->ignoredObjectCount] = ZR_NULL;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrCore_GarbageCollector_IsObjectIgnored(SZrGlobalState *global, SZrRawObject *object) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    return garbage_collector_ignore_registry_contains(global->garbageCollector, object);
}

void ZrCore_GarbageCollector_GcFull(SZrState *state, TZrBool isImmediate) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;
    TZrUInt64 startedUs;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    startedUs = garbage_collector_now_us();

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
    collector->isImmediateGcFlag = isImmediate;
    if (collector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL) {
        work = garbage_collector_run_generational_full(state);
    } else {
        garbage_collector_full_inc(state, global);
        work = collector->gcLastStepWork;
    }
    collector->isImmediateGcFlag = ZR_FALSE;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->gcLastStepWork = work > 0 ? work : 1;
    garbage_collector_record_step_telemetry(collector, startedUs);
    if (collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    }
}

void ZrCore_GarbageCollector_GcStep(SZrState *state) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;
    TZrMemoryOffset debtBefore;
    EZrGarbageCollectRunningStatus statusBefore;
    TZrSize totalWork = 0;
    TZrSize pauseBudget;
    TZrUInt64 startedUs;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    collector->gcLastStepWork = 0;
    debtBefore = collector->gcDebtSize;
    statusBefore = collector->gcRunningStatus;
    startedUs = garbage_collector_now_us();

    if (!gcrunning(global)) {
        if (collector->gcDebtSize > 0) {
            collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;
        } else {
            ZrCore_GarbageCollector_AddDebtSpace(global, -ZR_GC_DEBT_CREDIT_BYTES);
            collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
            collector->statsSnapshot.lastStepDurationUs = 0u;
            collector->statsSnapshot.lastStepWork = 0u;
            garbage_collector_refresh_cumulative_snapshot(collector);
            return;
        }
    }

    if (garbage_collector_is_generational_mode(global)) {
        collector->statsSnapshot.lastCollectionKind = collector->scheduledCollectionKind;
        collector->collectionPhase = collector->scheduledCollectionKind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR
                                             ? ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK
                                             : ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
        garbage_collector_run_generational_step(state);
        collector->gcLastStepWork = 1;
    } else {
        collector->statsSnapshot.lastCollectionKind = collector->scheduledCollectionKind;
        collector->collectionPhase = collector->scheduledCollectionKind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR
                                             ? ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK
                                             : ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
        pauseBudget = collector->gcPauseBudget > 0 ? (TZrSize)collector->gcPauseBudget : 1;
        for (TZrSize step = 0; step < pauseBudget; step++) {
            EZrGarbageCollectRunningStatus stepStatusBefore = collector->gcRunningStatus;
            TZrBool stepWasSweepPhase =
                    stepStatusBefore >= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS &&
                    stepStatusBefore <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
            TZrSize stepWork = garbage_collector_single_step(state);

            totalWork += stepWork;

            if (collector->stopGcFlag ||
                collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
                break;
            }

            if (stepWasSweepPhase || ZrCore_GarbageCollector_IsSweeping(global)) {
                break;
            }

            if (stepWork == 0 && stepStatusBefore == collector->gcRunningStatus) {
                break;
            }
        }

        collector->gcLastStepWork = totalWork;
        if (collector->gcLastStepWork > 0 && collector->gcDebtSize > 0) {
            TZrMemoryOffset workBytes = (TZrMemoryOffset)collector->gcLastStepWork * ZR_GC_WORK_TO_MEMORY_BYTES;
            ZrCore_GarbageCollector_AddDebtSpace(global, -workBytes);
        }
    }

    if (collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
        collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    }

    if (collector->gcLastStepWork == 0 &&
        (statusBefore != collector->gcRunningStatus || debtBefore != collector->gcDebtSize)) {
        collector->gcLastStepWork = 1;
    }
    garbage_collector_record_step_telemetry(collector, startedUs);
    collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
}

void ZrCore_GarbageCollector_SetHeapLimitBytes(SZrGlobalState *global, TZrMemoryOffset heapLimitBytes) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->heapLimitBytes = heapLimitBytes;
    global->garbageCollector->statsSnapshot.heapLimitBytes = heapLimitBytes;
}

void ZrCore_GarbageCollector_SetPauseBudgetUs(SZrGlobalState *global,
                                              TZrUInt64 pauseBudgetUs,
                                              TZrUInt64 remarkBudgetUs) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->pauseBudgetUs = pauseBudgetUs;
    global->garbageCollector->remarkBudgetUs = remarkBudgetUs;
    global->garbageCollector->gcPauseBudget = pauseBudgetUs > 0 ? pauseBudgetUs : 1u;
    global->garbageCollector->statsSnapshot.pauseBudgetUs = pauseBudgetUs;
    global->garbageCollector->statsSnapshot.remarkBudgetUs = remarkBudgetUs;
}

void ZrCore_GarbageCollector_SetWorkerCount(SZrGlobalState *global, TZrUInt32 workerCount) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->workerCount = workerCount;
    global->garbageCollector->statsSnapshot.workerCount = workerCount;
}

void ZrCore_GarbageCollector_ScheduleCollection(SZrGlobalState *global, EZrGarbageCollectCollectionKind kind) {
    garbage_collector_schedule_collection_internal(global, kind, ZR_TRUE);
}

void ZrCore_GarbageCollector_GetStatsSnapshot(SZrGlobalState *global, SZrGarbageCollectorStatsSnapshot *outSnapshot) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || outSnapshot == ZR_NULL) {
        return;
    }

    global->garbageCollector->statsSnapshot.heapLimitBytes = global->garbageCollector->heapLimitBytes;
    global->garbageCollector->statsSnapshot.pauseBudgetUs = global->garbageCollector->pauseBudgetUs;
    global->garbageCollector->statsSnapshot.remarkBudgetUs = global->garbageCollector->remarkBudgetUs;
    global->garbageCollector->statsSnapshot.workerCount = global->garbageCollector->workerCount;
    garbage_collector_refresh_pressure_snapshot(global->garbageCollector);
    global->garbageCollector->statsSnapshot.rememberedObjectCount =
            (TZrUInt32)global->garbageCollector->rememberedObjectCount;
    global->garbageCollector->statsSnapshot.lastStepWork = (TZrUInt64)global->garbageCollector->gcLastStepWork;
    garbage_collector_refresh_cumulative_snapshot(global->garbageCollector);
    *outSnapshot = global->garbageCollector->statsSnapshot;
}

TZrBool ZrCore_GarbageCollector_HasRememberedObject(SZrGlobalState *global, SZrRawObject *object) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    return garbage_collector_remembered_registry_contains(global->garbageCollector, object);
}

void ZrCore_GarbageCollector_MarkRawObjectEscaped(SZrState *state,
                                                  SZrRawObject *object,
                                                  TZrUInt32 escapeFlags,
                                                  TZrUInt32 scopeDepth,
                                                  EZrGarbageCollectPromotionReason promotionReason) {
    garbage_collector_mark_raw_object_escaped_internal(
            state,
            object,
            escapeFlags,
            scopeDepth,
            promotionReason,
            ZR_TRUE);
}

void ZrCore_GarbageCollector_MarkValueEscaped(SZrState *state,
                                              const SZrTypeValue *value,
                                              TZrUInt32 escapeFlags,
                                              TZrUInt32 scopeDepth,
                                              EZrGarbageCollectPromotionReason promotionReason) {
    SZrRawObject *object;

    if (value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object == ZR_NULL) {
        return;
    }

    garbage_collector_mark_raw_object_escaped_internal(
            state,
            object,
            escapeFlags,
            scopeDepth,
            promotionReason,
            ZR_TRUE);
}

void ZrCore_GarbageCollector_PinObject(SZrState *state,
                                       SZrRawObject *object,
                                       EZrGarbageCollectPinKind pinKind) {
    TZrUInt32 escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE;
    TZrSize objectSize;
    TZrUInt32 previousRegionId;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return;
    }

    objectSize = garbage_collector_get_object_base_size(state, object);
    previousRegionId = object->garbageCollectMark.regionId;
    object->garbageCollectMark.pinFlags |= (TZrUInt32)pinKind;
    if ((pinKind & ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE) != 0) {
        escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE;
    }
    if ((pinKind & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0) {
        escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE;
    }
    if ((pinKind & ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT) != 0) {
        escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
    }
    ZrCore_RawObject_SetStorageKind(object, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED);
    ZrCore_RawObject_SetRegionKind(object, ZR_GARBAGE_COLLECT_REGION_KIND_PINNED);
    object->garbageCollectMark.regionId = garbage_collector_reassign_region_id(
            state->global, previousRegionId, ZR_GARBAGE_COLLECT_REGION_KIND_PINNED, objectSize);
    garbage_collector_mark_raw_object_escaped_internal(state,
                                                       object,
                                                       escapeFlags,
                                                       object->garbageCollectMark.anchorScopeDepth,
                                                       ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED,
                                                       ZR_TRUE);
}

TZrBool ZrCore_GarbageCollector_IsInvariant(SZrGlobalState *global) {
    return global->garbageCollector->gcRunningStatus <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC;
}

TZrBool ZrCore_GarbageCollector_IsSweeping(SZrGlobalState *global) {
    EZrGarbageCollectRunningStatus status = global->garbageCollector->gcRunningStatus;

    return status >= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS &&
           status <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
}

void ZrCore_GarbageCollector_CheckGc(SZrState *state) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;

    if (collector->heapLimitBytes > 0 && collector->managedMemories >= collector->heapLimitBytes) {
        garbage_collector_schedule_collection_internal(global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL, ZR_FALSE);
    }

    if (collector->gcDebtSize > 0) {
        ZrCore_GarbageCollector_GcStep(state);
    }
#if defined(ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST)
    if (collector->gcStatus == ZR_GARBAGE_COLLECT_STATUS_RUNNING) {
        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
    }
#endif
}
