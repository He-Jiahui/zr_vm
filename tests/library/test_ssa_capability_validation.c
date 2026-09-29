#include "zr_vm_core/capability_manifest.h"
#include "zr_vm_core/hotpatch_capability.h"

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

    return g_testFailureCount == 0u ? 0 : 1;
}
