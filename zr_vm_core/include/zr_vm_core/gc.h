//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_GC_H
#define ZR_VM_CORE_GC_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc_budget_contract.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"


/* 增量标记的阶段判定由 gc_mark.c 的屏障和扫描器共用；扫描完成的对象新增
 * 指向未标记对象的引用时，写屏障须使目标进入可达集合。 */
#define ZR_GC_IS_INITED(x)      ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED)
#define ZR_GC_IS_WAIT_TO_SCAN(x)       ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN)
#define ZR_GC_IS_REFERENCED(x)      ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED)

#define ZR_GC_TO_FINALIZE(x)   ((x)->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_UNREFERENCED)

#define ZR_GC_OTHER_GENERATION(g)   ((g)->gcGeneration == ZR_GARBAGE_COLLECT_GENERATION_A ? ZR_GARBAGE_COLLECT_GENERATION_B : ZR_GARBAGE_COLLECT_GENERATION_A)
#define ZR_GC_IS_DEAD(g,v)     ((v)->garbageCollectMark.generation != (g)->gcGeneration)

/* TODO: 宏展开依赖调用处名为 global 的局部变量；仓内未见调用，需确认是否保留接口。 */
#define ZR_GC_CHANGE_GENERATION(x)  ((x)->garbageCollectMark.generation = ZR_GC_OTHER_GENERATION(global->garbageCollector))
#define ZR_GC_SET_REFERENCED(x)     ((x)->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED)

/* 分代年龄由疏散、晋升和屏障共同维护；这些宏只判定/改变标记位，
 * 不负责移动对象或更新 region 描述符。 */
#define ZR_GC_GET_AGE(o) ((o)->garbageCollectMark.generationalStatus)
#define ZR_GC_SET_AGE(o,a) ((o)->garbageCollectMark.generationalStatus = (a))
#define ZR_GC_IS_OLD(o) ((o)->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_SURVIVAL)
#define ZR_GC_IS_NEW(o) ((o)->garbageCollectMark.generationalStatus == ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW)

/* TODO: 当前 ZR_ASSERT 只接收一个实参，本宏在展开时会产生预处理错误；
 * 仓内尚无调用，需确认年龄迁移原本应使用断言后赋值还是条件赋值。 */
#define ZR_GC_CHANGE_AGE(o,f,t) \
    ZR_ASSERT((o)->garbageCollectMark.generationalStatus == (f), (o)->garbageCollectMark.generationalStatus = (t))
struct SZrGlobalState;
struct SZrState;
struct SZrAotGcRootFrame;
struct SZrAotGcRootMap;

/** @brief 调度器与遥测使用的回收请求级别；FULL 可由宿主或堆压力提出。 */
enum EZrGarbageCollectCollectionKind {
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR = 0,
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR = 1,
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL = 2,
    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX
};

typedef enum EZrGarbageCollectCollectionKind EZrGarbageCollectCollectionKind;

/** @brief 诊断用阶段快照，跨暂停和并发 major 步骤变化。 */
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

/** @brief 分代区段的记账视图；由 GC 管理并随对象迁移而更新。 */
typedef struct SZrGarbageCollectRegionDescriptor {
    TZrUInt32 id;
    EZrGarbageCollectRegionKind kind;
    TZrUInt64 capacityBytes;
    TZrUInt64 usedBytes;
    TZrUInt64 liveBytes;
    TZrUInt32 liveObjectCount;
    TZrUInt32 compactionSeenEpoch;
} SZrGarbageCollectRegionDescriptor;

