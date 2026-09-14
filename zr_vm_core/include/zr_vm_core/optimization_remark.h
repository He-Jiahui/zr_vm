#ifndef ZR_VM_CORE_OPTIMIZATION_REMARK_H
#define ZR_VM_CORE_OPTIMIZATION_REMARK_H

/*
 * Stable, pointer-free optimization remarks.
 *
 * A remark is a source fact produced by a pass/code generator/runtime site.
 * Consumers (CLI, JSON and LSP) must project this record; they must not infer
 * an optimization result by re-analyzing the source.  The record therefore
 * contains only fixed-width integers and bounded character arrays.  In
 * particular, `pass` is copied into the record and never points at a parser
 * or runtime allocation.
 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX ((TZrSize)48u)
#define ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX ((TZrSize)64u)

/* A source range uses byte offsets in the canonical source snapshot. */
typedef struct SZrOptimizationRemarkSourceRange {
    TZrUInt32 startOffset;
    TZrUInt32 endOffset;
} SZrOptimizationRemarkSourceRange;

typedef enum EZrOptimizationRemarkStatus {
    ZR_OPTIMIZATION_REMARK_SUCCESS = 0,
    ZR_OPTIMIZATION_REMARK_MISSED,
    ZR_OPTIMIZATION_REMARK_BLOCKED,
    ZR_OPTIMIZATION_REMARK_STATUS_COUNT
} EZrOptimizationRemarkStatus;

typedef enum EZrOptimizationRemarkReason {
    ZR_OPTIMIZATION_REMARK_REASON_NONE = 0,
    ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN,
    ZR_OPTIMIZATION_REMARK_REASON_ESCAPES,
    ZR_OPTIMIZATION_REMARK_REASON_ABI_VISIBLE,
    ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER,
    ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET,
    ZR_OPTIMIZATION_REMARK_REASON_PROFILE_STALE,
    ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED,
    ZR_OPTIMIZATION_REMARK_REASON_CAPABILITY_DENIED,
    ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE,
    ZR_OPTIMIZATION_REMARK_REASON_BOXING,
    ZR_OPTIMIZATION_REMARK_REASON_BOUNDS,
    ZR_OPTIMIZATION_REMARK_REASON_VECTOR,
    ZR_OPTIMIZATION_REMARK_REASON_BARRIER,
    ZR_OPTIMIZATION_REMARK_REASON_INLINING,
    ZR_OPTIMIZATION_REMARK_REASON_AOT,
    ZR_OPTIMIZATION_REMARK_REASON_LAYOUT,
    ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION,
    ZR_OPTIMIZATION_REMARK_REASON_DEOPT,
    ZR_OPTIMIZATION_REMARK_REASON_SOFTWARE_IC_MISS,
    ZR_OPTIMIZATION_REMARK_REASON_HARDWARE_CACHE_MISS,
    ZR_OPTIMIZATION_REMARK_REASON_BOUNDS_UNKNOWN,
    ZR_OPTIMIZATION_REMARK_REASON_CANCELLED,
    ZR_OPTIMIZATION_REMARK_REASON_TRUNCATED,
    ZR_OPTIMIZATION_REMARK_REASON_COUNT
} EZrOptimizationRemarkReason;

/* More descriptive spellings retained as source-level aliases. */
#define ZR_OPTIMIZATION_REMARK_REASON_BOXING_REQUIRED \
    ZR_OPTIMIZATION_REMARK_REASON_BOXING
#define ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION_REQUIRED \
    ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION
#define ZR_OPTIMIZATION_REMARK_REASON_DEOPT_REQUIRED \
    ZR_OPTIMIZATION_REMARK_REASON_DEOPT

/* The plan and early clients used both spellings; keep the type identity
 * explicit so code can use either without a compatibility struct. */
typedef EZrOptimizationRemarkReason ZrOptimizationRemarkReason;

typedef enum EZrOptimizationRemarkEvidence {
    ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN = 0,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_UNAVAILABLE,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT
} EZrOptimizationRemarkEvidence;

