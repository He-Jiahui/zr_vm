//
// Public GC lifecycle and scheduling entry points.
//

#include "gc/gc_internal.h"
#include "zr_vm_core/profile.h"
#include "gc/gc_domain_internal.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/execution_budget.h"

/* 暂停域等待上界，完整回收与步骤失败时均按此边界放弃本次尝试。 */
#define ZR_GC_DOMAIN_PAUSE_TIMEOUT_MILLISECONDS ((TZrUInt32)1000u)

#if defined(ZR_PLATFORM_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

/* 回收遥测使用的时钟；跨平台结果仅用于时长估计，不承担同步职责。 */
TZrUInt64 garbage_collector_now_us(void) {
#if defined(ZR_PLATFORM_WIN)
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return frequency.QuadPart > 0 ? (TZrUInt64)((counter.QuadPart * 1000000ULL) / frequency.QuadPart) : 0u;
#else
    struct timespec now;

    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return 0u;
    }
    return (TZrUInt64)now.tv_sec * 1000000ULL + (TZrUInt64)(now.tv_nsec / 1000);
#endif
}

/* 将每类回收的累计统计投影到宿主可读取的快照。 */
static void garbage_collector_refresh_cumulative_snapshot(SZrGarbageCollector *collector) {
    if (collector == ZR_NULL) {
        return;
    }

    collector->statsSnapshot.minorCollectionCount =
            collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->statsSnapshot.majorCollectionCount =
            collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    collector->statsSnapshot.fullCollectionCount =
            collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL];
    collector->statsSnapshot.minorCollectionTotalDurationUs =
            collector->collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->statsSnapshot.majorCollectionTotalDurationUs =
            collector->collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    collector->statsSnapshot.fullCollectionTotalDurationUs =
            collector->collectionTotalDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL];
    collector->statsSnapshot.minorCollectionMaxDurationUs =
            collector->collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->statsSnapshot.majorCollectionMaxDurationUs =
            collector->collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    collector->statsSnapshot.fullCollectionMaxDurationUs =
            collector->collectionMaxDurationUs[ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL];
}

/* 汇总仍有活动对象的区段压力；诊断视图不等同于分配器总占用。 */
static void garbage_collector_refresh_pressure_snapshot(SZrGarbageCollector *collector) {
    TZrUInt32 regionCount = 0u;
    TZrUInt32 edenRegionCount = 0u;
    TZrUInt32 survivorRegionCount = 0u;
    TZrUInt32 oldRegionCount = 0u;
    TZrUInt32 pinnedRegionCount = 0u;
    TZrUInt32 largeRegionCount = 0u;
    TZrUInt32 permanentRegionCount = 0u;
    TZrUInt64 edenUsedBytes = 0u;
    TZrUInt64 survivorUsedBytes = 0u;
    TZrUInt64 oldUsedBytes = 0u;
    TZrUInt64 pinnedUsedBytes = 0u;
    TZrUInt64 largeUsedBytes = 0u;
    TZrUInt64 permanentUsedBytes = 0u;
    TZrUInt64 edenLiveBytes = 0u;
    TZrUInt64 survivorLiveBytes = 0u;
    TZrUInt64 oldLiveBytes = 0u;
    TZrUInt64 pinnedLiveBytes = 0u;
    TZrUInt64 largeLiveBytes = 0u;
    TZrUInt64 permanentLiveBytes = 0u;

    if (collector == ZR_NULL) {
        return;
    }

    collector->statsSnapshot.managedMemoryBytes = (TZrUInt64)collector->managedMemories;
    collector->statsSnapshot.gcDebtBytes = (TZrInt64)collector->gcDebtSize;
    collector->statsSnapshot.ignoredObjectCount = (TZrUInt32)collector->ignoredObjectCount;

    for (TZrSize index = 0; index < collector->regionCount; index++) {
        const SZrGarbageCollectRegionDescriptor *region = &collector->regions[index];

        if (region->liveObjectCount == 0u) {
            continue;
        }

        regionCount++;
        switch (region->kind) {
            case ZR_GARBAGE_COLLECT_REGION_KIND_EDEN:
                edenRegionCount++;
                edenUsedBytes += region->usedBytes;
                edenLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR:
                survivorRegionCount++;
                survivorUsedBytes += region->usedBytes;
                survivorLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_OLD:
                oldRegionCount++;
                oldUsedBytes += region->usedBytes;
                oldLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_PINNED:
                pinnedRegionCount++;
                pinnedUsedBytes += region->usedBytes;
                pinnedLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_LARGE:
                largeRegionCount++;
                largeUsedBytes += region->usedBytes;
                largeLiveBytes += region->liveBytes;
                break;
            case ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT:
                permanentRegionCount++;
                permanentUsedBytes += region->usedBytes;
                permanentLiveBytes += region->liveBytes;
                break;
            default:
                break;
        }
    }

    collector->statsSnapshot.regionCount = regionCount;
    collector->statsSnapshot.edenRegionCount = edenRegionCount;
    collector->statsSnapshot.survivorRegionCount = survivorRegionCount;
    collector->statsSnapshot.oldRegionCount = oldRegionCount;
    collector->statsSnapshot.pinnedRegionCount = pinnedRegionCount;
    collector->statsSnapshot.largeRegionCount = largeRegionCount;
    collector->statsSnapshot.permanentRegionCount = permanentRegionCount;
    collector->statsSnapshot.edenUsedBytes = edenUsedBytes;
    collector->statsSnapshot.survivorUsedBytes = survivorUsedBytes;
    collector->statsSnapshot.oldUsedBytes = oldUsedBytes;
    collector->statsSnapshot.pinnedUsedBytes = pinnedUsedBytes;
    collector->statsSnapshot.largeUsedBytes = largeUsedBytes;
    collector->statsSnapshot.permanentUsedBytes = permanentUsedBytes;
    collector->statsSnapshot.edenLiveBytes = edenLiveBytes;
    collector->statsSnapshot.survivorLiveBytes = survivorLiveBytes;
    collector->statsSnapshot.oldLiveBytes = oldLiveBytes;
    collector->statsSnapshot.pinnedLiveBytes = pinnedLiveBytes;
    collector->statsSnapshot.largeLiveBytes = largeLiveBytes;
    collector->statsSnapshot.permanentLiveBytes = permanentLiveBytes;
}

