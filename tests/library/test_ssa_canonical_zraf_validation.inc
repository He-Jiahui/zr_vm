#include "zr_vm_core/capability_manifest.h"

#include <stdint.h>

static const TZrByte canonicalZrafTestSignature[] = {0xa5u, 0x5au, 0x01u};

typedef struct SCanonicalZrafVerifyContext {
    const TZrByte *outerBytes;
    const TZrByte *expectedCopy;
    TZrByte *mutableBytes;
    SZrHotPatchCapabilityManifest *mutableManifest;
    SZrHotPatchCapabilityRequirement *mutableRequirement;
    SZrArtifactPublicIdentity *mutableExpectedIdentity;
    TZrUInt32 outerLength;
    const TZrByte *expectedSignature;
    TZrUInt32 expectedSignatureLength;
    TZrUInt32 callCount;
    TZrUInt64 observedHash;
    TZrBool accept;
    TZrBool mutateAfterObservation;
    TZrBool mutatePolicyAfterObservation;
    TZrBool sawFullOuterSpan;
    TZrByte savedMutatedByte;
} SCanonicalZrafVerifyContext;

static TZrUInt64 canonical_zraf_hash(const TZrByte *bytes, TZrSize length) {
    require_true(bytes != ZR_NULL && length > 0u &&
                 length <= ZR_ARTIFACT_MAX_BYTE_LENGTH,
                 "bounded canonical ZRAF hash span");
    return ZrCore_ArtifactExecIr_HashBytes(bytes, (TZrUInt32)length);
}

static int canonical_zraf_identity_equal(
        const SZrArtifactPublicIdentity *left,
        const SZrArtifactPublicIdentity *right) {
    return left->canonicalTypeId == right->canonicalTypeId &&
           left->typeRefToken == right->typeRefToken &&
           left->typeSpecToken == right->typeSpecToken &&
           left->signatureToken == right->signatureToken &&
           left->typeRefHash == right->typeRefHash &&
           left->typeSpecHash == right->typeSpecHash &&
           left->signatureHash == right->signatureHash &&
           left->layoutVersion == right->layoutVersion &&
           left->reserved0 == right->reserved0 &&
           left->layoutHash == right->layoutHash &&
           left->callableContractHash == right->callableContractHash &&
           left->moduleHash == right->moduleHash;
}

static int canonical_zraf_identity_zero(
        const SZrArtifactPublicIdentity *identity) {
    static const SZrArtifactPublicIdentity zeroIdentity = {0};
    return canonical_zraf_identity_equal(identity, &zeroIdentity);
}

static int canonical_zraf_token_zero(
        const SZrValidatedHotPatchZraf *token) {
    return token->outerBytes == ZR_NULL && token->outerLength == 0u &&
           token->outerContentHash == 0u &&
           canonical_zraf_identity_zero(&token->publicIdentity) &&
           token->entryFunctionToken == 0u &&
           token->entrySignatureHash == 0u && token->patchId == 0u &&
           token->publicContractHash == 0u &&
           token->requiredCapabilities == 0u &&
           token->validationPolicyHash == 0u &&
           token->targetAbiVersion == 0u && token->targetProfile == 0u &&
           token->signatureVerified == ZR_FALSE &&
           token->canonicalExecIrVerified == ZR_FALSE;
}

