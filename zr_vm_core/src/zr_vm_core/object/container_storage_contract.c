#include "zr_vm_core/container_storage_contract.h"

#include <stdint.h>
#include <string.h>

/*
 * This file intentionally has no allocator or object access.  The contract is
 * a scalar witness consumed by the real hash-set/string implementations; a
 * rejected witness simply leaves those implementations on their generic path.
 */

#define ZR_CONTAINER_STORAGE_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_CONTAINER_STORAGE_FNV_PRIME UINT64_C(1099511628211)

enum {
    ZR_CONTAINER_STORAGE_FIELD_FLAGS = 1u,
    ZR_CONTAINER_STORAGE_FIELD_HASH = 2u,
    ZR_CONTAINER_STORAGE_FIELD_LAYOUT = 3u,
    ZR_CONTAINER_STORAGE_FIELD_OWNERSHIP = 4u,
    ZR_CONTAINER_STORAGE_FIELD_ITERATION = 5u,
    ZR_CONTAINER_STORAGE_FIELD_TOMBSTONE = 6u,
    ZR_CONTAINER_STORAGE_FIELD_STRING = 7u,
    ZR_CONTAINER_STORAGE_FIELD_EFFECTS = 8u,
    ZR_CONTAINER_STORAGE_FIELD_BUDGET = 9u
};

static void container_storage_diag_set(SZrContainerStorageDiagnostic *diagnostic,
                                       EZrContainerStorageDiagnosticCode code,
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

void ZrCore_ContainerStorage_DiagnosticClear(
        SZrContainerStorageDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE;
    }
}

const TZrChar *ZrCore_ContainerStorage_DiagnosticName(
        EZrContainerStorageDiagnosticCode code) {
    switch (code) {
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE: return "none";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_MAGIC: return "invalid-magic";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_SCHEMA: return "invalid-schema";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNKNOWN_FLAGS: return "unknown-flags";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT: return "invalid-layout";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH: return "unstable-hash";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY: return "unstable-equality";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_DOMAIN: return "hash-domain";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP: return "ownership";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION: return "iteration";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE: return "tombstone";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE: return "escape";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_IDENTITY_OBSERVED: return "identity-observed";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_INTERN_RETENTION: return "intern-retention";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT: return "short-string-limit";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH: return "effect-mismatch";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED: return "rope-unmeasured";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET: return "budget";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_DEPTH: return "depth";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_SEGMENTS: return "segments";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_OVERFLOW: return "overflow";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSUPPORTED: return "unsupported";
        case ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH: return "hash-mismatch";
        default: return "unknown";
    }
}

static TZrUInt64 container_storage_hash_mul_mod_u64(TZrUInt64 left,
                                                     TZrUInt64 right) {
    /* FNV-1a is defined over the low 64 bits.  Tell integer sanitizers that
     * this overflow is intentional while retaining a C11 fallback for MSVC. */
#if defined(__clang__) || \
    (defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__ >= 500))
    TZrUInt64 result;
    (void)__builtin_mul_overflow(left, right, &result);
    return result;
#else
    return left * right;
#endif
}

static TZrUInt64 container_storage_hash_byte(TZrUInt64 hash, TZrUInt8 byte) {
    return container_storage_hash_mul_mod_u64(
            hash ^ (TZrUInt64)byte, ZR_CONTAINER_STORAGE_FNV_PRIME);
}