/** @brief 供调试器和宿主读取的值快照；不要将其中的计数当作原子同步协议。 */
typedef struct SZrGarbageCollectorStatsSnapshot {
    TZrUInt64 domainId;
    TZrUInt32 domainGeneration;
    TZrUInt32 activeMutatorCount;
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
    TZrBool concurrentMajorActive;
    TZrUInt64 concurrentMajorCycleCount;
    TZrUInt64 concurrentMajorInitialPauseCount;
    TZrUInt64 concurrentMajorInitialPauseTotalUs;
    TZrUInt64 concurrentMajorInitialPauseMaxUs;
    TZrUInt64 concurrentMajorMarkSliceCount;
    TZrUInt64 concurrentMajorMarkTotalUs;
    TZrUInt64 concurrentMajorMarkMaxUs;
    TZrUInt64 concurrentMajorRemarkPauseCount;
    TZrUInt64 concurrentMajorRemarkPauseTotalUs;
    TZrUInt64 concurrentMajorRemarkPauseMaxUs;
    TZrUInt64 compactPauseCount;
    TZrUInt64 compactPauseTotalUs;
    TZrUInt64 compactPauseMaxUs;
    TZrUInt64 compactDeferredCount;
    TZrUInt64 concurrentBarrierCount;
    TZrUInt64 safepointWaitCount;
    TZrUInt64 safepointWaitTotalUs;
    TZrUInt64 safepointWaitMaxUs;
    TZrUInt64 outboundTransferPrepareCount;
    TZrUInt64 outboundTransferPublishCount;
    TZrUInt64 outboundTransferAbortCount;
    TZrUInt64 outboundTransferObjectCount;
    TZrUInt64 outboundTransferByteCount;
    TZrUInt64 inboundTransferClaimCount;
    TZrUInt64 inboundTransferCommitCount;
    TZrUInt64 inboundTransferAbortCount;
    TZrUInt64 inboundTransferObjectCount;
    TZrUInt64 inboundTransferByteCount;
    /* Bounded-major-GC scheduler telemetry.  These scalar fields are
     * intentionally appended so existing snapshot consumers remain valid. */
    TZrBool budgetConfigured;
    EZrGcBudgetStepStatus budgetLastStatus;
    EZrGcBudgetPhase budgetPhase;
    EZrGcBudgetPauseReason budgetPauseReason;
    TZrUInt64 budgetCursor;
    TZrUInt64 budgetWorkDone;
    TZrUInt64 budgetElapsedUs;
    TZrInt64 budgetDebtBytes;
    TZrUInt64 budgetOverBudgetCount;
    TZrUInt64 budgetCompactDeferredCount;
    TZrBool budgetPressure;
    TZrBool budgetFallback;
} SZrGarbageCollectorStatsSnapshot;

/** @brief 全局状态拥有的回收管理器；字段跨增量、分代、并发标记和预算驱动器共享。
 * @note 构造由 GlobalState_New 发起，关闭时由 GlobalState_Free 释放；外部不应直接改字段。
 * @note 可选预算记录在构造时初始化为未配置状态和 idle/zero 遥测。 */
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
    TZrBool concurrentMajorActive;
    TZrBool concurrentMajorForceCompact;
    TZrBool concurrentMajorMarkDrained;
    TZrUInt64 concurrentMajorCycleId;
    TZrSize concurrentMajorWork;
    SZrGarbageCollectorStatsSnapshot statsSnapshot;
    TZrUInt64 collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX];
    TZrUInt64 collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX];
    TZrUInt64 collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX];

    /* 可选预算驱动器状态；由预算 API 配置，回收入口记录其阶段和结果。 */
    SZrGcBudget budget;
    TZrBool budgetConfigured;
    EZrGcBudgetStepStatus budgetLastStatus;
    EZrGcBudgetPhase budgetPhase;
    EZrGcBudgetPauseReason budgetPauseReason;
    TZrUInt64 budgetCursor;
    TZrUInt64 budgetWorkDone;
    TZrUInt64 budgetElapsedUs;
    TZrInt64 budgetDebtBytes;
    TZrUInt64 budgetOverBudgetCount;
    TZrUInt64 budgetCompactDeferredCount;
    TZrBool budgetPressure;
    TZrBool budgetFallback;

    SZrRawObject *aliveObjectList;
    SZrRawObject *circleMoreObjectList;
    SZrRawObject *circleOnceObjectList;
    SZrRawObject *aliveObjectWithReleaseFunctionList;
    SZrRawObject *circleMoreObjectWithReleaseFunctionList;
    SZrRawObject *circleOnceObjectWithReleaseFunctionList;
};

typedef struct SZrGarbageCollector SZrGarbageCollector;

/** @brief 一次本地调用期间的临时保活凭据，记录该调用实际增加的 pin/ignore 状态。 */
typedef struct SZrGcNativeCallPin {
    SZrRawObject *object;
    EZrGarbageCollectPinKind pinKind;
    TZrBool ignoredAddedByCaller;
    TZrBool pinKindAddedByCaller;
} SZrGcNativeCallPin;

