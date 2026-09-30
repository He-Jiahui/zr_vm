// 本文件把域内 major 回收拆成初始快照、并发标记切片和停世界收尾。

#include "gc/gc_internal.h"
#include "gc/gc_domain_internal.h"
/* 时钟回退或粒度为零时仍返回非零耗时，供阶段遥测保持可判读。 */
static TZrUInt64 concurrent_major_elapsed_us(TZrUInt64 startedUs) {
    TZrUInt64 finishedUs = garbage_collector_now_us();
    TZrUInt64 durationUs = finishedUs >= startedUs
                                   ? finishedUs - startedUs
                                   : 0u;
    return durationUs > 0u ? durationUs : 1u;
}
/* GcStep 在目标域停世界时调用；开启并发 major，让 mutator 随后在写屏障保护下恢复。 */
TZrSize garbage_collector_concurrent_major_begin(SZrState *state,
                                                  TZrBool forceCompact) {
    SZrGarbageCollector *collector;
    TZrSize work;
    TZrUInt64 startedUs;
    TZrUInt64 durationUs;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return 0u;
    }

    collector = state->global->garbageCollector;
    if (collector->concurrentMajorActive) {
        return 0u;
    }
    /* TODO: 初始暂停先全表重置标记；确认 pauseBudgetUs 是否也应限制这段 O(heap) preparation。 */
    startedUs = garbage_collector_now_us();
    work = garbage_collector_prepare_major_collection(state);
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->waitToReleaseObjectList = ZR_NULL;
    collector->releasedObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
    collector->concurrentMajorActive = ZR_TRUE;
    collector->concurrentMajorForceCompact = forceCompact;
    collector->concurrentMajorMarkDrained = ZR_FALSE;
    collector->concurrentMajorCycleId++; /* TODO: 当前仓内未见 cycle id 读取方，确认此字段用途。 */
    if (collector->concurrentMajorCycleId == 0u) {
        collector->concurrentMajorCycleId = 1u;
    }
    collector->concurrentMajorWork = work; /* TODO: 当前仓内未见该累计器的读取方，确认是否仍需维护。 */
    collector->collectionPhase =
            ZR_GARBAGE_COLLECT_COLLECTION_PHASE_MAJOR_MARK_CONCURRENT;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    /* 初始暂停内重启标记并扫描全部已登记 mutator 根；恢复后由屏障补入新边。 */
    ZrGarbageCollectorRestartCollection(state);
    work += garbage_collector_snapshot_concurrent_thread_roots(state);
    collector->concurrentMajorWork = work;
    durationUs = concurrent_major_elapsed_us(startedUs);
    collector->statsSnapshot.concurrentMajorCycleCount++;
    collector->statsSnapshot.concurrentMajorInitialPauseCount++;
    collector->statsSnapshot.concurrentMajorInitialPauseTotalUs += durationUs;
    if (collector->statsSnapshot.concurrentMajorInitialPauseMaxUs < durationUs) {
        collector->statsSnapshot.concurrentMajorInitialPauseMaxUs = durationUs;
    }
    collector->statsSnapshot.concurrentMajorActive = ZR_TRUE;
    return work > 0u ? work : 1u;
}
/* 每次至多弹出 objectBudget 个灰对象；配置的非零 maxObjects 可收紧单次切片，
 * 不限制单个对象扫描耗时或后续 remark/sweep；对象字段与写屏障共用域 mutation lock。 */
