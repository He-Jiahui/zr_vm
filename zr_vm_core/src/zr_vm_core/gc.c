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

void ZrGarbageCollectorInit(SZrGlobalState *global) {
    SZrGarbageCollector *gc = &global->garbageCollector;
    SZrState *state = global->mainThreadState;;
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
    gc->gcInitializeObjectStatus = ZR_GARBAGE_COLLECT_OBJECT_STATUS_INITED;
    gc->stopGcFlag = ZR_FALSE;
    gc->stopImmediateGcFlag = ZR_FALSE;
    gc->isImmediateGcFlag = ZR_FALSE;


    gc->permanentObjectList = ZR_NULL;
    gc->waitToReleaseObjectList = ZR_NULL;
    gc->releasedObjectList = ZR_NULL;
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

SZrRawObject *ZrRawObjectNew(SZrState *state, EZrValueType type, TZrSize size) {
    SZrGlobalState *global = state->global;
    TZrPtr memory = ZrMemoryGcMalloc(state, type, size);
    SZrRawObject *object = ZR_CAST_RAW_OBJECT(memory);
    ZrRawObjectInit(object, type);
    object->garbageCollectMark.status = global->garbageCollector.gcInitializeObjectStatus;
    // object->garbageCollectMark.generations = 0;
    object->next = global->garbageCollector.gcObjectList;
    global->garbageCollector.gcObjectList = object;
    return object;
}

void ZrRawObjectMarkAsPermanent(SZrState *state, SZrRawObject *object) {
    ZR_ASSERT(object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_OBJECT_STATUS_INITED);
    SZrGlobalState *global = state->global;
    // we assume that the object is the first object in gc list (latest created)
    ZR_ASSERT(global->garbageCollector.gcObjectList == object);
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_OBJECT_STATUS_PERMANENT;
    global->garbageCollector.gcObjectList = object->next;
    object->next = global->garbageCollector.permanentObjectList;
    global->garbageCollector.permanentObjectList = object;
}
