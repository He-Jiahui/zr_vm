#ifndef ZR_VM_CORE_GC_COMPACT_H
#define ZR_VM_CORE_GC_COMPACT_H

/* Selective-compaction admission is deliberately separate from relocation.
 * The planner consumes region facts, skips pinned/non-moving regions, and
 * reports a byte-bounded plan.  No object address or forwarding pointer is
 * retained in the result. */

#include "zr_vm_core/gc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_COMPACT_CONTRACT_MAGIC ((TZrUInt32)0x31434347u) /* GCC1 */

typedef enum EZrGcCompactMode {
    ZR_GC_COMPACT_MODE_NON_MOVING = 0,
    ZR_GC_COMPACT_MODE_SELECTIVE_MOVING,
    ZR_GC_COMPACT_MODE_COUNT
} EZrGcCompactMode;

typedef enum EZrGcCompactDiagnosticCode {
    ZR_GC_COMPACT_DIAGNOSTIC_NONE = 0,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION,
    ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW,
    ZR_GC_COMPACT_DIAGNOSTIC_BUDGET,
    ZR_GC_COMPACT_DIAGNOSTIC_COUNT
} EZrGcCompactDiagnosticCode;

typedef struct SZrGcCompactDiagnostic {
    EZrGcCompactDiagnosticCode code;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcCompactDiagnostic;

typedef struct SZrGcCompactRequest {
    const SZrGarbageCollectRegionDescriptor *regions;
    TZrSize regionCount;
    TZrUInt64 budgetBytes;
    TZrUInt32 fragmentationThresholdPercent;
    TZrBool allowMoving;
} SZrGcCompactRequest;

typedef struct SZrGcCompactPlan {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrGcCompactMode mode;
    TZrBool eligible;
    TZrBool deferred;
    TZrUInt32 regionCount;
    TZrUInt32 candidateRegionCount;
    TZrUInt32 pinnedRegionCount;
    TZrUInt64 candidateBytes;
    TZrUInt64 movableBytes;
    TZrUInt64 plannedBytes;
    TZrUInt64 pinnedBytes;
    TZrUInt64 estimatedWorkUnits;
} SZrGcCompactPlan;

ZR_CORE_API void ZrCore_GcCompact_DiagnosticClear(
        SZrGcCompactDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_GcCompact_DiagnosticName(
        EZrGcCompactDiagnosticCode code);
ZR_CORE_API void ZrCore_GcCompact_Init(SZrGcCompactPlan *plan);
ZR_CORE_API TZrBool ZrCore_GcCompact_Validate(
        const SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcCompact_Plan(
        const SZrGcCompactRequest *request,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GarbageCollector_PlanCompaction(
        struct SZrGlobalState *global,
        TZrUInt64 budgetBytes,
        TZrUInt32 fragmentationThresholdPercent,
        TZrBool allowMoving,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_COMPACT_H */
