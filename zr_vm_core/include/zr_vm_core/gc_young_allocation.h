#ifndef ZR_VM_CORE_GC_YOUNG_ALLOCATION_H
#define ZR_VM_CORE_GC_YOUNG_ALLOCATION_H

/*
 * Explicit young-generation allocation contracts.
 *
 * The existing collector owns the managed object graph and its region
 * descriptors.  This header describes the small, scalar state machines that
 * feed that collector: a worker-local TLAB, an old-to-young card table and a
 * remembered-root scan, plus the non-interruptible boundaries of a minor
 * evacuation.  None of the records below stores a managed object pointer in a
 * persisted result.  Callers publish a result only at a consistent boundary.
 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_GC_YOUNG_ALLOCATION_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_YOUNG_ALLOCATION_MAGIC ((TZrUInt32)0x31474159u) /* YAG1 */
#define ZR_GC_CARD_SHIFT ((TZrUInt32)9u)
#define ZR_GC_CARD_BYTES ((TZrSize)1u << ZR_GC_CARD_SHIFT)
#define ZR_GC_CARD_CLEAN ((TZrByte)0u)
#define ZR_GC_CARD_DIRTY ((TZrByte)1u)
#define ZR_GC_YOUNG_DEFAULT_LARGE_OBJECT_THRESHOLD \
    ((TZrSize)256u * (TZrSize)1024u)

typedef enum EZrGcYoungDiagnosticCode {
    ZR_GC_YOUNG_DIAGNOSTIC_NONE = 0,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ALIGNMENT,
    ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
    ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
    ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED,
    ZR_GC_YOUNG_DIAGNOSTIC_SAFEPOINT_REQUIRED,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_REGION,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_GENERATION,
    ZR_GC_YOUNG_DIAGNOSTIC_TOSPACE_EXHAUSTED,
    ZR_GC_YOUNG_DIAGNOSTIC_MUTATORS_NOT_STOPPED,
    ZR_GC_YOUNG_DIAGNOSTIC_ROOTS_UNAVAILABLE,
    ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE,
    ZR_GC_YOUNG_DIAGNOSTIC_UNRESOLVED_FORWARDING,
    ZR_GC_YOUNG_DIAGNOSTIC_INCONSISTENT_BOUNDARY,
    ZR_GC_YOUNG_DIAGNOSTIC_END_OF_SCAN,
    ZR_GC_YOUNG_DIAGNOSTIC_ALREADY_COMPLETE,
    ZR_GC_YOUNG_DIAGNOSTIC_COUNT
} EZrGcYoungDiagnosticCode;

