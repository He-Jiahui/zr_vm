#ifndef ZR_VM_CORE_HOTPATCH_PROFILE_H
#define ZR_VM_CORE_HOTPATCH_PROFILE_H

#include "zr_vm_core/artifact_exec_ir.h"

typedef enum EZrHotPatchRestrictedProfile {
    ZR_HOT_PATCH_PROFILE_IOS_INTERPRETER = 1,
    ZR_HOT_PATCH_PROFILE_WASM_INTERPRETER = 2
} EZrHotPatchRestrictedProfile;

typedef enum EZrHotPatchRestrictedStatus {
    ZR_HOT_PATCH_RESTRICTED_OK = 0,
    ZR_HOT_PATCH_RESTRICTED_INVALID_ARGUMENT,
    ZR_HOT_PATCH_RESTRICTED_PROFILE_MISMATCH,
    ZR_HOT_PATCH_RESTRICTED_SECTION_FORBIDDEN,
    ZR_HOT_PATCH_RESTRICTED_MACHINE_CODE,
    ZR_HOT_PATCH_RESTRICTED_IMPORT
} EZrHotPatchRestrictedStatus;

typedef struct SZrHotPatchRestrictedDiagnostic {
    EZrHotPatchRestrictedStatus status;
    TZrUInt32 sectionKind;
    TZrUInt32 sectionIndex;
} SZrHotPatchRestrictedDiagnostic;

ZR_CORE_API EZrHotPatchRestrictedStatus ZrCore_HotPatch_ValidateRestrictedProfile(
        const SZrArtifactExecIrView *artifact,
        TZrUInt32 profile,
        SZrHotPatchRestrictedDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_HotPatch_RestrictedStatusName(
        EZrHotPatchRestrictedStatus status);

#endif