/* 仅在一轮回收到达终态时累计次数，避免把切片计作完整周期。 */
static void garbage_collector_record_step_telemetry(SZrGarbageCollector *collector, TZrUInt64 startedUs) {
    TZrUInt64 finishedUs;
    TZrUInt64 durationUs;
    TZrUInt32 kindIndex;

    if (collector == ZR_NULL) {
        return;
    }

    finishedUs = garbage_collector_now_us();
    durationUs = finishedUs >= startedUs ? finishedUs - startedUs : 0u;
    if (durationUs == 0u && collector->gcLastStepWork > 0) {
        durationUs = 1u;
    }

    collector->statsSnapshot.lastStepDurationUs = durationUs;
    collector->statsSnapshot.lastStepWork = (TZrUInt64)collector->gcLastStepWork;

    kindIndex = (TZrUInt32)collector->statsSnapshot.lastCollectionKind;
    if (kindIndex < ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAX &&
        collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED &&
        collector->collectionPhase == ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE &&
        (collector->gcLastStepWork > 0 || durationUs > 0u)) {
        collector->collectionCounts[kindIndex] += 1u;
        collector->collectionTotalDurationUs[kindIndex] += durationUs;
        if (collector->collectionMaxDurationUs[kindIndex] < durationUs) {
            collector->collectionMaxDurationUs[kindIndex] = durationUs;
        }
    }

    garbage_collector_refresh_cumulative_snapshot(collector);
}

/* AOT 帧链借用调用方栈内 frame、根槽和映射；扫描前须保持三者有效。
 * BUG: 生成函数在受保护调用中抛异常可跳过 Pop，TryRun 不清链；后续 GC
 * 遍历已失效的栈内 frame，可能读取悬垂根槽。 */
TZrBool ZrCore_Gc_AotRootFramePush(SZrState *state,
                                    SZrAotGcRootFrame *frame,
                                    TZrStackValuePointer frameBase,
                                    const struct SZrAotGcRootMap *rootMap) {
    if (state == ZR_NULL ||
        frame == ZR_NULL ||
        frameBase == ZR_NULL ||
        rootMap == ZR_NULL ||
        rootMap->rootCount == 0u ||
        rootMap->roots == ZR_NULL ||
        state->aotGcRootFrameDepth == UINT32_MAX) {
        return ZR_FALSE;
    }

    frame->rootMap = rootMap;
    frame->frameBase = frameBase;
    frame->previous = state->aotGcRootFrameStack;
    state->aotGcRootFrameStack = frame;
    state->aotGcRootFrameDepth++;
    return ZR_TRUE;
}

/* 只接受栈顶帧，失败时不改动链；异常回退可用深度检查平衡。 */
TZrBool ZrCore_Gc_AotRootFramePop(SZrState *state, SZrAotGcRootFrame *frame) {
    if (state == ZR_NULL || frame == ZR_NULL || state->aotGcRootFrameStack != frame) {
        return ZR_FALSE;
    }

    state->aotGcRootFrameStack = frame->previous;
    if (state->aotGcRootFrameDepth > 0u) {
        state->aotGcRootFrameDepth--;
    }
    frame->rootMap = ZR_NULL;
    frame->frameBase = ZR_NULL;
    frame->previous = ZR_NULL;
    return ZR_TRUE;
}

/* 调用方用深度核对本次 AOT 根帧是否已完全弹出。 */
TZrUInt32 ZrCore_Gc_AotRootFrameDepth(const SZrState *state) {
    return state != ZR_NULL ? state->aotGcRootFrameDepth : 0u;
}

/* 逸出对象保留更外层的锚定作用域，避免后续较短生命周期覆盖旧约束。 */
static TZrUInt32 garbage_collector_merge_scope_depth(TZrUInt32 currentScopeDepth, TZrUInt32 incomingScopeDepth) {
    if (currentScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return incomingScopeDepth;
    }
    if (incomingScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return currentScopeDepth;
    }
    return currentScopeDepth < incomingScopeDepth ? currentScopeDepth : incomingScopeDepth;
}