typedef struct SZrGcYoungDiagnostic {
    EZrGcYoungDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcYoungDiagnostic;

/* A worker-local bump cursor.  The pointer fields are process-local and must
 * never be copied into an artifact or another domain. */
typedef struct SZrGcTlab {
    TZrByte *begin;
    TZrByte *cursor;
    TZrByte *limit;
    TZrUInt32 regionId;
    TZrSize capacity;
    TZrSize wasteLimit;
    TZrSize retiredBytes;
    TZrSize wasteBytes;
    TZrSize allocationCount;
    TZrSize refillCount;
} SZrGcTlab;

ZR_CORE_API void ZrCore_GcYoung_DiagnosticClear(
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_GcYoung_DiagnosticName(
        EZrGcYoungDiagnosticCode code);

ZR_CORE_API TZrBool ZrCore_GcTlab_Init(
        SZrGcTlab *tlab,
        TZrByte *storage,
        TZrSize capacity,
        TZrUInt32 regionId,
        TZrSize wasteLimit);
ZR_CORE_API TZrBool ZrCore_GcTlab_Validate(
        const SZrGcTlab *tlab,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrPtr ZrCore_GcTlab_AllocateFast(
        SZrGcTlab *tlab,
        TZrSize size,
        TZrSize alignment,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcTlab_Retire(
        SZrGcTlab *tlab,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcTlab_Refill(
        SZrGcTlab *tlab,
        TZrByte *storage,
        TZrSize capacity,
        TZrUInt32 regionId,
        TZrSize wasteLimit,
        TZrBool atSafepoint,
        SZrGcYoungDiagnostic *diagnostic);

typedef enum EZrGcYoungAllocationTarget {
    ZR_GC_YOUNG_ALLOCATION_INVALID = 0,
    ZR_GC_YOUNG_ALLOCATION_TLAB,
    ZR_GC_YOUNG_ALLOCATION_YOUNG_REGION,
    ZR_GC_YOUNG_ALLOCATION_OLD,
    ZR_GC_YOUNG_ALLOCATION_PINNED,
    ZR_GC_YOUNG_ALLOCATION_LARGE
} EZrGcYoungAllocationTarget;

typedef struct SZrGcYoungAllocationRequest {
    TZrSize objectBytes;
    TZrSize tlabAvailableBytes;
    TZrSize regionAvailableBytes;
    TZrSize largeObjectThreshold;
    TZrBool nativeVisible;
    TZrBool pinned;
    TZrBool atSafepoint;
} SZrGcYoungAllocationRequest;

typedef struct SZrGcYoungAllocationDecision {
    EZrGcYoungAllocationTarget target;
    EZrGarbageCollectRegionKind regionKind;
    EZrGarbageCollectStorageKind storageKind;
    EZrGarbageCollectPromotionReason reason;
    TZrSize alignedBytes;
    TZrBool requiresSafepoint;
    TZrBool usesYoungGeneration;
} SZrGcYoungAllocationDecision;

ZR_CORE_API void ZrCore_GcYoungAllocationRequest_Init(
        SZrGcYoungAllocationRequest *request);
ZR_CORE_API TZrBool ZrCore_GcYoung_SelectAllocation(
        const SZrGcYoungAllocationRequest *request,
        SZrGcYoungAllocationDecision *decision,
        SZrGcYoungDiagnostic *diagnostic);

typedef struct SZrGcPromotionRequest {
    TZrSize objectBytes;
    TZrSize largeObjectThreshold;
    TZrUInt32 survivalAge;
    TZrUInt32 survivorAgeThreshold;
    TZrUInt32 escapeFlags;
    TZrUInt32 pinFlags;
} SZrGcPromotionRequest;

typedef enum EZrGcYoungPromotionTarget {
    ZR_GC_YOUNG_PROMOTION_INVALID = 0,
    ZR_GC_YOUNG_PROMOTION_SURVIVOR,
    ZR_GC_YOUNG_PROMOTION_OLD,
    ZR_GC_YOUNG_PROMOTION_PINNED,
    ZR_GC_YOUNG_PROMOTION_LARGE
} EZrGcYoungPromotionTarget;

typedef struct SZrGcPromotionDecision {
    EZrGcYoungPromotionTarget target;
    EZrGarbageCollectRegionKind regionKind;
    EZrGarbageCollectStorageKind storageKind;
    EZrGarbageCollectPromotionReason reason;
    TZrUInt32 nextSurvivalAge;
} SZrGcPromotionDecision;

ZR_CORE_API void ZrCore_GcPromotionRequest_Init(
        SZrGcPromotionRequest *request);
ZR_CORE_API TZrBool ZrCore_GcYoung_DecidePromotion(
        const SZrGcPromotionRequest *request,
        SZrGcPromotionDecision *decision,
        SZrGcYoungDiagnostic *diagnostic);

/* Card table and remembered roots.  A card token is a card index; an object
 * token is a caller-owned stable identity, never a process address in a
 * persisted record. */
typedef struct SZrGcCardTable {
    TZrByte *cards;
    TZrSize cardCount;
    TZrByte *heapBegin;
    TZrSize heapBytes;
    TZrSize dirtyCount;
} SZrGcCardTable;

typedef enum EZrGcRememberedRootKind {
    ZR_GC_REMEMBERED_ROOT_CARD = 0,
    ZR_GC_REMEMBERED_ROOT_OBJECT = 1
} EZrGcRememberedRootKind;

typedef struct SZrGcRememberedRoot {
    EZrGcRememberedRootKind kind;
    TZrUInt64 token;
} SZrGcRememberedRoot;

typedef struct SZrGcRememberedRootSet {
    SZrGcRememberedRoot *entries;
    TZrSize capacity;
    TZrSize count;
    TZrUInt32 epoch;
} SZrGcRememberedRootSet;

ZR_CORE_API TZrBool ZrCore_GcCardTable_Init(
        SZrGcCardTable *table,
        TZrByte *cards,
        TZrSize cardCount,
        TZrByte *heapBegin,
        TZrSize heapBytes);
ZR_CORE_API TZrBool ZrCore_GcCardTable_Validate(
        const SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcCardTable_RecordStore(
        SZrGcCardTable *table,
        TZrByte *address,
        TZrSize size,
        EZrGarbageCollectHeapGenerationKind ownerGeneration,
        EZrGarbageCollectHeapGenerationKind valueGeneration,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcCardTable_IsDirty(
        const SZrGcCardTable *table,
        TZrSize cardIndex);
ZR_CORE_API TZrSize ZrCore_GcCardTable_DirtyCardCount(
        const SZrGcCardTable *table);
ZR_CORE_API TZrBool ZrCore_GcCardTable_Clear(
        SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic);

ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_Init(
        SZrGcRememberedRootSet *set,
        SZrGcRememberedRoot *entries,
        TZrSize capacity,
        TZrUInt32 epoch);
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_RecordCard(
        SZrGcRememberedRootSet *set,
        TZrSize cardIndex,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_RecordObject(
        SZrGcRememberedRootSet *set,
        TZrUInt64 objectToken,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_ScanNext(
        const SZrGcRememberedRootSet *set,
        TZrSize *cursor,
        SZrGcRememberedRoot *root,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcRememberedRootSet_Clear(
        SZrGcRememberedRootSet *set,
        SZrGcYoungDiagnostic *diagnostic);

typedef enum EZrGcMinorPhase {
    ZR_GC_MINOR_PHASE_IDLE = 0,
    ZR_GC_MINOR_PHASE_EVACUATE,
    ZR_GC_MINOR_PHASE_REWRITE_REFERENCES,
    ZR_GC_MINOR_PHASE_VERIFY,
    ZR_GC_MINOR_PHASE_RESUME,
    ZR_GC_MINOR_PHASE_COMPLETE,
    ZR_GC_MINOR_PHASE_ABORTED,
    ZR_GC_MINOR_PHASE_COUNT
} EZrGcMinorPhase;

typedef struct SZrGcMinorTransaction {
    EZrGcMinorPhase phase;
    TZrUInt32 regionCount;
    TZrUInt32 regionCursor;
    TZrUInt64 toSpaceBytes;
    TZrUInt64 toSpaceUsedBytes;
    TZrUInt64 evacuatedBytes;
    TZrUInt64 rewrittenReferences;
    TZrUInt64 promotedObjects;
    TZrUInt64 unresolvedForwarding;
    TZrBool mutatorsStopped;
    TZrBool rootsTraced;
    TZrBool consistentBoundary;
} SZrGcMinorTransaction;

ZR_CORE_API void ZrCore_GcMinorTransaction_Init(
        SZrGcMinorTransaction *transaction);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_Begin(
        SZrGcMinorTransaction *transaction,
        TZrUInt32 regionCount,
        TZrUInt64 toSpaceBytes,
        TZrUInt32 workBudget,
        TZrBool mutatorsStopped,
        TZrBool rootsAvailable,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_EvacuateRegion(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 liveBytes,
        TZrUInt32 objectCount,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_RewriteReferences(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 rewrittenReferences,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_VerifyForwarding(
        SZrGcMinorTransaction *transaction,
        TZrUInt64 unresolvedForwarding,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_CanResumeMutators(
        const SZrGcMinorTransaction *transaction);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_ResumeMutators(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcMinorTransaction_Abort(
        SZrGcMinorTransaction *transaction,
        SZrGcYoungDiagnostic *diagnostic);

/* Scalar request/result façade matching the plan's runtime entry-point shape.
 * `state` is intentionally opaque here; a collector may use the result as a
 * proof record before invoking its own stop-the-world machinery. */
struct SZrState;
typedef struct SZrGcMinorRequest {
    TZrUInt32 regionCount;
    TZrUInt64 toSpaceBytes;
    TZrUInt32 workBudget;
    TZrBool mutatorsStopped;
    TZrBool rootsAvailable;
} SZrGcMinorRequest;

typedef struct SZrGcMinorResult {
    EZrGcMinorPhase phase;
    TZrUInt32 regionCursor;
    TZrUInt64 evacuatedBytes;
    TZrUInt64 rewrittenReferences;
    TZrUInt64 promotedObjects;
    TZrBool mutatorsResumed;
    TZrBool consistentBoundary;
} SZrGcMinorResult;

ZR_CORE_API TZrBool ZrCore_Gc_RunMinorTransaction(
        struct SZrState *state,
        const SZrGcMinorRequest *request,
        SZrGcMinorResult *result,
        SZrGcYoungDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_YOUNG_ALLOCATION_H */
