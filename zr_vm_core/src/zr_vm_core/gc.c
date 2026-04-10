//
// Public GC lifecycle and scheduling entry points.
//

#include "gc_internal.h"

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
    gc->statsSnapshot.heapLimitBytes = gc->heapLimitBytes;
    gc->statsSnapshot.pauseBudgetUs = gc->pauseBudgetUs;
    gc->statsSnapshot.remarkBudgetUs = gc->remarkBudgetUs;
    gc->statsSnapshot.workerCount = gc->workerCount;
    gc->statsSnapshot.rememberedObjectCount = 0u;
    gc->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->statsSnapshot.collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
}

void ZrCore_GarbageCollector_Free(SZrGlobalState *global, SZrGarbageCollector *collector) {
    TZrSize ignoredBytes;

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
        return ZR_TRUE;
    }

    if (!garbage_collector_ensure_ignore_registry_capacity(state->global, collector->ignoredObjectCount + 1)) {
        return ZR_FALSE;
    }

    collector->ignoredObjects[collector->ignoredObjectCount++] = object;
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

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;

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
        garbage_collector_run_generational_full(state);
    } else {
        garbage_collector_full_inc(state, global);
    }
    collector->isImmediateGcFlag = ZR_FALSE;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
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

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    collector->gcLastStepWork = 0;
    debtBefore = collector->gcDebtSize;
    statusBefore = collector->gcRunningStatus;

    if (!gcrunning(global)) {
        if (collector->gcDebtSize > 0) {
            collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;
        } else {
            ZrCore_GarbageCollector_AddDebtSpace(global, -ZR_GC_DEBT_CREDIT_BYTES);
            collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
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
    SZrGarbageCollector *collector;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    collector = global->garbageCollector;
    collector->scheduledCollectionKind = kind;
    collector->statsSnapshot.lastRequestedCollectionKind = kind;
    if (collector->gcDebtSize <= 0) {
        ZrCore_GarbageCollector_AddDebtSpace(global, ZR_GC_DEBT_CREDIT_BYTES);
    }
}

void ZrCore_GarbageCollector_GetStatsSnapshot(SZrGlobalState *global, SZrGarbageCollectorStatsSnapshot *outSnapshot) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || outSnapshot == ZR_NULL) {
        return;
    }

    global->garbageCollector->statsSnapshot.heapLimitBytes = global->garbageCollector->heapLimitBytes;
    global->garbageCollector->statsSnapshot.pauseBudgetUs = global->garbageCollector->pauseBudgetUs;
    global->garbageCollector->statsSnapshot.remarkBudgetUs = global->garbageCollector->remarkBudgetUs;
    global->garbageCollector->statsSnapshot.workerCount = global->garbageCollector->workerCount;
    global->garbageCollector->statsSnapshot.rememberedObjectCount =
            (TZrUInt32)global->garbageCollector->rememberedObjectCount;
    *outSnapshot = global->garbageCollector->statsSnapshot;
}

TZrBool ZrCore_GarbageCollector_HasRememberedObject(SZrGlobalState *global, SZrRawObject *object) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    return garbage_collector_remembered_registry_contains(global->garbageCollector, object);
}

void ZrCore_GarbageCollector_PinObject(SZrState *state,
                                       SZrRawObject *object,
                                       EZrGarbageCollectPinKind pinKind) {
    TZrUInt32 escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return;
    }

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
    object->garbageCollectMark.escapeFlags |= escapeFlags;
    object->garbageCollectMark.promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
    ZrCore_RawObject_SetStorageKind(object, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED);
    ZrCore_RawObject_SetRegionKind(object, ZR_GARBAGE_COLLECT_REGION_KIND_PINNED);
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
        ZrCore_GarbageCollector_ScheduleCollection(global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL);
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