/* 将调用方的逸出类别转为诊断所需的晋升原因；显式原因优先。 */
static EZrGarbageCollectPromotionReason garbage_collector_promotion_reason_from_escape_flags(
        TZrUInt32 escapeFlags,
        EZrGarbageCollectPromotionReason promotionReason) {
    if (promotionReason != ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE) {
        return promotionReason;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT;
    }
    if ((escapeFlags & (ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE |
                        ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE)) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_HOST_HANDLE;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE;
    }
    if ((escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
    }
    if (escapeFlags != ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE;
    }
    return ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
}

/* 记录跨作用域对象及其闭包捕获的逸出，只在信息变化时传播捕获链。 */
static void garbage_collector_mark_raw_object_escaped_internal(SZrState *state,
                                                               SZrRawObject *object,
                                                               TZrUInt32 escapeFlags,
                                                               TZrUInt32 scopeDepth,
                                                               EZrGarbageCollectPromotionReason promotionReason,
                                                               TZrBool propagateCaptures) {
    EZrGarbageCollectPromotionReason resolvedPromotionReason;
    TZrUInt32 previousEscapeFlags;
    EZrGarbageCollectPromotionReason previousPromotionReason;
    TZrBool escapeInfoChanged = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL ||
        escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    /* TODO: 任意非零逸出登记都会撤销既有 ignore 根；屏障测试预期图接管，需核查宿主持有或无持久图边时的前提。 */
    if (object->garbageCollectMark.ignoredRegistryIndex != ZR_MAX_SIZE) {
        ZrCore_GarbageCollector_UnignoreObject(state->global, object);
    }

    previousEscapeFlags = object->garbageCollectMark.escapeFlags;
    previousPromotionReason = object->garbageCollectMark.promotionReason;
    object->garbageCollectMark.escapeFlags |= escapeFlags;
    object->garbageCollectMark.anchorScopeDepth =
            garbage_collector_merge_scope_depth(object->garbageCollectMark.anchorScopeDepth, scopeDepth);

    resolvedPromotionReason =
            garbage_collector_promotion_reason_from_escape_flags(escapeFlags, promotionReason);
    if (resolvedPromotionReason != ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE &&
        (object->garbageCollectMark.promotionReason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE ||
         object->garbageCollectMark.promotionReason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL)) {
        object->garbageCollectMark.promotionReason = resolvedPromotionReason;
    }
    escapeInfoChanged = object->garbageCollectMark.escapeFlags != previousEscapeFlags ||
                        object->garbageCollectMark.promotionReason != previousPromotionReason;

    if (propagateCaptures && escapeInfoChanged) {
        ZrCore_Closure_PropagateEscapeFromObject(state, object, escapeFlags, resolvedPromotionReason);
    }
}

/* 合并显式请求和堆压力请求为待执行类型，并以正债务唤醒安全点。
 * TODO: 显式 kind 未做范围检查；需核对无效枚举的宿主处理契约。 */
static void garbage_collector_schedule_collection_internal(SZrGlobalState *global,
                                                           EZrGarbageCollectCollectionKind kind,
                                                           TZrBool isExplicitRequest) {
    SZrGarbageCollector *collector;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    collector = global->garbageCollector;
    collector->scheduledCollectionKind = kind;
    collector->statsSnapshot.lastRequestedCollectionKind = kind;
    if (isExplicitRequest) {
        collector->gcFlags |= ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    } else {
        collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    }
    if (collector->gcDebtSize <= 0) {
        ZrCore_GarbageCollector_AddDebtSpace(global, ZR_GC_DEBT_CREDIT_BYTES);
    }
}

/* 全局状态在创建主线程后调用；管理器所有权立即转给 global。 */
void ZrCore_GarbageCollector_New(SZrGlobalState *global) {
    SZrGarbageCollector *gc =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrGarbageCollector), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    SZrState *state;

    /* BUG: global.c 的构造路径无失败分支；管理器分配为 NULL 时下方首次访问 gc 即崩溃。 */
    global->garbageCollector = gc;
    state = global->mainThreadState;

    /* BUG: 此管理器并非零初始化；budgetConfigured 与预算字段未写入初值，
     * 宿主首次调用 GetBudget/GetBudgetStats 时会读取未初始化状态。 */
    gc->managedMemories = sizeof(SZrGlobalState) + sizeof(SZrState);
    gc->gcDebtSize = 0;
    gc->atomicMemories = 0;
    gc->aliveMemories = 0;
    gc->ignoredObjectCount = 0;
    gc->ignoredObjectCapacity = 0;
    gc->ignoredObjects = ZR_NULL;
    gc->rememberedObjects = ZR_NULL;
    gc->rememberedObjectCount = 0;
    gc->rememberedObjectCapacity = 0;
    gc->nextRegionId = 1u;
    gc->currentEdenRegionId = 0u;
    gc->currentSurvivorRegionId = 0u;
    gc->currentOldRegionId = 0u;
    gc->currentEdenRegionIndex = ZR_MAX_SIZE;
    gc->currentSurvivorRegionIndex = ZR_MAX_SIZE;
    gc->currentOldRegionIndex = ZR_MAX_SIZE;
    gc->currentEdenRegionUsedBytes = 0u;
    gc->currentSurvivorRegionUsedBytes = 0u;
    gc->currentOldRegionUsedBytes = 0u;
    gc->regions = ZR_NULL;
    gc->regionCount = 0;
    gc->regionCapacity = 0;
    gc->gcPauseBudget = ZR_GC_DEFAULT_PAUSE_BUDGET;
    gc->gcSweepSliceBudget = ZR_GC_DEFAULT_SWEEP_SLICE_BUDGET;
    gc->gcLastStepWork = 0;
    gc->gcLastCompletedRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;

    gc->gcObjectList = ZR_CAST_RAW_OBJECT_AS_SUPER(state);

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

    gc->heapLimitBytes = 0;
    gc->youngRegionSize = 256u * 1024u;
    gc->youngRegionCountTarget = 4u;
    gc->survivorAgeThreshold = 2u;
    gc->pauseBudgetUs = 2000u;
    gc->remarkBudgetUs = 1000u;
    gc->workerCount = 1u;
    gc->fragmentationCompactThreshold = 35u;
    gc->gcFlags = 0u;
    gc->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    gc->minorCollectionEpoch = 0u;
    gc->oldCompactionScanEpoch = 0u;
    gc->concurrentMajorActive = ZR_FALSE;
    gc->concurrentMajorForceCompact = ZR_FALSE;
    gc->concurrentMajorMarkDrained = ZR_FALSE;
    gc->concurrentMajorCycleId = 0u;
    gc->concurrentMajorWork = 0u;
    memset(&gc->statsSnapshot, 0, sizeof(gc->statsSnapshot));
    gc->statsSnapshot.heapLimitBytes = gc->heapLimitBytes;
    gc->statsSnapshot.managedMemoryBytes = 0u;
    gc->statsSnapshot.gcDebtBytes = 0;
    gc->statsSnapshot.pauseBudgetUs = gc->pauseBudgetUs;
    gc->statsSnapshot.remarkBudgetUs = gc->remarkBudgetUs;
    gc->statsSnapshot.workerCount = gc->workerCount;
    gc->statsSnapshot.ignoredObjectCount = 0u;
    gc->statsSnapshot.rememberedObjectCount = 0u;
    gc->statsSnapshot.regionCount = 0u;
    gc->statsSnapshot.edenRegionCount = 0u;
    gc->statsSnapshot.survivorRegionCount = 0u;
    gc->statsSnapshot.oldRegionCount = 0u;
    gc->statsSnapshot.pinnedRegionCount = 0u;
    gc->statsSnapshot.largeRegionCount = 0u;
    gc->statsSnapshot.permanentRegionCount = 0u;
    gc->statsSnapshot.edenUsedBytes = 0u;
    gc->statsSnapshot.survivorUsedBytes = 0u;
    gc->statsSnapshot.oldUsedBytes = 0u;
    gc->statsSnapshot.pinnedUsedBytes = 0u;
    gc->statsSnapshot.largeUsedBytes = 0u;
    gc->statsSnapshot.permanentUsedBytes = 0u;
    gc->statsSnapshot.edenLiveBytes = 0u;
    gc->statsSnapshot.survivorLiveBytes = 0u;
    gc->statsSnapshot.oldLiveBytes = 0u;
    gc->statsSnapshot.pinnedLiveBytes = 0u;
    gc->statsSnapshot.largeLiveBytes = 0u;
    gc->statsSnapshot.permanentLiveBytes = 0u;
    gc->statsSnapshot.lastStepDurationUs = 0u;
    gc->statsSnapshot.lastStepWork = 0u;
    gc->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    gc->statsSnapshot.collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    memset(gc->collectionCounts, 0, sizeof(gc->collectionCounts));
    memset(gc->collectionTotalDurationUs, 0, sizeof(gc->collectionTotalDurationUs));
    memset(gc->collectionMaxDurationUs, 0, sizeof(gc->collectionMaxDurationUs));
    garbage_collector_refresh_cumulative_snapshot(gc);
}

