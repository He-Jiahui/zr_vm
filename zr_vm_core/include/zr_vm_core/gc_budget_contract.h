#ifndef ZR_VM_CORE_GC_BUDGET_CONTRACT_H
#define ZR_VM_CORE_GC_BUDGET_CONTRACT_H

/*
 * Pointer-free scheduling witness for a sliced GC step.
 *
 * This contract does not run a collector and does not own GC state.  It lets a
 * collector or host scheduler validate one bounded unit and persist its
 * scalar cursor/debt/telemetry at a coherent boundary.  A compact phase with
 * no compact budget is explicitly deferred; it is never silently converted
 * into an unbounded relocation.
 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_BUDGET_CONTRACT_MAGIC ((TZrUInt32)0x31474243u) /* CBG1 */

#define ZR_GC_BUDGET_FLAG_ALLOW_COMPACT ((TZrUInt32)1u << 0u)
#define ZR_GC_BUDGET_FLAG_REPORT_PRESSURE ((TZrUInt32)1u << 1u)
#define ZR_GC_BUDGET_FLAG_KNOWN_MASK \
    (ZR_GC_BUDGET_FLAG_ALLOW_COMPACT | ZR_GC_BUDGET_FLAG_REPORT_PRESSURE)

typedef enum EZrGcBudgetDiagnosticCode {
    ZR_GC_BUDGET_DIAGNOSTIC_NONE = 0,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_BUDGET_DIAGNOSTIC_UNKNOWN_FLAGS,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_PHASE,
    ZR_GC_BUDGET_DIAGNOSTIC_OVERFLOW,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
    ZR_GC_BUDGET_DIAGNOSTIC_COUNT
} EZrGcBudgetDiagnosticCode;

typedef struct SZrGcBudgetDiagnostic {
    EZrGcBudgetDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcBudgetDiagnostic;

typedef enum EZrGcBudgetPhase {
    ZR_GC_BUDGET_PHASE_IDLE = 0,
    ZR_GC_BUDGET_PHASE_INITIAL_SNAPSHOT,
    ZR_GC_BUDGET_PHASE_MARK_CONCURRENT,
    ZR_GC_BUDGET_PHASE_REMARK,
    ZR_GC_BUDGET_PHASE_SWEEP,
    ZR_GC_BUDGET_PHASE_COMPACT,
    ZR_GC_BUDGET_PHASE_COMPLETE,
    ZR_GC_BUDGET_PHASE_COUNT
} EZrGcBudgetPhase;

typedef enum EZrGcBudgetPauseReason {
    ZR_GC_BUDGET_PAUSE_NONE = 0,
    ZR_GC_BUDGET_PAUSE_BUDGET,
    ZR_GC_BUDGET_PAUSE_SAFEPOINT,
    ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION,
    ZR_GC_BUDGET_PAUSE_PRESSURE,
    ZR_GC_BUDGET_PAUSE_OOM,
    ZR_GC_BUDGET_PAUSE_CANCELLED,
    ZR_GC_BUDGET_PAUSE_COMPLETE,
    ZR_GC_BUDGET_PAUSE_COUNT
} EZrGcBudgetPauseReason;

typedef enum EZrGcBudgetStepStatus {
    ZR_GC_BUDGET_STEP_ACCEPTED = 0,
    ZR_GC_BUDGET_STEP_REJECTED,
    ZR_GC_BUDGET_STEP_DEFERRED,
    ZR_GC_BUDGET_STEP_OVER_BUDGET
} EZrGcBudgetStepStatus;

/* Zero means that dimension is not independently limiting. */
typedef struct SZrGcBudget {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 reserved;
    TZrUInt64 maxElapsedUs;
    TZrUInt64 maxWorkUnits;
    TZrUInt64 maxBytes;
    TZrUInt64 maxObjects;
    TZrUInt64 compactBudgetBytes;
    TZrUInt64 pressureThresholdBytes;
    TZrUInt64 maxAtomicPauseUs;
} SZrGcBudget;

/* Scalar output suitable for a phase/cursor record or telemetry snapshot. */
typedef struct SZrGcBudgetStepResult {
    EZrGcBudgetStepStatus status;
    EZrGcBudgetPhase phase;
    EZrGcBudgetPauseReason pauseReason;
    TZrUInt64 nextCursor;
    TZrUInt64 workDone;
    TZrUInt64 elapsedUs;
    TZrUInt64 bytesDone;
    TZrUInt64 objectsDone;
    TZrInt64 debtBytes;
    TZrUInt64 maxAtomicPauseUs;
    TZrUInt64 overBudgetCount;
    TZrUInt64 compactDeferredCount;
    TZrBool pressure;
    TZrBool consistentBoundary;
} SZrGcBudgetStepResult;

ZR_CORE_API void ZrCore_GcBudget_DiagnosticClear(
        SZrGcBudgetDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_GcBudget_DiagnosticName(
        EZrGcBudgetDiagnosticCode code);
ZR_CORE_API void ZrCore_GcBudget_Init(SZrGcBudget *budget);
ZR_CORE_API TZrBool ZrCore_GcBudget_Validate(
        const SZrGcBudget *budget,
        SZrGcBudgetDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcBudget_EvaluateStep(
        const SZrGcBudget *budget,
        EZrGcBudgetPhase phase,
        TZrUInt64 cursor,
        TZrUInt64 workUnits,
        TZrUInt64 elapsedUs,
        TZrUInt64 bytes,
        TZrUInt64 objects,
        TZrInt64 debtBytes,
        TZrUInt64 atomicPauseUs,
        SZrGcBudgetStepResult *result,
        SZrGcBudgetDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_BUDGET_CONTRACT_H */
