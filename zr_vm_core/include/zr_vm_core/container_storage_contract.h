#ifndef ZR_VM_CORE_CONTAINER_STORAGE_CONTRACT_H
#define ZR_VM_CORE_CONTAINER_STORAGE_CONTRACT_H

/*
 * Cacheable map/string storage witnesses.
 *
 * The records in this header deliberately contain scalar values only.  They
 * can therefore be copied into an ExecIR/artifact side table without keeping
 * a managed pointer, a callback address, or a borrowed allocation alive.  A
 * witness is an optimisation permission, not a replacement for the generic
 * map/string implementation: callers must still run equality on a hash
 * collision and must fall back when any fact is unknown.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_COMPACT_MAP_CONTRACT_MAGIC ((TZrUInt32)0x314d5043u) /* CPM1 */
#define ZR_STRING_STORAGE_CONTRACT_MAGIC ((TZrUInt32)0x31545343u) /* CST1 */
#define ZR_COMPACT_MAP_OFFSET_NONE ((TZrUInt32)UINT32_MAX)
#define ZR_STRING_STORAGE_DEFAULT_SHORT_LIMIT ((TZrUInt64)ZR_VM_SHORT_STRING_MAX)

typedef enum EZrContainerStorageDiagnosticCode {
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE = 0,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_MAGIC,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNKNOWN_FLAGS,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_DOMAIN,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_IDENTITY_OBSERVED,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INTERN_RETENTION,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_DEPTH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_SEGMENTS,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_OVERFLOW,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSUPPORTED,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_COUNT
} EZrContainerStorageDiagnosticCode;

/* Compatibility spellings used by callers that describe the reason first. */
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_UNSTABLE \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_EQUALITY_UNSTABLE \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_CUSTOM_EQUALITY \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_CUSTOM_EQUALITY \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE_OBSERVED \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE

typedef struct SZrContainerStorageDiagnostic {
    EZrContainerStorageDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrContainerStorageDiagnostic;

/* Hash/equality facts for a map key.  Unknown/custom equality is intentionally
 * represented as a flag instead of being guessed from a type name. */
#define ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY ((TZrUInt32)1u << 0u)
#define ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE ((TZrUInt32)1u << 1u)
#define ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE ((TZrUInt32)1u << 2u)
#define ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL ((TZrUInt32)1u << 3u)
#define ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY ((TZrUInt32)1u << 4u)
#define ZR_COMPACT_MAP_HASH_FLAG_IDENTITY_OBSERVED ((TZrUInt32)1u << 5u)
#define ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE ((TZrUInt32)1u << 6u)
#define ZR_COMPACT_MAP_HASH_FLAG_KNOWN_MASK \
    (ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY | \
     ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE | \
     ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE | \
     ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL | \
     ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY | \
     ZR_COMPACT_MAP_HASH_FLAG_IDENTITY_OBSERVED | \
     ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE)

typedef struct SZrCompactMapHash {
    TZrUInt64 seed;
    TZrUInt64 domain;
    TZrUInt32 version;
    TZrUInt32 flags;
} SZrCompactMapHash;

typedef enum EZrCompactMapTombstonePolicy {
    ZR_COMPACT_MAP_TOMBSTONE_NONE = 0,
    ZR_COMPACT_MAP_TOMBSTONE_MARK = 1,
    ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT = 2
} EZrCompactMapTombstonePolicy;

typedef enum EZrCompactMapIterationPolicy {
    ZR_COMPACT_MAP_ITERATION_UNSPECIFIED = 0,
    ZR_COMPACT_MAP_ITERATION_BUCKET_ORDER = 1,
    ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER = 2
} EZrCompactMapIterationPolicy;

typedef enum EZrCompactMapOwnershipPolicy {
    ZR_COMPACT_MAP_OWNERSHIP_NONE = 0,
    ZR_COMPACT_MAP_OWNERSHIP_BORROWED = 1,
    ZR_COMPACT_MAP_OWNERSHIP_OWNED = 2,
    ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED = 3
} EZrCompactMapOwnershipPolicy;

/* Candidate layout is a description only; it does not contain entry data. */
typedef struct SZrCompactMapLayout {
    TZrUInt32 entrySize;
    TZrUInt32 entryAlignment;
    TZrUInt32 bucketCount;
    TZrUInt32 maxLoadNumerator;
    TZrUInt32 maxLoadDenominator;
    TZrUInt32 hashOffset;
    TZrUInt32 keyOffset;
    TZrUInt32 valueOffset;
    TZrUInt32 nextOffset;
    EZrCompactMapTombstonePolicy tombstonePolicy;
    EZrCompactMapIterationPolicy iterationPolicy;
    EZrCompactMapOwnershipPolicy ownershipPolicy;
} SZrCompactMapLayout;

#define ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION ((TZrUInt32)1u << 0u)
#define ZR_COMPACT_MAP_FLAG_GC_ROOTS ((TZrUInt32)1u << 1u)
#define ZR_COMPACT_MAP_FLAG_CACHE_HASH ((TZrUInt32)1u << 2u)
#define ZR_COMPACT_MAP_FLAG_COMPACT_LAYOUT ((TZrUInt32)1u << 3u)
#define ZR_COMPACT_MAP_FLAG_KNOWN_MASK \
    (ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION | ZR_COMPACT_MAP_FLAG_GC_ROOTS | \
     ZR_COMPACT_MAP_FLAG_CACHE_HASH | ZR_COMPACT_MAP_FLAG_COMPACT_LAYOUT)

typedef struct SZrCompactMapCandidate {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    SZrCompactMapHash hash;
    SZrCompactMapLayout layout;
    TZrUInt32 flags;
    TZrUInt32 reserved;
    TZrUInt64 layoutHash;
    TZrUInt64 candidateHash;
} SZrCompactMapCandidate;

ZR_CORE_API void ZrCore_ContainerStorage_DiagnosticClear(
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_ContainerStorage_DiagnosticName(
        EZrContainerStorageDiagnosticCode code);

ZR_CORE_API void ZrCore_CompactMapCandidate_Init(
        SZrCompactMapCandidate *candidate);
ZR_CORE_API TZrBool ZrCore_CompactMapCandidate_Finalize(
        SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompactMapCandidate_Validate(
        const SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_CompactMap_CanCacheHash(
        const SZrCompactMapHash *hash);
ZR_CORE_API TZrUInt64 ZrCore_CompactMap_HashBytes(
        const SZrCompactMapHash *hash,
        const TZrByte *data,
        TZrSize length);
ZR_CORE_API TZrBool ZrCore_CompactMap_HashWitnessEqual(
        const SZrCompactMapHash *left,
        const SZrCompactMapHash *right);
/* A matching hash is only a probe hint.  The caller must invoke the language
 * equality operation, including custom/effectful equality, before accepting a
 * key. */
ZR_CORE_API TZrBool ZrCore_CompactMap_ProbeNeedsEquality(
        TZrUInt64 storedHash,
        TZrUInt64 incomingHash);
ZR_CORE_API TZrSize ZrCore_CompactMap_BucketIndex(
        TZrUInt64 hash,
        TZrSize bucketCount);

/* String facts and strategy candidates.  All fields are scalar and are safe
 * to persist in a cache/artifact record. */
#define ZR_STRING_STORAGE_FLAG_IMMUTABLE ((TZrUInt32)1u << 0u)
#define ZR_STRING_STORAGE_FLAG_UTF8_VALID ((TZrUInt32)1u << 1u)
#define ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED ((TZrUInt32)1u << 2u)
#define ZR_STRING_STORAGE_FLAG_ESCAPES ((TZrUInt32)1u << 3u)
#define ZR_STRING_STORAGE_FLAG_FFI_VISIBLE ((TZrUInt32)1u << 4u)
#define ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE ((TZrUInt32)1u << 5u)
#define ZR_STRING_STORAGE_FLAG_INTERN_WEAK ((TZrUInt32)1u << 6u)
#define ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE ((TZrUInt32)1u << 7u)
#define ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT ((TZrUInt32)1u << 8u)
#define ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT ((TZrUInt32)1u << 9u)
#define ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN ((TZrUInt32)1u << 10u)
#define ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY ((TZrUInt32)1u << 11u)
#define ZR_STRING_STORAGE_FLAG_ADDRESS_ESCAPES ((TZrUInt32)1u << 12u)
#define ZR_STRING_STORAGE_FLAG_KNOWN_MASK \
    (ZR_STRING_STORAGE_FLAG_IMMUTABLE | ZR_STRING_STORAGE_FLAG_UTF8_VALID | \
     ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED | ZR_STRING_STORAGE_FLAG_ESCAPES | \
     ZR_STRING_STORAGE_FLAG_FFI_VISIBLE | \
     ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE | \
     ZR_STRING_STORAGE_FLAG_INTERN_WEAK | \
     ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE | \
     ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT | \
     ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT | \
     ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN | \
     ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY | \
     ZR_STRING_STORAGE_FLAG_ADDRESS_ESCAPES)

typedef struct SZrStringStorageFacts {
    TZrUInt32 flags;
    TZrUInt32 reserved;
    TZrUInt64 byteLength;
    TZrUInt64 shortStringLimit;
    TZrUInt64 internDomain;
    TZrUInt64 storageGeneration;
    TZrUInt64 declaredEffects;
    TZrUInt64 replacementEffects;
    TZrUInt64 intermediateCount;
    TZrUInt64 maxBuilderBytes;
    TZrUInt64 ropeDepth;
    TZrUInt64 segmentCount;
    TZrUInt64 ropeMaxDepth;
    TZrUInt64 ropeMaxSegments;
    TZrUInt64 measuredCopyBytes;
    TZrUInt64 projectedRopeBytes;
    TZrUInt64 flattenBudgetBytes;
    TZrUInt64 flattenBudgetMicros;
} SZrStringStorageFacts;

typedef enum EZrStringStorageStrategy {
    ZR_STRING_STORAGE_STRATEGY_GENERIC = 0,
    ZR_STRING_STORAGE_STRATEGY_SSO = 1,
    ZR_STRING_STORAGE_STRATEGY_INTERN = 2,
    ZR_STRING_STORAGE_STRATEGY_BUILDER = 3,
    ZR_STRING_STORAGE_STRATEGY_ROPE = 4
} EZrStringStorageStrategy;

/* Common spelling aliases keep the contract readable to C callers that use
 * the all-caps abbreviation for short-string optimisation. */
#define ZR_STRING_STORAGE_STRATEGY_SHORT_STRING ZR_STRING_STORAGE_STRATEGY_SSO
#define ZrCore_StringStorage_CanUseSSO ZrCore_StringStorage_CanUseSso

typedef struct SZrStringStorageCandidate {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrStringStorageStrategy strategy;
    TZrUInt32 flags;
    TZrUInt64 byteLength;
    TZrUInt64 shortStringLimit;
    TZrUInt64 internDomain;
    TZrUInt64 ropeDepth;
    TZrUInt64 segmentCount;
    TZrUInt64 ropeMaxDepth;
    TZrUInt64 ropeMaxSegments;
    TZrUInt64 measuredCopyBytes;
    TZrUInt64 projectedRopeBytes;
    TZrUInt64 flattenBudgetBytes;
    TZrUInt64 flattenBudgetMicros;
    TZrUInt64 declaredEffects;
    TZrUInt64 replacementEffects;
    TZrUInt64 intermediateCount;
    TZrUInt64 maxBuilderBytes;
    TZrUInt64 candidateHash;
} SZrStringStorageCandidate;

ZR_CORE_API void ZrCore_StringStorageFacts_Init(
        SZrStringStorageFacts *facts);
ZR_CORE_API TZrBool ZrCore_StringStorage_Validate(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseSso(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseIntern(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseBuilder(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseRope(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API EZrStringStorageStrategy ZrCore_StringStorage_SelectStrategy(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_StringStorage_BuildCandidate(
        const SZrStringStorageFacts *facts,
        SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_StringStorage_ValidateCandidate(
        const SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_CONTAINER_STORAGE_CONTRACT_H */
