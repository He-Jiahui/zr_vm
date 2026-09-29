#include "zr_vm_core/capability_manifest.h"
#include "zr_vm_core/hotpatch_capability.h"
#include "zr_vm_core/hotpatch_rollback.h"

#include <stdio.h>
#include <string.h>

static TZrUInt32 g_testFailureCount = 0u;

/* Unlike assert, TEST_CHECK evaluates its expression in NDEBUG builds too. */
#define TEST_CHECK(condition) \
    do { \
        if (!(condition)) { \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                          __FILE__, __LINE__, #condition); \
            ++g_testFailureCount; \
        } \
    } while (0)

static SZrHotPatchCapabilityRequirement g_maxRequirements[
        ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS];

/* 测试专用签名桩，只验证输入形状与哨兵字节，不提供加密真实性保证。 */
static TZrBool verify(const TZrByte *content, TZrUInt32 length,
                      const TZrByte *signature, TZrUInt32 signatureLength,
                      TZrPtr userData) {
    (void)userData;
    return content != ZR_NULL && length == 4u && signature != ZR_NULL &&
           signatureLength == 3u && signature[0] == 0xa5u;
}

static TZrBool verify_and_mutate(const TZrByte *content, TZrUInt32 length,
                                 const TZrByte *signature,
                                 TZrUInt32 signatureLength,
                                 TZrPtr userData) {
    TZrByte *mutableBytes = (TZrByte *)userData;
    if (!verify(content, length, signature, signatureLength, ZR_NULL) ||
        mutableBytes == ZR_NULL) return ZR_FALSE;
    mutableBytes[1] ^= 0x80u;
    return ZR_TRUE;
}

static TZrBool count_and_reject_signature(
        const TZrByte *content, TZrUInt32 length,
        const TZrByte *signature, TZrUInt32 signatureLength,
        TZrPtr userData) {
    TZrUInt32 *callCount = (TZrUInt32 *)userData;
    (void)content;
    (void)length;
    (void)signature;
    (void)signatureLength;
    if (callCount != ZR_NULL) ++*callCount;
    return ZR_FALSE;
}

