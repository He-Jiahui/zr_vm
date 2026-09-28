#include "zr_vm_core/gc_major.h"

#include <limits.h>
#include <string.h>

/* 诊断是可选输出，校验与阶段入口均可在不提供诊断缓冲的情况下拒绝调用。 */
static void gc_major_diag(SZrGcMajorDiagnostic *diagnostic,
                          EZrGcMajorDiagnosticCode code,
                          TZrUInt32 field,
                          TZrUInt64 expected,
                          TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->field = field;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

/* 遥测计数允许饱和；不同于 cursor，累计值不得因溢出回绕。 */
static TZrUInt64 gc_major_sat_add(TZrUInt64 left, TZrUInt64 right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

/* Begin 拒绝覆盖活动 cycle，Cancel 只接受这些尚未结束的阶段。 */
static TZrBool gc_major_phase_is_active(EZrGcMajorPhase phase) {
    return phase >= ZR_GC_MAJOR_PHASE_INITIAL_SNAPSHOT &&
           phase <= ZR_GC_MAJOR_PHASE_COMPACT;
}

/* 同一切片必须归入当前 major 阶段对应的预算桶，不能跨阶段记账。 */
static TZrBool gc_major_budget_phase_matches(EZrGcMajorPhase phase,
                                              EZrGcBudgetPhase budgetPhase) {
    switch (phase) {
        case ZR_GC_MAJOR_PHASE_INITIAL_SNAPSHOT:
            return budgetPhase == ZR_GC_BUDGET_PHASE_INITIAL_SNAPSHOT;
        case ZR_GC_MAJOR_PHASE_CONCURRENT_MARK:
            return budgetPhase == ZR_GC_BUDGET_PHASE_MARK_CONCURRENT;
        case ZR_GC_MAJOR_PHASE_REMARK:
            return budgetPhase == ZR_GC_BUDGET_PHASE_REMARK;
        case ZR_GC_MAJOR_PHASE_SWEEP:
            return budgetPhase == ZR_GC_BUDGET_PHASE_SWEEP;
        case ZR_GC_MAJOR_PHASE_COMPACT:
            return budgetPhase == ZR_GC_BUDGET_PHASE_COMPACT;
        default:
            return ZR_FALSE;
    }
}

void ZrCore_GcMajor_DiagnosticClear(SZrGcMajorDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

const TZrChar *ZrCore_GcMajor_DiagnosticName(
        EZrGcMajorDiagnosticCode code) {
    switch (code) {
        case ZR_GC_MAJOR_DIAGNOSTIC_NONE:
            return "none";
        case ZR_GC_MAJOR_DIAGNOSTIC_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_GC_MAJOR_DIAGNOSTIC_INVALID_MAGIC:
            return "invalid-magic";
        case ZR_GC_MAJOR_DIAGNOSTIC_INVALID_SCHEMA:
            return "invalid-schema";
        case ZR_GC_MAJOR_DIAGNOSTIC_UNKNOWN_FLAGS:
            return "unknown-flags";
        case ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE:
            return "invalid-phase";
        case ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION:
            return "invalid-transition";
        case ZR_GC_MAJOR_DIAGNOSTIC_OVERFLOW:
            return "overflow";
        case ZR_GC_MAJOR_DIAGNOSTIC_INCONSISTENT_BOUNDARY:
            return "inconsistent-boundary";
        case ZR_GC_MAJOR_DIAGNOSTIC_BUDGET_REJECTED:
            return "budget-rejected";
        default:
            return "unknown";
    }
}

void ZrCore_GcMajor_Init(SZrGcMajorState *state) {
    if (state == ZR_NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->magic = ZR_GC_MAJOR_CONTRACT_MAGIC;
    state->schemaVersion = ZR_GC_MAJOR_CONTRACT_SCHEMA_VERSION;
    state->phase = ZR_GC_MAJOR_PHASE_IDLE;
    state->consistentBoundary = ZR_TRUE;
}

/* 快照作为不持有指针的外部见证，恢复或阶段转换前先拒绝不一致边界。 */
TZrBool ZrCore_GcMajor_Validate(
        const SZrGcMajorState *state,
        SZrGcMajorDiagnostic *diagnostic) {
    ZrCore_GcMajor_DiagnosticClear(diagnostic);
    if (state == ZR_NULL) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_ARGUMENT,
                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (state->magic != ZR_GC_MAJOR_CONTRACT_MAGIC) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_MAGIC,
                      1u, ZR_GC_MAJOR_CONTRACT_MAGIC, state->magic);
        return ZR_FALSE;
    }
    if (state->schemaVersion != ZR_GC_MAJOR_CONTRACT_SCHEMA_VERSION) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_SCHEMA,
                      2u, ZR_GC_MAJOR_CONTRACT_SCHEMA_VERSION,
                      state->schemaVersion);
        return ZR_FALSE;
    }
    if ((state->flags & ~ZR_GC_MAJOR_FLAG_KNOWN_MASK) != 0u ||
        state->reserved != 0u) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_UNKNOWN_FLAGS,
                      3u, ZR_GC_MAJOR_FLAG_KNOWN_MASK, state->flags);
        return ZR_FALSE;
    }
    /* BUG: 有符号枚举实现下，负 phase 可作为外部快照通过校验，Begin/Cancel 还会接受它。 */
    if (state->phase >= ZR_GC_MAJOR_PHASE_COUNT) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE,
                      4u, ZR_GC_MAJOR_PHASE_IDLE, state->phase);
        return ZR_FALSE;
    }
    if (!state->consistentBoundary) {
        gc_major_diag(diagnostic,
                      ZR_GC_MAJOR_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
                      5u, 1u, 0u);
        return ZR_FALSE;
    }
    if (state->phase != ZR_GC_MAJOR_PHASE_IDLE && state->cycleId == 0u) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE,
                      6u, 1u, state->cycleId);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 先验证并在局部候选中重置累计值，确保失败不破坏上一轮完成记录。 */
