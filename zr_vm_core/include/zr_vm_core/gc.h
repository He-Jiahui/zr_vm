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

enum EZrGarbageCollectCollectionKind {
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR = 0,
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR = 1,
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL = 2,
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX
};

typedef enum EZrGarbageCollectCollectionKind EZrGarbageCollectCollectionKind;

enum EZrGarbageCollectCollectionPhase {
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE = 0,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK = 1,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_EVACUATE = 2,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT = 3,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK = 4,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_SWEEP = 5,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_COMPACT = 6,
    ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAX
};

typedef enum EZrGarbageCollectCollectionPhase EZrGarbageCollectCollectionPhase;

typedef struct SZrGarbageCollectRegionDescriptor {
    TZrUInt32 id;
    EZrGarbageCollectRegionKind kind;
    TZrUInt64 capacityBytes;
    TZrUInt64 usedBytes;
    TZrUInt64 liveBytes;
    TZrUInt32 liveObjectCount;
    TZrUInt32 compactionSeenEpoch;
} SZrGarbageCollectRegionDescriptor;

typedef struct SZrGarbageCollectorStatsSnapshot {
    TZrMemoryOffset heapLimitBytes;
    TZrUInt64 managedMemoryBytes;
    TZrInt64 gcDebtBytes;
    TZrUInt64 pauseBudgetUs;
    TZrUInt64 remarkBudgetUs;
    TZrUInt32 workerCount;
    TZrUInt32 ignoredObjectCount;
    TZrUInt32 rememberedObjectCount;
    TZrUInt32 regionCount;
    TZrUInt32 edenRegionCount;
    TZrUInt32 survivorRegionCount;
    TZrUInt32 oldRegionCount;
    TZrUInt32 pinnedRegionCount;
    TZrUInt32 largeRegionCount;
    TZrUInt32 permanentRegionCount;
    TZrUInt64 edenUsedBytes;
    TZrUInt64 survivorUsedBytes;
    TZrUInt64 oldUsedBytes;
    TZrUInt64 pinnedUsedBytes;
    TZrUInt64 largeUsedBytes;
    TZrUInt64 permanentUsedBytes;
    TZrUInt64 edenLiveBytes;
    TZrUInt64 survivorLiveBytes;
    TZrUInt64 oldLiveBytes;
    TZrUInt64 pinnedLiveBytes;
    TZrUInt64 largeLiveBytes;
    TZrUInt64 permanentLiveBytes;
    TZrUInt64 lastStepDurationUs;
    TZrUInt64 lastStepWork;
    EZrGarbageCollectCollectionKind lastCollectionKind;
    EZrGarbageCollectCollectionKind lastRequestedCollectionKind;
    EZrGarbageCollectCollectionPhase collectionPhase;
    TZrUInt64 minorCollectionCount;
    TZrUInt64 majorCollectionCount;
    TZrUInt64 fullCollectionCount;
    TZrUInt64 minorCollectionTotalDurationUs;
    TZrUInt64 majorCollectionTotalDurationUs;
    TZrUInt64 fullCollectionTotalDurationUs;
    TZrUInt64 minorCollectionMaxDurationUs;
    TZrUInt64 majorCollectionMaxDurationUs;
    TZrUInt64 fullCollectionMaxDurationUs;
} SZrGarbageCollectorStatsSnapshot;

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

    TZrBool stopGcFlag;
    TZrBool stopImmediateGcFlag;
    TZrBool isImmediateGcFlag;

    TZrUInt64 gcMinorGenerationMultiplier;
    TZrUInt64 gcMajorGenerationMultiplier;
    TZrUInt64 gcStepMultiplierPercent;
    TZrUInt64 gcStepSizeLog2;
    TZrUInt64 gcPauseThresholdPercent;
    TZrUInt64 gcPauseBudget;
    TZrUInt64 gcSweepSliceBudget;

    TZrMemoryOffset managedMemories;
    TZrMemoryOffset gcDebtSize;

    TZrSize atomicMemories;
    TZrSize aliveMemories;
    TZrSize ignoredObjectCount;
    TZrSize ignoredObjectCapacity;
    TZrSize gcLastStepWork;
    EZrGarbageCollectRunningStatus gcLastCompletedRunningStatus;

    SZrRawObject **ignoredObjects;
    SZrRawObject **rememberedObjects;
    TZrSize rememberedObjectCount;
    TZrSize rememberedObjectCapacity;
    TZrUInt32 nextRegionId;
    TZrUInt32 currentEdenRegionId;
    TZrUInt32 currentSurvivorRegionId;
    TZrUInt32 currentOldRegionId;
    TZrSize currentEdenRegionIndex;
    TZrSize currentSurvivorRegionIndex;
    TZrSize currentOldRegionIndex;
    TZrUInt64 currentEdenRegionUsedBytes;
    TZrUInt64 currentSurvivorRegionUsedBytes;
    TZrUInt64 currentOldRegionUsedBytes;
    SZrGarbageCollectRegionDescriptor *regions;
    TZrSize regionCount;
    TZrSize regionCapacity;

    TZrMemoryOffset heapLimitBytes;
    TZrUInt64 youngRegionSize;
    TZrUInt32 youngRegionCountTarget;
    TZrUInt32 survivorAgeThreshold;
    TZrUInt64 pauseBudgetUs;
    TZrUInt64 remarkBudgetUs;
    TZrUInt32 workerCount;
    TZrUInt32 fragmentationCompactThreshold;
    TZrUInt32 gcFlags;
    EZrGarbageCollectCollectionKind scheduledCollectionKind;
    EZrGarbageCollectCollectionPhase collectionPhase;
    TZrUInt32 minorCollectionEpoch;
    TZrUInt32 oldCompactionScanEpoch;
    SZrGarbageCollectorStatsSnapshot statsSnapshot;
    TZrUInt64 collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX];
    TZrUInt64 collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX];
    TZrUInt64 collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX];

    SZrRawObject *aliveObjectList;
    SZrRawObject *circleMoreObjectList;
    SZrRawObject *circleOnceObjectList;
    SZrRawObject *aliveObjectWithReleaseFunctionList;
    SZrRawObject *circleMoreObjectWithReleaseFunctionList;
    SZrRawObject *circleOnceObjectWithReleaseFunctionList;
};

