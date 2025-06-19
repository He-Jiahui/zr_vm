//
// Created by HeJiahui on 2025/6/20.
//

#include "zr_vm_core/gc.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/global.h"

void ZrGarbageCollectorInit(SZrGlobalState *global) {
    SZrGarbageCollector *gc = &global->garbageCollector;
    SZrState *state = global->mainThreadState;;
    // reset gc
    // reference new created state to global gc list
    gc->gcObjectList = ZR_CAST_RAW_OBJECT(state);

    // init gc parameters
    gc->gcMajorGenerationMultiplier = ZR_GARBAGE_COLLECT_MAJOR_MULTIPLIER;
    gc->gcMinorGenerationMultiplier = ZR_GARBAGE_COLLECT_MINOR_MULTIPLIER;
    gc->gcStepMultiplierPercent = ZR_GARBAGE_COLLECT_STEP_MULTIPLIER_PERCENT;
    gc->gcStepSizeLog2 = ZR_GARBAGE_COLLECT_STEP_LOG2_SIZE;
    gc->gcPauseThresholdPercent = ZR_GARBAGE_COLLECT_PAUSE_THRESHOLD_PERCENT;

    gc->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    gc->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
    gc->stopGcFlag = ZR_FALSE;
    gc->stopImmediateGcFlag = ZR_FALSE;
    gc->isImmediateGcFlag = ZR_FALSE;


    gc->permanentObjectList = ZR_NULL;
    gc->waitToReleaseObjectList = ZR_NULL;
    gc->releasedObjectList = ZR_NULL;
}
