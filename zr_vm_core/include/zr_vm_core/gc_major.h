#ifndef ZR_VM_CORE_GC_MAJOR_H
#define ZR_VM_CORE_GC_MAJOR_H

/*
 * Pointer-free state contract for a resumable major collection.
 *
 * The collector owns queues, locks and object storage.  This record is the
 * scalar witness published at phase boundaries and can therefore be copied
 * into scheduler telemetry or a debugger snapshot without retaining a heap
 * address.  Actual mark/sweep work is performed by the existing collector
 * adapter; this contract makes its ordering and budget decisions checkable.
 */

#include "zr_vm_core/gc_budget_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_GC_MAJOR_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_MAJOR_CONTRACT_MAGIC ((TZrUInt32)0x314a4347u) /* GCJ1 */

#define ZR_GC_MAJOR_FLAG_FORCE_COMPACT ((TZrUInt32)1u << 0u)
#define ZR_GC_MAJOR_FLAG_CONCURRENT_MARK ((TZrUInt32)1u << 1u)
#define ZR_GC_MAJOR_FLAG_KNOWN_MASK \
    (ZR_GC_MAJOR_FLAG_FORCE_COMPACT | ZR_GC_MAJOR_FLAG_CONCURRENT_MARK)

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

typedef struct SZrGcMajorDiagnostic {
    EZrGcMajorDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcMajorDiagnostic;

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

ZR_CORE_API void ZrCore_GcMajor_DiagnosticClear(
        SZrGcMajorDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_GcMajor_DiagnosticName(
        EZrGcMajorDiagnosticCode code);
ZR_CORE_API void ZrCore_GcMajor_Init(SZrGcMajorState *state);
ZR_CORE_API TZrBool ZrCore_GcMajor_Validate(
        const SZrGcMajorState *state,
        SZrGcMajorDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMajor_Begin(
        SZrGcMajorState *state,
        TZrUInt64 cycleId,
        TZrUInt32 flags,
        SZrGcMajorDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMajor_Advance(
        SZrGcMajorState *state,
        EZrGcMajorPhase nextPhase,
        TZrBool consistentBoundary,
        SZrGcMajorDiagnostic *diagnostic);
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
ZR_CORE_API TZrBool ZrCore_GcMajor_Cancel(
        SZrGcMajorState *state,
        SZrGcMajorDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_MAJOR_H */