/** @brief 快速检查对象是否仍在该 global 的显式保活表中。
 * @pre object 在查询期间有效；调用者负责并发访问的同步。
 * BUG: NativeCallPinObject 的多 mutator 调用链未持锁，登记表扩容可使本查询读取悬垂数组。 */
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


/** @brief 在全局状态构造期间安装 GC 管理器。
 * @pre global 和 mainThreadState 已建立。
 * BUG: 管理器分配失败时实现仍解引用空指针；global.c 的构造入口无法返回分配失败。 */
ZR_CORE_API void ZrCore_GarbageCollector_New(struct SZrGlobalState *global);
/** @brief 全局状态关闭时收束回收并释放管理器及其登记表。
 * @pre 无其他 mutator 继续使用该 global；collector 属于该 global。 */
ZR_CORE_API void ZrCore_GarbageCollector_Free(struct SZrGlobalState *global, SZrGarbageCollector *collector);

/** @brief 由分配和回收工作量调整待偿 GC 债务，正债务驱动后续安全点。
 * @pre 现有债务非负；本入口在此前提下将负增量扣减至零。 */
ZR_CORE_API void ZrCore_GarbageCollector_AddDebtSpace(struct SZrGlobalState *global, TZrMemoryOffset size);

/** @brief 请求完整回收；关闭和分配压力路径也会调用。
 * @note 可能因跨 mutator 暂停超时而直接返回，void 结果不保证实际完成。
 * BUG: 对象 scanMarkGcFunction 在受保护调用内抛异常可跳过域锁、暂停和预算清理。 */
ZR_CORE_API void ZrCore_GarbageCollector_GcFull(struct SZrState *state, TZrBool isImmediate);

/** @brief 偿还一个回收步骤，分代并发标记可在非暂停区执行。
 * BUG: 分代 minor 的对象复制 OOM 经受保护调用 longjmp 时，暂停域和预算收尾被跳过。
 * BUG: 多 mutator 并发标记切片在锁外更新共用遥测，造成数据竞争和丢失计数。 */
ZR_CORE_API void ZrCore_GarbageCollector_GcStep(struct SZrState *state);

/** @brief 将对象暂列为外部持有的 GC 根。
 * @note 重复登记不计数；调用方只应撤销自身新增的根，非零逸出登记也会自动撤根。
 * BUG: 同一 global 的多个运行中 mutator 可无锁并行登记，竞争数组槽位并丢失根。
 * TODO: ownership.c 把已有登记视作自身所有，需核查其释放时对象是否仍有其他强根。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_IgnoreObject(struct SZrState *state, SZrRawObject *object);
/** @brief 撤销对象的显式保活，不验证登记者身份；调用方只应撤销自身新增的根，失败时也可能清理过期索引。
 * BUG: 多 mutator 并行增删同一登记表未同步，可能破坏索引或过早失根。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_UnignoreObject(struct SZrGlobalState *global, SZrRawObject *object);
/** @brief 查询显式保活登记，供所有权和反射路径决定是否仍需解除。
 * BUG: 底层无锁读取登记表，多 mutator 扩容时可能读到已释放数组。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_IsObjectIgnored(struct SZrGlobalState *global, SZrRawObject *object);

/** @brief 保活已登记对象或登记新对象，并报告是否由本次调用新增根。
 * @note 撤销时仅应解除本次新增的登记，避免移除其他持有者的根。
 * BUG: 并行 mutator 的查找与登记不构成原子操作，可重复认领或丢失根。 */
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

/** @brief 判断回收器是否仍在标记/原子阶段，供对象写入屏障选择策略。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_IsInvariant(struct SZrGlobalState *global);

/** @brief 判断增量驱动器是否正在扫描释放阶段。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_IsSweeping(struct SZrGlobalState *global);

/** @brief 在债务或堆阈值达到条件时启动一轮步骤。 */
ZR_CORE_API void ZrCore_GarbageCollector_CheckGc(struct SZrState *state);
/** @brief 执行线程暂停轮询及 GC 债务检查，是解释器和 AOT 的合作式安全点。 */
ZR_CORE_API void ZrCore_Gc_SafePoint(struct SZrState *state);
/** @brief 对对象字段写入执行增量/分代屏障并计入性能遥测。 */
ZR_CORE_API void ZrCore_Gc_WriteBarrier(struct SZrState *state,
                                        SZrRawObject *ownerObject,
                                        SZrTypeValue *value);
