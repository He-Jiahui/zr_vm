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
            TZrSize maxIterations = 1000;
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
            ZrCore_GarbageCollector_AddDebtSpace(global, -2000);
            collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
            return;
        }
    }

    if (garbage_collector_is_generational_mode(global)) {
        garbage_collector_run_generational_step(state);
        collector->gcLastStepWork = 1;
    } else {
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
            TZrMemoryOffset workBytes = (TZrMemoryOffset)collector->gcLastStepWork * ZR_WORK_TO_MEM;
            ZrCore_GarbageCollector_AddDebtSpace(global, -workBytes);
        }
    }

    if (collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    }

    if (collector->gcLastStepWork == 0 &&
        (statusBefore != collector->gcRunningStatus || debtBefore != collector->gcDebtSize)) {
        collector->gcLastStepWork = 1;
    }
    collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
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
    SZrGlobalState *global = state->global;

    if (global->garbageCollector->gcDebtSize > 0) {
        ZrCore_GarbageCollector_GcStep(state);
    }
#if defined(ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST)
    if (global->garbageCollector->gcStatus == ZR_GARBAGE_COLLECT_STATUS_RUNNING) {
        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
    }
#endif
}
