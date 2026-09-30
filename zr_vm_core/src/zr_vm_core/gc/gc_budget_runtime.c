#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

#include <limits.h>
#include <string.h>

/* 预算状态属于 global 的单一回收器；空 global 由宿主入口直接拒绝。 */
static SZrGarbageCollector *gc_budget_collector(SZrGlobalState *global) {
    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        return ZR_NULL;
    }
    return global->garbageCollector;
}

/* 遥测累计不能因长时间运行而回绕到较小值。 */
static TZrUInt64 gc_budget_sat_add(TZrUInt64 left, TZrUInt64 right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

/* GC 债务使用宿主 ptrdiff_t，预算见证转成固定宽度的有符号标量。 */
static TZrInt64 gc_budget_debt(const SZrGarbageCollector *collector) {
    /* TODO: 32 位宿主把 INT64_MAX 转成较窄 TZrMemoryOffset 的结果依实现而定；
     * 需确认支持的目标位宽，并检查非负债务是否被误判为溢出。 */
    if (collector->gcDebtSize > (TZrMemoryOffset)INT64_MAX) {
        return INT64_MAX;
    }
    return (TZrInt64)collector->gcDebtSize;
}

/* 预算入口同步同一回收器的通用 GC 快照；调用者需保证没有并发写入。 */
static void gc_budget_sync_snapshot(SZrGarbageCollector *collector) {
    SZrGarbageCollectorStatsSnapshot *snapshot = &collector->statsSnapshot;
    snapshot->budgetConfigured = collector->budgetConfigured;
    snapshot->budgetLastStatus = collector->budgetLastStatus;
    snapshot->budgetPhase = collector->budgetPhase;
    snapshot->budgetPauseReason = collector->budgetPauseReason;
    snapshot->budgetCursor = collector->budgetCursor;
    snapshot->budgetWorkDone = collector->budgetWorkDone;
    snapshot->budgetElapsedUs = collector->budgetElapsedUs;
    snapshot->budgetDebtBytes = collector->budgetDebtBytes;
    snapshot->budgetOverBudgetCount = collector->budgetOverBudgetCount;
    snapshot->budgetCompactDeferredCount = collector->budgetCompactDeferredCount;
    snapshot->budgetPressure = collector->budgetPressure;
    snapshot->budgetFallback = collector->budgetFallback;
}

TZrBool ZrCore_GarbageCollector_SetBudget(SZrGlobalState *global,
                                          const SZrGcBudget *budget) {
    SZrGarbageCollector *collector = gc_budget_collector(global);
    SZrGcBudgetDiagnostic diagnostic;

    if (collector == ZR_NULL ||
        !ZrCore_GcBudget_Validate(budget, &diagnostic)) {
        return ZR_FALSE;
    }

    /* TODO: 同一 global 可供多个 mutator 访问，而设置、评估及读统计均未持锁；
     * 仓内仅测试单线程调用，需定义宿主并发调用前提或统一同步方式。 */
    /* 先验证再整体替换配置；无效更新不改变既有预算和累计见证。 */
    collector->budget = *budget;
    collector->budgetConfigured = ZR_TRUE;
    collector->budgetLastStatus = ZR_GC_BUDGET_STEP_ACCEPTED;
    collector->budgetPhase = ZR_GC_BUDGET_PHASE_IDLE;
    collector->budgetPauseReason = ZR_GC_BUDGET_PAUSE_NONE;
    collector->budgetCursor = 0u;
    collector->budgetWorkDone = 0u;
    collector->budgetElapsedUs = 0u;
    collector->budgetDebtBytes = gc_budget_debt(collector);
    collector->budgetOverBudgetCount = 0u;
    collector->budgetCompactDeferredCount = 0u;
    collector->budgetPressure = ZR_FALSE;
    collector->budgetFallback = ZR_FALSE;
    gc_budget_sync_snapshot(collector);
    return ZR_TRUE;
}

TZrBool ZrCore_GarbageCollector_GetBudget(SZrGlobalState *global,
                                          SZrGcBudget *outBudget) {
    SZrGarbageCollector *collector = gc_budget_collector(global);
    if (collector == ZR_NULL || outBudget == ZR_NULL ||
        !collector->budgetConfigured) {
        return ZR_FALSE;
    }
    *outBudget = collector->budget;
    return ZR_TRUE;
}

TZrBool ZrCore_GarbageCollector_EvaluateBudgetStep(
        SZrGlobalState *global,
        EZrGcBudgetPhase phase,
        TZrUInt64 workUnits,
        TZrUInt64 elapsedUs,
        TZrUInt64 bytes,
        TZrUInt64 objects,
        TZrUInt64 atomicPauseUs,
        SZrGcBudgetStepResult *outResult) {
    SZrGarbageCollector *collector = gc_budget_collector(global);
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic diagnostic;
    TZrBool evaluated;

    if (collector == ZR_NULL || outResult == ZR_NULL ||
        !collector->budgetConfigured) {
        return ZR_FALSE;
    }

    evaluated = ZrCore_GcBudget_EvaluateStep(
            &collector->budget, phase, collector->budgetCursor,
            workUnits, elapsedUs, bytes, objects, gc_budget_debt(collector),
            atomicPauseUs, &result, &diagnostic);
    if (!evaluated) {
        *outResult = result;
        return ZR_FALSE;
    }

    collector->budgetLastStatus = result.status;
    collector->budgetPhase = result.phase;
    collector->budgetPauseReason = result.pauseReason;
    collector->budgetDebtBytes = result.debtBytes;
    collector->budgetPressure = result.pressure;
    collector->budgetWorkDone = gc_budget_sat_add(collector->budgetWorkDone,
                                                   result.workDone);
    collector->budgetElapsedUs = gc_budget_sat_add(collector->budgetElapsedUs,
                                                    result.elapsedUs);
    collector->budgetOverBudgetCount = gc_budget_sat_add(
            collector->budgetOverBudgetCount, result.overBudgetCount);
    collector->budgetCompactDeferredCount = gc_budget_sat_add(
            collector->budgetCompactDeferredCount,
            result.compactDeferredCount);
    /* TODO: 契约、账本和 major 将 OVER_BUDGET 视为已完成工作并推进游标；
     * 此宿主包装层按现有测试保留旧游标，接入实际驱动前需统一回退语义。 */
    /* 拒绝与原子暂停超限向宿主报告回退；仅接纳状态发布新游标。 */
    collector->budgetFallback = result.status == ZR_GC_BUDGET_STEP_REJECTED ||
                                result.status == ZR_GC_BUDGET_STEP_OVER_BUDGET;
    if (result.status == ZR_GC_BUDGET_STEP_ACCEPTED) {
        collector->budgetCursor = result.nextCursor;
    }
    gc_budget_sync_snapshot(collector);
    *outResult = result;
    return ZR_TRUE;
}

TZrBool ZrCore_GarbageCollector_GetBudgetStats(
        SZrGlobalState *global,
        SZrGcBudgetStepResult *outResult) {
    SZrGarbageCollector *collector = gc_budget_collector(global);
    if (collector == ZR_NULL || outResult == ZR_NULL ||
        !collector->budgetConfigured) {
        return ZR_FALSE;
    }
    /* 分配路径独立维护债务，读取时重新取样以反映最新内存压力。 */
    collector->budgetDebtBytes = gc_budget_debt(collector);
    collector->budgetPressure =
            (collector->budget.flags & ZR_GC_BUDGET_FLAG_REPORT_PRESSURE) != 0u &&
            collector->budget.pressureThresholdBytes != 0u &&
            collector->budgetDebtBytes >= 0 &&
            (TZrUInt64)collector->budgetDebtBytes >=
                collector->budget.pressureThresholdBytes;
    gc_budget_sync_snapshot(collector);
    memset(outResult, 0, sizeof(*outResult));
    outResult->status = collector->budgetLastStatus;
    outResult->phase = collector->budgetPhase;
    outResult->pauseReason = collector->budgetPauseReason;
    outResult->nextCursor = collector->budgetCursor;
    outResult->workDone = collector->budgetWorkDone;
    outResult->elapsedUs = collector->budgetElapsedUs;
    outResult->debtBytes = collector->budgetDebtBytes;
    /* TODO: 统计结果复用单步类型，但 maxAtomicPauseUs 返回配置阈值而非实测值，
     * bytesDone/objectsDone 保持零值；需明确这些字段在累计视图中的语义。 */
    outResult->maxAtomicPauseUs = collector->budget.maxAtomicPauseUs;
    outResult->overBudgetCount = collector->budgetOverBudgetCount;
    outResult->compactDeferredCount = collector->budgetCompactDeferredCount;
    outResult->pressure = collector->budgetPressure;
    outResult->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_Gc_SetBudget(SZrState *state, const SZrGcBudget *budget) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_GarbageCollector_SetBudget(state->global, budget);
}

TZrBool ZrCore_Gc_GetStats(SZrState *state, SZrGcBudgetStats *stats) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_GarbageCollector_GetBudgetStats(state->global, stats);
}
