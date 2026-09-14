#ifndef ZR_VM_LIBRARY_CONTAINER_STORAGE_CONTRACT_H
#define ZR_VM_LIBRARY_CONTAINER_STORAGE_CONTRACT_H

/*
 * Library-facing gate for the core map/string storage witness.  The library
 * does not own managed entries or string bytes; it only republishes the
 * scalar contract after the caller has crossed the public library boundary.
 */
#include "zr_vm_library/conf.h"
#include "zr_vm_core/container_storage_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

ZR_LIBRARY_API void ZrLibrary_ContainerStorage_DiagnosticClear(
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API const TZrChar *ZrLibrary_ContainerStorage_DiagnosticName(
        EZrContainerStorageDiagnosticCode code);
ZR_LIBRARY_API void ZrLibrary_CompactMapCandidate_Init(
        SZrCompactMapCandidate *candidate);
ZR_LIBRARY_API TZrBool ZrLibrary_CompactMapCandidate_Finalize(
        SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_CompactMapCandidate_Validate(
        const SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_CompactMap_CanCacheHash(
        const SZrCompactMapHash *hash);
ZR_LIBRARY_API TZrUInt64 ZrLibrary_CompactMap_HashBytes(
        const SZrCompactMapHash *hash,
        const TZrByte *data,
        TZrSize length);
ZR_LIBRARY_API TZrBool ZrLibrary_CompactMap_HashWitnessEqual(
        const SZrCompactMapHash *left,
        const SZrCompactMapHash *right);
ZR_LIBRARY_API TZrBool ZrLibrary_CompactMap_ProbeNeedsEquality(
        TZrUInt64 storedHash,
        TZrUInt64 incomingHash);
ZR_LIBRARY_API TZrSize ZrLibrary_CompactMap_BucketIndex(
        TZrUInt64 hash,
        TZrSize bucketCount);
ZR_LIBRARY_API void ZrLibrary_StringStorageFacts_Init(
        SZrStringStorageFacts *facts);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_Validate(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_CanUseSso(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_CanUseIntern(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_CanUseBuilder(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_CanUseRope(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API EZrStringStorageStrategy ZrLibrary_StringStorage_SelectStrategy(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_BuildCandidate(
        const SZrStringStorageFacts *facts,
        SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
ZR_LIBRARY_API TZrBool ZrLibrary_StringStorage_ValidateCandidate(
        const SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);

/* Descriptive aliases used by provider code. */
#define ZrLibrary_ContainerStorage_ValidateCompactMap \
    ZrLibrary_CompactMapCandidate_Validate
#define ZrLibrary_ContainerStorage_SelectString \
    ZrLibrary_StringStorage_SelectStrategy
#define ZrLibrary_StringStorage_CanUseSSO \
    ZrLibrary_StringStorage_CanUseSso

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_LIBRARY_CONTAINER_STORAGE_CONTRACT_H */