static TZrUInt64 container_storage_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        hash = container_storage_hash_byte(hash,
                                           (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static TZrUInt64 container_storage_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        hash = container_storage_hash_byte(hash,
                                           (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static TZrBool container_storage_power_of_two_u32(TZrUInt32 value) {
    return (TZrBool)(value != 0u && (value & (value - 1u)) == 0u);
}

static TZrBool container_storage_power_of_two_size(TZrSize value) {
    return (TZrBool)(value != 0u && (value & (value - 1u)) == 0u);
}

static TZrBool compact_map_hash_well_formed(const SZrCompactMapHash *hash) {
    if (hash == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)((hash->flags & ~((TZrUInt32)ZR_COMPACT_MAP_HASH_FLAG_KNOWN_MASK)) == 0u &&
                     hash->seed != 0u && hash->domain != 0u && hash->version != 0u);
}

TZrBool ZrCore_CompactMap_CanCacheHash(const SZrCompactMapHash *hash) {
    TZrUInt32 required;

    if (hash == ZR_NULL) {
        return ZR_FALSE;
    }
    required = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
               ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
               ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
               ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL |
               ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE;
    if ((hash->flags & ~((TZrUInt32)ZR_COMPACT_MAP_HASH_FLAG_KNOWN_MASK)) != 0u ||
        (hash->flags & required) != required ||
        (hash->flags & (ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY |
                        ZR_COMPACT_MAP_HASH_FLAG_IDENTITY_OBSERVED)) != 0u ||
        hash->seed == 0u || hash->domain == 0u || hash->version == 0u) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrUInt64 ZrCore_CompactMap_HashBytes(const SZrCompactMapHash *hash,
                                      const TZrByte *data,
                                      TZrSize length) {
    TZrUInt64 result;
    TZrSize index;

    if (hash == ZR_NULL || (data == ZR_NULL && length != 0u) ||
        hash->seed == 0u || hash->version == 0u || hash->domain == 0u ||
        (hash->flags & ~((TZrUInt32)ZR_COMPACT_MAP_HASH_FLAG_KNOWN_MASK)) != 0u) {
        return 0u;
    }

    /* Seed, domain, version, and length are part of the hash transcript.  A
     * cache entry from another VM/domain therefore cannot be reused merely
     * because the bytes happen to match. */
    result = ZR_CONTAINER_STORAGE_FNV_OFFSET;
    result = container_storage_hash_u64(result, hash->seed);
    result = container_storage_hash_u64(result, hash->domain);
    result = container_storage_hash_u32(result, hash->version);
    result = container_storage_hash_u64(result, (TZrUInt64)length);
    for (index = 0u; index < length; ++index) {
        result = container_storage_hash_byte(result, data[index]);
    }
    /* Keep zero available as an invalid/uncomputed marker. */
    return result == 0u ? UINT64_C(1) : result;
}

TZrBool ZrCore_CompactMap_HashWitnessEqual(const SZrCompactMapHash *left,
                                           const SZrCompactMapHash *right) {
    return (TZrBool)(compact_map_hash_well_formed(left) &&
                     compact_map_hash_well_formed(right) &&
                     left->seed == right->seed &&
                     left->domain == right->domain &&
                     left->version == right->version &&
                     left->flags == right->flags);
}

TZrBool ZrCore_CompactMap_ProbeNeedsEquality(TZrUInt64 storedHash,
                                            TZrUInt64 incomingHash) {
    /* Hash equality is only the collision gate.  Never turn this into an
     * equality bypass: the caller still invokes the language comparator. */
    return (TZrBool)(storedHash == incomingHash);
}

TZrSize ZrCore_CompactMap_BucketIndex(TZrUInt64 hash, TZrSize bucketCount) {
    if (!container_storage_power_of_two_size(bucketCount)) {
        return 0u;
    }
    return (TZrSize)(hash & (bucketCount - 1u));
}

static TZrBool compact_map_layout_valid(const SZrCompactMapCandidate *candidate,
                                        SZrContainerStorageDiagnostic *diagnostic) {
    const SZrCompactMapLayout *layout;
    TZrUInt32 alignmentMask;

    if (candidate == ZR_NULL) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT, 1u, 0u);
        return ZR_FALSE;
    }
    layout = &candidate->layout;
    if (layout->entrySize < sizeof(TZrUInt64) * 2u ||
        layout->entryAlignment == 0u ||
        !container_storage_power_of_two_u32(layout->entryAlignment) ||
        layout->entryAlignment > layout->entrySize ||
        !container_storage_power_of_two_u32(layout->bucketCount) ||
        layout->maxLoadNumerator == 0u ||
        layout->maxLoadDenominator == 0u ||
        layout->maxLoadNumerator >= layout->maxLoadDenominator) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT, 1u, 0u);
        return ZR_FALSE;
    }
    if (layout->hashOffset > layout->entrySize - sizeof(TZrUInt64) ||
        layout->keyOffset > layout->entrySize - sizeof(TZrUInt64) ||
        layout->valueOffset > layout->entrySize - sizeof(TZrUInt64) ||
        (layout->nextOffset != ZR_COMPACT_MAP_OFFSET_NONE &&
         layout->nextOffset > layout->entrySize - sizeof(TZrUInt64))) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT,
                                   layout->entrySize, layout->hashOffset);
        return ZR_FALSE;
    }

    /* Every scalar lane occupies one machine-independent 64-bit word in the
     * candidate description.  Overlapping key/value/hash lanes would make a
     * supposedly compact layout alias ownership or cached-hash state. */
    if ((layout->hashOffset < layout->keyOffset + sizeof(TZrUInt64) &&
         layout->keyOffset < layout->hashOffset + sizeof(TZrUInt64)) ||
        (layout->hashOffset < layout->valueOffset + sizeof(TZrUInt64) &&
         layout->valueOffset < layout->hashOffset + sizeof(TZrUInt64)) ||
        (layout->keyOffset < layout->valueOffset + sizeof(TZrUInt64) &&
         layout->valueOffset < layout->keyOffset + sizeof(TZrUInt64)) ||
        (layout->nextOffset != ZR_COMPACT_MAP_OFFSET_NONE &&
         ((layout->nextOffset < layout->hashOffset + sizeof(TZrUInt64) &&
           layout->hashOffset < layout->nextOffset + sizeof(TZrUInt64)) ||
          (layout->nextOffset < layout->keyOffset + sizeof(TZrUInt64) &&
           layout->keyOffset < layout->nextOffset + sizeof(TZrUInt64)) ||
          (layout->nextOffset < layout->valueOffset + sizeof(TZrUInt64) &&
           layout->valueOffset < layout->nextOffset + sizeof(TZrUInt64))))) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT, 0u,
                                   layout->entrySize);
        return ZR_FALSE;
    }
    alignmentMask = layout->entryAlignment - 1u;
    if (((layout->hashOffset | layout->keyOffset | layout->valueOffset) &
         alignmentMask) != 0u ||
        (layout->nextOffset != ZR_COMPACT_MAP_OFFSET_NONE &&
         (layout->nextOffset & alignmentMask) != 0u)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT,
                                   layout->entryAlignment, layout->keyOffset);
        return ZR_FALSE;
    }
    if (layout->tombstonePolicy < ZR_COMPACT_MAP_TOMBSTONE_NONE ||
        layout->tombstonePolicy > ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE,
                                   ZR_CONTAINER_STORAGE_FIELD_TOMBSTONE, 2u,
                                   (TZrUInt64)layout->tombstonePolicy);
        return ZR_FALSE;
    }
    if (layout->iterationPolicy < ZR_COMPACT_MAP_ITERATION_UNSPECIFIED ||
        layout->iterationPolicy > ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION,
                                   ZR_CONTAINER_STORAGE_FIELD_ITERATION, 2u,
                                   (TZrUInt64)layout->iterationPolicy);
        return ZR_FALSE;
    }
    if (layout->ownershipPolicy > ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
                                   ZR_CONTAINER_STORAGE_FIELD_OWNERSHIP, 1u,
                                   (TZrUInt64)layout->ownershipPolicy);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 compact_map_layout_hash(const SZrCompactMapCandidate *candidate) {
    TZrUInt64 result = ZR_CONTAINER_STORAGE_FNV_OFFSET;
    const SZrCompactMapLayout *layout;

    if (candidate == ZR_NULL) {
        return 0u;
    }
    layout = &candidate->layout;
    result = container_storage_hash_u32(result, layout->entrySize);
    result = container_storage_hash_u32(result, layout->entryAlignment);
    result = container_storage_hash_u32(result, layout->bucketCount);
    result = container_storage_hash_u32(result, layout->maxLoadNumerator);
    result = container_storage_hash_u32(result, layout->maxLoadDenominator);
    result = container_storage_hash_u32(result, layout->hashOffset);
    result = container_storage_hash_u32(result, layout->keyOffset);
    result = container_storage_hash_u32(result, layout->valueOffset);
    result = container_storage_hash_u32(result, layout->nextOffset);
    result = container_storage_hash_u32(result, (TZrUInt32)layout->tombstonePolicy);
    result = container_storage_hash_u32(result, (TZrUInt32)layout->iterationPolicy);
    result = container_storage_hash_u32(result, (TZrUInt32)layout->ownershipPolicy);
    return result == 0u ? UINT64_C(1) : result;
}