#define ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_AOT ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_JIT ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_LLVM ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_INTERPRETER ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_KNOWN_MASK \
    (ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC | \
     ZR_OPTIMIZATION_REMARK_BACKEND_AOT | \
     ZR_OPTIMIZATION_REMARK_BACKEND_JIT | \
     ZR_OPTIMIZATION_REMARK_BACKEND_LLVM | \
     ZR_OPTIMIZATION_REMARK_BACKEND_INTERPRETER)

#define ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_HARDWARE_CACHE ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_BRANCH ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_ALLOCATIONS ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_DEOPT ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_KNOWN_MASK \
    (ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC | \
     ZR_OPTIMIZATION_REMARK_COUNTER_HARDWARE_CACHE | \
     ZR_OPTIMIZATION_REMARK_COUNTER_BRANCH | \
     ZR_OPTIMIZATION_REMARK_COUNTER_ALLOCATIONS | \
     ZR_OPTIMIZATION_REMARK_COUNTER_DEOPT)

#define ZR_OPTIMIZATION_REMARK_REASON_MASK(reason) \
    ((TZrUInt32)(TZrUInt32)(reason) < 32u \
         ? ((TZrUInt32)1u << (TZrUInt32)(reason)) : 0u)

#define ZR_OPTIMIZATION_REMARK_STATUS_MASK(status) \
    ((TZrUInt32)(TZrUInt32)(status) < 32u \
         ? ((TZrUInt32)1u << (TZrUInt32)(status)) : 0u)

#define ZR_OPTIMIZATION_REMARK_EVIDENCE_MASK(evidence) \
    ((TZrUInt32)(TZrUInt32)(evidence) < 32u \
         ? ((TZrUInt32)1u << (TZrUInt32)(evidence)) : 0u)

/* Validation/query failures are intentionally independent from ExecIR
 * diagnostics: a remark is tooling data and must never masquerade as a
 * compile error. */
typedef enum EZrOptimizationRemarkDiagnosticCode {
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NONE = 0,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_SCHEMA,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_RANGE,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_STATUS,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_EVIDENCE,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_BACKEND,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_COUNTER,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NOT_FOUND,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNT
} EZrOptimizationRemarkDiagnosticCode;

