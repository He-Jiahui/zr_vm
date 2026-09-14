#include "zr_vm_core/container_storage_contract.h"
#include "zr_vm_library/container_storage_contract.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_map_contract_and_stable_hash(void) {
    SZrCompactMapCandidate candidate;
    SZrContainerStorageDiagnostic diagnostic;
    SZrCompactMapHash otherHash;
    TZrUInt64 first;
    TZrUInt64 second;

    ZrCore_CompactMapCandidate_Init(&candidate);
    candidate.hash.seed = UINT64_C(0x0102030405060708);
    candidate.hash.domain = UINT64_C(0x1112131415161718);
    candidate.hash.version = 1u;
    candidate.hash.flags = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
                           ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL |
                           ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE;
    candidate.flags = ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION |
                      ZR_COMPACT_MAP_FLAG_GC_ROOTS |
                      ZR_COMPACT_MAP_FLAG_CACHE_HASH;
    candidate.layout.entrySize = 32u;
    candidate.layout.entryAlignment = 8u;
    candidate.layout.bucketCount = 16u;
    candidate.layout.maxLoadNumerator = 3u;
    candidate.layout.maxLoadDenominator = 4u;
    candidate.layout.hashOffset = 0u;
    candidate.layout.keyOffset = 8u;
    candidate.layout.valueOffset = 16u;
    candidate.layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_MARK;
    candidate.layout.iterationPolicy = ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER;
    candidate.layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED;

    assert(ZrCore_CompactMapCandidate_Finalize(&candidate, &diagnostic));
    assert(ZrCore_CompactMapCandidate_Validate(&candidate, &diagnostic));
    assert(ZrCore_CompactMap_CanCacheHash(&candidate.hash));
    first = ZrCore_CompactMap_HashBytes(&candidate.hash,
                                        (const TZrByte *)"collision", 9u);
    second = ZrCore_CompactMap_HashBytes(&candidate.hash,
                                         (const TZrByte *)"collision", 9u);
    assert(first == second);
    assert(ZrCore_CompactMap_HashBytes(&candidate.hash, ZR_NULL, 0u) != 0u);
    otherHash = candidate.hash;
    otherHash.domain += 1u;
    assert(first != ZrCore_CompactMap_HashBytes(&otherHash,
                                                (const TZrByte *)"collision",
                                                9u));
    assert(ZrCore_CompactMap_ProbeNeedsEquality(first, first));
    assert(!ZrCore_CompactMap_ProbeNeedsEquality(first, first + 1u));
    assert(ZrCore_CompactMap_BucketIndex(first, candidate.layout.bucketCount) <
           candidate.layout.bucketCount);
}