static TZrUInt64 compact_map_candidate_hash(const SZrCompactMapCandidate *candidate) {
    TZrUInt64 result = ZR_CONTAINER_STORAGE_FNV_OFFSET;

    if (candidate == ZR_NULL) {
        return 0u;
    }
    result = container_storage_hash_u32(result, candidate->magic);
    result = container_storage_hash_u32(result, candidate->schemaVersion);
    result = container_storage_hash_u64(result, candidate->hash.seed);
    result = container_storage_hash_u64(result, candidate->hash.domain);
    result = container_storage_hash_u32(result, candidate->hash.version);
    result = container_storage_hash_u32(result, candidate->hash.flags);
    result = container_storage_hash_u64(result, candidate->layoutHash);
    result = container_storage_hash_u32(result, candidate->flags);
    result = container_storage_hash_u32(result, candidate->reserved);
    /* Include the complete layout so changing a field invalidates the record. */
    result = container_storage_hash_u64(result, compact_map_layout_hash(candidate));
    return result == 0u ? UINT64_C(1) : result;
}

void ZrCore_CompactMapCandidate_Init(SZrCompactMapCandidate *candidate) {
    if (candidate == ZR_NULL) {
        return;
    }
    memset(candidate, 0, sizeof(*candidate));
    candidate->magic = ZR_COMPACT_MAP_CONTRACT_MAGIC;
    candidate->schemaVersion = ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION;
    candidate->layout.nextOffset = ZR_COMPACT_MAP_OFFSET_NONE;
}