typedef struct SZrOptimizationRemarkDiagnostic {
    EZrOptimizationRemarkDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrOptimizationRemarkDiagnostic;

typedef struct SZrOptimizationRemarkCounters {
    TZrUInt64 softwareIcMisses;
    TZrUInt64 hardwareCacheMisses;
    TZrUInt64 branchMisses;
    TZrUInt64 allocations;
    TZrUInt64 deopts;
} SZrOptimizationRemarkCounters;

/*
 * This is the on-record schema.  Do not add pointers here: it is copied to
 * artifacts and across the CLI/LSP boundary as a POD value.
 */
typedef struct SZrOptimizationRemark {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 moduleVersion;
    TZrUInt64 irVersion;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reserved0;
    /* Stable profile/runtime site identity.  It is a scalar key (never a
     * code address) and remains distinct from the proof witness below. */
    TZrUInt64 siteKey;
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
    EZrOptimizationRemarkStatus status;
    union {
        EZrOptimizationRemarkReason reason;
        EZrOptimizationRemarkReason reasonCode;
    };
    union {
        TZrUInt32 backendMask;
        TZrUInt32 backend;
    };
    union {
        EZrOptimizationRemarkEvidence evidence;
        EZrOptimizationRemarkEvidence evidenceKind;
    };
    TZrUInt32 reserved1;
    union {
        TZrUInt64 proofId;
        TZrUInt64 proof;
    };
    union {
        TZrUInt64 profileCount;
        TZrUInt64 profile;
    };
    union {
        TZrUInt64 estimatedCost;
        TZrUInt64 cost;
    };
    union {
        TZrUInt32 before;
        TZrUInt32 beforeRepresentation;
    };
    union {
        TZrUInt32 after;
        TZrUInt32 afterRepresentation;
    };
    TZrUInt32 measuredCounterMask;
    union {
        TZrUInt32 boxingFlags;
        TZrUInt32 boxing;
    };
    union {
        TZrUInt32 allocationFlags;
        TZrUInt32 allocation;
    };
    union {
        TZrUInt32 cacheFlags;
        TZrUInt32 cache;
    };
    union {
        TZrUInt32 deoptFlags;
        TZrUInt32 deopt;
    };
    union {
        SZrOptimizationRemarkCounters measuredCounters;
        SZrOptimizationRemarkCounters counters;
        struct {
            TZrUInt64 softwareIcMisses;
            TZrUInt64 hardwareCacheMisses;
            TZrUInt64 branchMisses;
            TZrUInt64 allocations;
            TZrUInt64 deopts;
        };
    };
} SZrOptimizationRemark;

typedef struct SZrOptimizationRemarkStore {
    SZrOptimizationRemark *items;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 maxRecords;
    TZrUInt32 reserved;
    TZrUInt64 droppedCount;
    TZrBool truncated;
} SZrOptimizationRemarkStore;

/* Query fields with a zero value are wildcards.  `has*` flags make zero a
 * selectable value for callers that need to query a genuine version/hash 0. */
typedef struct SZrOptimizationRemarkQuery {
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reasonMask;
    TZrUInt32 statusMask;
    TZrUInt32 backendMask;
    TZrUInt32 evidenceMask;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrBool hasModuleHash;
    TZrBool hasIrHash;
    TZrBool hasSourceVersion;
    TZrBool hasSourceId;
    TZrBool hasSourceRange;
    TZrBool hasPass;
    TZrBool hasModule;
    TZrBool reservedFlags[1];
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
} SZrOptimizationRemarkQuery;

typedef struct SZrOptimizationRemarkPage {
    SZrOptimizationRemark *items;
    TZrUInt32 count;
    TZrUInt32 totalMatches;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrUInt64 droppedCount;
    TZrBool truncated;
} SZrOptimizationRemarkPage;

ZR_CORE_API void ZrCore_OptimizationRemark_Init(SZrOptimizationRemark *remark);
ZR_CORE_API void ZrCore_OptimizationRemarkDiagnostic_Clear(
        SZrOptimizationRemarkDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_DiagnosticName(
        EZrOptimizationRemarkDiagnosticCode code);
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_StatusName(
        EZrOptimizationRemarkStatus status);
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_ReasonName(
        EZrOptimizationRemarkReason reason);
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_EvidenceName(
        EZrOptimizationRemarkEvidence evidence);
ZR_CORE_API TZrBool ZrCore_OptimizationRemark_Validate(
        const SZrOptimizationRemark *remark,
        SZrOptimizationRemarkDiagnostic *diagnostic);

ZR_CORE_API void ZrCore_OptimizationRemarks_StoreInit(
        SZrOptimizationRemarkStore *store);
ZR_CORE_API void ZrCore_OptimizationRemarks_StoreFree(
        SZrOptimizationRemarkStore *store);
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_Append(
        SZrOptimizationRemarkStore *store,
        const SZrOptimizationRemark *remark,
        SZrOptimizationRemarkDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_OptimizationRemarkQuery_Init(
        SZrOptimizationRemarkQuery *query);
ZR_CORE_API void ZrCore_OptimizationRemarks_PageFree(
        SZrOptimizationRemarkPage *page);
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_Query(
        const SZrOptimizationRemarkStore *store,
        const SZrOptimizationRemarkQuery *query,
        SZrOptimizationRemarkPage *page,
        SZrOptimizationRemarkDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_InvalidateSourceVersion(
        SZrOptimizationRemarkStore *store,
        TZrUInt64 moduleHash,
        TZrUInt64 sourceVersion,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/* Deterministic projections.  `outWrittenSize` excludes the NUL terminator. */
ZR_CORE_API TZrBool ZrCore_OptimizationRemark_WriteJson(
        const SZrOptimizationRemark *remark,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_PageWriteJson(
        const SZrOptimizationRemarkPage *page,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_OptimizationRemark_WriteText(
        const SZrOptimizationRemark *remark,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_OPTIMIZATION_REMARK_H */
