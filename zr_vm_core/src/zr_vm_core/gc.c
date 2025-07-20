//
// Created by HeJiahui on 2025/6/20.
//

#include "zr_vm_core/gc.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"


static TZrSize ZrGarbageCollectorRunGenerationalFull(SZrState *state) {
    ZR_TODO_PARAMETER(state);
    return 0;
}

static TZrSize ZrGarbageCollectorRunIncreasementFull(SZrState *state) {
    ZR_TODO_PARAMETER(state);
    return 0;
}

static void ZrGarbageCollectorRunGenerationalStep(SZrState *state) { ZR_TODO_PARAMETER(state); }

static TZrSize ZrGarbageCollectorRunIncreasementStep(SZrState *state) {
    ZR_TODO_PARAMETER(state);
    return 0;
}

static ZR_FORCE_INLINE TBool ZrGarbageCollectorIsGenerationalMode(SZrGlobalState *global) {
    return global->garbageCollector.gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL ||
           global->garbageCollector.atomicMemories != 0;
}

void ZrGarbageCollectorInit(SZrGlobalState *global) {
    SZrGarbageCollector *gc = &global->garbageCollector;
    SZrState *state = global->mainThreadState;
    // set size
    gc->managedMemories = sizeof(SZrGlobalState) + sizeof(SZrState);
    gc->gcDebtSize = 0;
    gc->atomicMemories = 0;

    // reset gc
    // reference new created state to global gc list
    gc->gcObjectList = ZR_CAST_RAW_OBJECT_AS_SUPER(state);

    // init gc parameters
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
    // todo:
}

void ZrGarbageCollectorAddDebtSpace(struct SZrGlobalState *global, TZrMemoryOffset size) {
    TZrMemoryOffset totalSize = global->garbageCollector.gcDebtSize + global->garbageCollector.gcDebtSize;
    ZR_ASSERT(totalSize > 0);
    if (ZR_UNLIKELY(size < totalSize - ZR_MAX_MEMORY_OFFSET)) {
        size = totalSize - ZR_MAX_MEMORY_OFFSET;
    }
    global->garbageCollector.managedMemories = totalSize - size;
    global->garbageCollector.gcDebtSize = size;
}

void ZrGarbageCollectorGcFull(SZrState *state, TBool isImmediate) {
    SZrGlobalState *global = state->global;
    ZR_ASSERT(!global->garbageCollector.isImmediateGcFlag);
    global->garbageCollector.isImmediateGcFlag = isImmediate;
    if (global->garbageCollector.gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL) {
        ZrGarbageCollectorRunGenerationalFull(state);
    } else {
        ZrGarbageCollectorRunIncreasementFull(state);
    }
    global->garbageCollector.isImmediateGcFlag = ZR_FALSE;
}

ZR_CORE_API void ZrGarbageCollectorGcStep(struct SZrState *state) {
    SZrGlobalState *global = state->global;
    if (global->garbageCollector.gcStatus != ZR_GARBAGE_COLLECT_STATUS_RUNNING) {
        ZrGarbageCollectorAddDebtSpace(global, ZR_GARBAGE_COLLECT_DEBT_SIZE);
    } else {
        if (ZrGarbageCollectorIsGenerationalMode(global)) {
            ZrGarbageCollectorRunGenerationalStep(state);
        } else {
            ZrGarbageCollectorRunIncreasementStep(state);
        }
    }
}

TBool ZrGarbageCollectorIsInvariant(struct SZrGlobalState *global) {
    return global->garbageCollector.gcStatus <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC;
}

TBool ZrGarbageCollectorIsSweeping(struct SZrGlobalState *global) {
    EZrGarbageCollectRunningStatus status = global->garbageCollector.gcStatus;
    return status >= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS &&
           status <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
}

void ZrGarbageCollectorCheckGc(struct SZrState *state) {
    SZrGlobalState *global = state->global;
    if (global->garbageCollector.gcDebtSize > 0) {
        ZrGarbageCollectorGcStep(state);
    }
#if defined(ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST)
    if (global->garbageCollector.gcStatus == ZR_GARBAGE_COLLECT_STATUS_RUNNING) {
        ZrGarbageCollectorGcFull(state, ZR_FALSE);
    }
#endif
}

static void ZrGarbageCollectorMarkObject(struct SZrState *state, SZrRawObject *object);
static ZR_FORCE_INLINE void ZrGarbageCollectorMarkValue(struct SZrState *state, SZrTypeValue *value) {
    ZrGlobalValueStaticAssertIsAlive(state, value);
    if (ZrValueIsGarbageCollectable(value) && ZrRawObjectIsMarkInited(value->value.object)) {
        ZrGarbageCollectorMarkObject(state, value->value.object);
    }
}