static void test_map_contract_rejects_observable_equality(void) {
    SZrCompactMapCandidate candidate;
    SZrContainerStorageDiagnostic diagnostic;

    ZrCore_CompactMapCandidate_Init(&candidate);
    candidate.hash.seed = 1u;
    candidate.hash.domain = 2u;
    candidate.hash.version = 1u;
    candidate.hash.flags = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
                           ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY;
    candidate.layout.entrySize = 32u;
    candidate.layout.entryAlignment = 8u;
    candidate.layout.bucketCount = 8u;
    candidate.layout.maxLoadNumerator = 1u;
    candidate.layout.maxLoadDenominator = 2u;
    candidate.layout.keyOffset = 8u;
    candidate.layout.valueOffset = 16u;
    candidate.layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_MARK;
    candidate.layout.iterationPolicy = ZR_COMPACT_MAP_ITERATION_BUCKET_ORDER;
    candidate.layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_BORROWED;
    assert(!ZrCore_CompactMapCandidate_Validate(&candidate, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY);
}

static void test_map_contract_requires_total_equality_for_cached_hash(void) {
    SZrCompactMapCandidate candidate;
    SZrContainerStorageDiagnostic diagnostic;

    ZrCore_CompactMapCandidate_Init(&candidate);
    candidate.hash.seed = 3u;
    candidate.hash.domain = 5u;
    candidate.hash.version = 1u;
    candidate.hash.flags = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
                           ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE;
    candidate.flags = ZR_COMPACT_MAP_FLAG_CACHE_HASH;
    candidate.layout.entrySize = 32u;
    candidate.layout.entryAlignment = 8u;
    candidate.layout.bucketCount = 8u;
    candidate.layout.maxLoadNumerator = 1u;
    candidate.layout.maxLoadDenominator = 2u;
    candidate.layout.hashOffset = 0u;
    candidate.layout.keyOffset = 8u;
    candidate.layout.valueOffset = 16u;
    candidate.layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_NONE;
    candidate.layout.iterationPolicy = ZR_COMPACT_MAP_ITERATION_BUCKET_ORDER;
    candidate.layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_NONE;
    assert(!ZrCore_CompactMapCandidate_Finalize(&candidate, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH);
}

static void test_map_contract_preserves_iteration_and_ownership_obligations(void) {
    SZrCompactMapCandidate candidate;
    SZrContainerStorageDiagnostic diagnostic;

    ZrCore_CompactMapCandidate_Init(&candidate);
    candidate.hash.seed = 7u;
    candidate.hash.domain = 11u;
    candidate.hash.version = 1u;
    candidate.hash.flags = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
                           ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
                           ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL |
                           ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE;
    candidate.flags = ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION |
                      ZR_COMPACT_MAP_FLAG_GC_ROOTS |
                      ZR_COMPACT_MAP_FLAG_CACHE_HASH;
    candidate.layout.entrySize = 32u;
    candidate.layout.entryAlignment = 8u;
    candidate.layout.bucketCount = 4u;
    candidate.layout.maxLoadNumerator = 1u;
    candidate.layout.maxLoadDenominator = 2u;
    candidate.layout.hashOffset = 0u;
    candidate.layout.keyOffset = 8u;
    candidate.layout.valueOffset = 16u;
    candidate.layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT;
    candidate.layout.iterationPolicy = ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER;
    candidate.layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED;
    assert(!ZrCore_CompactMapCandidate_Finalize(&candidate, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION ||
           diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE);

    candidate.layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_MARK;
    candidate.layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_BORROWED;
    assert(!ZrCore_CompactMapCandidate_Finalize(&candidate, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP);

    candidate.layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED;
    candidate.layout.keyOffset = 4u;
    assert(!ZrCore_CompactMapCandidate_Finalize(&candidate, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT);
}

static void test_string_candidates_are_conservative(void) {
    SZrStringStorageFacts facts;
    SZrContainerStorageDiagnostic diagnostic;

    ZrCore_StringStorageFacts_Init(&facts);
    facts.flags = ZR_STRING_STORAGE_FLAG_IMMUTABLE |
                  ZR_STRING_STORAGE_FLAG_INTERN_WEAK |
                  ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE |
                  ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT;
    facts.byteLength = 4u;
    facts.shortStringLimit = 127u;
    facts.internDomain = 9u;
    facts.declaredEffects = 0u;
    facts.replacementEffects = 0u;
    facts.intermediateCount = 2u;
    facts.maxBuilderBytes = 1024u;
    assert(ZrCore_StringStorage_Validate(&facts, &diagnostic));
    assert(ZrCore_StringStorage_CanUseSso(&facts, &diagnostic));
    assert(ZrCore_StringStorage_CanUseIntern(&facts, &diagnostic));
    facts.byteLength = facts.shortStringLimit + 1u;
    assert(!ZrCore_StringStorage_CanUseIntern(&facts, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT);
    facts.byteLength = 4u;
    assert(ZrCore_StringStorage_CanUseBuilder(&facts, &diagnostic));
    assert(ZrCore_StringStorage_SelectStrategy(&facts, &diagnostic) ==
           ZR_STRING_STORAGE_STRATEGY_INTERN);
    {
        SZrStringStorageCandidate candidate;
        assert(ZrCore_StringStorage_BuildCandidate(&facts, &candidate, &diagnostic));
        assert(candidate.strategy == ZR_STRING_STORAGE_STRATEGY_INTERN);
        assert(ZrCore_StringStorage_ValidateCandidate(&candidate, &diagnostic));
        candidate.candidateHash ^= 1u;
        assert(!ZrCore_StringStorage_ValidateCandidate(&candidate, &diagnostic));
        assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH);
    }

    ZrCore_StringStorageFacts_Init(&facts);
    facts.flags = ZR_STRING_STORAGE_FLAG_IMMUTABLE;
    facts.byteLength = (TZrUInt64)strlen("\xE4\xB8\xAD\xE6\x96\x87");
    assert(ZrCore_StringStorage_CanUseSso(&facts, &diagnostic));
    assert(ZrCore_StringStorage_SelectStrategy(&facts, &diagnostic) ==
           ZR_STRING_STORAGE_STRATEGY_SSO);

    facts.byteLength = 0u;
    assert(ZrCore_StringStorage_CanUseSso(&facts, &diagnostic));

    facts.flags |= ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED;
    assert(!ZrCore_StringStorage_CanUseBuilder(&facts, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_IDENTITY_OBSERVED);

    facts.flags = ZR_STRING_STORAGE_FLAG_IMMUTABLE |
                  ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE |
                  ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT;
    facts.intermediateCount = 1u;
    facts.maxBuilderBytes = 64u;
    assert(!ZrCore_StringStorage_CanUseSso(&facts, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH);

    facts.flags = ZR_STRING_STORAGE_FLAG_IMMUTABLE |
                  ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT;
    facts.byteLength = 8u;
    facts.intermediateCount = 1u;
    facts.maxBuilderBytes = 4u;
    assert(!ZrCore_StringStorage_CanUseBuilder(&facts, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET);

    ZrCore_StringStorageFacts_Init(&facts);
    facts.flags = ZR_STRING_STORAGE_FLAG_IMMUTABLE |
                  ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT |
                  ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN;
    facts.byteLength = 4096u;
    facts.ropeDepth = 2u;
    facts.segmentCount = 4u;
    facts.measuredCopyBytes = 8192u;
    facts.projectedRopeBytes = 4096u;
    facts.flattenBudgetBytes = 8192u;
    facts.ropeMaxDepth = 8u;
    facts.ropeMaxSegments = 16u;
    assert(ZrCore_StringStorage_CanUseRope(&facts, &diagnostic));
    facts.flattenBudgetBytes = 1u;
    assert(!ZrCore_StringStorage_CanUseRope(&facts, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET);

    facts.flattenBudgetBytes = 8192u;
    facts.projectedRopeBytes = facts.measuredCopyBytes;
    assert(!ZrCore_StringStorage_CanUseRope(&facts, &diagnostic));
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED);
}

static void test_library_boundary_republishes_scalar_contract(void) {
    SZrCompactMapHash hash = { 1u, 2u, 1u,
        ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
        ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
        ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
        ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL |
        ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE };
    const TZrByte bytes[] = { 9u, 8u, 7u };
    SZrContainerStorageDiagnostic diagnostic;
    assert(ZrLibrary_CompactMap_CanCacheHash(&hash));
    assert(ZrLibrary_CompactMap_HashBytes(&hash, bytes, sizeof(bytes)) ==
           ZrCore_CompactMap_HashBytes(&hash, bytes, sizeof(bytes)));
    ZrLibrary_ContainerStorage_DiagnosticClear(&diagnostic);
    assert(diagnostic.code == ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE);
}

int main(void) {
    test_map_contract_and_stable_hash();
    test_map_contract_rejects_observable_equality();
    test_map_contract_requires_total_equality_for_cached_hash();
    test_map_contract_preserves_iteration_and_ownership_obligations();
    test_string_candidates_are_conservative();
    test_library_boundary_republishes_scalar_contract();
    return 0;
}
