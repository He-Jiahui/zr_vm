//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_GC_H
#define ZR_VM_CORE_GC_H

#include "zr_vm_core/conf.h"
struct SZrGlobalState;

// generational mode
struct ZR_STRUCT_ALIGN SZrGarbageCollector {
    EZrGarbageCollectStatus gcStatus;
    EZrGarbageCollectRunningStatus gcRunningStatus;
    SZrRawObject *gcObjectList;
    SZrRawObject **gcObjectListSweeper;
    SZrRawObject *waitToReleaseObjectList;
    SZrRawObject *releasedObjectList;
    SZrRawObject *permanentObjectList;
    TBool stopGcFlag;
    TBool stopImmediateGcFlag;
    TBool isImmediateGcFlag;

    TUInt64 gcMinorGenerationMultiplier;
    TUInt64 gcMajorGenerationMultiplier;
    TUInt64 gcStepMultiplierPercent;
    TUInt64 gcStepSizeLog2;
    TUInt64 gcPauseThresholdPercent;
    SZrRawObject *aliveObjectList;
    SZrRawObject *circleMoreObjectList;
    SZrRawObject *circleOnceObjectList;
    SZrRawObject *aliveObjectWithReleaseFunctionList;
    SZrRawObject *circleMoreObjectWithReleaseFunctionList;
    SZrRawObject *circleOnceObjectWithReleaseFunctionList;
};

typedef struct SZrGarbageCollector SZrGarbageCollector;

ZR_CORE_API void ZrGarbageCollectorInit(struct SZrGlobalState *global);

#endif //ZR_VM_CORE_GC_H