TZrBool ZrCore_CompactMapCandidate_Finalize(
        SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
    if (candidate == ZR_NULL) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT, 1u, 0u);
        return ZR_FALSE;
    }
    if (candidate->magic != ZR_COMPACT_MAP_CONTRACT_MAGIC) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_MAGIC, 0u,
                                   ZR_COMPACT_MAP_CONTRACT_MAGIC, candidate->magic);
        return ZR_FALSE;
    }
    if (candidate->schemaVersion != ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_SCHEMA, 0u,
                                   ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION,
                                   candidate->schemaVersion);
        return ZR_FALSE;
    }
    if (!compact_map_hash_well_formed(&candidate->hash)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u,
                                   candidate->hash.flags);
        return ZR_FALSE;
    }
    if ((candidate->hash.flags & ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 0u,
                                   candidate->hash.flags);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_CACHE_HASH) != 0u &&
        !ZrCore_CompactMap_CanCacheHash(&candidate->hash)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u,
                                   candidate->hash.flags);
        return ZR_FALSE;
    }
    if (candidate->reserved != 0u ||
        (candidate->flags & ~((TZrUInt32)ZR_COMPACT_MAP_FLAG_KNOWN_MASK)) != 0u ||
        !compact_map_layout_valid(candidate, diagnostic)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE) {
            container_storage_diag_set(diagnostic,
                                       ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNKNOWN_FLAGS,
                                       ZR_CONTAINER_STORAGE_FIELD_FLAGS, 0u,
                                       candidate->flags);
        }
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_GC_ROOTS) != 0u &&
        candidate->layout.ownershipPolicy != ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
                                   ZR_CONTAINER_STORAGE_FIELD_OWNERSHIP,
                                   ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED,
                                   (TZrUInt64)candidate->layout.ownershipPolicy);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_GC_ROOTS) == 0u &&
        candidate->layout.ownershipPolicy == ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
                                   ZR_CONTAINER_STORAGE_FIELD_OWNERSHIP,
                                   ZR_COMPACT_MAP_FLAG_GC_ROOTS, 0u);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION) != 0u &&
        (candidate->layout.iterationPolicy != ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER ||
         candidate->layout.tombstonePolicy != ZR_COMPACT_MAP_TOMBSTONE_MARK)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION,
                                   ZR_CONTAINER_STORAGE_FIELD_ITERATION,
                                   ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER,
                                   (TZrUInt64)candidate->layout.iterationPolicy);
        return ZR_FALSE;
    }
    if (candidate->layout.iterationPolicy == ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER &&
        candidate->layout.tombstonePolicy == ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE,
                                   ZR_CONTAINER_STORAGE_FIELD_TOMBSTONE,
                                   ZR_COMPACT_MAP_TOMBSTONE_MARK,
                                   ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT);
        return ZR_FALSE;
    }
    candidate->layoutHash = compact_map_layout_hash(candidate);
    candidate->candidateHash = compact_map_candidate_hash(candidate);
    return ZR_TRUE;
}

