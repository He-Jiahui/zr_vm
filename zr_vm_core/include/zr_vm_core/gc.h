//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_GC_H
#define ZR_VM_CORE_GC_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/value.h"


/*
** GC对象标记状态宏定义
** 使用 EZrGarbageCollectIncrementalObjectStatus 枚举类型系统来管理对象的垃圾收集标记状态
**
** 对象标记状态（基于三色标记法的概念）：
** - INITED: 已初始化但未标记，对应 ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED
** - WAIT_TO_SCAN: 已标记但未扫描完成，对应 ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN
** - REFERENCED: 已标记且扫描完成，对应 ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED
**
** 主要不变式：INITED对象不能指向REFERENCED对象（对应三色标记法中的白色对象不能指向黑色对象）
** 在清除阶段，这个不变式可能被打破，因为变成INITED的对象可能仍指向REFERENCED对象
** 当清除阶段结束时，所有对象再次变为INITED，不变式恢复
*/
#define ZR_GC_IS_INITED(x)      ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED)
#define ZR_GC_IS_WAIT_TO_SCAN(x)       ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN)
#define ZR_GC_IS_REFERENCED(x)      ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED)

#define ZR_GC_TO_FINALIZE(x)   ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED)

#define ZR_GC_OTHER_GENERATION(g)   ((g)->gcGeneration == ZR_GARBAGE_COLLECT_GENERATION_A ? ZR_GARBAGE_COLLECT_GENERATION_B : ZR_GARBAGE_COLLECT_GENERATION_A)
#define ZR_GC_IS_DEAD(g,v)     ((v)->garbageCollectMark.generation != (g)->gcGeneration)

#define ZR_GC_CHANGE_GENERATION(x)  ((x)->garbageCollectMark.generation = ZR_GC_OTHER_GENERATION(global->garbageCollector))
#define ZR_GC_SET_REFERENCED(x)     ((x)->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED)

/*
** 分代GC状态宏定义
** 使用 EZrGarbageCollectGenerationalObjectStatus 枚举类型系统来管理对象的分代状态
**
** 分代状态值（从新到老）：
** - NEW: 新生代对象
** - SURVIVAL: 存活对象（经历一次GC后）
** - BARRIER: 通过屏障晋升的对象
** - ALIVE: 活跃对象
** - LONG_ALIVE: 长期存活对象
** - SCANNED: 已扫描对象
** - SCANNED_PREVIOUS: 上一轮已扫描对象
*/
#define ZR_GC_GET_AGE(o) ((o)->garbageCollectMark.generationalStatus)
#define ZR_GC_SET_AGE(o,a) ((o)->garbageCollectMark.generationalStatus = (a))
#define ZR_GC_IS_OLD(o) ((o)->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL)
#define ZR_GC_IS_NEW(o) ((o)->garbageCollectMark.generationalStatus == ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW)

#define ZR_GC_CHANGE_AGE(o,f,t) \
    ZR_ASSERT((o)->garbageCollectMark.generationalStatus == (f), (o)->garbageCollectMark.generationalStatus = (t))
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
ZR_CORE_API void ZrGarbageCollectorFree(struct SZrGlobalState *global, SZrGarbageCollector *collector);

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
    ZR_UNUSED_PARAMETER(state);
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