static TZrBool canonical_zraf_verify_signature(
        const TZrByte *content, TZrUInt32 contentLength,
        const TZrByte *signature, TZrUInt32 signatureLength,
        TZrPtr userData) {
    SCanonicalZrafVerifyContext *context =
            (SCanonicalZrafVerifyContext *)userData;
    context->callCount++;
    context->sawFullOuterSpan = (TZrBool)(content != ZR_NULL &&
            content == context->outerBytes &&
            contentLength == context->outerLength &&
            memcmp(content, context->expectedCopy, contentLength) == 0);
    if (context->sawFullOuterSpan) {
        context->observedHash = ZrCore_ArtifactExecIr_HashBytes(
                content, contentLength);
    }
    if (context->mutateAfterObservation && context->mutableBytes != ZR_NULL &&
        context->outerLength > 0u) {
        TZrUInt32 finalIndex = context->outerLength - 1u;
        context->savedMutatedByte = context->mutableBytes[finalIndex];
        context->mutableBytes[finalIndex] ^= 0x80u;
    }
    if (context->mutatePolicyAfterObservation) {
        if (context->mutableManifest != ZR_NULL) {
            context->mutableManifest->patchId = 2u;
            context->mutableManifest->requiredCapabilities = UINT64_C(0x04);
        }
        if (context->mutableRequirement != ZR_NULL) {
            context->mutableRequirement->requiredBits = UINT64_C(0x04);
        }
        if (context->mutableExpectedIdentity != ZR_NULL) {
            context->mutableExpectedIdentity->signatureHash++;
        }
    }
    return (TZrBool)(context->accept && context->sawFullOuterSpan &&
            signature != ZR_NULL &&
            signatureLength == context->expectedSignatureLength &&
            memcmp(signature, context->expectedSignature,
                   signatureLength) == 0);
}

static void canonical_zraf_init_manifest(
        SZrHotPatchCapabilityManifest *manifest,
        SZrHotPatchCapabilityRequirement *requirement,
        TZrUInt64 contentHash) {
    memset(manifest, 0, sizeof(*manifest));
    requirement->token = TEST_MEMBER;
    requirement->requiredBits = UINT64_C(0x03);
    requirement->sourceOffset = 42u;
    requirement->reserved = 0u;
    manifest->schemaVersion = ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION;
    manifest->patchId = 1u;
    manifest->contentHash = contentHash;
    manifest->baseModuleHash = TEST_MODULE_HASH;
    manifest->publicContractHash = TEST_CONTRACT_HASH;
    manifest->targetAbiVersion = 17u;
    manifest->targetProfile = 2u;
    manifest->requirementCount = 1u;
    manifest->requirements = requirement;
}

/* The expected identity and input live only in this helper's frame. The
 * successful token must retain scalar snapshots, never these local views. */
static EZrHotPatchCapabilityStatus canonical_zraf_validate_with_selector(
        const TZrByte *outerBytes, TZrSize outerLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        const SZrHotPatchCapabilityManifest *manifest,
        TZrMetadataToken entryFunctionToken,
        TZrUInt64 entrySignatureHash,
        TZrUInt64 expectedContentHash,
        SCanonicalZrafVerifyContext *verifyContext,
        SZrValidatedHotPatchZraf *outToken,
        SZrHotPatchDiagnostic *diagnostic) {
    SZrArtifactPublicIdentity localExpectedIdentity = *expectedIdentity;
    SZrHotPatchZrafValidationInput input;
    memset(&input, 0, sizeof(input));
    input.outerBytes = outerBytes;
    require_true(outerLength <= ZR_ARTIFACT_MAX_BYTE_LENGTH,
                 "canonical outer span is within the API limit");
    input.outerLength = outerLength;
    input.expectedPublicIdentity = &localExpectedIdentity;
    input.manifest = manifest;
    input.entryFunctionToken = entryFunctionToken;
    input.entrySignatureHash = entrySignatureHash;
    input.loadedBaseModuleHash = TEST_MODULE_HASH;
    input.loadedPublicContractHash = TEST_CONTRACT_HASH;
    input.hostAbiVersion = 17u;
    input.hostProfile = 2u;
    input.hostAllowedCapabilities = UINT64_C(0x03);
    input.expectedPatchId = 1u;
    input.expectedContentHash = expectedContentHash;
    input.signature = canonicalZrafTestSignature;
    input.signatureLength = sizeof(canonicalZrafTestSignature);
    return ZrCore_HotPatch_ValidateZraf(
            &input, canonical_zraf_verify_signature, verifyContext,
            outToken, diagnostic);
}

static EZrHotPatchCapabilityStatus canonical_zraf_validate_from_scope(
        const TZrByte *outerBytes, TZrSize outerLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        const SZrHotPatchCapabilityManifest *manifest,
        TZrUInt64 expectedContentHash,
        SCanonicalZrafVerifyContext *verifyContext,
        SZrValidatedHotPatchZraf *outToken,
        SZrHotPatchDiagnostic *diagnostic) {
    return canonical_zraf_validate_with_selector(
            outerBytes, outerLength, expectedIdentity, manifest,
            TEST_MEMBER, TEST_SIGNATURE_HASH, expectedContentHash,
            verifyContext, outToken, diagnostic);
}

