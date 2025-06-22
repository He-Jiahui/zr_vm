//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_GC_H
#define ZR_VM_CORE_GC_H

#include "zr_vm_core/conf.h"
struct SZrGlobalState;
struct SZrState;

// generational mode
struct ZR_STRUCT_ALIGN SZrGarbageCollector {
    EZrGarbageCollectMode gcMode;
    EZrGarbageCollectStatus gcStatus;
    EZrGarbageCollectRunningStatus gcRunningStatus;
    EZrGarbageCollectObjectStatus gcInitializeObjectStatus;

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

    TZrSize gcAllocatedNotManagedSize;

    SZrRawObject *aliveObjectList;
    SZrRawObject *circleMoreObjectList;
    SZrRawObject *circleOnceObjectList;
    SZrRawObject *aliveObjectWithReleaseFunctionList;
    SZrRawObject *circleMoreObjectWithReleaseFunctionList;
    SZrRawObject *circleOnceObjectWithReleaseFunctionList;
};

typedef struct SZrGarbageCollector SZrGarbageCollector;

ZR_CORE_API void ZrGarbageCollectorInit(struct SZrGlobalState *global);

ZR_CORE_API void ZrGarbageCollectorGcFull(struct SZrState *state, TBool isImmediate);

ZR_CORE_API SZrRawObject *ZrRawObjectNew(struct SZrState *state, EZrValueType type, TZrSize size);


#endif //ZR_VM_CORE_GC_H
