#include "zr_vm_core/gc_young_allocation.h"

#include <string.h>

static void gc_young_set_diagnostic_minor(
        SZrGcYoungDiagnostic *diagnostic,
        EZrGcYoungDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
}

static TZrBool gc_young_minor_phase_is_active(EZrGcMinorPhase phase) {
    return phase == ZR_GC_MINOR_PHASE_EVACUATE ||
           phase == ZR_GC_MINOR_PHASE_REWRITE_REFERENCES ||
           phase == ZR_GC_MINOR_PHASE_VERIFY ||
           phase == ZR_GC_MINOR_PHASE_RESUME;
}

void ZrCore_GcMinorTransaction_Init(
        SZrGcMinorTransaction *transaction) {
    if (transaction != ZR_NULL) {
        memset(transaction, 0, sizeof(*transaction));
        transaction->phase = ZR_GC_MINOR_PHASE_IDLE;
        transaction->consistentBoundary = ZR_TRUE; /* IDLE 的初始状态位，不代表堆或 roots 已完成扫描。 */
    }
}

TZrBool ZrCore_GcMinorTransaction_Begin(
        SZrGcMinorTransaction *transaction,
        TZrUInt32 regionCount,
        TZrUInt64 toSpaceBytes,
        TZrUInt32 workBudget,
        TZrBool mutatorsStopped,
        TZrBool rootsAvailable,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL || regionCount == 0u || toSpaceBytes == 0u ||
        workBudget == 0u) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!mutatorsStopped) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_MUTATORS_NOT_STOPPED,
                                      4u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!rootsAvailable) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_ROOTS_UNAVAILABLE,
                                      5u, 1u, 0u);
        return ZR_FALSE;
    }
    if (transaction->phase != ZR_GC_MINOR_PHASE_IDLE &&
        transaction->phase != ZR_GC_MINOR_PHASE_COMPLETE &&
        transaction->phase != ZR_GC_MINOR_PHASE_ABORTED) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_IDLE,
                                      (TZrUInt64)transaction->phase);
        return ZR_FALSE;
    }
    /* 此状态机只接收调用方提供的停 mutator/root 可用标志，不执行 pause 或 root trace；
     * 事务本体只在所有检查通过后清零。 */
    /* TODO: workBudget 当前只拒绝 0，未限制区域或工作量；明确预算单位及其消费方后再落实。 */
    memset(transaction, 0, sizeof(*transaction));
    transaction->phase = ZR_GC_MINOR_PHASE_EVACUATE;
    transaction->regionCount = regionCount;
    transaction->toSpaceBytes = toSpaceBytes;
    transaction->mutatorsStopped = ZR_TRUE;
    transaction->rootsTraced = ZR_TRUE;
    transaction->consistentBoundary = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_EvacuateRegion(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 liveBytes,
        TZrUInt32 objectCount,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL ||
        transaction->phase != ZR_GC_MINOR_PHASE_EVACUATE ||
        !transaction->mutatorsStopped || !transaction->rootsTraced) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_EVACUATE,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    /* TODO: 确认 regionCount 是否只统计非空 region；现有测试传入的 objectCount 均为 1。 */
    if (transaction->regionCursor >= transaction->regionCount || objectCount == 0u) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_REGION,
                                      1u, (TZrUInt64)transaction->regionCount,
                                      (TZrUInt64)transaction->regionCursor);
        return ZR_FALSE;
    }
    if (liveBytes > transaction->toSpaceBytes - transaction->toSpaceUsedBytes) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_TOSPACE_EXHAUSTED,
                                      1u, transaction->toSpaceBytes -
                                              transaction->toSpaceUsedBytes,
                                      liveBytes);
        /* 本分支只保持 regionCursor 与字节计数不变；调用方须保证该 region 未部分提交，
         * 并保持 mutator 停止，不能绕过失败边界恢复执行。 */
        transaction->consistentBoundary = ZR_FALSE;
        return ZR_FALSE;
    }
    /* 这里只累计调用方报告的 evacuation 结果，不执行对象分配或复制。 */
    transaction->toSpaceUsedBytes += liveBytes;
    transaction->evacuatedBytes += liveBytes;
    /* TODO: 确认 objectCount 是否只统计晋升对象；真实 minor GC 也会把存活对象留在 Survivor。 */
    transaction->promotedObjects += (TZrUInt64)objectCount;
    transaction->regionCursor++;
    transaction->consistentBoundary = ZR_FALSE;
    if (transaction->regionCursor == transaction->regionCount) {
        transaction->phase = ZR_GC_MINOR_PHASE_REWRITE_REFERENCES;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_RewriteReferences(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 rewrittenReferences,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL ||
        transaction->phase != ZR_GC_MINOR_PHASE_REWRITE_REFERENCES ||
        !transaction->mutatorsStopped) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_REWRITE_REFERENCES,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    /* rewrittenReferences 是调用方完成重写后报告的数量；本函数不遍历或改写对象引用。 */
    transaction->rewrittenReferences = rewrittenReferences;
    transaction->phase = ZR_GC_MINOR_PHASE_VERIFY;
    transaction->consistentBoundary = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_VerifyForwarding(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 unresolvedForwarding,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL ||
        transaction->phase != ZR_GC_MINOR_PHASE_VERIFY ||
        !transaction->mutatorsStopped) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, ZR_GC_MINOR_PHASE_VERIFY,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    /* 未解转发数由调用方的对象扫描提供；这里只据此决定能否进入恢复边界。 */
    transaction->unresolvedForwarding = unresolvedForwarding;
    if (unresolvedForwarding != 0u) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_UNRESOLVED_FORWARDING,
                                      1u, 0u, unresolvedForwarding);
        transaction->consistentBoundary = ZR_FALSE;
        return ZR_FALSE;
    }
    transaction->phase = ZR_GC_MINOR_PHASE_RESUME;
    transaction->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_CanResumeMutators(
        const SZrGcMinorTransaction *transaction) {
    return (TZrBool)(transaction != ZR_NULL &&
                     transaction->phase == ZR_GC_MINOR_PHASE_RESUME &&
                     transaction->mutatorsStopped && transaction->rootsTraced &&
                     transaction->unresolvedForwarding == 0u &&
                     transaction->consistentBoundary);
}