static void canonical_zraf_init_verify_context(
        SCanonicalZrafVerifyContext *context,
        const TZrByte *outerBytes, TZrSize outerLength,
        const TZrByte *expectedCopy, TZrBool accept) {
    memset(context, 0, sizeof(*context));
    require_true(outerLength <= UINT32_MAX,
                 "outer ZRAF fits signature callback length");
    context->outerBytes = outerBytes;
    context->expectedCopy = expectedCopy;
    context->outerLength = (TZrUInt32)outerLength;
    context->expectedSignature = canonicalZrafTestSignature;
    context->expectedSignatureLength = sizeof(canonicalZrafTestSignature);
    context->accept = accept;
}

static void canonical_zraf_require_rejected(
        EZrHotPatchCapabilityStatus status,
        EZrHotPatchCapabilityStatus expectedStatus,
        const SZrValidatedHotPatchZraf *token,
        const SZrHotPatchDiagnostic *diagnostic,
        const char *message) {
    require_true(status == expectedStatus, message);
    require_true(diagnostic->status == expectedStatus,
                 "canonical ZRAF diagnostic matches status");
    require_true(canonical_zraf_token_zero(token),
                 "rejected canonical ZRAF validation clears token");
}

static void canonical_zraf_reject_oversized_span(
        const SZrArtifactPublicIdentity *expectedIdentity,
        const SZrHotPatchCapabilityManifest *manifest) {
    TZrByte sentinel = 0u;
    TZrSize oversizedLength;
    SZrHotPatchZrafValidationInput input;
    SZrValidatedHotPatchZraf rejectedToken;
    SZrHotPatchDiagnostic diagnostic;
    SCanonicalZrafVerifyContext verify;
    EZrHotPatchCapabilityStatus status;

#if SIZE_MAX > UINT32_MAX
    /* This also catches an accidental pre-validation UInt32 truncation. */
    oversizedLength = (TZrSize)UINT32_MAX + (TZrSize)1u;
#else
    oversizedLength = (TZrSize)ZR_ARTIFACT_MAX_BYTE_LENGTH + (TZrSize)1u;
#endif

    memset(&input, 0, sizeof(input));
    memset(&verify, 0, sizeof(verify));
    input.outerBytes = &sentinel;
    input.outerLength = oversizedLength;
    input.expectedPublicIdentity = expectedIdentity;
    input.manifest = manifest;
    input.entryFunctionToken = TEST_MEMBER;
    input.entrySignatureHash = TEST_SIGNATURE_HASH;
    input.loadedBaseModuleHash = TEST_MODULE_HASH;
    input.loadedPublicContractHash = TEST_CONTRACT_HASH;
    input.hostAbiVersion = 17u;
    input.hostProfile = 2u;
    input.hostAllowedCapabilities = UINT64_C(0x03);
    input.expectedPatchId = manifest->patchId;
    input.expectedContentHash = manifest->contentHash;
    input.signature = canonicalZrafTestSignature;
    input.signatureLength = sizeof(canonicalZrafTestSignature);
    verify.outerBytes = &sentinel;
    verify.expectedCopy = &sentinel;
    verify.expectedSignature = canonicalZrafTestSignature;
    verify.expectedSignatureLength = sizeof(canonicalZrafTestSignature);
    verify.accept = ZR_TRUE;
    memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
    memset(&diagnostic, 0, sizeof(diagnostic));

    status = ZrCore_HotPatch_ValidateZraf(
            &input, canonical_zraf_verify_signature, &verify,
            &rejectedToken, &diagnostic);
    canonical_zraf_require_rejected(
            status, ZR_HOT_PATCH_LIMIT, &rejectedToken, &diagnostic,
            "reject full-width outer length above the artifact limit");
    require_true(diagnostic.expected == ZR_ARTIFACT_MAX_BYTE_LENGTH &&
                 diagnostic.actual == (TZrUInt64)oversizedLength,
                 "oversized span diagnostic preserves the caller's full length");
    require_true(verify.callCount == 0u,
                 "oversized span is rejected before callback or byte access");
}

