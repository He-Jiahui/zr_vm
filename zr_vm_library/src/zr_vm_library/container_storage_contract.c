#include "zr_vm_library/container_storage_contract.h"

void ZrLibrary_ContainerStorage_DiagnosticClear(
        SZrContainerStorageDiagnostic *diagnostic) {
    ZrCore_ContainerStorage_DiagnosticClear(diagnostic);
}

const TZrChar *ZrLibrary_ContainerStorage_DiagnosticName(
        EZrContainerStorageDiagnosticCode code) {
    return ZrCore_ContainerStorage_DiagnosticName(code);
}

void ZrLibrary_CompactMapCandidate_Init(
        SZrCompactMapCandidate *candidate) {
    ZrCore_CompactMapCandidate_Init(candidate);
}

TZrBool ZrLibrary_CompactMapCandidate_Finalize(
        SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_CompactMapCandidate_Finalize(candidate, diagnostic);
}

TZrBool ZrLibrary_CompactMapCandidate_Validate(
        const SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_CompactMapCandidate_Validate(candidate, diagnostic);
}

TZrBool ZrLibrary_CompactMap_CanCacheHash(
        const SZrCompactMapHash *hash) {
    return ZrCore_CompactMap_CanCacheHash(hash);
}

TZrUInt64 ZrLibrary_CompactMap_HashBytes(
        const SZrCompactMapHash *hash,
        const TZrByte *data,
        TZrSize length) {
    return ZrCore_CompactMap_HashBytes(hash, data, length);
}

TZrBool ZrLibrary_CompactMap_HashWitnessEqual(
        const SZrCompactMapHash *left,
        const SZrCompactMapHash *right) {
    return ZrCore_CompactMap_HashWitnessEqual(left, right);
}

TZrBool ZrLibrary_CompactMap_ProbeNeedsEquality(
        TZrUInt64 storedHash,
        TZrUInt64 incomingHash) {
    return ZrCore_CompactMap_ProbeNeedsEquality(storedHash, incomingHash);
}

TZrSize ZrLibrary_CompactMap_BucketIndex(
        TZrUInt64 hash,
        TZrSize bucketCount) {
    return ZrCore_CompactMap_BucketIndex(hash, bucketCount);
}

void ZrLibrary_StringStorageFacts_Init(SZrStringStorageFacts *facts) {
    ZrCore_StringStorageFacts_Init(facts);
}

TZrBool ZrLibrary_StringStorage_Validate(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_Validate(facts, diagnostic);
}

TZrBool ZrLibrary_StringStorage_CanUseSso(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_CanUseSso(facts, diagnostic);
}

TZrBool ZrLibrary_StringStorage_CanUseIntern(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_CanUseIntern(facts, diagnostic);
}

TZrBool ZrLibrary_StringStorage_CanUseBuilder(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_CanUseBuilder(facts, diagnostic);
}

TZrBool ZrLibrary_StringStorage_CanUseRope(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_CanUseRope(facts, diagnostic);
}

EZrStringStorageStrategy ZrLibrary_StringStorage_SelectStrategy(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_SelectStrategy(facts, diagnostic);
}

TZrBool ZrLibrary_StringStorage_BuildCandidate(
        const SZrStringStorageFacts *facts,
        SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_BuildCandidate(facts, candidate, diagnostic);
}

TZrBool ZrLibrary_StringStorage_ValidateCandidate(
        const SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic) {
    return ZrCore_StringStorage_ValidateCandidate(candidate, diagnostic);
}