TZrBool ZrCore_CompactMapCandidate_Validate(
        const SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    TZrUInt64 expectedLayoutHash;
    TZrUInt64 expectedCandidateHash;

    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
    if (candidate == ZR_NULL) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT, 1u, 0u);
        return ZR_FALSE;
    }
    if (candidate->magic != ZR_COMPACT_MAP_CONTRACT_MAGIC) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_MAGIC, 0u,
                                   ZR_COMPACT_MAP_CONTRACT_MAGIC, candidate->magic);
        return ZR_FALSE;
    }
    if (candidate->schemaVersion != ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_SCHEMA, 0u,
                                   ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION,
                                   candidate->schemaVersion);
        return ZR_FALSE;
    }
    if (candidate->reserved != 0u ||
        (candidate->flags & ~((TZrUInt32)ZR_COMPACT_MAP_FLAG_KNOWN_MASK)) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNKNOWN_FLAGS,
                                   ZR_CONTAINER_STORAGE_FIELD_FLAGS,
                                   ZR_COMPACT_MAP_FLAG_KNOWN_MASK, candidate->flags);
        return ZR_FALSE;
    }
    if (!compact_map_layout_valid(candidate, diagnostic)) {
        return ZR_FALSE;
    }
    if (!compact_map_hash_well_formed(&candidate->hash)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u,
                                   candidate->hash.flags);
        return ZR_FALSE;
    }
    if ((candidate->hash.flags & ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 0u,
                                   candidate->hash.flags);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_CACHE_HASH) != 0u &&
        !ZrCore_CompactMap_CanCacheHash(&candidate->hash)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u,
                                   candidate->hash.flags);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_GC_ROOTS) != 0u &&
        candidate->layout.ownershipPolicy != ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
                                   ZR_CONTAINER_STORAGE_FIELD_OWNERSHIP,
                                   ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED,
                                   (TZrUInt64)candidate->layout.ownershipPolicy);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_GC_ROOTS) == 0u &&
        candidate->layout.ownershipPolicy == ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
                                   ZR_CONTAINER_STORAGE_FIELD_OWNERSHIP,
                                   ZR_COMPACT_MAP_FLAG_GC_ROOTS, 0u);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION) != 0u &&
        (candidate->layout.iterationPolicy != ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER ||
         candidate->layout.tombstonePolicy != ZR_COMPACT_MAP_TOMBSTONE_MARK)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION,
                                   ZR_CONTAINER_STORAGE_FIELD_ITERATION,
                                   ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER,
                                   (TZrUInt64)candidate->layout.iterationPolicy);
        return ZR_FALSE;
    }
    if (candidate->layout.iterationPolicy == ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER &&
        candidate->layout.tombstonePolicy == ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE,
                                   ZR_CONTAINER_STORAGE_FIELD_TOMBSTONE,
                                   ZR_COMPACT_MAP_TOMBSTONE_MARK,
                                   ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT);
        return ZR_FALSE;
    }
    expectedLayoutHash = compact_map_layout_hash(candidate);
    if (candidate->layoutHash == 0u || candidate->layoutHash != expectedLayoutHash) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_LAYOUT,
                                   expectedLayoutHash, candidate->layoutHash);
        return ZR_FALSE;
    }
    expectedCandidateHash = compact_map_candidate_hash(candidate);
    if (candidate->candidateHash == 0u || candidate->candidateHash != expectedCandidateHash) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH,
                                   expectedCandidateHash, candidate->candidateHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrCore_StringStorageFacts_Init(SZrStringStorageFacts *facts) {
    if (facts == ZR_NULL) {
        return;
    }
    memset(facts, 0, sizeof(*facts));
    facts->shortStringLimit = ZR_STRING_STORAGE_DEFAULT_SHORT_LIMIT;
}