/* 关闭阶段没有存续根；保留主 state 本体，释放最终回收后仍挂在对象链的条目。 */
static void garbage_collector_release_shutdown_objects(SZrState *state, SZrGarbageCollector *collector) {
    SZrRawObject *stateObject = ZR_CAST_RAW_OBJECT_AS_SUPER(state);

    while (collector->gcObjectList != ZR_NULL) {
        SZrRawObject *object = collector->gcObjectList;

        collector->gcObjectList = object->next;
        object->next = ZR_NULL;
        if (object != stateObject) {
            garbage_collector_free_object(state, object);
        }
    }
}

/* GlobalState_Free 的单次收尾入口；先尝试终结，再释放剩余对象和辅助表。 */
void ZrCore_GarbageCollector_Free(SZrGlobalState *global, SZrGarbageCollector *collector) {
    TZrSize ignoredBytes;
    TZrSize regionBytes;

    if (global == ZR_NULL || collector == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    collector->ignoredObjectCount = 0;

    if (global->mainThreadState != ZR_NULL) {
        SZrState *state = global->mainThreadState;
        SZrRawObject *stateObject = ZR_CAST_RAW_OBJECT_AS_SUPER(state);

        if (stateObject != ZR_NULL &&
            stateObject->type < ZR_RAW_OBJECT_TYPE_CLOSURE_ENUM_MAX &&
            stateObject->type != ZR_RAW_OBJECT_TYPE_INVALID) {
            TZrSize maxIterations = ZR_GC_SHUTDOWN_FULL_COLLECTION_ITERATION_LIMIT;
            TZrSize iterationCount = 0;

            global->garbageCollector->waitToScanObjectList = ZR_NULL;
            global->garbageCollector->waitToScanAgainObjectList = ZR_NULL;
            global->garbageCollector->waitToReleaseObjectList = ZR_NULL;

            ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
            while (global->garbageCollector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
                if (++iterationCount > maxIterations) {
                    global->garbageCollector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
                    break;
                }

                garbage_collector_single_step(state);
                if (global->garbageCollector->stopGcFlag) {
                    break;
                }
            }
        }

        /* Global shutdown has no surviving VM roots. Release objects that the
         * final full collection kept alive through global registry fields. */
        garbage_collector_release_shutdown_objects(state, collector);
    }

    collector->gcObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->permanentObjectList = ZR_NULL;
    collector->aliveObjectList = ZR_NULL;
    collector->circleMoreObjectList = ZR_NULL;
    collector->circleOnceObjectList = ZR_NULL;
    collector->aliveObjectWithReleaseFunctionList = ZR_NULL;
    collector->circleMoreObjectWithReleaseFunctionList = ZR_NULL;
    collector->circleOnceObjectWithReleaseFunctionList = ZR_NULL;

    if (collector->ignoredObjects != ZR_NULL) {
        ignoredBytes = collector->ignoredObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(
                global, collector->ignoredObjects, ignoredBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        collector->ignoredObjects = ZR_NULL;
    }
    collector->ignoredObjectCapacity = 0;
    if (collector->rememberedObjects != ZR_NULL) {
        TZrSize rememberedBytes = collector->rememberedObjectCapacity * sizeof(SZrRawObject *);
        ZrCore_Memory_RawFreeWithType(
                global, collector->rememberedObjects, rememberedBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        collector->rememberedObjects = ZR_NULL;
    }
    collector->rememberedObjectCapacity = 0;
    collector->rememberedObjectCount = 0;
    if (collector->regions != ZR_NULL) {
        regionBytes = collector->regionCapacity * sizeof(SZrGarbageCollectRegionDescriptor);
        ZrCore_Memory_RawFreeWithType(global, collector->regions, regionBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        collector->regions = ZR_NULL;
    }
    collector->regionCount = 0;
    collector->regionCapacity = 0;

    ZrCore_Memory_RawFreeWithType(global, collector, sizeof(SZrGarbageCollector), ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

/* 分配累加、回收抵扣均落在同一债务槽；调用方须持有有效管理器。 */
void ZrCore_GarbageCollector_AddDebtSpace(SZrGlobalState *global, TZrMemoryOffset size) {
    TZrMemoryOffset currentDebt = global->garbageCollector->gcDebtSize;

    if (size > 0) {
        if (currentDebt > ZR_MAX_MEMORY_OFFSET - size) {
            global->garbageCollector->gcDebtSize = ZR_MAX_MEMORY_OFFSET;
        } else {
            global->garbageCollector->gcDebtSize = currentDebt + size;
        }
    } else if (size < 0) {
        TZrMemoryOffset newDebt = currentDebt + size;
        if (newDebt < 0 || newDebt > currentDebt) {
            global->garbageCollector->gcDebtSize = 0;
        } else {
            global->garbageCollector->gcDebtSize = newDebt;
        }
    }
}

/* 显式保活表承担宿主/反射临时根；重复登记不增加引用计数，扩容失败时调用方须保留其他根。
 * BUG: NativeCallPinObject 可从同域多个 RUNNING mutator 并发进入，本表的计数/数组读写未持锁。 */
TZrBool ZrCore_GarbageCollector_IgnoreObject(SZrState *state, SZrRawObject *object) {
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    collector = state->global->garbageCollector;
    if (collector == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->garbageCollectMark.ignoredRegistryIndex != ZR_MAX_SIZE &&
        garbage_collector_ignore_registry_contains(collector, object)) {
        garbage_collector_mark_ignored_root_if_needed_fast(state, object);
        return ZR_TRUE;
    }

    if (collector->ignoredObjectCount >= collector->ignoredObjectCapacity &&
        !garbage_collector_ensure_ignore_registry_capacity(state->global, collector->ignoredObjectCount + 1)) {
        return ZR_FALSE;
    }

    /* BUG: 两个首登者可写同一槽并丢根；扩容还可释放另一线程正在读取的旧表。 */
    collector->ignoredObjects[collector->ignoredObjectCount] = object;
    object->garbageCollectMark.ignoredRegistryIndex = collector->ignoredObjectCount;
    collector->ignoredObjectCount++;
    garbage_collector_mark_ignored_root_if_needed_fast(state, object);
    return ZR_TRUE;
}

/* 撤销该 global 的保活登记；过期索引只清对象侧，不碰其他登记。
 * BUG: 与并行登记/撤销共用无锁计数和交换删除，可能破坏其他对象的索引。 */
TZrBool ZrCore_GarbageCollector_UnignoreObject(SZrGlobalState *global, SZrRawObject *object) {
    SZrGarbageCollector *collector;
    TZrSize index;
    TZrSize lastIndex;
    SZrRawObject *movedObject;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->garbageCollectMark.ignoredRegistryIndex == ZR_MAX_SIZE) {
        return ZR_FALSE;
    }

    collector = global->garbageCollector;
    index = object->garbageCollectMark.ignoredRegistryIndex;
    if (collector->ignoredObjects == ZR_NULL || index >= collector->ignoredObjectCount ||
        collector->ignoredObjects[index] != object) {
        object->garbageCollectMark.ignoredRegistryIndex = ZR_MAX_SIZE;
        return ZR_FALSE;
    }

    lastIndex = collector->ignoredObjectCount - 1u;
    movedObject = collector->ignoredObjects[lastIndex];
    collector->ignoredObjects[lastIndex] = ZR_NULL;
    collector->ignoredObjectCount = lastIndex;
    if (index != lastIndex) {
        collector->ignoredObjects[index] = movedObject;
        if (movedObject != ZR_NULL) {
            movedObject->garbageCollectMark.ignoredRegistryIndex = index;
        }
    }
    object->garbageCollectMark.ignoredRegistryIndex = ZR_MAX_SIZE;

    return ZR_TRUE;
}

TZrBool ZrCore_GarbageCollector_IsObjectIgnored(SZrGlobalState *global, SZrRawObject *object) {
    return ZrCore_GarbageCollector_IsObjectIgnoredFast(global, object);
}

/* 供关闭、内存压力和宿主强制回收共用的暂停域完整回收入口。 */
void ZrCore_GarbageCollector_GcFull(SZrState *state, TZrBool isImmediate) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;
    TZrUInt64 startedUs;
    TZrSize work = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    ZrCore_ExecutionBudget_GcBegin(state);
    if (!ZrCore_GcDomain_StopTheWorldBegin(
                state, ZR_GC_DOMAIN_PAUSE_TIMEOUT_MILLISECONDS, ZR_NULL)) {
        ZrCore_ExecutionBudget_GcEnd(state);
        return;
    }
    ZrCore_GcDomain_MutationLock(state->gcDomain);
    garbage_collector_concurrent_major_cancel(state);
    startedUs = garbage_collector_now_us();

    collector->gcRunningStatus = ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->statsSnapshot.lastRequestedCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->statsSnapshot.lastCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_REMARK;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;

    ZR_ASSERT(!collector->isImmediateGcFlag);
    collector->isImmediateGcFlag = isImmediate;
    /* BUG: 对象 scanMarkGcFunction 在受保护调用内抛异常时，longjmp 越过下方 mutation unlock、
     * StopTheWorldEnd 和 GcEnd，后续 mutator 可被留在暂停域外等待。 */
    if (collector->gcMode == ZR_GARBAGE_COLLECT_MODE_GENERATIONAL) {
        work += garbage_collector_run_generational_full(state);
    } else {
        work += garbage_collector_prepare_major_collection(state);
        garbage_collector_full_inc(state, global);
        work += collector->gcLastStepWork;
    }
    collector->isImmediateGcFlag = ZR_FALSE;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->gcLastStepWork = work > 0 ? work : 1;
    garbage_collector_record_step_telemetry(collector, startedUs);
    if (collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    }
    ZrCore_GcDomain_MutationUnlock(state->gcDomain);
    ZrCore_GcDomain_StopTheWorldEnd(state);
    ZrCore_ExecutionBudget_GcEnd(state);
}

/* 安全点的债务偿还入口：按回收模式执行一个暂停或并发标记切片。 */
void ZrCore_GarbageCollector_GcStep(SZrState *state) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;
    TZrMemoryOffset debtBefore;
    EZrGarbageCollectRunningStatus statusBefore;
    TZrSize totalWork = 0;
    TZrSize pauseBudget;
    TZrUInt64 startedUs;
    TZrBool domainPaused = ZR_FALSE;
    TZrBool concurrentMarkSlice = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;
    ZrCore_ExecutionBudget_GcBegin(state);
    collector->gcLastStepWork = 0;
    debtBefore = collector->gcDebtSize;
    statusBefore = collector->gcRunningStatus;
    startedUs = garbage_collector_now_us();

    if (!gcrunning(global)) {
        if (collector->gcDebtSize > 0) {
            collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_RUNNING;
        } else {
            ZrCore_GarbageCollector_AddDebtSpace(global, -ZR_GC_DEBT_CREDIT_BYTES);
            collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
            collector->statsSnapshot.lastStepDurationUs = 0u;
            collector->statsSnapshot.lastStepWork = 0u;
            garbage_collector_refresh_cumulative_snapshot(collector);
            ZrCore_ExecutionBudget_GcEnd(state);
            return;
        }
    }

    concurrentMarkSlice =
            garbage_collector_is_generational_mode(global) &&
            collector->concurrentMajorActive &&
            !collector->concurrentMajorMarkDrained;
    if (!concurrentMarkSlice) {
        if (!ZrCore_GcDomain_StopTheWorldBegin(
                    state, ZR_GC_DOMAIN_PAUSE_TIMEOUT_MILLISECONDS, ZR_NULL)) {
            ZrCore_ExecutionBudget_GcEnd(state);
            return;
        }
        domainPaused = ZR_TRUE;
    }

    if (garbage_collector_is_generational_mode(global)) {
        collector->statsSnapshot.lastCollectionKind = collector->scheduledCollectionKind;
        if (!concurrentMarkSlice) {
            collector->collectionPhase =
                    collector->scheduledCollectionKind ==
                                    ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR
                            ? ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK
                            : ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
        }
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
        if (concurrentMarkSlice) {
            /* BUG: 多个 RUNNING mutator 可同时到达此分支；扫描本体虽持 mutation lock，
             * 切片累计与本函数的共享遥测写入在锁外，形成数据竞争。 */
            collector->gcLastStepWork =
                    garbage_collector_concurrent_major_mark_slice(
                            state,
                            collector->workerCount > 0u
                                    ? (TZrSize)collector->workerCount * 8u
                                    : 8u);
        } else {
            /* BUG: minor 疏散时复制分配 OOM 可由 Exception_Throw longjmp；
             * 受保护调用因此跳过 StopTheWorldEnd/GcEnd，暂停请求残留。 */
            garbage_collector_run_generational_step(state);
        }
    } else {
        collector->statsSnapshot.lastCollectionKind = collector->scheduledCollectionKind;
        collector->collectionPhase = collector->scheduledCollectionKind == ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR
                                             ? ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MINOR_MARK
                                             : ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
        pauseBudget = collector->gcPauseBudget > 0 ? (TZrSize)collector->gcPauseBudget : 1;
        for (TZrSize step = 0; step < pauseBudget; step++) {
            EZrGarbageCollectRunningStatus stepStatusBefore = collector->gcRunningStatus;
            TZrBool stepWasSweepPhase =
                    stepStatusBefore >= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS &&
                    stepStatusBefore <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
            TZrSize stepWork = garbage_collector_single_step(state);

            totalWork += stepWork;

            if (collector->stopGcFlag ||
                collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
                break;
            }

            if (stepWasSweepPhase || ZrCore_GarbageCollector_IsSweeping(global)) {
                break;
            }

            if (stepWork == 0 && stepStatusBefore == collector->gcRunningStatus) {
                break;
            }
        }

        collector->gcLastStepWork = totalWork;
        if (collector->gcLastStepWork > 0 && collector->gcDebtSize > 0) {
            TZrMemoryOffset workBytes = (TZrMemoryOffset)collector->gcLastStepWork * ZR_GC_WORK_TO_MEMORY_BYTES;
            ZrCore_GarbageCollector_AddDebtSpace(global, -workBytes);
        }
    }

    if (collector->gcRunningStatus == ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
        collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
        collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    }

    if (collector->gcLastStepWork == 0 &&
        (statusBefore != collector->gcRunningStatus || debtBefore != collector->gcDebtSize)) {
        collector->gcLastStepWork = 1;
    }
    garbage_collector_record_step_telemetry(collector, startedUs);
    collector->gcLastCompletedRunningStatus = collector->gcRunningStatus;
    if (domainPaused) {
        ZrCore_GcDomain_StopTheWorldEnd(state);
    }
    ZrCore_ExecutionBudget_GcEnd(state);
}

/* 宿主设置软阈值；达到阈值仅安排完整回收，不在此入口分配/暂停。 */
void ZrCore_GarbageCollector_SetHeapLimitBytes(SZrGlobalState *global, TZrMemoryOffset heapLimitBytes) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->heapLimitBytes = heapLimitBytes;
    global->garbageCollector->statsSnapshot.heapLimitBytes = heapLimitBytes;
}

/* 更新暂停阶段的预算配置并同步诊断快照。 */
void ZrCore_GarbageCollector_SetPauseBudgetUs(SZrGlobalState *global,
                                              TZrUInt64 pauseBudgetUs,
                                              TZrUInt64 remarkBudgetUs) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->pauseBudgetUs = pauseBudgetUs;
    global->garbageCollector->remarkBudgetUs = remarkBudgetUs;
    global->garbageCollector->gcPauseBudget = pauseBudgetUs > 0 ? pauseBudgetUs : 1u;
    global->garbageCollector->statsSnapshot.pauseBudgetUs = pauseBudgetUs;
    global->garbageCollector->statsSnapshot.remarkBudgetUs = remarkBudgetUs;
}

/* workerCount 当前只作为并发标记单次切片的工作量系数。 */
void ZrCore_GarbageCollector_SetWorkerCount(SZrGlobalState *global, TZrUInt32 workerCount) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return;
    }

    global->garbageCollector->workerCount = workerCount;
    global->garbageCollector->statsSnapshot.workerCount = workerCount;
}

