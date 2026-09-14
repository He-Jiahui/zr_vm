#ifndef ZR_VM_CORE_HOTPATCH_GENERATION_H
#define ZR_VM_CORE_HOTPATCH_GENERATION_H

#include "zr_vm_core/capability_manifest.h"

#include <stdatomic.h>

typedef enum EZrHotPatchGenerationStatus {
    ZR_HOT_PATCH_GENERATION_OK = 0,
    ZR_HOT_PATCH_GENERATION_INVALID_ARGUMENT,
    ZR_HOT_PATCH_GENERATION_CAPACITY,
    ZR_HOT_PATCH_GENERATION_NOT_PREPARED,
    ZR_HOT_PATCH_GENERATION_STALE_LINK,
    ZR_HOT_PATCH_GENERATION_INVALID_STATE,
    ZR_HOT_PATCH_GENERATION_OVERFLOW
} EZrHotPatchGenerationStatus;

typedef enum EZrHotPatchVersionState {
    ZR_HOT_PATCH_VERSION_FREE = 0,
    ZR_HOT_PATCH_VERSION_PREPARED,
    ZR_HOT_PATCH_VERSION_ACTIVE,
    ZR_HOT_PATCH_VERSION_RETIRED
} EZrHotPatchVersionState;

typedef struct SZrHotPatchVersionRecord {
    TZrUInt64 generation;
    TZrUInt64 moduleHash;
    TZrUInt64 contentHash;
    TZrUInt64 publicContractHash;
    TZrUInt32 targetProfile;
    TZrUInt32 state;
    atomic_uint_fast32_t leaseCount;
} SZrHotPatchVersionRecord;

typedef struct SZrHotPatchGenerationManager {
    atomic_flag lock;
    _Atomic(SZrHotPatchVersionRecord *) active;
    atomic_uint_fast64_t nextGeneration;
    SZrHotPatchVersionRecord *records;
    TZrUInt32 capacity;
    TZrUInt32 count;
} SZrHotPatchGenerationManager;

typedef struct SZrHotPatchGenerationHandle {
    SZrHotPatchVersionRecord *record;
    TZrUInt64 generation;
    TZrBool leased;
} SZrHotPatchGenerationHandle;

typedef struct SZrHotPatchVersionView {
    TZrUInt64 generation;
    TZrUInt64 moduleHash;
    TZrUInt64 contentHash;
    TZrUInt64 publicContractHash;
    TZrUInt32 targetProfile;
    EZrHotPatchVersionState state;
    TZrUInt32 leaseCount;
} SZrHotPatchVersionView;

typedef struct SZrHotPatchGenerationDiagnostic {
    EZrHotPatchGenerationStatus status;
    TZrUInt64 expectedGeneration;
    TZrUInt64 actualGeneration;
    TZrUInt32 leaseCount;
} SZrHotPatchGenerationDiagnostic;

ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_GenerationManager_Init(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchVersionRecord *records,
        TZrUInt32 capacity,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_HotPatch_GenerationManager_Deinit(
        SZrHotPatchGenerationManager *manager);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Prepare(
        SZrHotPatchGenerationManager *manager,
        const SZrValidatedHotPatch *validated,
        TZrUInt64 moduleHash,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Publish(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *prepared,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_AcquireActive(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Acquire(
        SZrHotPatchGenerationManager *manager,
        TZrUInt64 generation,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Resolve(
        const SZrHotPatchGenerationManager *manager,
        const SZrHotPatchGenerationHandle *handle,
        SZrHotPatchVersionView *outView,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_Release(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchGenerationHandle *handle,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API EZrHotPatchGenerationStatus ZrCore_HotPatch_Generation_CollectRetired(
        SZrHotPatchGenerationManager *manager,
        TZrUInt32 *outCollected,
        SZrHotPatchGenerationDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_HotPatch_Generation_StatusName(
        EZrHotPatchGenerationStatus status);

#endif