static ZR_FORCE_INLINE void ZrGarbageCollectorToGcListAndMarkWaitToScan(SZrRawObject *object,
                                                                        SZrRawObject **objectList) {
    ZR_ASSERT(!ZrRawObjectIsMarkWaitToScan(object));
    SZrRawObject **gcList = &object->gcList;
    *gcList = *objectList;
    *objectList = object;
    ZrRawObjectMarkAsWaitToScan(object);
}

static void ZrGarbageCollectorMarkObject(struct SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;
    switch (object->type) {
        case ZR_RAW_OBJECT_TYPE_STRING: {
            ZrRawObjectMarkAsReferenced(object);
        } break;
        case ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE: {
            SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, object);
            if (ZrClosureValueIsClosed(closureValue)) {
                ZrRawObjectMarkAsReferenced(object);
            } else {
                ZrRawObjectMarkAsWaitToScan(object);
            }
            ZrGarbageCollectorMarkValue(state, &closureValue->value.valuePointer->value);
        } break;
        case ZR_RAW_OBJECT_TYPE_NATIVE_DATA: {
            // todo: native data is not finished
        } break;
        case ZR_RAW_OBJECT_TYPE_BUFFER:
        case ZR_RAW_OBJECT_TYPE_ARRAY:
        case ZR_RAW_OBJECT_TYPE_FUNCTION:
        case ZR_RAW_OBJECT_TYPE_OBJECT:
        case ZR_RAW_OBJECT_TYPE_THREAD: {
            ZrGarbageCollectorToGcListAndMarkWaitToScan(object, &global->garbageCollector.waitToScanObjectList);
        } break;
        default: {
            ZR_ASSERT(ZR_FALSE);
        } break;
    }
}

void ZrGarbageCollectorBarrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject) {
    SZrGlobalState *global = state->global;
    ZR_ASSERT(ZrRawObjectIsMarkReferenced(object) && ZrRawObjectIsMarkInited(valueObject) &&
              !ZrRawObjectIsUnreferenced(state, valueObject));
    if (ZrGarbageCollectorIsInvariant(global)) {
        ZrGarbageCollectorMarkObject(state, valueObject);
        if (ZrRawObjectIsGenerationalThroughBarrier(object)) {
            ZR_ASSERT(!ZrRawObjectIsGenerationalThroughBarrier(valueObject));
            ZrRawObjectSetGenerationalStatus(valueObject, ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER);
        }
    } else {
        ZR_ASSERT(ZrGarbageCollectorIsSweeping(global));
        if (global->garbageCollector.gcMode == ZR_GARBAGE_COLLECT_MODE_INCREMENTAL) {
            ZrRawObjectMarkAsInit(state, valueObject);
        }
    }
}

SZrRawObject *ZrRawObjectNew(SZrState *state, EZrValueType type, TZrSize size, TBool isNative) {
    SZrGlobalState *global = state->global;
    TZrPtr memory = ZrMemoryGcMalloc(state, type, size);
    SZrRawObject *object = ZR_CAST_RAW_OBJECT(memory);
    ZrRawObjectConstruct(object, type);
    object->isNative = isNative;
    object->garbageCollectMark.status = global->garbageCollector.gcInitializeObjectStatus;
    object->garbageCollectMark.generation = global->garbageCollector.gcGeneration;
    object->next = global->garbageCollector.gcObjectList;
    global->garbageCollector.gcObjectList = object;
    return object;
}

TBool ZrRawObjectIsUnreferenced(struct SZrState *state, SZrRawObject *object) {
    EZrGarbageCollectIncrementalObjectStatus status = object->garbageCollectMark.status;
    EZrGarbageCollectGeneration generation = object->garbageCollectMark.generation;
    SZrGlobalState *global = state->global;
    return status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED ||
           status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED ||
           (status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED &&
            generation != global->garbageCollector.gcGeneration);
}


void ZrRawObjectMarkAsInit(struct SZrState *state, SZrRawObject *object) {
    SZrGlobalState *global = state->global;
    EZrGarbageCollectGeneration generation = global->garbageCollector.gcGeneration;
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    object->garbageCollectMark.generation = generation;
}

void ZrRawObjectMarkAsPermanent(SZrState *state, SZrRawObject *object) {
    ZR_ASSERT(object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED);
    SZrGlobalState *global = state->global;
    // we assume that the object is the first object in gc list (latest created)
    ZR_ASSERT(global->garbageCollector.gcObjectList == object);
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
    global->garbageCollector.gcObjectList = object->next;
    object->next = global->garbageCollector.permanentObjectList;
    global->garbageCollector.permanentObjectList = object;
}