/* 将宿主请求提交给安全点，保留“显式”来源供驱动器决策。 */
void ZrCore_GarbageCollector_ScheduleCollection(SZrGlobalState *global, EZrGarbageCollectCollectionKind kind) {
    garbage_collector_schedule_collection_internal(global, kind, ZR_TRUE);
}

/* 调试器/宿主拉取压力与域指标；域字段只在域锁内采集。
 * BUG: 压力汇总遍历 regions 时另一 mutator 可扩容释放旧数组，产生悬垂读取。 */
void ZrCore_GarbageCollector_GetStatsSnapshot(SZrGlobalState *global, SZrGarbageCollectorStatsSnapshot *outSnapshot) {
    SZrGcDomain *domain;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || outSnapshot == ZR_NULL) {
        return;
    }

    global->garbageCollector->statsSnapshot.heapLimitBytes = global->garbageCollector->heapLimitBytes;
    global->garbageCollector->statsSnapshot.pauseBudgetUs = global->garbageCollector->pauseBudgetUs;
    global->garbageCollector->statsSnapshot.remarkBudgetUs = global->garbageCollector->remarkBudgetUs;
    global->garbageCollector->statsSnapshot.workerCount = global->garbageCollector->workerCount;
    garbage_collector_refresh_pressure_snapshot(global->garbageCollector);
    global->garbageCollector->statsSnapshot.rememberedObjectCount =
            (TZrUInt32)global->garbageCollector->rememberedObjectCount;
    global->garbageCollector->statsSnapshot.lastStepWork = (TZrUInt64)global->garbageCollector->gcLastStepWork;
    domain = global->gcDomain;
    if (domain != ZR_NULL) {
        ZrCore_GcDomain_Lock(domain);
        global->garbageCollector->statsSnapshot.domainId = domain->identity.id;
        global->garbageCollector->statsSnapshot.domainGeneration =
                domain->identity.generation;
        global->garbageCollector->statsSnapshot.activeMutatorCount = 0u;
        for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
            if (domain->mutators[index].status ==
                        ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING ||
                domain->mutators[index].status ==
                        ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL) {
                global->garbageCollector->statsSnapshot.activeMutatorCount++;
            }
        }
        global->garbageCollector->statsSnapshot.safepointWaitCount =
                domain->safepointWaitCount;
        global->garbageCollector->statsSnapshot.safepointWaitTotalUs =
                domain->safepointWaitTotalUs;
        global->garbageCollector->statsSnapshot.safepointWaitMaxUs =
                domain->safepointWaitMaxUs;
        global->garbageCollector->statsSnapshot.outboundTransferPrepareCount =
                domain->outboundTransferPrepareCount;
        global->garbageCollector->statsSnapshot.outboundTransferPublishCount =
                domain->outboundTransferPublishCount;
        global->garbageCollector->statsSnapshot.outboundTransferAbortCount =
                domain->outboundTransferAbortCount;
        global->garbageCollector->statsSnapshot.outboundTransferObjectCount =
                domain->outboundTransferObjectCount;
        global->garbageCollector->statsSnapshot.outboundTransferByteCount =
                domain->outboundTransferByteCount;
        global->garbageCollector->statsSnapshot.inboundTransferClaimCount =
                domain->inboundTransferClaimCount;
        global->garbageCollector->statsSnapshot.inboundTransferCommitCount =
                domain->inboundTransferCommitCount;
        global->garbageCollector->statsSnapshot.inboundTransferAbortCount =
                domain->inboundTransferAbortCount;
        global->garbageCollector->statsSnapshot.inboundTransferObjectCount =
                domain->inboundTransferObjectCount;
        global->garbageCollector->statsSnapshot.inboundTransferByteCount =
                domain->inboundTransferByteCount;
        ZrCore_GcDomain_Unlock(domain);
    }
    global->garbageCollector->statsSnapshot.concurrentMajorActive =
            global->garbageCollector->concurrentMajorActive;
    garbage_collector_refresh_cumulative_snapshot(global->garbageCollector);
    *outSnapshot = global->garbageCollector->statsSnapshot;
}