static TZrBool string_storage_observable(const SZrStringStorageFacts *facts,
                                         SZrContainerStorageDiagnostic *diagnostic) {
    TZrUInt32 observableFlags = ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED |
                                ZR_STRING_STORAGE_FLAG_ESCAPES |
                                ZR_STRING_STORAGE_FLAG_ADDRESS_ESCAPES |
                                ZR_STRING_STORAGE_FLAG_FFI_VISIBLE;
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_EFFECTS,
                                   facts->declaredEffects, facts->replacementEffects);
        return ZR_TRUE;
    }
    if ((facts->flags & observableFlags) != 0u) {
        if ((facts->flags & ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED) != 0u) {
            container_storage_diag_set(diagnostic,
                                       ZR_CONTAINER_STORAGE_DIAGNOSTIC_IDENTITY_OBSERVED,
                                       ZR_CONTAINER_STORAGE_FIELD_STRING, 0u,
                                       facts->flags);
        } else {
            container_storage_diag_set(diagnostic,
                                       ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE,
                                       ZR_CONTAINER_STORAGE_FIELD_STRING, 0u,
                                       facts->flags);
        }
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_StringStorage_Validate(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
    if (facts == ZR_NULL) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u, 0u);
        return ZR_FALSE;
    }
    if (facts->reserved != 0u ||
        (facts->flags & ~((TZrUInt32)ZR_STRING_STORAGE_FLAG_KNOWN_MASK)) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNKNOWN_FLAGS,
                                   ZR_CONTAINER_STORAGE_FIELD_FLAGS,
                                   ZR_STRING_STORAGE_FLAG_KNOWN_MASK, facts->flags);
        return ZR_FALSE;
    }
    if (facts->shortStringLimit == 0u ||
        facts->shortStringLimit > (TZrUInt64)ZR_VM_SHORT_STRING_MAX) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   (TZrUInt64)ZR_VM_SHORT_STRING_MAX,
                                   facts->shortStringLimit);
        return ZR_FALSE;
    }
    if ((facts->declaredEffects & ~((TZrUInt64)ZR_EXECUTION_EFFECT_KNOWN_MASK)) != 0u ||
        (facts->replacementEffects & ~((TZrUInt64)ZR_EXECUTION_EFFECT_KNOWN_MASK)) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_EFFECTS,
                                   ZR_EXECUTION_EFFECT_KNOWN_MASK,
                                   facts->declaredEffects | facts->replacementEffects);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE) != 0u &&
        facts->internDomain == 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_DOMAIN,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u, 0u);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN) != 0u &&
        (facts->flattenBudgetBytes == 0u || facts->ropeMaxDepth == 0u ||
         facts->ropeMaxSegments == 0u)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET,
                                   ZR_CONTAINER_STORAGE_FIELD_BUDGET, 1u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool string_storage_common_eligibility(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    if (!ZrCore_StringStorage_Validate(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_IMMUTABLE) == 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSUPPORTED,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u,
                                   facts->flags);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_StringStorage_CanUseSso(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    if (!string_storage_common_eligibility(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if (facts->byteLength > facts->shortStringLimit) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   facts->shortStringLimit, facts->byteLength);
        return ZR_FALSE;
    }
    if (string_storage_observable(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 0u,
                                   facts->flags);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_StringStorage_CanUseIntern(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    if (!string_storage_common_eligibility(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if (string_storage_observable(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 0u,
                                   facts->flags);
        return ZR_FALSE;
    }
    if (facts->byteLength > facts->shortStringLimit) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   facts->shortStringLimit, facts->byteLength);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_INTERN_WEAK) == 0u ||
        (facts->flags & ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE) == 0u ||
        facts->internDomain == 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INTERN_RETENTION,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u,
                                   facts->internDomain);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_StringStorage_CanUseBuilder(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    if (!string_storage_common_eligibility(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if (string_storage_observable(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE) != 0u ||
        (facts->flags & ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_EFFECTS,
                                   facts->declaredEffects, facts->replacementEffects);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT) == 0u ||
        facts->declaredEffects != facts->replacementEffects) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_EFFECTS,
                                   facts->declaredEffects, facts->replacementEffects);
        return ZR_FALSE;
    }
    if (facts->intermediateCount == 0u || facts->maxBuilderBytes == 0u ||
        facts->byteLength > facts->maxBuilderBytes) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET,
                                   ZR_CONTAINER_STORAGE_FIELD_BUDGET,
                                   facts->maxBuilderBytes, facts->byteLength);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_StringStorage_CanUseRope(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    if (!string_storage_common_eligibility(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if (string_storage_observable(facts, diagnostic)) {
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT) == 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u, 0u);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN) == 0u ||
        facts->flattenBudgetBytes < facts->byteLength ||
        facts->flattenBudgetBytes == 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET,
                                   ZR_CONTAINER_STORAGE_FIELD_BUDGET,
                                   facts->byteLength, facts->flattenBudgetBytes);
        return ZR_FALSE;
    }
    if (facts->ropeDepth == 0u || facts->ropeDepth > facts->ropeMaxDepth) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_DEPTH,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   facts->ropeMaxDepth, facts->ropeDepth);
        return ZR_FALSE;
    }
    if (facts->segmentCount == 0u || facts->segmentCount > facts->ropeMaxSegments) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_SEGMENTS,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   facts->ropeMaxSegments, facts->segmentCount);
        return ZR_FALSE;
    }
    if (facts->projectedRopeBytes == 0u ||
        facts->projectedRopeBytes >= facts->measuredCopyBytes) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   facts->measuredCopyBytes, facts->projectedRopeBytes);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

EZrStringStorageStrategy ZrCore_StringStorage_SelectStrategy(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    SZrContainerStorageDiagnostic local;

    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
    if (!ZrCore_StringStorage_Validate(facts, diagnostic)) {
        return ZR_STRING_STORAGE_STRATEGY_GENERIC;
    }
    /* Existing short strings are interned by ZrCore_String_Create.  Preserve
     * that canonical identity before considering a physical SSO-only lane. */
    if (ZrCore_StringStorage_CanUseIntern(facts, &local)) {
        return ZR_STRING_STORAGE_STRATEGY_INTERN;
    }
    if (ZrCore_StringStorage_CanUseSso(facts, &local)) {
        return ZR_STRING_STORAGE_STRATEGY_SSO;
    }
    if (ZrCore_StringStorage_CanUseBuilder(facts, &local)) {
        return ZR_STRING_STORAGE_STRATEGY_BUILDER;
    }
    if (ZrCore_StringStorage_CanUseRope(facts, &local)) {
        return ZR_STRING_STORAGE_STRATEGY_ROPE;
    }
    if (diagnostic != ZR_NULL) {
        *diagnostic = local;
        if (diagnostic->code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE) {
            diagnostic->code = ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSUPPORTED;
        }
    }
    return ZR_STRING_STORAGE_STRATEGY_GENERIC;
}