/** @brief 为本地调用暂时固定对象并建立显式保活；成功后必须 Unpin。
 * BUG: 保活表扩容失败时仅撤销 pin 位，已改变的存储、区段与逸出状态未回退。 */
ZR_CORE_API TZrBool ZrCore_Gc_NativeCallPinObject(struct SZrState *state,
                                                  SZrRawObject *object,
                                                  SZrGcNativeCallPin *pin);
/** @brief 值可由 GC 移动时转入 NativeCallPinObject，非对象值视为无需保活。 */
ZR_CORE_API TZrBool ZrCore_Gc_NativeCallPinValue(struct SZrState *state,
                                                 const SZrTypeValue *value,
                                                 SZrGcNativeCallPin *pin);
/** @brief 结束本地调用的临时保活，只撤销该凭据新增的标记和登记。 */
ZR_CORE_API void ZrCore_Gc_NativeCallUnpin(struct SZrGlobalState *global, SZrGcNativeCallPin *pin);

/** @brief 对普通对象引用执行当前回收模式的写屏障。 */
ZR_CORE_API void ZrCore_GarbageCollector_Barrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject);

/** @brief 对容器既有边执行回溯屏障以维持增量扫描队列。 */
ZR_CORE_API void ZrCore_GarbageCollector_BarrierBack(struct SZrState *state, SZrRawObject *object);

/** @brief 原始对象字段写入时使用的 GC 屏障入口。 */
ZR_CORE_API void ZrCore_RawObject_Barrier(struct SZrState *state, SZrRawObject *object, SZrRawObject *valueObject);

/** @brief 设置下次安全点用于触发完整回收的托管堆阈值；非正值禁用。 */
ZR_CORE_API void ZrCore_GarbageCollector_SetHeapLimitBytes(struct SZrGlobalState *global, TZrMemoryOffset heapLimitBytes);
/** @brief 更新普通暂停和 remark 暂停预算，供宿主限制回收停顿。 */
ZR_CORE_API void ZrCore_GarbageCollector_SetPauseBudgetUs(struct SZrGlobalState *global,
                                                          TZrUInt64 pauseBudgetUs,
                                                          TZrUInt64 remarkBudgetUs);
/** @brief 为可选预算评估 API 安装配置。
 * TODO: 当前回收步骤未调用 EvaluateBudgetStep；需确认该配置是否预期影响实际调度。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_SetBudget(struct SZrGlobalState *global,
                                                       const SZrGcBudget *budget);
/** @brief 读取全局回收器当前配置的预算；首次配置前返回 false。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_GetBudget(struct SZrGlobalState *global,
                                                       SZrGcBudget *outBudget);
/** @brief 按当前阶段与工作量计算可选预算状态；未配置预算时返回 false。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_EvaluateBudgetStep(
        struct SZrGlobalState *global,
        EZrGcBudgetPhase phase,
        TZrUInt64 workUnits,
        TZrUInt64 elapsedUs,
        TZrUInt64 bytes,
        TZrUInt64 objects,
        TZrUInt64 atomicPauseUs,
        SZrGcBudgetStepResult *outResult);
/** @brief 取得最近一次预算步骤的状态，用于宿主诊断；未配置时返回 false。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_GetBudgetStats(
        struct SZrGlobalState *global,
        SZrGcBudgetStepResult *outResult);
/* 宿主使用 state 入口访问同一 global 所拥有的预算状态。 */
typedef SZrGcBudgetStepResult SZrGcBudgetStats;
/** @brief 从 state 为当前全局回收器设置预算。 */
ZR_CORE_API TZrBool ZrCore_Gc_SetBudget(struct SZrState *state,
                                        const SZrGcBudget *budget);
/** @brief 从 state 读取当前全局回收器的预算统计；未配置时返回 false。 */
ZR_CORE_API TZrBool ZrCore_Gc_GetStats(struct SZrState *state,
                                       SZrGcBudgetStats *stats);
/** @brief 设置分代并发标记切片的工作量系数；当前实现不创建线程。 */
ZR_CORE_API void ZrCore_GarbageCollector_SetWorkerCount(struct SZrGlobalState *global, TZrUInt32 workerCount);
/** @brief 记录请求类型并唤醒债务驱动；真正回收发生于后续步骤。
 * TODO: 当前未验证 kind 的枚举范围，需确定无效请求应拒绝还是归入某类回收。 */