TZrBool ZrCore_GcMajor_Begin(
        SZrGcMajorState *state,
        TZrUInt64 cycleId,
        TZrUInt32 flags,
        SZrGcMajorDiagnostic *diagnostic) {
    SZrGcMajorState candidate;

    ZrCore_GcMajor_DiagnosticClear(diagnostic);
    if (state == ZR_NULL || cycleId == 0u) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_ARGUMENT,
                      0u, 1u, cycleId);
        return ZR_FALSE;
    }
    if ((flags & ~ZR_GC_MAJOR_FLAG_KNOWN_MASK) != 0u) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_UNKNOWN_FLAGS,
                      3u, ZR_GC_MAJOR_FLAG_KNOWN_MASK, flags);
        return ZR_FALSE;
    }
    if (!ZrCore_GcMajor_Validate(state, diagnostic)) {
        return ZR_FALSE;
    }
    if (gc_major_phase_is_active(state->phase)) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION,
                      4u, ZR_GC_MAJOR_PHASE_IDLE, state->phase);
        return ZR_FALSE;
    }
    candidate = *state;
    candidate.flags = flags;
    candidate.phase = ZR_GC_MAJOR_PHASE_INITIAL_SNAPSHOT;
    candidate.cycleId = cycleId;
    candidate.cursor = 0u;
    candidate.workDone = 0u;
    candidate.elapsedUs = 0u;
    candidate.bytesDone = 0u;
    candidate.objectsDone = 0u;
    candidate.debtBytes = 0;
    candidate.maxAtomicPauseUs = 0u;
    candidate.overBudgetCount = 0u;
    candidate.compactDeferredCount = 0u;
    candidate.consistentBoundary = ZR_TRUE;
    *state = candidate;
    return ZR_TRUE;
}