static TZrUInt64 string_storage_candidate_hash(const SZrStringStorageCandidate *candidate) {
    TZrUInt64 result = ZR_CONTAINER_STORAGE_FNV_OFFSET;

    if (candidate == ZR_NULL) {
        return 0u;
    }
    result = container_storage_hash_u32(result, candidate->magic);
    result = container_storage_hash_u32(result, candidate->schemaVersion);
    result = container_storage_hash_u32(result, (TZrUInt32)candidate->strategy);
    result = container_storage_hash_u32(result, candidate->flags);
    result = container_storage_hash_u64(result, candidate->byteLength);
    result = container_storage_hash_u64(result, candidate->shortStringLimit);
    result = container_storage_hash_u64(result, candidate->internDomain);
    result = container_storage_hash_u64(result, candidate->ropeDepth);
    result = container_storage_hash_u64(result, candidate->segmentCount);
    result = container_storage_hash_u64(result, candidate->ropeMaxDepth);
    result = container_storage_hash_u64(result, candidate->ropeMaxSegments);
    result = container_storage_hash_u64(result, candidate->measuredCopyBytes);
    result = container_storage_hash_u64(result, candidate->projectedRopeBytes);
    result = container_storage_hash_u64(result, candidate->flattenBudgetBytes);
    result = container_storage_hash_u64(result, candidate->flattenBudgetMicros);
    result = container_storage_hash_u64(result, candidate->declaredEffects);
    result = container_storage_hash_u64(result, candidate->replacementEffects);
    result = container_storage_hash_u64(result, candidate->intermediateCount);
    result = container_storage_hash_u64(result, candidate->maxBuilderBytes);
    return result == 0u ? UINT64_C(1) : result;
}

TZrBool ZrCore_StringStorage_BuildCandidate(
        const SZrStringStorageFacts *facts,
        SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    EZrStringStorageStrategy strategy;

    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
    if (candidate == ZR_NULL) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u, 0u);
        return ZR_FALSE;
    }
    memset(candidate, 0, sizeof(*candidate));
    strategy = ZrCore_StringStorage_SelectStrategy(facts, diagnostic);
    if (strategy == ZR_STRING_STORAGE_STRATEGY_GENERIC) {
        return ZR_FALSE;
    }
    candidate->magic = ZR_STRING_STORAGE_CONTRACT_MAGIC;
    candidate->schemaVersion = ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION;
    candidate->strategy = strategy;
    candidate->flags = facts->flags;
    candidate->byteLength = facts->byteLength;
    candidate->shortStringLimit = facts->shortStringLimit;
    candidate->internDomain = facts->internDomain;
    candidate->ropeDepth = facts->ropeDepth;
    candidate->segmentCount = facts->segmentCount;
    candidate->ropeMaxDepth = facts->ropeMaxDepth;
    candidate->ropeMaxSegments = facts->ropeMaxSegments;
    candidate->measuredCopyBytes = facts->measuredCopyBytes;
    candidate->projectedRopeBytes = facts->projectedRopeBytes;
    candidate->flattenBudgetBytes = facts->flattenBudgetBytes;
    candidate->flattenBudgetMicros = facts->flattenBudgetMicros;
    candidate->declaredEffects = facts->declaredEffects;
    candidate->replacementEffects = facts->replacementEffects;
    candidate->intermediateCount = facts->intermediateCount;
    candidate->maxBuilderBytes = facts->maxBuilderBytes;
    candidate->candidateHash = string_storage_candidate_hash(candidate);
    return ZR_TRUE;
}