ZR_CORE_API void ZrCore_GarbageCollector_ScheduleCollection(struct SZrGlobalState *global,
                                                            EZrGarbageCollectCollectionKind kind);
/** @brief 输出管理器遥测供诊断；调用方必须提供可写快照存储。
 * BUG: 遍历 regions 时不持同步锁；另一 mutator 扩容并释放旧数组会造成悬垂读取。 */
ZR_CORE_API void ZrCore_GarbageCollector_GetStatsSnapshot(struct SZrGlobalState *global,
                                                          SZrGarbageCollectorStatsSnapshot *outSnapshot);
/** @brief 将 AOT 栈根映射压入当前 state 的扫描链；每个活动节点只可压入一次。
 * C11 longjmp 路径的 TryRun 回调通过 Throw 非局部退出时，Throw 在域 mutator 标为 inactive
 * 并广播之前恢复入口根帧链顶和深度；TryRun catch 再幂等恢复，不遍历已结束的回调栈帧。
 * forced-C++ 展开期间析构函数回入 VM/GC 不属于此生命周期保证。
 * Reusing an active node is rejected so the linked chain stays acyclic.
 * @pre frame, rootMap, and the descriptor entries in rootMap->roots remain
 * address-stable until Pop. The frame node must use host-stable storage and must
 * not overlap the movable VM stack allocation. Values selected by those
 * descriptors, including FRAME_BYTE_OFFSET values, may reside in the VM stack;
 * frameBase is rebased when that storage moves. */
ZR_CORE_API TZrBool ZrCore_Gc_AotRootFramePush(struct SZrState *state,
                                               struct SZrAotGcRootFrame *frame,
                                               TZrStackValuePointer frameBase,
                                               const struct SZrAotGcRootMap *rootMap);
/** @brief 按后进先出顺序弹出 AOT 根帧；非栈顶帧会被拒绝。 */
ZR_CORE_API TZrBool ZrCore_Gc_AotRootFramePop(struct SZrState *state,
                                              struct SZrAotGcRootFrame *frame);
/** @brief 读取 state 当前已登记的 AOT 根帧深度，供回退路径核对平衡。 */
ZR_CORE_API TZrUInt32 ZrCore_Gc_AotRootFrameDepth(const struct SZrState *state);
/** @brief 查询分代记忆集中的对象，用于屏障与测试诊断。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_HasRememberedObject(struct SZrGlobalState *global, SZrRawObject *object);
/** @brief 将对象提升到固定存储并登记逸出；登记会撤销既有 ignore 根，宿主持有期间须重新保活。
 * BUG: 区段登记扩容 OOM 时仍返回成功语义，固定对象可能留下零区段 ID。 */
ZR_CORE_API void ZrCore_GarbageCollector_PinObject(struct SZrState *state,
                                                   SZrRawObject *object,
                                                   EZrGarbageCollectPinKind pinKind);
/** @brief 记录对象跨作用域逸出并传播闭包捕获；非零逸出登记会撤销既有 ignore 根。 */
ZR_CORE_API void ZrCore_GarbageCollector_MarkRawObjectEscaped(struct SZrState *state,
                                                              SZrRawObject *object,
                                                              TZrUInt32 escapeFlags,
                                                              TZrUInt32 scopeDepth,
                                                              EZrGarbageCollectPromotionReason promotionReason);
/** @brief 值为 GC 对象时转交逸出登记并撤销其既有 ignore 根；标量值无需登记。 */
ZR_CORE_API void ZrCore_GarbageCollector_MarkValueEscaped(struct SZrState *state,
                                                          const SZrTypeValue *value,
                                                          TZrUInt32 escapeFlags,
                                                          TZrUInt32 scopeDepth,
                                                          EZrGarbageCollectPromotionReason promotionReason);

/** @brief 分配并登记托管对象；调用方应在后续可触发 GC 的步骤前建立根。
 * @note 无效 state 或域登记失败可返回空；原生对象分配 OOM 可抛内存异常。
 * BUG: 区段表扩容 OOM 后仍将 regionId 为零的对象挂入 GC 链并返回。 */