/* 屏障和测试用成员查询；仅接受该 global 管理器中的对象。 */
TZrBool ZrCore_GarbageCollector_HasRememberedObject(SZrGlobalState *global, SZrRawObject *object) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    return garbage_collector_remembered_registry_contains(global->garbageCollector, object);
}

/* 对象跨作用域、模块或宿主持有时登记逸出，通知闭包捕获传播。 */
void ZrCore_GarbageCollector_MarkRawObjectEscaped(SZrState *state,
                                                  SZrRawObject *object,
                                                  TZrUInt32 escapeFlags,
                                                  TZrUInt32 scopeDepth,
                                                  EZrGarbageCollectPromotionReason promotionReason) {
    garbage_collector_mark_raw_object_escaped_internal(
            state,
            object,
            escapeFlags,
            scopeDepth,
            promotionReason,
            ZR_TRUE);
}

/* 泛值入口仅把 GC 对象交给同一逸出登记逻辑。 */
void ZrCore_GarbageCollector_MarkValueEscaped(SZrState *state,
                                              const SZrTypeValue *value,
                                              TZrUInt32 escapeFlags,
                                              TZrUInt32 scopeDepth,
                                              EZrGarbageCollectPromotionReason promotionReason) {
    SZrRawObject *object;

    if (value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object == ZR_NULL) {
        return;
    }

    garbage_collector_mark_raw_object_escaped_internal(
            state,
            object,
            escapeFlags,
            scopeDepth,
            promotionReason,
            ZR_TRUE);
}