typedef struct SZrGarbageCollector SZrGarbageCollector;

ZR_FORCE_INLINE TZrBool ZrCore_GarbageCollector_IsObjectIgnoredFast(struct SZrGlobalState *global,
                                                                    SZrRawObject *object) {
    SZrGarbageCollector *collector;
    TZrSize index;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    if (collector->ignoredObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    index = object->garbageCollectMark.ignoredRegistryIndex;
    return index < collector->ignoredObjectCount && collector->ignoredObjects[index] == object;
}


ZR_CORE_API void ZrCore_GarbageCollector_New(struct SZrGlobalState *global);
ZR_CORE_API void ZrCore_GarbageCollector_Free(struct SZrGlobalState *global, SZrGarbageCollector *collector);

ZR_CORE_API void ZrCore_GarbageCollector_AddDebtSpace(struct SZrGlobalState *global, TZrMemoryOffset size);

ZR_CORE_API void ZrCore_GarbageCollector_GcFull(struct SZrState *state, TZrBool isImmediate);

ZR_CORE_API void ZrCore_GarbageCollector_GcStep(struct SZrState *state);

ZR_CORE_API TZrBool ZrCore_GarbageCollector_IgnoreObject(struct SZrState *state, SZrRawObject *object);
ZR_CORE_API TZrBool ZrCore_GarbageCollector_UnignoreObject(struct SZrGlobalState *global, SZrRawObject *object);
ZR_CORE_API TZrBool ZrCore_GarbageCollector_IsObjectIgnored(struct SZrGlobalState *global, SZrRawObject *object);

ZR_FORCE_INLINE TZrBool ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(struct SZrGlobalState *global,
                                                                          struct SZrState *state,
                                                                          SZrRawObject *object,
                                                                          TZrBool *outAddedByCaller) {
    if (outAddedByCaller != ZR_NULL) {
        *outAddedByCaller = ZR_FALSE;
    }

    if (global == ZR_NULL || state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnoredFast(global, object)) {
        return ZR_TRUE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObject(state, object)) {
        return ZR_FALSE;
    }

    if (outAddedByCaller != ZR_NULL) {
        *outAddedByCaller = ZR_TRUE;
    }
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_GarbageCollector_IsInvariant(struct SZrGlobalState *global);

ZR_CORE_API TZrBool ZrCore_GarbageCollector_IsSweeping(struct SZrGlobalState *global);

ZR_CORE_API void ZrCore_GarbageCollector_CheckGc(struct SZrState *state);

ZR_CORE_API void ZrCore_GarbageCollector_Barrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject);

ZR_CORE_API void ZrCore_GarbageCollector_BarrierBack(struct SZrState *state, SZrRawObject *object);

ZR_CORE_API void ZrCore_RawObject_Barrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject);

ZR_CORE_API void ZrCore_GarbageCollector_SetHeapLimitBytes(struct SZrGlobalState *global, TZrMemoryOffset heapLimitBytes);
ZR_CORE_API void ZrCore_GarbageCollector_SetPauseBudgetUs(struct SZrGlobalState *global,
                                                          TZrUInt64 pauseBudgetUs,
                                                          TZrUInt64 remarkBudgetUs);