TZrSize garbage_collector_concurrent_major_mark_slice(
        SZrState *state,
        TZrSize objectBudget) {
    SZrGarbageCollector *collector;
    TZrSize work = 0u;
    TZrUInt64 startedUs;
    TZrUInt64 durationUs;
    /* BUG: gc.c/gc_cycle.c 仍在此锁外读取 active/drained 选择切片或收尾；该调用者竞态待另行处理。 */
    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return 0u;
    }
    collector = state->global->garbageCollector;
    if (objectBudget == 0u) {
        objectBudget = 1u;
    }
    /* objectBudget 限制队列弹出次数而非耗时；零值归一为一次，单个对象扫描仍可很重。 */
    startedUs = garbage_collector_now_us();
    ZrCore_GcDomain_MutationLock(state->gcDomain);
    if (!collector->concurrentMajorActive ||
        collector->concurrentMajorMarkDrained) {
        ZrCore_GcDomain_MutationUnlock(state->gcDomain);
        return 0u;
    }
    if (collector->budgetConfigured && collector->budget.maxObjects > 0u &&
        collector->budget.maxObjects < (TZrUInt64)objectBudget) {
        /* 仅收紧调用方上限；比较后才转回宿主大小，避免窄位宽截断。 */
        objectBudget = (TZrSize)collector->budget.maxObjects;
    }
    while (objectBudget-- > 0u) {
        if (collector->waitToScanObjectList == ZR_NULL) {
            if (collector->waitToScanAgainObjectList != ZR_NULL) {
                collector->waitToScanObjectList =
                        collector->waitToScanAgainObjectList;
                collector->waitToScanAgainObjectList = ZR_NULL;
            } else {
                collector->concurrentMajorMarkDrained = ZR_TRUE;
                break;
            }
        }
        work += ZrGarbageCollectorPropagateMark(state);
    }
    ZrCore_GcDomain_MutationUnlock(state->gcDomain);
    /* BUG: gc.c 允许多个 RUNNING mutator 同时进入此函数；锁外普通字段累加形成数据竞争。 */
    collector->concurrentMajorWork += work;
    durationUs = concurrent_major_elapsed_us(startedUs);
    collector->statsSnapshot.concurrentMajorMarkSliceCount++;
    collector->statsSnapshot.concurrentMajorMarkTotalUs += durationUs;
    if (collector->statsSnapshot.concurrentMajorMarkMaxUs < durationUs) {
        collector->statsSnapshot.concurrentMajorMarkMaxUs = durationUs;
    }
    return work;
}
/* 仅在标记排空后由停世界路径调用，再交给既有 major remark/sweep/可选压缩流程。 */
TZrSize garbage_collector_concurrent_major_finish(
        SZrState *state,
        TZrBool *outDidCompact) {
    SZrGarbageCollector *collector;
    TZrSize work;

    if (outDidCompact != ZR_NULL) {
        *outDidCompact = ZR_FALSE;
    }
    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return 0u;
    }
    collector = state->global->garbageCollector;
    if (!collector->concurrentMajorActive ||
        !collector->concurrentMajorMarkDrained) {
        return 0u;
    }
    /* BUG: 这里同步完成 atomic/remark、sweep 与 finalizer；remarkBudgetUs 未限制这段停顿。 */
    work = garbage_collector_finish_generational_major_collection(
            state, collector->concurrentMajorForceCompact, outDidCompact);
    collector->concurrentMajorWork += work;
    collector->concurrentMajorActive = ZR_FALSE;
    collector->concurrentMajorForceCompact = ZR_FALSE;
    collector->concurrentMajorMarkDrained = ZR_FALSE;
    collector->scheduledCollectionKind =
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->gcFlags &= ~ZR_GC_FLAG_EXPLICIT_COLLECTION_REQUEST;
    collector->collectionPhase = ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE;
    collector->statsSnapshot.collectionPhase = collector->collectionPhase;
    collector->statsSnapshot.concurrentMajorActive = ZR_FALSE;
    collector->gcStatus = ZR_GARBAGE_COLLECT_STATUS_STOP_BY_SELF;
    return work > 0u ? work : 1u;
}
/* ZrCore_GarbageCollector_GcFull 在停世界并持 mutation lock 后调用，以废弃半轮标记。 */
void garbage_collector_concurrent_major_cancel(SZrState *state) {
    SZrGarbageCollector *collector;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL) {
        return;
    }
    /* 完整回收路径另行清理 release 队列并复位最终 phase；此处丢弃并发标记队列。 */
    collector = state->global->garbageCollector;
    collector->concurrentMajorActive = ZR_FALSE;
    collector->concurrentMajorForceCompact = ZR_FALSE;
    collector->concurrentMajorMarkDrained = ZR_FALSE;
    collector->concurrentMajorWork = 0u;
    collector->statsSnapshot.concurrentMajorActive = ZR_FALSE;
    collector->waitToScanObjectList = ZR_NULL;
    collector->waitToScanAgainObjectList = ZR_NULL;
    collector->gcObjectListSweeper = ZR_NULL;
}