ZR_CORE_API SZrRawObject *ZrCore_RawObject_New(struct SZrState *state, EZrValueType type, TZrSize size, TZrBool isNative);


/** @brief 按当前标记状态、代号与显式保活登记判断对象是否视为不可达。 */
ZR_CORE_API TZrBool ZrCore_RawObject_IsUnreferenced(struct SZrState *state, SZrRawObject *object);

/** @brief 初始化对象的增量标记状态，供分配后的登记路径使用。 */
ZR_CORE_API void ZrCore_RawObject_MarkAsInit(struct SZrState *state, SZrRawObject *object);

/** @brief 将对象转入永久集合；调用方须保证其生命周期与全局状态相同。
 * BUG: 永久区段登记 OOM 时已改变状态和存储类别，regionId 留零且 void 入口不报告失败。 */
ZR_CORE_API void ZrCore_RawObject_MarkAsPermanent(struct SZrState *state, SZrRawObject *object);

/* 增量屏障步骤从 gc_mark.c 导出，供共享库测试直接链接。 */
/** @brief 完成当前待扫描队列的传播工作，供完整回收和测试驱动。 */
ZR_CORE_API TZrSize ZrGarbageCollectorPropagateAll(struct SZrState *state);
/** @brief 重启增量回收状态机，供完整回收和测试驱动。 */
ZR_CORE_API void ZrGarbageCollectorRestartCollection(struct SZrState *state);

/** @brief 标记对象已被扫描，可由屏障与回收驱动器共享。 */
ZR_FORCE_INLINE void ZrCore_RawObject_MarkAsReferenced(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

/** @brief 读取增量扫描完成状态；object 必须有效。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsReferenced(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}

/** @brief 标记对象已完成终结并进入 released 状态，供清扫路径避免重复处理。 */
ZR_FORCE_INLINE void ZrCore_RawObject_MarkAsReleased(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

/** @brief 判断对象是否仍在待扫描队列对应状态。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsWaitToScan(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

/** @brief 将对象交给增量传播阶段的待扫描状态。 */
ZR_FORCE_INLINE void ZrCore_RawObject_MarkAsWaitToScan(SZrRawObject *object) {
    object->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}

/** @brief 判断对象是否已经被设为永久对象；state 不参与判定。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsPermanent(struct SZrState *state, SZrRawObject *object) {
    ZR_UNUSED_PARAMETER(state);
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_PERMANENT;
}

/** @brief 判断对象是否已进入释放状态。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsReleased(SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED;
}

/** @brief 在增量回收轮次内判定对象代号是否落后；不是通用对象存活查询。 */
ZR_FORCE_INLINE TZrBool ZrCore_Gc_RawObjectIsDead(struct SZrGlobalState *global, SZrRawObject *object) {
    return object->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED &&
           global->garbageCollector->gcGeneration != object->garbageCollectMark.generation;
}

/** @brief 判断对象是否达到分代屏障晋升状态。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsGenerationalThroughBarrier(SZrRawObject *object) {
    return object->garbageCollectMark.generationalStatus >= ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_BARRIER;
}

/** @brief 更新年龄状态；迁移及 region 记账由上层回收路径完成。 */
ZR_FORCE_INLINE void ZrCore_RawObject_SetGenerationalStatus(SZrRawObject *object,
                                                      EZrGarbageCollectGenerationalObjectStatus status) {
    object->garbageCollectMark.generationalStatus = status;
}

/** @brief 更新存储类别和对应代别；不执行实际迁移。 */
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

/** @brief 更新区段类别标记；调用方负责同步 region 描述符。 */
ZR_FORCE_INLINE void ZrCore_RawObject_SetRegionKind(SZrRawObject *object, EZrGarbageCollectRegionKind regionKind) {
    object->garbageCollectMark.regionKind = regionKind;
}

/** @brief 调试构建断言值所引用对象仍活着；非调试构建的断言不执行。 */
ZR_CORE_API void ZrCore_Gc_ValueStaticAssertIsAlive(struct SZrState *state, SZrTypeValue *value);

/** @brief 返回托管对象的逻辑基大小，供堆记账验证和宿主工具链接。 */
ZR_CORE_API TZrSize ZrCore_GarbageCollector_GetObjectBaseSize(struct SZrState *state, struct SZrRawObject *object);
#endif // ZR_VM_CORE_GC_H
