//
// GC cycle, phase-transition, and scheduling helpers.
//

#include "gc_internal.h"

void garbage_collector_run_until_state(SZrState *state, EZrGarbageCollectRunningStatus targetState) {
    SZrGlobalState *global = state->global;
    TZrSize iterationCount = 0;
    const TZrSize maxIterations = ZR_GC_RUN_UNTIL_STATE_ITERATION_LIMIT;

    while (global->garbageCollector->gcRunningStatus != targetState) {
        if (++iterationCount > maxIterations) {
            global->garbageCollector->gcRunningStatus = targetState;
            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            global->garbageCollector->waitToScanAgainObjectList = ZR_NULL;
            break;
        }

        garbage_collector_single_step(state);
        if (global->garbageCollector->stopGcFlag) {
            break;
        }
    }
}

void garbage_collector_check_sizes(SZrState *state, SZrGlobalState *global) {
    TZrMemoryOffset actualMemories = 0;
    SZrRawObject *object = global->garbageCollector->gcObjectList;

    while (object != ZR_NULL) {
        if (!ZrCore_RawObject_IsUnreferenced(state, object)) {
            actualMemories += garbage_collector_get_object_base_size(state, object);
        }
        object = object->next;
    }

    TZrMemoryOffset estimatedMemories = global->garbageCollector->managedMemories;
    TZrMemoryOffset difference = actualMemories - estimatedMemories;
    TZrMemoryOffset driftTolerance = estimatedMemories / ZR_GC_MANAGED_MEMORY_DRIFT_TOLERANCE_DIVISOR;
    if (difference > driftTolerance || difference < -driftTolerance) {
        global->garbageCollector->managedMemories = actualMemories;
    }
}

TZrSize garbage_collector_run_generational_full(SZrState *state) {
    SZrGlobalState *global = state->global;
    TZrSize work = 0;
    TZrSize sweepIterationCount = 0;
    const TZrSize maxSweepIterations = ZR_GC_GENERATIONAL_FULL_SWEEP_ITERATION_LIMIT;
    SZrRawObject **previousSweeper = ZR_NULL;
    SZrRawObject *object;

    ZrGarbageCollectorRestartCollection(state);
    work += ZrGarbageCollectorPropagateAll(state);
    work += garbage_collector_atomic(state);
    garbage_collector_enter_sweep(state);

    while (global->garbageCollector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END) {
        if (++sweepIterationCount > maxSweepIterations) {
            global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
            global->garbageCollector->gcObjectListSweeper = ZR_NULL;
            break;
        }

        if (global->garbageCollector->gcObjectListSweeper == previousSweeper && previousSweeper != ZR_NULL) {
            global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
            global->garbageCollector->gcObjectListSweeper = ZR_NULL;
            break;
        }
        previousSweeper = global->garbageCollector->gcObjectListSweeper;
        garbage_collector_sweep_step(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END, NULL);
    }

    garbage_collector_check_sizes(state, global);
    while (global->garbageCollector->waitToReleaseObjectList != ZR_NULL) {
        garbage_collector_run_a_few_finalizers(state, ZR_GC_FINALIZER_BATCH_MAX);
    }

    object = global->garbageCollector->gcObjectList;
    while (object != ZR_NULL) {
        if (!ZrCore_RawObject_IsUnreferenced(state, object) &&
            object->garbageCollectMark.generationalStatus == ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW) {
            object->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL;
        }
        object = object->next;
    }

    global->garbageCollector->gcObjectListSweeper = ZR_NULL;
    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;

    return work;
}

TZrSize garbage_collector_process_weak_tables(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);

    /*
     * Weak table support is not wired up yet. The previous placeholder walked
     * every object nodeMap and eagerly removed entries based on GC color,
     * which corrupts ordinary tables during shutdown/full collections.
     * Leave the phase as a no-op until dedicated weak-table metadata exists.
     */
    return 0;
}

TZrSize garbage_collector_atomic(SZrState *state) {
    SZrGlobalState *global;
    TZrSize work = 0;
    SZrRawObject *stateObject;

    if (state == ZR_NULL) {
        return 0;
    }

    global = state->global;
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return 0;
    }

    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC;

    stateObject = ZR_CAST_RAW_OBJECT_AS_SUPER(state);
    if (stateObject != ZR_NULL &&
        stateObject->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
        stateObject->type != ZR_RAW_OBJECT_TYPE_INVALID) {
        ZrGarbageCollectorReallyMarkObject(state, stateObject);
    }

    if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry)) {
        garbage_collector_mark_value(state, &global->loadedModulesRegistry);
    }
    if (global->errorPrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->errorPrototype));
    }
    if (global->stackFramePrototype != ZR_NULL) {
        garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->stackFramePrototype));
    }
    if (global->hasUnhandledExceptionHandler &&
        ZrCore_Value_IsGarbageCollectable(&global->unhandledExceptionHandler)) {
        garbage_collector_mark_value(state, &global->unhandledExceptionHandler);
    }

    work += garbage_collector_mark_string_roots(state);

    for (TZrSize i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        if (global->basicTypeObjectPrototype[i] != ZR_NULL) {
            garbage_collector_mark_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->basicTypeObjectPrototype[i]));
        }
    }

    work += ZrGarbageCollectorPropagateAll(state);
    work += garbage_collector_process_weak_tables(state);
    global->garbageCollector->gcGeneration = ZR_GC_OTHER_GENERATION(global->garbageCollector);
    return work;
}