/* 严格顺序维护快照的可恢复性；COMPLETE/CANCELLED 可重复发布但不能继续工作。 */
TZrBool ZrCore_GcMajor_Advance(
        SZrGcMajorState *state,
        EZrGcMajorPhase nextPhase,
        TZrBool consistentBoundary,
        SZrGcMajorDiagnostic *diagnostic) {
    EZrGcMajorPhase current;
    TZrBool allowed = ZR_FALSE;

    ZrCore_GcMajor_DiagnosticClear(diagnostic);
    /* BUG: 负 nextPhase 在有符号枚举实现下未按 INVALID_PHASE 分类。 */
    if (!ZrCore_GcMajor_Validate(state, diagnostic) ||
        nextPhase >= ZR_GC_MAJOR_PHASE_COUNT) {
        if (diagnostic != ZR_NULL &&
            diagnostic->code == ZR_GC_MAJOR_DIAGNOSTIC_NONE) {
            gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE,
                          4u, ZR_GC_MAJOR_PHASE_IDLE, nextPhase);
        }
        return ZR_FALSE;
    }
    if (!consistentBoundary) {
        gc_major_diag(diagnostic,
                      ZR_GC_MAJOR_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
                      5u, 1u, 0u);
        return ZR_FALSE;
    }
    current = state->phase;
    if (current == ZR_GC_MAJOR_PHASE_INITIAL_SNAPSHOT &&
               nextPhase == ZR_GC_MAJOR_PHASE_CONCURRENT_MARK) {
        allowed = ZR_TRUE;
    } else if (current == ZR_GC_MAJOR_PHASE_CONCURRENT_MARK &&
               nextPhase == ZR_GC_MAJOR_PHASE_REMARK) {
        allowed = ZR_TRUE;
    } else if (current == ZR_GC_MAJOR_PHASE_REMARK &&
               nextPhase == ZR_GC_MAJOR_PHASE_SWEEP) {
        allowed = ZR_TRUE;
    } else if (current == ZR_GC_MAJOR_PHASE_SWEEP &&
               (nextPhase == ZR_GC_MAJOR_PHASE_COMPACT ||
                nextPhase == ZR_GC_MAJOR_PHASE_COMPLETE)) {
        allowed = ZR_TRUE;
    } else if (current == ZR_GC_MAJOR_PHASE_COMPACT &&
               nextPhase == ZR_GC_MAJOR_PHASE_COMPLETE) {
        allowed = ZR_TRUE;
    } else if (gc_major_phase_is_active(current) &&
               nextPhase == ZR_GC_MAJOR_PHASE_CANCELLED) {
        allowed = ZR_TRUE;
    } else if ((current == ZR_GC_MAJOR_PHASE_COMPLETE ||
                current == ZR_GC_MAJOR_PHASE_CANCELLED) &&
               nextPhase == current) {
        allowed = ZR_TRUE;
    }
    if (!allowed) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION,
                      4u, current, nextPhase);
        return ZR_FALSE;
    }
    state->phase = nextPhase;
    state->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