TZrBool ZrCore_StringStorage_ValidateCandidate(
        const SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    TZrUInt64 expectedHash;

    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
    if (candidate == ZR_NULL) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u, 0u);
        return ZR_FALSE;
    }
    if (candidate->magic != ZR_STRING_STORAGE_CONTRACT_MAGIC) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_MAGIC, 0u,
                                   ZR_STRING_STORAGE_CONTRACT_MAGIC, candidate->magic);
        return ZR_FALSE;
    }
    if (candidate->schemaVersion != ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_SCHEMA, 0u,
                                   ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION,
                                   candidate->schemaVersion);
        return ZR_FALSE;
    }
    if (candidate->strategy <= ZR_STRING_STORAGE_STRATEGY_GENERIC ||
        candidate->strategy > ZR_STRING_STORAGE_STRATEGY_ROPE ||
        (candidate->flags & ~((TZrUInt32)ZR_STRING_STORAGE_FLAG_KNOWN_MASK)) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSUPPORTED,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u,
                                   (TZrUInt64)candidate->strategy);
        return ZR_FALSE;
    }
    if ((candidate->declaredEffects &
         ~((TZrUInt64)ZR_EXECUTION_EFFECT_KNOWN_MASK)) != 0u ||
        (candidate->replacementEffects &
         ~((TZrUInt64)ZR_EXECUTION_EFFECT_KNOWN_MASK)) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_EFFECTS,
                                   ZR_EXECUTION_EFFECT_KNOWN_MASK,
                                   candidate->declaredEffects |
                                           candidate->replacementEffects);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE) != 0u &&
        candidate->internDomain == 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_DOMAIN,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u, 0u);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN) != 0u &&
        (candidate->flattenBudgetBytes == 0u || candidate->ropeMaxDepth == 0u ||
         candidate->ropeMaxSegments == 0u)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET,
                                   ZR_CONTAINER_STORAGE_FIELD_BUDGET, 1u, 0u);
        return ZR_FALSE;
    }
    if ((candidate->flags & ZR_STRING_STORAGE_FLAG_IMMUTABLE) == 0u ||
        (candidate->flags & (ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED |
                             ZR_STRING_STORAGE_FLAG_ESCAPES |
                             ZR_STRING_STORAGE_FLAG_ADDRESS_ESCAPES |
                             ZR_STRING_STORAGE_FLAG_FFI_VISIBLE |
                             ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE)) != 0u ||
        (candidate->flags & ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY) != 0u) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 0u,
                                   candidate->flags);
        return ZR_FALSE;
    }
    if (candidate->strategy == ZR_STRING_STORAGE_STRATEGY_INTERN &&
        ((candidate->flags & ZR_STRING_STORAGE_FLAG_INTERN_WEAK) == 0u ||
         (candidate->flags & ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE) == 0u)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INTERN_RETENTION,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH, 1u,
                                   candidate->flags);
        return ZR_FALSE;
    }
    if (candidate->strategy == ZR_STRING_STORAGE_STRATEGY_BUILDER &&
        ((candidate->flags & ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT) == 0u ||
         candidate->declaredEffects != candidate->replacementEffects ||
         candidate->intermediateCount == 0u ||
         candidate->maxBuilderBytes == 0u ||
         candidate->byteLength > candidate->maxBuilderBytes)) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_EFFECTS, 1u,
                                   candidate->flags);
        return ZR_FALSE;
    }
    if (candidate->strategy == ZR_STRING_STORAGE_STRATEGY_ROPE &&
        ((candidate->flags & (ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT |
                              ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN)) !=
         (ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT |
          ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN))) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u,
                                   candidate->flags);
        return ZR_FALSE;
    }
    if ((candidate->strategy == ZR_STRING_STORAGE_STRATEGY_SSO ||
         candidate->strategy == ZR_STRING_STORAGE_STRATEGY_INTERN) &&
        candidate->byteLength > candidate->shortStringLimit) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING,
                                   candidate->shortStringLimit,
                                   candidate->byteLength);
        return ZR_FALSE;
    }
    if (candidate->shortStringLimit == 0u ||
        candidate->shortStringLimit > (TZrUInt64)ZR_VM_SHORT_STRING_MAX ||
        (candidate->strategy == ZR_STRING_STORAGE_STRATEGY_INTERN &&
         (candidate->internDomain == 0u ||
          candidate->byteLength > candidate->shortStringLimit)) ||
        (candidate->strategy == ZR_STRING_STORAGE_STRATEGY_ROPE &&
         (candidate->ropeDepth == 0u || candidate->ropeMaxDepth == 0u ||
          candidate->ropeDepth > candidate->ropeMaxDepth ||
          candidate->segmentCount == 0u || candidate->ropeMaxSegments == 0u ||
          candidate->segmentCount > candidate->ropeMaxSegments ||
          candidate->flattenBudgetBytes == 0u ||
          candidate->flattenBudgetBytes < candidate->byteLength ||
          candidate->measuredCopyBytes == 0u ||
          candidate->projectedRopeBytes == 0u ||
          candidate->projectedRopeBytes >= candidate->measuredCopyBytes))) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
                                   ZR_CONTAINER_STORAGE_FIELD_STRING, 1u,
                                   candidate->byteLength);
        return ZR_FALSE;
    }
    expectedHash = string_storage_candidate_hash(candidate);
    if (candidate->candidateHash == 0u || candidate->candidateHash != expectedHash) {
        container_storage_diag_set(diagnostic,
                                   ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH,
                                   ZR_CONTAINER_STORAGE_FIELD_HASH,
                                   expectedHash, candidate->candidateHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