/* 宿主和跨域资源持有的对象转入不可移动区，并记录晋升原因。 */
void ZrCore_GarbageCollector_PinObject(SZrState *state,
                                       SZrRawObject *object,
                                       EZrGarbageCollectPinKind pinKind) {
    TZrUInt32 escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE;
    TZrSize objectSize;
    TZrUInt32 previousRegionId;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL || object == ZR_NULL) {
        return;
    }

    objectSize = garbage_collector_get_object_base_size_fast(object);
    previousRegionId = object->garbageCollectMark.regionId;
    object->garbageCollectMark.pinFlags |= (TZrUInt32)pinKind;
    if ((pinKind & ZR_GARBAGE_COLLECT_PIN_KIND_HOST_HANDLE) != 0) {
        escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE;
    }
    if ((pinKind & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0) {
        escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE;
    }
    if ((pinKind & ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT) != 0) {
        escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
    }
    ZrCore_RawObject_SetStorageKind(object, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED);
    ZrCore_RawObject_SetRegionKind(object, ZR_GARBAGE_COLLECT_REGION_KIND_PINNED);
    /* BUG: 区段登记表扩容 OOM 时重分配返回零；此 void 入口已改固定类别，
     * 仍把零 ID 留给对象且不向调用方报告失败，区段快照会漏计该对象。 */
    object->garbageCollectMark.regionId = garbage_collector_reassign_region_id_cached(
            state->global,
            previousRegionId,
            object->garbageCollectMark.regionDescriptorIndex,
            ZR_GARBAGE_COLLECT_REGION_KIND_PINNED,
            objectSize,
            &object->garbageCollectMark.regionDescriptorIndex);
    garbage_collector_mark_raw_object_escaped_internal(state,
                                                       object,
                                                       escapeFlags,
                                                       object->garbageCollectMark.anchorScopeDepth,
                                                       ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED,
                                                       ZR_TRUE);
}