int garbage_collector_sweep_step(SZrState *state,
                                 EZrGarbageCollectRunningStatus nextstate,
                                 SZrRawObject **nextlist) {
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    int sweepBudget;

    if (collector->gcObjectListSweeper) {
        TZrMemoryOffset olddebt = global->garbageCollector->gcDebtSize;
        TZrUInt64 maxSweepSliceBudget = (TZrUInt64)ZR_GC_SWEEP_SLICE_BUDGET_MAX;
        int count;

        sweepBudget = (collector->gcSweepSliceBudget == 0)
                              ? 1
                              : (collector->gcSweepSliceBudget > maxSweepSliceBudget
                                         ? (int)maxSweepSliceBudget
                                         : (int)collector->gcSweepSliceBudget);
        collector->gcObjectListSweeper =
                garbage_collector_sweep_list(state, collector->gcObjectListSweeper, sweepBudget, &count);
        collector->managedMemories += global->garbageCollector->gcDebtSize - olddebt;
        if (collector->gcObjectListSweeper != ZR_NULL && *collector->gcObjectListSweeper == ZR_NULL) {
            collector->gcRunningStatus = nextstate;
            collector->gcObjectListSweeper = nextlist;
        }
        return count;
    }

    collector->gcRunningStatus = nextstate;
    collector->gcObjectListSweeper = nextlist;
    return 0;
}

TZrSize garbage_collector_single_step(SZrState *state) {
    SZrGlobalState *global = state->global;
    TZrSize work;

    switch (global->garbageCollector->gcRunningStatus) {
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED:
            ZrGarbageCollectorRestartCollection(state);
            global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION;
            work = 1;
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION:
            if (global->garbageCollector->waitToScanObjectList == NULL) {
                global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC;
                work = 0;
            } else {
                work = ZrGarbageCollectorPropagateMark(state);
            }
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC:
            work = garbage_collector_atomic(state);
            garbage_collector_enter_sweep(state);
            global->garbageCollector->managedMemories = global->garbageCollector->managedMemories;
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS:
            work = garbage_collector_sweep_step(
                    state,
                    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS,
                    &global->garbageCollector->waitToReleaseObjectList);
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_WAIT_TO_RELEASE_OBJECTS:
            work = garbage_collector_sweep_step(
                    state,
                    ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_RELEASED_OBJECTS,
                    &global->garbageCollector->waitToReleaseObjectList);
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_RELEASED_OBJECTS:
            work = garbage_collector_sweep_step(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END, NULL);
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END:
            garbage_collector_check_sizes(state, global);
            global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_END;
            work = 0;
            break;
        case ZR_GARBAGE_COLLECT_RUNNING_STATUS_END:
            if (global->garbageCollector->waitToReleaseObjectList &&
                !global->garbageCollector->isImmediateGcFlag) {
                global->garbageCollector->stopImmediateGcFlag = 0;
                work = garbage_collector_run_a_few_finalizers(state, ZR_GC_FINALIZER_BATCH_MAX);
            } else {
                global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
                work = 0;
            }
            break;
        default:
            return 0;
    }

    return work;
}

TZrBool garbage_collector_is_generational_mode(SZrGlobalState *global) {
    return global->garbageCollector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL ||
           global->garbageCollector->atomicMemories != 0;
}

void garbage_collector_full_inc(SZrState *state, SZrGlobalState *global) {
    if (ZrCore_GarbageCollector_IsInvariant(global)) {
        garbage_collector_enter_sweep(state);
    }

    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED);
    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_FLAG_PROPAGATION);
    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_BEFORE_ATOMIC;
    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_END);
    garbage_collector_run_until_state(state, ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED);
    ZrCore_GarbageCollector_AddDebtSpace(global, -ZR_GC_DEBT_CREDIT_BYTES);
}

TZrBool gcrunning(SZrGlobalState *global) {
    return global->garbageCollector->gcStatus == ZR_GARBAGE_COLLECT_STATUS_RUNNING;
}

void garbage_collector_run_generational_step(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrGarbageCollector *collector = global->garbageCollector;
    TZrMemoryOffset managedMemories = collector->managedMemories;
    TZrMemoryOffset threshold = collector->gcPauseThresholdPercent * managedMemories / 100;
    SZrRawObject *object;
    SZrRawObject **current;

    if (collector->scheduledCollectionKind != ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR ||
        (collector->heapLimitBytes > 0 && managedMemories >= collector->heapLimitBytes) ||
        managedMemories > threshold) {
        garbage_collector_run_generational_full(state);
        collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
        collector->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
        return;
    }

    object = global->garbageCollector->gcObjectList;
    while (object != ZR_NULL) {
        if (object->garbageCollectMark.generationalStatus == ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW &&
            ZrCore_RawObject_IsMarkInited(object)) {
            ZrGarbageCollectorReallyMarkObject(state, object);
        }
        object = object->next;
    }

    ZrGarbageCollectorPropagateAll(state);

    current = &global->garbageCollector->gcObjectList;
    while (*current != ZR_NULL) {
        object = *current;
        if (object->garbageCollectMark.generationalStatus == ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW) {
            if (ZrCore_RawObject_IsUnreferenced(state, object)) {
                *current = object->next;
                garbage_collector_free_object(state, object);
            } else {
                object->garbageCollectMark.generationalStatus =
                        ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL;
                current = &object->next;
            }
        } else {
            current = &object->next;
        }
    }
}