TZrBool ZrCore_GcMinorTransaction_ResumeMutators(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_GcMinorTransaction_CanResumeMutators(transaction)) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
                                      0u, ZR_GC_MINOR_PHASE_RESUME,
                                      (TZrUInt64)transaction->phase);
        return ZR_FALSE;
    }
    /* 这里只推进事务记录；运行时调度器仍负责真正恢复 mutator。 */
    transaction->mutatorsStopped = ZR_FALSE;
    transaction->phase = ZR_GC_MINOR_PHASE_COMPLETE;
    transaction->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_GcMinorTransaction_Abort(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (transaction == ZR_NULL || !gc_young_minor_phase_is_active(transaction->phase)) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      transaction == ZR_NULL
                                          ? ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT
                                          : ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
                                      0u, 1u,
                                      transaction != ZR_NULL
                                          ? (TZrUInt64)transaction->phase
                                          : 0u);
        return ZR_FALSE;
    }
    /* TODO: Abort 只标记 ABORTED，不回滚对象或根；确认并约束调用方保持 mutator 停止，
     * 直到自行恢复 roots 或从头重启事务。 */
    transaction->phase = ZR_GC_MINOR_PHASE_ABORTED;
    transaction->consistentBoundary = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrCore_Gc_RunMinorTransaction(
        struct SZrState *state,
        const SZrGcMinorRequest *request,
        SZrGcMinorResult *result,
        SZrGcYoungDiagnostic *diagnostic) {
    SZrGcMinorTransaction transaction;

    ZR_UNUSED_PARAMETER(state);
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (request == ZR_NULL || result == ZR_NULL) {
        gc_young_set_diagnostic_minor(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    memset(result, 0, sizeof(*result));
    ZrCore_GcMinorTransaction_Init(&transaction);
    if (!ZrCore_GcMinorTransaction_Begin(
                &transaction,
                request->regionCount,
                request->toSpaceBytes,
                request->workBudget,
                request->mutatorsStopped,
                request->rootsAvailable,
                diagnostic)) {
        return ZR_FALSE;
    }
    /* 该标量 façade 只校验请求并记录入口边界：不读取 state、不扫描 object/card，
     * 也不驱动 evacuation、reference rewrite 或 mutator resume。成功只表示入口条件有效；
     * 结果仍是 EVACUATE 起点，实际 minor GC 须由运行时自己的调用链完成。 */
    result->phase = transaction.phase;
    result->regionCursor = transaction.regionCursor;
    result->consistentBoundary = transaction.consistentBoundary;
    result->mutatorsResumed = ZR_FALSE;
    return ZR_TRUE;
}