TZrBool ZrCore_GarbageCollector_IsInvariant(SZrGlobalState *global) {
    return global->garbageCollector->gcRunningStatus <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_ATOMIC;
}

TZrBool ZrCore_GarbageCollector_IsSweeping(SZrGlobalState *global) {
    EZrGarbageCollectRunningStatus status = global->garbageCollector->gcRunningStatus;

    return status >= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS &&
           status <= ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_END;
}

/* 安全点按堆压力/债务触发回收；请求与实际完成之间可能跨多个步骤。 */
void ZrCore_GarbageCollector_CheckGc(SZrState *state) {
    SZrGlobalState *global;
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return;
    }

    global = state->global;
    collector = global->garbageCollector;

    if (collector->heapLimitBytes > 0 && collector->managedMemories >= collector->heapLimitBytes) {
        garbage_collector_schedule_collection_internal(global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL, ZR_FALSE);
    }

    if (collector->gcDebtSize > 0) {
        ZrCore_GarbageCollector_GcStep(state);
    }
#if defined(ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST)
    if (collector->gcStatus == ZR_GARBAGE_COLLECT_STATUS_RUNNING &&
        !collector->isImmediateGcFlag &&
        !collector->concurrentMajorActive) {
        ZrCore_GarbageCollector_GcFull(state, ZR_FALSE);
    }
#endif
}

/* 先响应跨线程暂停，再执行本线程的 GC 债务检查。 */
void ZrCore_Gc_SafePoint(SZrState *state) {
    ZrCore_GcDomain_MutatorPoll(state);
    ZrCore_GarbageCollector_CheckGc(state);
}

/* VM 写入路径统一转交值屏障，并记录屏障频次。 */
void ZrCore_Gc_WriteBarrier(SZrState *state, SZrRawObject *ownerObject, SZrTypeValue *value) {
    ZrCore_Profile_RecordMemoryFromState(state, ZR_PROFILE_MEMORY_WRITE_BARRIER_COUNT, 1u);
    ZrCore_Value_Barrier(state, ownerObject, value);
}

/* 凭据只记录本次调用新增的状态，避免解除其他调用方的长期保活。 */
static void garbage_collector_reset_native_call_pin(SZrGcNativeCallPin *pin) {
    if (pin == ZR_NULL) {
        return;
    }

    pin->object = ZR_NULL;
    pin->pinKind = ZR_GARBAGE_COLLECT_PIN_KIND_NONE;
    pin->ignoredAddedByCaller = ZR_FALSE;
    pin->pinKindAddedByCaller = ZR_FALSE;
}

/* 本地调用前取得临时固定和保活，失败时清理本次凭据。 */
TZrBool ZrCore_Gc_NativeCallPinObject(SZrState *state, SZrRawObject *object, SZrGcNativeCallPin *pin) {
    TZrUInt32 previousPinFlags;
    TZrBool wasIgnored;
    TZrBool ignoredAddedAfterPin = ZR_FALSE;

    if (pin == ZR_NULL) {
        return ZR_FALSE;
    }

    garbage_collector_reset_native_call_pin(pin);
    if (object == ZR_NULL) {
        return ZR_TRUE;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->garbageCollector == ZR_NULL) {
        return ZR_FALSE;
    }

    pin->object = object;
    pin->pinKind = ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE;
    wasIgnored = ZrCore_GarbageCollector_IsObjectIgnoredFast(state->global, object);
    previousPinFlags = object->garbageCollectMark.pinFlags;
    if ((previousPinFlags & ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0u) {
        pin->pinKindAddedByCaller = ZR_TRUE;
    }
    ZrCore_GarbageCollector_PinObject(state, object, ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    /* BUG: 保活表扩容失败时只回退 pinFlags；PinObject 已更改 storage、region、
     * escape 状态，失败后对象仍可能被排除老区压实。 */
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global,
                                                          state,
                                                          object,
                                                          &ignoredAddedAfterPin)) {
        if (pin->pinKindAddedByCaller) {
            object->garbageCollectMark.pinFlags &= (TZrUInt32)~((TZrUInt32)pin->pinKind);
        }
        garbage_collector_reset_native_call_pin(pin);
        return ZR_FALSE;
    }
    pin->ignoredAddedByCaller = !wasIgnored && ignoredAddedAfterPin;
    return ZR_TRUE;
}

/* 值不是托管对象时不建立凭据，调用方仍可统一执行 Unpin。 */
TZrBool ZrCore_Gc_NativeCallPinValue(SZrState *state, const SZrTypeValue *value, SZrGcNativeCallPin *pin) {
    SZrRawObject *object;

    if (pin == ZR_NULL) {
        return ZR_FALSE;
    }
    garbage_collector_reset_native_call_pin(pin);
    if (value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }

    object = ZrCore_Value_GetRawObject(value);
    return ZrCore_Gc_NativeCallPinObject(state, object, pin);
}

/* 本地调用结束时仅撤销凭据新增的 pin 标记和显式根。 */
void ZrCore_Gc_NativeCallUnpin(SZrGlobalState *global, SZrGcNativeCallPin *pin) {
    SZrRawObject *object;

    if (pin == ZR_NULL || pin->object == ZR_NULL) {
        return;
    }

    object = pin->object;
    if (pin->pinKindAddedByCaller && pin->pinKind != ZR_GARBAGE_COLLECT_PIN_KIND_NONE) {
        object->garbageCollectMark.pinFlags &= (TZrUInt32)~((TZrUInt32)pin->pinKind);
    }
    if (pin->ignoredAddedByCaller && global != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }

    garbage_collector_reset_native_call_pin(pin);
}