static void expect_apply_rejects_sequential_content_mutation(
        TZrBool mutateDuringVerification) {
    TZrByte content[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    TZrByte signature[3] = {0xa5u, 0x5au, 0x01u};
    SZrArtifactExecIrView artifact;
    SZrHotPatchCapabilityRequirement requirement = {19u, UINT64_C(0x01), 5u, 0u};
    SZrHotPatchCapabilityManifest manifest;
    SZrHotPatchValidationInput input;
    SZrValidatedHotPatch validated;
    SZrHotPatchDiagnostic validationDiagnostic;
    SZrHotPatchVersionRecord records[1];
    SZrHotPatchGenerationManager manager;
    SZrHotPatchGenerationDiagnostic generationDiagnostic;
    SZrHotPatchRegistryEntry entries[1];
    SZrHotPatchRegistry registry;
    SZrHotPatchGenerationHandle appliedHandle;
    SZrHotPatchApplyDiagnostic applyDiagnostic;
    EZrHotPatchCapabilityStatus validationStatus;
    EZrHotPatchGenerationStatus generationStatus;
    EZrHotPatchApplyStatus applyStatus;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;

    memset(&artifact, 0, sizeof(artifact));
    artifact.buffer = content;
    artifact.bufferLength = (TZrUInt32)sizeof(content);
    artifact.moduleHash = 99u;
    artifact.abiVersion = 16u;

    memset(&manifest, 0, sizeof(manifest));
    manifest.schemaVersion = ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION;
    manifest.patchId = 41u;
    manifest.contentHash = ZrCore_ArtifactExecIr_HashBytes(
            content, (TZrUInt32)sizeof(content));
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
    input.hostAllowedCapabilities = UINT64_C(0x01);
    input.expectedPatchId = 41u;
    input.expectedContentHash = manifest.contentHash;
    input.signature = signature;
    input.signatureLength = (TZrUInt32)sizeof(signature);

    memset(&validated, 0, sizeof(validated));
    validationStatus = ZrCore_HotPatch_Validate(
            &input,
            mutateDuringVerification ? verify_and_mutate : verify,
            mutateDuringVerification ? (TZrPtr)content : ZR_NULL,
            &validated, &validationDiagnostic);
    TEST_CHECK(validationStatus == ZR_HOT_PATCH_OK);
    if (validationStatus != ZR_HOT_PATCH_OK) return;

    expectedHash = validated.contentHash;
    TEST_CHECK(validated.contentBytes == content);
    TEST_CHECK(validated.contentLength == (TZrUInt32)sizeof(content));
    if (!mutateDuringVerification) content[1] ^= 0x80u;
    actualHash = ZrCore_ArtifactExecIr_HashBytes(
            content, (TZrUInt32)sizeof(content));
    TEST_CHECK(actualHash != expectedHash);

    /* A later descriptor edit must not change the range used for rehashing.
     * The original byte array stays alive until this helper returns. */
    artifact.buffer = ZR_NULL;
    artifact.bufferLength = (TZrUInt32)~(TZrUInt32)0u;

    memset(entries, 0, sizeof(entries));
    registry.entries = entries;
    registry.capacity = 1u;
    registry.count = 0u;
    memset(&appliedHandle, 0xa5, sizeof(appliedHandle));
    memset(&applyDiagnostic, 0, sizeof(applyDiagnostic));
    generationStatus = ZrCore_HotPatch_GenerationManager_Init(
            &manager, records, 1u, &generationDiagnostic);
    TEST_CHECK(generationStatus == ZR_HOT_PATCH_GENERATION_OK);
    if (generationStatus != ZR_HOT_PATCH_GENERATION_OK) return;

    applyStatus = ZrCore_HotPatch_ApplyValidated(
            &manager, &registry, &validated, 99u, &appliedHandle,
            &applyDiagnostic);
    TEST_CHECK(applyStatus == ZR_HOT_PATCH_APPLY_CONTENT_MISMATCH);
    TEST_CHECK(strcmp(ZrCore_HotPatch_ApplyStatusName(applyStatus),
                      "content-mismatch") == 0);
    TEST_CHECK(applyDiagnostic.status == ZR_HOT_PATCH_APPLY_CONTENT_MISMATCH);
    TEST_CHECK(applyDiagnostic.patchId == 41u);
    TEST_CHECK(applyDiagnostic.expectedHash == expectedHash);
    TEST_CHECK(applyDiagnostic.actualHash == actualHash);
    TEST_CHECK(applyDiagnostic.generation == 0u);
    TEST_CHECK(registry.count == 0u);
    TEST_CHECK(entries[0].patchId == 0u);
    TEST_CHECK(manager.count == 0u);
    TEST_CHECK(records[0].state == ZR_HOT_PATCH_VERSION_FREE);
    TEST_CHECK(records[0].generation == 0u);
    TEST_CHECK(appliedHandle.record == ZR_NULL);
    TEST_CHECK(appliedHandle.generation == 0u);
    TEST_CHECK(appliedHandle.leased == ZR_FALSE);
    ZrCore_HotPatch_GenerationManager_Deinit(&manager);
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
    TZrUInt32 verificationCallCount = 0u;
    TZrUInt32 index;
    EZrHotPatchCapabilityStatus status;

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
    status = ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                      &diagnostic);
    TEST_CHECK(status == ZR_HOT_PATCH_OK);
    TEST_CHECK(validated.signatureVerified && validated.immutableContent);
    TEST_CHECK(validated.requiredCapabilities == UINT64_C(0x03));

    /* 独立核对声明能力闭包，不把主 Validate 的通过当作闭包证据。 */
    {
        TZrUInt64 required = 0u;
        status = ZrCore_HotPatch_ComputeRequiredCapabilities(
                &manifest, UINT64_C(0x03), &required, &diagnostic);
        TEST_CHECK(status == ZR_HOT_PATCH_OK);
        TEST_CHECK(required == UINT64_C(0x03));
    }

    /* 升级拒绝后 validated 应清空，不能让调用方复用旧授权。 */
    manifest.requiredCapabilities = UINT64_C(0x04);
    validated.contentHash = 777u;
    status = ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                      &diagnostic);
    TEST_CHECK(status == ZR_HOT_PATCH_CAPABILITY_ESCALATION);
    {
        TZrUInt64 required = 0u;
        status = ZrCore_HotPatch_ComputeCapabilityClosure(
                &requirement, 1u, 0u, UINT64_C(0x01), &required,
                &diagnostic);
        TEST_CHECK(status == ZR_HOT_PATCH_CAPABILITY_ESCALATION);
        TEST_CHECK(required == 0u);
    }
    TEST_CHECK(validated.contentHash == 0u);
    manifest.requiredCapabilities = 0u;
    manifest.flags = ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE;
    status = ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                      &diagnostic);
    TEST_CHECK(status == ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN);
    manifest.flags = 0u;
    signature[0] = 0u;
    status = ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                      &diagnostic);
    TEST_CHECK(status == ZR_HOT_PATCH_SIGNATURE_REJECTED);

    /* A count above the closure limit must fail before host signature code or
     * dereferencing beyond this deliberately single-slot requirement array. */
    signature[0] = 0xa5u;
    manifest.requirementCount =
            ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS + 1u;
    manifest.requirements = &requirement;
    verificationCallCount = 0u;
    memset(&validated, 0xa5, sizeof(validated));
    status = ZrCore_HotPatch_Validate(&input, count_and_reject_signature,
                                     &verificationCallCount, &validated,
                                     &diagnostic);
    TEST_CHECK(status == ZR_HOT_PATCH_LIMIT);
    TEST_CHECK(strcmp(ZrCore_HotPatch_StatusName(status), "limit") == 0);
    TEST_CHECK(verificationCallCount == 0u);
    TEST_CHECK(diagnostic.status == ZR_HOT_PATCH_LIMIT);
    TEST_CHECK(diagnostic.expected ==
               ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS);
    TEST_CHECK(diagnostic.actual ==
               ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS + 1u);
    TEST_CHECK(validated.artifact == ZR_NULL);
    TEST_CHECK(validated.manifest == ZR_NULL);
    TEST_CHECK(validated.contentHash == 0u);
    TEST_CHECK(validated.patchId == 0u);
    TEST_CHECK(validated.publicContractHash == 0u);
    TEST_CHECK(validated.requiredCapabilities == 0u);
    TEST_CHECK(validated.validationPolicyHash == 0u);
    TEST_CHECK(validated.targetProfile == 0u);
    TEST_CHECK(validated.signatureVerified == ZR_FALSE);
    TEST_CHECK(validated.immutableContent == ZR_FALSE);

    /* The shared limit is inclusive; a complete 4096-entry manifest remains
     * valid and reaches the ordinary signature and capability checks. */
    for (index = 0u; index < ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS;
         ++index) {
        g_maxRequirements[index].token = index + 1u;
        g_maxRequirements[index].requiredBits = UINT64_C(0x01);
        g_maxRequirements[index].sourceOffset = index;
        g_maxRequirements[index].reserved = 0u;
    }
    manifest.requirementCount = ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS;
    manifest.requirements = g_maxRequirements;
    status = ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                     &diagnostic);
    TEST_CHECK(status == ZR_HOT_PATCH_OK);
    TEST_CHECK(validated.requiredCapabilities == UINT64_C(0x01));
    TEST_CHECK(diagnostic.status == ZR_HOT_PATCH_OK);

    /* Apply must publish metadata captured by validation even if the caller
     * later changes the manifest and requirement array. */
    {
        SZrHotPatchVersionRecord records[2];
        SZrHotPatchGenerationManager manager;
        SZrHotPatchGenerationDiagnostic generationDiagnostic;
        SZrHotPatchRegistryEntry registryEntries[2] = {{0}};
        SZrHotPatchRegistry registry = {registryEntries, 2u, 0u};
        SZrHotPatchGenerationHandle appliedHandle;
        SZrHotPatchGenerationHandle activeHandle;
        SZrHotPatchVersionView activeView;
        SZrHotPatchApplyDiagnostic applyDiagnostic;
        EZrHotPatchApplyStatus applyStatus;
        EZrHotPatchGenerationStatus generationStatus;
        TZrUInt64 originalContentHash;

        manifest.patchId = 1u;
        manifest.publicContractHash = 123u;
        manifest.requiredCapabilities = 0u;
        manifest.requirementCount = 1u;
        manifest.requirements = &requirement;
        requirement.requiredBits = UINT64_C(0x03);
        input.expectedPatchId = 1u;
        input.expectedContentHash = manifest.contentHash;
        status = ZrCore_HotPatch_Validate(&input, verify, ZR_NULL, &validated,
                                         &diagnostic);
        TEST_CHECK(status == ZR_HOT_PATCH_OK);
        originalContentHash = validated.contentHash;

        requirement.requiredBits = UINT64_C(0x01);
        manifest.patchId = 9u;
        manifest.publicContractHash = 456u;
        manifest.requiredCapabilities = UINT64_C(0x01);
        TEST_CHECK(validated.requiredCapabilities == UINT64_C(0x03));
        TEST_CHECK(validated.contentHash == originalContentHash);

        generationStatus = ZrCore_HotPatch_GenerationManager_Init(
                &manager, records, 2u, &generationDiagnostic);
        TEST_CHECK(generationStatus == ZR_HOT_PATCH_GENERATION_OK);
        if (generationStatus == ZR_HOT_PATCH_GENERATION_OK) {
            applyStatus = ZrCore_HotPatch_ApplyValidated(
                    &manager, &registry, &validated, 99u, &appliedHandle,
                    &applyDiagnostic);
            TEST_CHECK(applyStatus == ZR_HOT_PATCH_APPLY_OK);
            if (applyStatus == ZR_HOT_PATCH_APPLY_OK) {
                TEST_CHECK(registryEntries[0].patchId == 1u);
                TEST_CHECK(registryEntries[0].contentHash == originalContentHash);
                generationStatus = ZrCore_HotPatch_Generation_AcquireActive(
                        &manager, &activeHandle, &generationDiagnostic);
                TEST_CHECK(generationStatus == ZR_HOT_PATCH_GENERATION_OK);
                if (generationStatus == ZR_HOT_PATCH_GENERATION_OK) {
                    generationStatus = ZrCore_HotPatch_Generation_Resolve(
                            &manager, &activeHandle, &activeView,
                            &generationDiagnostic);
                    TEST_CHECK(generationStatus == ZR_HOT_PATCH_GENERATION_OK);
                    if (generationStatus == ZR_HOT_PATCH_GENERATION_OK) {
                        TEST_CHECK(activeView.contentHash == originalContentHash);
                        TEST_CHECK(activeView.publicContractHash == 123u);
                    }
                    (void)ZrCore_HotPatch_Generation_Release(
                            &manager, &activeHandle, &generationDiagnostic);
                }
            }
            ZrCore_HotPatch_GenerationManager_Deinit(&manager);
        }
    }

    expect_apply_rejects_sequential_content_mutation(ZR_FALSE);
    expect_apply_rejects_sequential_content_mutation(ZR_TRUE);

    return g_testFailureCount == 0u ? 0 : 1;
}
