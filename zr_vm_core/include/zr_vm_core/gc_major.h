#ifndef ZR_VM_CORE_GC_MAJOR_H
#define ZR_VM_CORE_GC_MAJOR_H

/*
 * 可恢复 major GC 的无指针阶段契约：回收器另行拥有队列、锁和对象存储，
 * 此标量记录只在一致边界交给调度器或调试器，不能代替实际标记、清扫和压缩。
 */

#include "zr_vm_core/gc_budget_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 版本和魔数保护跨阶段保存的标量快照；宿主恢复前须先 Validate。 */
#define ZR_GC_MAJOR_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_MAJOR_CONTRACT_MAGIC ((TZrUInt32)0x314a4347u) /* GCJ1 */

/* 仅接受已定义策略位；TODO: 仓内未见生产调度器消费 FORCE_COMPACT/CONCURRENT_MARK，需核实接入路径。 */
#define ZR_GC_MAJOR_FLAG_FORCE_COMPACT ((TZrUInt32)1u << 0u)
#define ZR_GC_MAJOR_FLAG_CONCURRENT_MARK ((TZrUInt32)1u << 1u)
#define ZR_GC_MAJOR_FLAG_KNOWN_MASK \
    (ZR_GC_MAJOR_FLAG_FORCE_COMPACT | ZR_GC_MAJOR_FLAG_CONCURRENT_MARK)

/** @brief 调度器可发布的 major 阶段；跳阶段与非一致边界不能进入下阶段。 */
typedef enum EZrGcMajorPhase {
    ZR_GC_MAJOR_PHASE_IDLE = 0,
    ZR_GC_MAJOR_PHASE_INITIAL_SNAPSHOT,
    ZR_GC_MAJOR_PHASE_CONCURRENT_MARK,
    ZR_GC_MAJOR_PHASE_REMARK,
    ZR_GC_MAJOR_PHASE_SWEEP,
    ZR_GC_MAJOR_PHASE_COMPACT,
    ZR_GC_MAJOR_PHASE_COMPLETE,
    ZR_GC_MAJOR_PHASE_CANCELLED,
    ZR_GC_MAJOR_PHASE_COUNT
} EZrGcMajorPhase;

/** @brief 调用失败时用于区分损坏快照、非法状态转换和预算拒绝的稳定分类。 */
typedef enum EZrGcMajorDiagnosticCode {
    ZR_GC_MAJOR_DIAGNOSTIC_NONE = 0,
    ZR_GC_MAJOR_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_MAJOR_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_MAJOR_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_MAJOR_DIAGNOSTIC_UNKNOWN_FLAGS,
    ZR_GC_MAJOR_DIAGNOSTIC_INVALID_PHASE,
    ZR_GC_MAJOR_DIAGNOSTIC_INVALID_TRANSITION,
    ZR_GC_MAJOR_DIAGNOSTIC_OVERFLOW,
    ZR_GC_MAJOR_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
    ZR_GC_MAJOR_DIAGNOSTIC_BUDGET_REJECTED,
    ZR_GC_MAJOR_DIAGNOSTIC_COUNT
} EZrGcMajorDiagnosticCode;

/** @brief 可选的拒绝原因输出；field/expected/actual 是标量诊断，不持有对象。 */
typedef struct SZrGcMajorDiagnostic {
    EZrGcMajorDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcMajorDiagnostic;

/** @brief 一致边界上的可复制见证；记录预算评估值，不拥有 GC 资源。 */
typedef struct SZrGcMajorState {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 reserved;
    EZrGcMajorPhase phase;
    TZrUInt64 cycleId;
    TZrUInt64 cursor;
    TZrUInt64 workDone;
    TZrUInt64 elapsedUs;
    TZrUInt64 bytesDone;
    TZrUInt64 objectsDone;
    TZrInt64 debtBytes;
    TZrUInt64 maxAtomicPauseUs;
    TZrUInt64 overBudgetCount;
    TZrUInt64 compactDeferredCount;
    TZrBool consistentBoundary;
} SZrGcMajorState;

/** @brief 清除可选诊断，供各阶段入口在首次校验前复用。 */
ZR_CORE_API void ZrCore_GcMajor_DiagnosticClear(
        SZrGcMajorDiagnostic *diagnostic);
/** @brief 返回诊断展示名；控制流仍应使用枚举值。 */
ZR_CORE_API const TZrChar *ZrCore_GcMajor_DiagnosticName(
        EZrGcMajorDiagnosticCode code);
/** @brief 初始化可验证的空闲快照；Begin 前必须调用。 */
ZR_CORE_API void ZrCore_GcMajor_Init(SZrGcMajorState *state);
/** @brief 核查外部保存的快照及一致边界；失败时不得继续推进阶段。
 * BUG: 有符号枚举实现下，负 phase 未被当前上界检查拒绝。 */
ZR_CORE_API TZrBool ZrCore_GcMajor_Validate(
        const SZrGcMajorState *state,
        SZrGcMajorDiagnostic *diagnostic);
/** @brief 从非活动阶段开启新 cycle 并清空累计值；cycleId 必须非零。 */
ZR_CORE_API TZrBool ZrCore_GcMajor_Begin(
        SZrGcMajorState *state,
        TZrUInt64 cycleId,
        TZrUInt32 flags,
        SZrGcMajorDiagnostic *diagnostic);
/** @brief 只在一致边界按 major 生命周期前进；失败保持旧阶段。
 * BUG: 有符号枚举实现下，负 nextPhase 被报为非法转换而非非法阶段。 */
ZR_CORE_API TZrBool ZrCore_GcMajor_Advance(
        SZrGcMajorState *state,
        EZrGcMajorPhase nextPhase,
        TZrBool consistentBoundary,
        SZrGcMajorDiagnostic *diagnostic);
/** @brief 将宿主测得的切片交预算契约评估，再发布标量见证。
 * @return 布尔成功只表示评估完成；是否接纳或推迟应读取 outResult.status。
 * BUG: 非法 budget 使 EvaluateStep 在写局部 result 前失败，当前失败分支仍复制该未初始化结果。
 * BUG: evaluator 报告的 cursor 溢出被折叠为普通预算拒绝，宿主拿不到 OVERFLOW 分类。 */
ZR_CORE_API TZrBool ZrCore_GcMajor_Step(
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
        SZrGcMajorDiagnostic *diagnostic);
/** @brief 取消活动 cycle 并发布一致边界；已取消调用可幂等复用。 */
ZR_CORE_API TZrBool ZrCore_GcMajor_Cancel(
        SZrGcMajorState *state,
        SZrGcMajorDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_MAJOR_H */
