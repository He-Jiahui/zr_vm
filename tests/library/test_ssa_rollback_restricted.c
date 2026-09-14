#include "zr_vm_core/hotpatch_rollback.h"
#include "zr_vm_core/hotpatch_profile.h"

#include <assert.h>
#include <string.h>

int main(void) {
    SZrHotPatchVersionRecord records[4];
    SZrHotPatchGenerationManager manager;
    SZrHotPatchGenerationHandle h1, h2;
    SZrHotPatchGenerationDiagnostic gd;
    SZrHotPatchRegistryEntry entries[4] = {0};
    SZrHotPatchRegistry registry = {entries, 4u, 0u};
    SZrHotPatchCapabilityManifest manifest = {1u, 0u, 7u, 55u, 9u, 11u, 0u, 16u, 1u, 0u, ZR_NULL};
    TZrByte bytes[1] = {0x42u};
    SZrArtifactExecIrView artifact;
    SZrValidatedHotPatch validated;
    SZrHotPatchApplyDiagnostic ad;
    SZrHotPatchRestrictedDiagnostic rd;
    memset(&artifact, 0, sizeof(artifact)); artifact.buffer = bytes; artifact.bufferLength = sizeof(bytes);
    validated.artifact = &artifact; validated.manifest = &manifest; validated.contentHash = 55u;
    validated.signatureVerified = ZR_TRUE; validated.immutableContent = ZR_TRUE; validated.targetProfile = 1u;
    assert(ZrCore_HotPatch_GenerationManager_Init(&manager, records, 4u, &gd) == ZR_HOT_PATCH_GENERATION_OK);
    assert(ZrCore_HotPatch_ApplyValidated(&manager, &registry, &validated, 9u, &h1, &ad) == ZR_HOT_PATCH_APPLY_OK);
    assert(entries[0].generation == h1.generation);
    assert(ZrCore_HotPatch_ApplyValidated(&manager, &registry, &validated, 9u, &h2, &ad) == ZR_HOT_PATCH_APPLY_ALREADY_APPLIED);
    manifest.contentHash = 56u; validated.contentHash = 56u;
    assert(ZrCore_HotPatch_ApplyValidated(&manager, &registry, &validated, 9u, &h2, &ad) == ZR_HOT_PATCH_APPLY_ID_COLLISION);
    manifest.contentHash = 55u; validated.contentHash = 55u;
    assert(ZrCore_HotPatch_Rollback(&manager, h1.generation, &h2, &ad) == ZR_HOT_PATCH_APPLY_OK);
    assert(h2.generation != h1.generation);
    assert(ZrCore_HotPatch_ValidateRestrictedProfile(&artifact, ZR_HOT_PATCH_PROFILE_IOS_INTERPRETER, &rd) == ZR_HOT_PATCH_RESTRICTED_OK);
    artifact.sectionCount = 1u; artifact.sections[0].kind = ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS;
    assert(ZrCore_HotPatch_ValidateRestrictedProfile(&artifact, ZR_HOT_PATCH_PROFILE_WASM_INTERPRETER, &rd) == ZR_HOT_PATCH_RESTRICTED_SECTION_FORBIDDEN);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
    return 0;
}
