#ifndef ZR_VM_CORE_HOTPATCH_ROLLBACK_H
#define ZR_VM_CORE_HOTPATCH_ROLLBACK_H

#include "zr_vm_core/hotpatch_generation.h"

typedef enum EZrHotPatchApplyStatus {
    ZR_HOT_PATCH_APPLY_OK = 0,
    ZR_HOT_PATCH_APPLY_INVALID_ARGUMENT,
    ZR_HOT_PATCH_APPLY_ALREADY_APPLIED,
    ZR_HOT_PATCH_APPLY_ID_COLLISION,
    ZR_HOT_PATCH_APPLY_CAPACITY,
    ZR_HOT_PATCH_APPLY_PUBLISH_FAILED,
    ZR_HOT_PATCH_APPLY_ROLLBACK_NOT_FOUND,
    ZR_HOT_PATCH_APPLY_ROLLBACK_FAILED
} EZrHotPatchApplyStatus;

typedef struct SZrHotPatchRegistryEntry {
    TZrUInt64 patchId;
    TZrUInt64 contentHash;
    TZrUInt64 generation;
    TZrUInt32 state;
} SZrHotPatchRegistryEntry;

typedef struct SZrHotPatchRegistry {
    SZrHotPatchRegistryEntry *entries;
    TZrUInt32 capacity;
    TZrUInt32 count;
} SZrHotPatchRegistry;

typedef struct SZrHotPatchApplyDiagnostic {
    EZrHotPatchApplyStatus status;
    TZrUInt64 patchId;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
    TZrUInt64 generation;
} SZrHotPatchApplyDiagnostic;

ZR_CORE_API EZrHotPatchApplyStatus ZrCore_HotPatch_ApplyValidated(
        SZrHotPatchGenerationManager *manager,
        SZrHotPatchRegistry *registry,
        const SZrValidatedHotPatch *validated,
        TZrUInt64 moduleHash,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchApplyDiagnostic *diagnostic);

ZR_CORE_API EZrHotPatchApplyStatus ZrCore_HotPatch_Rollback(
        SZrHotPatchGenerationManager *manager,
        TZrUInt64 targetGeneration,
        SZrHotPatchGenerationHandle *outHandle,
        SZrHotPatchApplyDiagnostic *diagnostic);

ZR_CORE_API const TZrChar *ZrCore_HotPatch_ApplyStatusName(EZrHotPatchApplyStatus status);

#endif
