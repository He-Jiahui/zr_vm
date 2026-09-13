#ifndef ZR_VM_CORE_CAPABILITY_MANIFEST_H
#define ZR_VM_CORE_CAPABILITY_MANIFEST_H

#include "zr_vm_core/artifact_exec_ir.h"

#define ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION ((TZrUInt32)1u)

typedef enum EZrHotPatchCapabilityStatus {
    ZR_HOT_PATCH_OK = 0,
    ZR_HOT_PATCH_INVALID_ARGUMENT,
    ZR_HOT_PATCH_ARTIFACT_INVALID,
    ZR_HOT_PATCH_SIGNATURE_REJECTED,
    ZR_HOT_PATCH_BASE_MISMATCH,
    ZR_HOT_PATCH_ABI_MISMATCH,
    ZR_HOT_PATCH_CAPABILITY_ESCALATION,
    ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,
    ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN,
    ZR_HOT_PATCH_IMPORT_FORBIDDEN,
    ZR_HOT_PATCH_PROFILE_MISMATCH,
    ZR_HOT_PATCH_LIMIT
} EZrHotPatchCapabilityStatus;

typedef struct SZrHotPatchCapabilityRequirement {
    TZrUInt32 token;
    TZrUInt64 requiredBits;
    TZrUInt32 sourceOffset;
    TZrUInt32 reserved;
} SZrHotPatchCapabilityRequirement;

typedef struct SZrHotPatchCapabilityManifest {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt64 patchId;
    TZrUInt64 contentHash;
    TZrUInt64 baseModuleHash;
    TZrUInt64 publicContractHash;
    TZrUInt64 requiredCapabilities;
    TZrUInt32 targetAbiVersion;
    TZrUInt32 targetProfile;
    TZrUInt32 requirementCount;
    const SZrHotPatchCapabilityRequirement *requirements;
} SZrHotPatchCapabilityManifest;

#define ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE ((TZrUInt32)1u << 0u)
#define ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT ((TZrUInt32)1u << 1u)
#define ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT ((TZrUInt32)1u << 2u)
#define ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE ((TZrUInt32)1u << 3u)
#define ZR_HOT_PATCH_FLAG_KNOWN_MASK \
    (ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE | ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT | \
     ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT | ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE)

typedef struct SZrHotPatchValidationInput {
    const SZrArtifactExecIrView *artifact;
    const SZrHotPatchCapabilityManifest *manifest;
    TZrUInt64 loadedBaseModuleHash;
    TZrUInt64 loadedPublicContractHash;
    TZrUInt32 hostAbiVersion;
    TZrUInt32 hostProfile;
    TZrUInt64 hostAllowedCapabilities;
    TZrUInt64 expectedPatchId;
    TZrUInt64 expectedContentHash;
    const TZrByte *signature;
    TZrUInt32 signatureLength;
} SZrHotPatchValidationInput;

typedef TZrBool (*FZrHotPatchVerifySignature)(const TZrByte *content,
                                               TZrUInt32 contentLength,
                                               const TZrByte *signature,
                                               TZrUInt32 signatureLength,
                                               TZrPtr userData);

typedef struct SZrValidatedHotPatch {
    const SZrArtifactExecIrView *artifact;
    const SZrHotPatchCapabilityManifest *manifest;
    TZrUInt64 contentHash;
    TZrUInt64 requiredCapabilities;
    TZrUInt64 validationPolicyHash;
    TZrUInt32 targetProfile;
    TZrBool signatureVerified;
    TZrBool immutableContent;
} SZrValidatedHotPatch;

typedef struct SZrHotPatchDiagnostic {
    EZrHotPatchCapabilityStatus status;
    TZrUInt32 token;
    TZrUInt32 sourceOffset;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrHotPatchDiagnostic;

ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_Validate(
        const SZrHotPatchValidationInput *input,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatch *validated,
        SZrHotPatchDiagnostic *diagnostic);
ZR_CORE_API TZrUInt64 ZrCore_HotPatch_ComputePolicyHash(
        const SZrHotPatchValidationInput *input);
ZR_CORE_API const TZrChar *ZrCore_HotPatch_StatusName(
        EZrHotPatchCapabilityStatus status);

#endif