/* 宿主先执行并测量工作片，这里只评估预算并记录一致边界的标量结果。 */
TZrBool ZrCore_GcMajor_Step(
        SZrGcMajorState *state,
        const SZrGcBudget *budget,
        EZrGcBudgetPhase budgetPhase,
        TZrUInt64 workUnits,
        TZrUInt64 elapsedUs,
        TZrUInt64 bytes,
        TZrUInt64 objects,
        TZrInt64 debtBytes,
        TZrUInt64 atomicPauseUs,
        SZrGcBudgetStepResult *outResult,
        SZrGcMajorDiagnostic *diagnostic) {
    SZrGcBudgetStepResult result;
    SZrGcBudgetDiagnostic budgetDiagnostic;
    TZrBool advancesCursor;

    ZrCore_GcMajor_DiagnosticClear(diagnostic);
    if (outResult != ZR_NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }
    if (state == ZR_NULL || budget == ZR_NULL || outResult == ZR_NULL ||
        !ZrCore_GcMajor_Validate(state, diagnostic) ||
        !gc_major_phase_is_active(state->phase) ||
        !gc_major_budget_phase_matches(state->phase, budgetPhase)) {
        if (diagnostic != ZR_NULL &&
            diagnostic->code == ZR_GC_MAJOR_DIAGNOSTIC_NONE) {
            gc_major_diag(diagnostic,
                          ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION,
                          4u, state != ZR_NULL ? state->phase : 0u,
                          budgetPhase);
        }
        return ZR_FALSE;
    }
    /* BUG: 预算预检失败时 EvaluateStep 可不写 result，下面仍复制未初始化局部值给宿主。 */
    if (!ZrCore_GcBudget_EvaluateStep(
            budget, budgetPhase, state->cursor, workUnits, elapsedUs,
            bytes, objects, debtBytes, atomicPauseUs, &result,
            &budgetDiagnostic)) {
        /* BUG: evaluator 的 cursor OVERFLOW 被统一归为 BUDGET_REJECTED，误导宿主恢复决策。 */
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_BUDGET_REJECTED,
                      budgetDiagnostic.field, budgetDiagnostic.expected,
                      budgetDiagnostic.actual);
        *outResult = result;
        return ZR_FALSE;
    }

    /* 只有实际接纳工作或事后发现原子暂停超限时才推进游标；拒绝/推迟保留原位置。 */
    advancesCursor = result.status == ZR_GC_BUDGET_STEP_ACCEPTED ||
                     result.status == ZR_GC_BUDGET_STEP_OVER_BUDGET;
    if (advancesCursor && result.nextCursor < state->cursor) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_OVERFLOW,
                      7u, state->cursor, result.nextCursor);
        return ZR_FALSE;
    }
    if (advancesCursor) {
        state->cursor = result.nextCursor;
    }
    /* TODO: REJECTED/DEFERRED 保留输入的 bytes/objects/elapsed，重试可能重复累计；需与预算账本统一口径。 */
    state->workDone = gc_major_sat_add(state->workDone, result.workDone);
    state->elapsedUs = gc_major_sat_add(state->elapsedUs, result.elapsedUs);
    state->bytesDone = gc_major_sat_add(state->bytesDone, result.bytesDone);
    state->objectsDone = gc_major_sat_add(state->objectsDone,
                                          result.objectsDone);
    state->debtBytes = result.debtBytes;
    if (state->maxAtomicPauseUs < result.maxAtomicPauseUs) {
        state->maxAtomicPauseUs = result.maxAtomicPauseUs;
    }
    state->overBudgetCount = gc_major_sat_add(
            state->overBudgetCount, result.overBudgetCount);
    state->compactDeferredCount = gc_major_sat_add(
            state->compactDeferredCount, result.compactDeferredCount);
    state->consistentBoundary = result.consistentBoundary;
    *outResult = result;
    return ZR_TRUE;
}

/* 取消不回滚已经确认的切片计数，结束边界可以安全地重复通知。 */
TZrBool ZrCore_GcMajor_Cancel(
        SZrGcMajorState *state,
        SZrGcMajorDiagnostic *diagnostic) {
    ZrCore_GcMajor_DiagnosticClear(diagnostic);
    if (!ZrCore_GcMajor_Validate(state, diagnostic)) {
        return ZR_FALSE;
    }
    if (state->phase == ZR_GC_MAJOR_PHASE_CANCELLED) {
        return ZR_TRUE;
    }
    if (state->phase == ZR_GC_MAJOR_PHASE_COMPLETE ||
        state->phase == ZR_GC_MAJOR_PHASE_IDLE) {
        gc_major_diag(diagnostic, ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION,
                      4u, ZR_GC_MAJOR_PHASE_CONCURRENT_MARK,
                      state->phase);
        return ZR_FALSE;
    }
    state->phase = ZR_GC_MAJOR_PHASE_CANCELLED;
    state->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}