#include "test_ssa_canonical_zraf_eis6_guard.inc"

static void validate_zraf_phase(const char *v1Path, const char *v2Path) {
    SFixture fixture;
    SZrHotPatchCapabilityRequirement requirementV1;
    SZrHotPatchCapabilityRequirement requirementV2;
    SZrHotPatchCapabilityManifest manifestV1;
    SZrHotPatchCapabilityManifest manifestV2;
    SZrHotPatchDiagnostic diagnostic;
    SZrValidatedHotPatchZraf tokenV1;
    SZrValidatedHotPatchZraf tokenV2;
    SCanonicalZrafVerifyContext verifyV1;
    SCanonicalZrafVerifyContext verifyV2;
    TZrSize v1Length;
    TZrSize v2Length;
    TZrByte *v1Bytes = read_file(v1Path, &v1Length);
    TZrByte *v2Bytes = read_file(v2Path, &v2Length);
    TZrByte *v1Copy = (TZrByte *)malloc(v1Length);
    TZrByte *v2Copy = (TZrByte *)malloc(v2Length);
    TZrByte *mutated = (TZrByte *)malloc(v1Length);
    TZrUInt64 v1Hash;
    TZrUInt64 v2Hash;
    EZrHotPatchCapabilityStatus status;
    TZrByte finalV1Byte;
    TZrByte finalV2Byte;

    require_true(v1Copy != ZR_NULL && v2Copy != ZR_NULL &&
                 mutated != ZR_NULL, "allocate canonical ZRAF test snapshots");
    memcpy(v1Copy, v1Bytes, v1Length);
    memcpy(v2Copy, v2Bytes, v2Length);
    v1Hash = canonical_zraf_hash(v1Bytes, v1Length);
    v2Hash = canonical_zraf_hash(v2Bytes, v2Length);
    require_true(v1Hash != v2Hash,
                 "V1 and V2 use distinct full outer ZRAF content hashes");
    init_fixture(&fixture);
    canonical_zraf_test_eis6_guard();

    canonical_zraf_init_manifest(&manifestV1, &requirementV1, v1Hash);
    canonical_zraf_reject_oversized_span(
            &fixture.metadata.identity, &manifestV1);
    canonical_zraf_init_verify_context(&verifyV1, v1Bytes, v1Length,
                                       v1Copy, ZR_TRUE);
    memset(&tokenV1, 0xa5, sizeof(tokenV1));
    memset(&diagnostic, 0, sizeof(diagnostic));
    status = canonical_zraf_validate_from_scope(
            v1Bytes, v1Length, &fixture.metadata.identity, &manifestV1,
            v1Hash, &verifyV1, &tokenV1, &diagnostic);
    require_true(status == ZR_HOT_PATCH_OK,
                 "validate full-span canonical ZRAF V1");
    require_true(verifyV1.callCount == 1u && verifyV1.sawFullOuterSpan &&
                 verifyV1.observedHash == v1Hash,
                 "V1 signature callback receives the complete outer ZRAF");
    require_true(tokenV1.outerBytes == v1Bytes &&
                 tokenV1.outerLength == v1Length &&
                 tokenV1.outerContentHash == v1Hash,
                 "V1 token captures borrowed full-span identity");
    require_true(canonical_zraf_identity_equal(
                     &tokenV1.publicIdentity, &fixture.metadata.identity),
                 "V1 token snapshots expected public identity by value");
    require_true(tokenV1.entryFunctionToken == TEST_MEMBER &&
                 tokenV1.entrySignatureHash == TEST_SIGNATURE_HASH,
                 "V1 token snapshots explicit entry selector");
    require_true(tokenV1.patchId == manifestV1.patchId &&
                 tokenV1.publicContractHash == TEST_CONTRACT_HASH &&
                 tokenV1.requiredCapabilities == UINT64_C(0x03) &&
                 tokenV1.validationPolicyHash != 0u &&
                 tokenV1.targetAbiVersion == 17u &&
                 tokenV1.targetProfile == 2u &&
                 tokenV1.signatureVerified &&
                 tokenV1.canonicalExecIrVerified,
                 "V1 token snapshots validated policy scalars");
    status = ZrCore_HotPatch_RecheckZrafContent(&tokenV1, &diagnostic);
    require_true(status == ZR_HOT_PATCH_OK &&
                 diagnostic.status == ZR_HOT_PATCH_OK,
                 "unmodified V1 borrowed span rechecks successfully");

    /* The callback may mutate host-owned policy inputs, but the validated
     * result must use only the snapshots consumed before it was invoked. */
    {
        SZrHotPatchCapabilityRequirement mutableRequirement;
        SZrHotPatchCapabilityRequirement originalRequirement;
        SZrHotPatchCapabilityManifest mutableManifest;
        SZrHotPatchCapabilityManifest originalManifest;
        SZrArtifactPublicIdentity mutableIdentity = fixture.metadata.identity;
        SZrArtifactPublicIdentity originalIdentity = mutableIdentity;
        SZrHotPatchZrafValidationInput mutationInput;
        SZrValidatedHotPatchZraf snapshotToken;
        SCanonicalZrafVerifyContext verify;

        canonical_zraf_init_manifest(&mutableManifest, &mutableRequirement,
                                     v1Hash);
        originalRequirement = mutableRequirement;
        originalManifest = mutableManifest;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        verify.mutableManifest = &mutableManifest;
        verify.mutableRequirement = &mutableRequirement;
        verify.mutableExpectedIdentity = &mutableIdentity;
        verify.mutatePolicyAfterObservation = ZR_TRUE;
        memset(&mutationInput, 0, sizeof(mutationInput));
        mutationInput.outerBytes = v1Bytes;
        mutationInput.outerLength = v1Length;
        mutationInput.expectedPublicIdentity = &mutableIdentity;
        mutationInput.manifest = &mutableManifest;
        mutationInput.entryFunctionToken = TEST_MEMBER;
        mutationInput.entrySignatureHash = TEST_SIGNATURE_HASH;
        mutationInput.loadedBaseModuleHash = TEST_MODULE_HASH;
        mutationInput.loadedPublicContractHash = TEST_CONTRACT_HASH;
        mutationInput.hostAbiVersion = 17u;
        mutationInput.hostProfile = 2u;
        mutationInput.hostAllowedCapabilities = UINT64_C(0x03);
        mutationInput.expectedPatchId = 1u;
        mutationInput.expectedContentHash = v1Hash;
        mutationInput.signature = canonicalZrafTestSignature;
        mutationInput.signatureLength = sizeof(canonicalZrafTestSignature);
        memset(&snapshotToken, 0xa5, sizeof(snapshotToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = ZrCore_HotPatch_ValidateZraf(
                &mutationInput, canonical_zraf_verify_signature, &verify,
                &snapshotToken, &diagnostic);
        require_true(status == ZR_HOT_PATCH_OK &&
                     verify.callCount == 1u && verify.sawFullOuterSpan,
                     "policy snapshots survive callback mutation");
        require_true(mutableManifest.patchId == 2u &&
                     mutableManifest.requiredCapabilities == UINT64_C(0x04) &&
                     mutableRequirement.requiredBits == UINT64_C(0x04) &&
                     mutableIdentity.signatureHash ==
                             originalIdentity.signatureHash + 1u,
                     "callback actually mutates each caller-owned policy input");
        require_true(snapshotToken.patchId == originalManifest.patchId &&
                     snapshotToken.requiredCapabilities == UINT64_C(0x03) &&
                     snapshotToken.validationPolicyHash ==
                             tokenV1.validationPolicyHash &&
                     canonical_zraf_identity_equal(
                             &snapshotToken.publicIdentity, &originalIdentity),
                     "callback cannot replace verified token policy or identity");

        /* Restore host-owned test inputs after the callback's intentional writes. */
        mutableRequirement = originalRequirement;
        mutableManifest = originalManifest;
        mutableIdentity = originalIdentity;
    }

    canonical_zraf_init_manifest(&manifestV2, &requirementV2, v2Hash);
    canonical_zraf_init_verify_context(&verifyV2, v2Bytes, v2Length,
                                       v2Copy, ZR_TRUE);
    memset(&tokenV2, 0xa5, sizeof(tokenV2));
    memset(&diagnostic, 0, sizeof(diagnostic));
    status = canonical_zraf_validate_from_scope(
            v2Bytes, v2Length, &fixture.metadata.identity, &manifestV2,
            v2Hash, &verifyV2, &tokenV2, &diagnostic);
    require_true(status == ZR_HOT_PATCH_OK,
                 "validate full-span canonical ZRAF V2");
    require_true(verifyV2.callCount == 1u && verifyV2.sawFullOuterSpan &&
                 verifyV2.observedHash == v2Hash,
                 "V2 signature callback receives the complete outer ZRAF");
    require_true(tokenV2.outerContentHash == v2Hash &&
                 tokenV2.outerContentHash != tokenV1.outerContentHash &&
                 tokenV2.entryFunctionToken == TEST_MEMBER &&
                 tokenV2.entrySignatureHash == TEST_SIGNATURE_HASH &&
                 canonical_zraf_identity_equal(
                     &tokenV2.publicIdentity, &fixture.metadata.identity),
                 "V2 token preserves its own outer and entry identity");

    /* A later mutation is detectable only while the borrowed allocation lives. */
    finalV1Byte = v1Bytes[v1Length - 1u];
    v1Bytes[v1Length - 1u] ^= 0x01u;
    memset(&diagnostic, 0, sizeof(diagnostic));
    status = ZrCore_HotPatch_RecheckZrafContent(&tokenV1, &diagnostic);
    require_true(status == ZR_HOT_PATCH_CONTENT_CHANGED &&
                 diagnostic.status == ZR_HOT_PATCH_CONTENT_CHANGED &&
                 diagnostic.expected == v1Hash &&
                 diagnostic.actual == canonical_zraf_hash(v1Bytes, v1Length),
                 "Recheck rejects changed but still-live V1 bytes");
    require_true(strcmp(ZrCore_HotPatch_StatusName(status),
                        "content-changed") == 0,
                 "content-change status has a diagnostic name");
    v1Bytes[v1Length - 1u] = finalV1Byte;

    /* Identity failure retains a structured diagnostic and clears the token. */
    {
        SZrArtifactPublicIdentity wrongIdentity = fixture.metadata.identity;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        wrongIdentity.signatureHash++;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v1Bytes, v1Length, &wrongIdentity, &manifestV1, v1Hash,
                &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_ARTIFACT_INVALID, &rejectedToken,
                &diagnostic, "reject expected public identity mismatch");
        require_true(verify.callCount == 1u && verify.sawFullOuterSpan &&
                     diagnostic.expected != diagnostic.actual,
                     "identity mismatch is diagnosed after full-span authentication");
    }

    /* Keep manifest identity valid while independently varying the host's
     * expected identity fields, so diagnostics report the actual mismatch. */
    {
        SZrArtifactPublicIdentity wrongIdentity = fixture.metadata.identity;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        wrongIdentity.moduleHash++;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v1Bytes, v1Length, &wrongIdentity, &manifestV1, v1Hash,
                &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_BASE_MISMATCH, &rejectedToken,
                &diagnostic, "reject expected module identity mismatch");
        require_true(diagnostic.expected == TEST_MODULE_HASH &&
                     diagnostic.actual == wrongIdentity.moduleHash &&
                     verify.callCount == 0u,
                     "module identity diagnostic reports its actual value before callback");
    }
    {
        SZrArtifactPublicIdentity wrongIdentity = fixture.metadata.identity;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        wrongIdentity.callableContractHash++;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v1Bytes, v1Length, &wrongIdentity, &manifestV1, v1Hash,
                &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE, &rejectedToken,
                &diagnostic, "reject expected callable contract mismatch");
        require_true(diagnostic.expected == manifestV1.publicContractHash &&
                     diagnostic.actual == wrongIdentity.callableContractHash &&
                     verify.callCount == 0u,
                     "callable identity diagnostic reports its actual value before callback");
    }

    /* The entry selector must match a canonical function exactly. */
    {
        TZrMetadataToken wrongFunctionToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 4u);
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_with_selector(
                v1Bytes, v1Length, &fixture.metadata.identity, &manifestV1,
                wrongFunctionToken, TEST_SIGNATURE_HASH, v1Hash, &verify,
                &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_ARTIFACT_INVALID, &rejectedToken,
                &diagnostic, "reject entry function token mismatch");
        require_true(diagnostic.expected == wrongFunctionToken &&
                     diagnostic.actual == 0u && verify.callCount == 1u &&
                     verify.sawFullOuterSpan,
                     "missing entry token reports no match after authentication");
    }
    {
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_with_selector(
                v1Bytes, v1Length, &fixture.metadata.identity, &manifestV1,
                TEST_MEMBER, TEST_SIGNATURE_HASH + 1u, v1Hash, &verify,
                &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_ARTIFACT_INVALID, &rejectedToken,
                &diagnostic, "reject entry signature hash mismatch");
        require_true(diagnostic.expected == TEST_SIGNATURE_HASH + 1u &&
                     diagnostic.actual == TEST_SIGNATURE_HASH &&
                     verify.callCount == 1u && verify.sawFullOuterSpan,
                     "signature selector mismatch follows authentication");
    }
    {
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_with_selector(
                v1Bytes, v1Length, &fixture.metadata.identity, &manifestV1,
                0u, 0u, v1Hash, &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_INVALID_ARGUMENT, &rejectedToken,
                &diagnostic, "reject missing entry selector");
        require_true(verify.callCount == 0u,
                     "missing selector is rejected before signature callback");
    }

    /* Malformed outer metadata is rejected even when its supplied hash matches. */
    memcpy(mutated, v1Bytes, v1Length);
    mutated[0] ^= 0x01u;
    {
        SZrHotPatchCapabilityRequirement requirement;
        SZrHotPatchCapabilityManifest manifest;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        TZrUInt64 malformedHash = canonical_zraf_hash(mutated, v1Length);
        canonical_zraf_init_manifest(&manifest, &requirement, malformedHash);
        canonical_zraf_init_verify_context(&verify, mutated, v1Length,
                                           mutated, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                mutated, v1Length, &fixture.metadata.identity, &manifest,
                malformedHash, &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_ARTIFACT_INVALID, &rejectedToken,
                &diagnostic, "reject malformed outer ZRAF metadata");
        require_true(verify.callCount == 1u && verify.sawFullOuterSpan &&
                     diagnostic.sourceOffset == 0u,
                     "malformed outer header is checked after signature callback");
    }

    /* A valid outer envelope with an unsupported nested ABI must also fail. */
    {
        SZrArtifactView outer;
        SZrArtifactSectionView bundle;
        SZrArtifactDiagnostic artifactDiagnostic;
        SZrHotPatchCapabilityRequirement requirement;
        SZrHotPatchCapabilityManifest manifest;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        TZrSize bundleOffset;
        TZrUInt64 unsupportedHash;
        require_true(ZrCore_Artifact_Read(v1Bytes, v1Length, &outer,
                                          &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "read canonical outer before unsupported-graph mutation");
        require_true(ZrCore_Artifact_FindSection(
                         &outer, ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE,
                         &bundle, &artifactDiagnostic) == ZR_ARTIFACT_STATUS_OK,
                     "find nested graph for unsupported-ABI mutation");
        bundleOffset = (TZrSize)(bundle.data - v1Bytes);
        memcpy(mutated, v1Bytes, v1Length);
        put16(mutated + bundleOffset + 8u, 16u);
        unsupportedHash = canonical_zraf_hash(mutated, v1Length);
        canonical_zraf_init_manifest(&manifest, &requirement, unsupportedHash);
        canonical_zraf_init_verify_context(&verify, mutated, v1Length,
                                           mutated, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                mutated, v1Length, &fixture.metadata.identity, &manifest,
                unsupportedHash, &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_ARTIFACT_INVALID, &rejectedToken,
                &diagnostic, "reject unsupported nested ExecIR ABI");
        require_true(diagnostic.expected == 17u &&
                     diagnostic.actual == 16u && diagnostic.sourceOffset > 0u &&
                     verify.callCount == 1u && verify.sawFullOuterSpan,
                     "unsupported nested ABI keeps diagnostics after authentication");
    }

    /* The manifest hash is the complete outer span, not an inner ERI view. */
    {
        SZrHotPatchCapabilityRequirement requirement;
        SZrHotPatchCapabilityManifest wrongHashManifest;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        canonical_zraf_init_manifest(&wrongHashManifest, &requirement,
                                     v1Hash + 1u);
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v1Bytes, v1Length, &fixture.metadata.identity,
                &wrongHashManifest, v1Hash + 1u, &verify, &rejectedToken,
                &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_BASE_MISMATCH, &rejectedToken,
                &diagnostic, "reject manifest hash that does not cover outer ZRAF");
        require_true(verify.callCount == 0u &&
                     diagnostic.expected == v1Hash + 1u &&
                     diagnostic.actual == v1Hash,
                     "outer content hash mismatch is structured");
    }

    /* Capability closure remains shared with the established host policy. */
    {
        SZrHotPatchCapabilityRequirement requirement;
        SZrHotPatchCapabilityManifest deniedManifest;
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        canonical_zraf_init_manifest(&deniedManifest, &requirement, v1Hash);
        deniedManifest.requiredCapabilities = UINT64_C(0x04);
        canonical_zraf_init_verify_context(&verify, v1Bytes, v1Length,
                                           v1Copy, ZR_TRUE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v1Bytes, v1Length, &fixture.metadata.identity,
                &deniedManifest, v1Hash, &verify, &rejectedToken,
                &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_CAPABILITY_ESCALATION, &rejectedToken,
                &diagnostic, "reject capability outside host allow-set");
        require_true(diagnostic.expected == UINT64_C(0x03) &&
                     diagnostic.actual == UINT64_C(0x04) &&
                     verify.callCount == 0u,
                     "capability denial reports before signature callback");
    }

    /* A host signature rejection still observes the complete outer artifact. */
    {
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        canonical_zraf_init_verify_context(&verify, v2Bytes, v2Length,
                                           v2Copy, ZR_FALSE);
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v2Bytes, v2Length, &fixture.metadata.identity, &manifestV2,
                v2Hash, &verify, &rejectedToken, &diagnostic);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_SIGNATURE_REJECTED, &rejectedToken,
                &diagnostic, "reject host signature callback decision");
        require_true(verify.callCount == 1u && verify.sawFullOuterSpan &&
                     verify.observedHash == v2Hash,
                     "rejected signature callback received full V2 bytes");
    }

    /* A callback that mutates the borrowed span is detected before publication. */
    {
        SZrValidatedHotPatchZraf rejectedToken;
        SCanonicalZrafVerifyContext verify;
        TZrUInt64 changedHash;
        canonical_zraf_init_verify_context(&verify, v2Bytes, v2Length,
                                           v2Copy, ZR_TRUE);
        verify.mutableBytes = v2Bytes;
        verify.mutateAfterObservation = ZR_TRUE;
        finalV2Byte = v2Bytes[v2Length - 1u];
        memset(&rejectedToken, 0xa5, sizeof(rejectedToken));
        memset(&diagnostic, 0, sizeof(diagnostic));
        status = canonical_zraf_validate_from_scope(
                v2Bytes, v2Length, &fixture.metadata.identity, &manifestV2,
                v2Hash, &verify, &rejectedToken, &diagnostic);
        changedHash = canonical_zraf_hash(v2Bytes, v2Length);
        canonical_zraf_require_rejected(
                status, ZR_HOT_PATCH_CONTENT_CHANGED, &rejectedToken,
                &diagnostic, "reject callback mutation after full-span verify");
        require_true(verify.callCount == 1u && verify.sawFullOuterSpan &&
                     verify.savedMutatedByte == finalV2Byte &&
                     diagnostic.expected == v2Hash &&
                     diagnostic.actual == changedHash,
                     "post-callback rehash reports changed outer content");
        v2Bytes[v2Length - 1u] = finalV2Byte;
    }

    free(mutated);
    free(v1Copy);
    free(v2Copy);
    free(v1Bytes);
    free(v2Bytes);
}
