//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_GC_H
#define ZR_VM_CORE_GC_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/value.h"
struct SZrGlobalState;
struct SZrState;

// generational mode
struct ZR_STRUCT_ALIGN SZrGarbageCollector {
    EZrGarbageCollectMode gcMode;
    EZrGarbageCollectStatus gcStatus;
    EZrGarbageCollectRunningStatus gcRunningStatus;
    EZrGarbageCollectIncrementalObjectStatus gcInitializeObjectStatus;
    EZrGarbageCollectGeneration gcGeneration;

    SZrRawObject *gcObjectList;
    SZrRawObject **gcObjectListSweeper;
    SZrRawObject *waitToScanObjectList;
    SZrRawObject *waitToScanAgainObjectList;
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

    TZrMemoryOffset managedMemories;
    TZrMemoryOffset gcDebtSize;

    TZrSize atomicMemories;
    TZrSize aliveMemories;

    SZrRawObject *aliveObjectList;
    SZrRawObject *circleMoreObjectList;
    SZrRawObject *circleOnceObjectList;
    SZrRawObject *aliveObjectWithReleaseFunctionList;
    SZrRawObject *circleMoreObjectWithReleaseFunctionList;
    SZrRawObject *circleOnceObjectWithReleaseFunctionList;
};

typedef struct SZrGarbageCollector SZrGarbageCollector;


ZR_CORE_API void ZrGarbageCollectorNew(struct SZrGlobalState *global);

ZR_CORE_API void ZrGarbageCollectorAddDebtSpace(struct SZrGlobalState *global, TZrMemoryOffset size);

ZR_CORE_API void ZrGarbageCollectorGcFull(struct SZrState *state, TBool isImmediate);

ZR_CORE_API void ZrGarbageCollectorGcStep(struct SZrState *state);

ZR_CORE_API TBool ZrGarbageCollectorIsInvariant(struct SZrGlobalState *global);

ZR_CORE_API TBool ZrGarbageCollectorIsSweeping(struct SZrGlobalState *global);

ZR_CORE_API void ZrGarbageCollectorCheckGc(struct SZrState *state);

ZR_CORE_API void ZrGarbageCollectorBarrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject);

ZR_CORE_API void ZrGarbageCollectorBarrierBack(struct SZrState *state, SZrRawObject *object);

ZR_CORE_API void ZrRawObjectBarrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject);

ZR_CORE_API SZrRawObject *ZrRawObjectNew(struct SZrState *state, EZrValueType type, TZrSize size, TBool isNative);


ZR_CORE_API TBool ZrRawObjectIsUnreferenced(struct SZrState *state, SZrRawObject *object);

ZR_CORE_API void ZrRawObjectMarkAsInit(struct SZrState *state, SZrRawObject *object);

// only available for latest generated object
ZR_CORE_API void ZrRawObjectMarkAsPermanent(struct SZrState *state, SZrRawObject *object);

ZR_FORCE_INLINE void ZrRawObjectMarkAsReferenced(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsReferenced(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

ZR_FORCE_INLINE void ZrRawObjectMarkAsReleased(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsWaitToScan(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE void ZrRawObjectMarkAsWaitToScan(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsPermanent(struct SZrState *state, SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsReleased(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

ZR_FORCE_INLINE TBool ZrGcRawObjectIsDead(struct SZrGlobalState *global, SZrRawObject *object) {
    return global->garbageCollector->gcGeneration != object->garbageCollectMark.generation;
}

ZR_FORCE_INLINE TBool ZrRawObjectIsGenerationalThroughBarrier(SZrRawObject *object) {
    return object->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER;
}

ZR_FORCE_INLINE void ZrRawObjectSetGenerationalStatus(SZrRawObject *object,
                                                      EZrGarbageCollectGenerationalObjectStatus status) {
    object->garbageCollectMark.generationalStatus = status;
}

ZR_CORE_API void ZrGcValueStaticAssertIsAlive(struct SZrState *state, SZrTypeValue *value);
#endif // ZR_VM_CORE_GC_H