ZR_CORE_API void ZrCore_GarbageCollector_SetWorkerCount(struct SZrGlobalState *global, TZrUInt32 workerCount);
ZR_CORE_API void ZrCore_GarbageCollector_ScheduleCollection(struct SZrGlobalState *global,
                                                            EZrGarbageCollectCollectionKind kind);
ZR_CORE_API void ZrCore_GarbageCollector_GetStatsSnapshot(struct SZrGlobalState *global,
                                                          SZrGarbageCollectorStatsSnapshot *outSnapshot);
ZR_CORE_API TZrBool ZrCore_GarbageCollector_HasRememberedObject(struct SZrGlobalState *global, SZrRawObject *object);
ZR_CORE_API void ZrCore_GarbageCollector_PinObject(struct SZrState *state,
                                                   SZrRawObject *object,
                                                   EZrGarbageCollectPinKind pinKind);
ZR_CORE_API void ZrCore_GarbageCollector_MarkRawObjectEscaped(struct SZrState *state,
                                                              SZrRawObject *object,
                                                              TZrUInt32 escapeFlags,
                                                              TZrUInt32 scopeDepth,
                                                              EZrGarbageCollectPromotionReason promotionReason);
ZR_CORE_API void ZrCore_GarbageCollector_MarkValueEscaped(struct SZrState *state,
                                                          const SZrTypeValue *value,
                                                          TZrUInt32 escapeFlags,
                                                          TZrUInt32 scopeDepth,
                                                          EZrGarbageCollectPromotionReason promotionReason);

ZR_CORE_API SZrRawObject *ZrCore_RawObject_New(struct SZrState *state, EZrValueType type, TZrSize size, TZrBool isNative);


ZR_CORE_API TZrBool ZrCore_RawObject_IsUnreferenced(struct SZrState *state, SZrRawObject *object);

ZR_CORE_API void ZrCore_RawObject_MarkAsInit(struct SZrState *state, SZrRawObject *object);

// only available for latest generated object
ZR_CORE_API void ZrCore_RawObject_MarkAsPermanent(struct SZrState *state, SZrRawObject *object);

/*
** Incremental GC barrier stepping (gc_mark.c). Exported so test binaries can link against zr_vm_core_shared on MSVC.
*/
ZR_CORE_API TZrSize ZrGarbageCollectorPropagateAll(struct SZrState *state);
ZR_CORE_API void ZrGarbageCollectorRestartCollection(struct SZrState *state);

ZR_FORCE_INLINE void ZrCore_RawObject_MarkAsReferenced(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsReferenced(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

ZR_FORCE_INLINE void ZrCore_RawObject_MarkAsReleased(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsWaitToScan(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE void ZrCore_RawObject_MarkAsWaitToScan(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsPermanent(struct SZrState *state, SZrRawObject *object) {
    ZR_UNUSED_PARAMETER(state);
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsReleased(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

ZR_FORCE_INLINE TZrBool ZrCore_Gc_RawObjectIsDead(struct SZrGlobalState *global, SZrRawObject *object) {
    return global->garbageCollector->gcGeneration != object->garbageCollectMark.generation;
}

ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsGenerationalThroughBarrier(SZrRawObject *object) {
    return object->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER;
}

ZR_FORCE_INLINE void ZrCore_RawObject_SetGenerationalStatus(SZrRawObject *object,
                                                      EZrGarbageCollectGenerationalObjectStatus status) {
    object->garbageCollectMark.generationalStatus = status;
}

ZR_FORCE_INLINE void ZrCore_RawObject_SetStorageKind(SZrRawObject *object,
                                                     EZrGarbageCollectStorageKind storageKind) {
    object->garbageCollectMark.storageKind = storageKind;
    object->garbageCollectMark.heapGenerationKind =
            storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE
                    ? ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG
                    : (storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT
                               ? ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_PERMANENT
                               : ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD);
}

ZR_FORCE_INLINE void ZrCore_RawObject_SetRegionKind(SZrRawObject *object, EZrGarbageCollectRegionKind regionKind) {
    object->garbageCollectMark.regionKind = regionKind;
}

ZR_CORE_API void ZrCore_Gc_ValueStaticAssertIsAlive(struct SZrState *state, SZrTypeValue *value);

/*
 * 可 GC 对象在堆上的逻辑基大小（与 gc_object 内部记账一致），供验证测试与 DLL 宿主工具链链接（MSVC 导入库）。
 */
ZR_CORE_API TZrSize ZrCore_GarbageCollector_GetObjectBaseSize(struct SZrState *state, struct SZrRawObject *object);
#endif // ZR_VM_CORE_GC_H
