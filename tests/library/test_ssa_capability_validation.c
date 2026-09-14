#include "zr_vm_core/capability_manifest.h"
#include "zr_vm_core/hotpatch_capability.h"

#include <assert.h>
#include <string.h>

static TZrBool verify(const TZrByte *content, TZrUInt32 length,
                      const TZrByte *signature, TZrUInt32 signatureLength,
                      TZrPtr userData) {
    (void)userData;
    return content != ZR_NULL && length == 4u && signature != ZR_NULL &&
           signatureLength == 3u && signature[0] == 0xa5u;
}

int main(void) {
    TZrByte bytes[4] = {1u, 2u, 3u, 4u};
    TZrByte signature[3] = {0xa5u, 0x5au, 0x01u};
    SZrArtifactExecIrView artifact;
    SZrHotPatchCapabilityRequirement requirement = {7u, UINT64_C(0x03), 42u, 0u};
    SZrHotPatchCapabilityManifest manifest;
    SZrHotPatchValidationInput input;
    SZrValidatedHotPatch validated;
    SZrHotPatchDiagnostic diagnostic;

    memset(&artifact, 0, sizeof(artifact));
    artifact.abiVersion = 16u;
    artifact.moduleHash = 99u;
    artifact.buffer = bytes;
    artifact.bufferLength = sizeof(bytes);
    memset(&manifest, 0, sizeof(manifest));
    manifest.schemaVersion = ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION;
    manifest.patchId = 1u;
    manifest.contentHash = ZrCore_ArtifactExecIr_HashBytes(bytes, sizeof(bytes));
    manifest.baseModuleHash = 99u;
    manifest.publicContractHash = 123u;
    manifest.targetAbiVersion = 16u;
    manifest.targetProfile = 2u;
    manifest.requirementCount = 1u;
    manifest.requirements = &requirement;
    memset(&input, 0, sizeof(input));
    input.artifact = &artifact;
    input.manifest = &manifest;
    input.loadedBaseModuleHash = 99u;
    input.loadedPublicContractHash = 123u;
    input.hostAbiVersion = 16u;
    input.hostProfile = 2u;
    input.hostAllowedCapabilities = UINT64_C(0x03);
    input.expectedPatchId = 1u;
    input.expectedContentHash = manifest.contentHash;
    input.signature = signature;
    input.signatureLength = sizeof(signature);

    memset(&validated, 0, sizeof(validated));
    assert(ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                    &diagnostic) == ZR_HOT_PATCH_OK);
    assert(validated.signatureVerified && validated.immutableContent);
    assert(validated.requiredCapabilities == UINT64_C(0x03));

    {
        TZrUInt64 required = 0u;
        assert(ZrCore_HotPatch_ComputeRequiredCapabilities(
                       &manifest, UINT64_C(0x03), &required, &diagnostic) ==
               ZR_HOT_PATCH_OK);
        assert(required == UINT64_C(0x03));
    }

    manifest.requiredCapabilities = UINT64_C(0x04);
    validated.contentHash = 777u;
    assert(ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                    &diagnostic) ==
           ZR_HOT_PATCH_CAPABILITY_ESCALATION);
    {
        TZrUInt64 required = 0u;
        assert(ZrCore_HotPatch_ComputeCapabilityClosure(
                       &requirement, 1u, 0u, UINT64_C(0x01), &required,
                       &diagnostic) == ZR_HOT_PATCH_CAPABILITY_ESCALATION);
        assert(required == 0u);
    }
    assert(validated.contentHash == 0u);
    manifest.requiredCapabilities = 0u;
    manifest.flags = ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE;
    assert(ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                    &diagnostic) ==
           ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN);
    manifest.flags = 0u;
    signature[0] = 0u;
    assert(ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                    &diagnostic) == ZR_HOT_PATCH_SIGNATURE_REJECTED);
    return 0;
}
