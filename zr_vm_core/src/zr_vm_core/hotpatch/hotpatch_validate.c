#include "zr_vm_core/capability_manifest.h"
#include "zr_vm_core/hotpatch_capability.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/exec_ir.h"

#include <stdint.h>
#include <string.h>

/* 把主验证器的首个拒绝原因与相关来源位置送回部署入口。 */
static EZrHotPatchCapabilityStatus hp_fail(SZrHotPatchDiagnostic *d,
                                             EZrHotPatchCapabilityStatus s,
                                             TZrUInt32 token, TZrUInt32 source,
                                             TZrUInt64 expected, TZrUInt64 actual) {
    if (d) { d->status=s; d->token=token; d->sourceOffset=source; d->expected=expected; d->actual=actual; }
    return s;
}

/* Canonical artifact diagnostics retain byte offsets and the most specific
 * version/hash/token pair in the hotpatch-facing diagnostic. */
static EZrHotPatchCapabilityStatus hp_zraf_artifact_fail(
        SZrHotPatchDiagnostic *diagnostic,
        const SZrArtifactDiagnostic *artifactDiagnostic) {
    TZrUInt32 token = 0u;
    TZrUInt32 sourceOffset = 0u;
    TZrUInt64 expected = 0u;
    TZrUInt64 actual = 0u;

    if (artifactDiagnostic != ZR_NULL) {
        token = artifactDiagnostic->actualToken != 0u
                ? artifactDiagnostic->actualToken
                : artifactDiagnostic->expectedToken;
        sourceOffset = artifactDiagnostic->byteOffset;
        expected = artifactDiagnostic->expectedHash;
        actual = artifactDiagnostic->actualHash;
        if (expected == 0u && actual == 0u) {
            expected = artifactDiagnostic->expectedVersion;
            actual = artifactDiagnostic->actualVersion;
        }
        if (expected == 0u && actual == 0u) {
            expected = artifactDiagnostic->expectedToken;
            actual = artifactDiagnostic->actualToken;
        }
    }
    return hp_fail(diagnostic, ZR_HOT_PATCH_ARTIFACT_INVALID, token,
                   sourceOffset, expected, actual);
}

static EZrHotPatchCapabilityStatus hp_zraf_selector_fail(
        SZrHotPatchDiagnostic *diagnostic,
        TZrMetadataToken requestedToken,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    return hp_fail(diagnostic, ZR_HOT_PATCH_ARTIFACT_INVALID,
                   requestedToken, 0u, expected, actual);
}

/* 策略哈希仅绑定 manifest 所选策略字段与当前 host/base 身份，供候选后续比较；
 * 它不是签名验证，也不替代 artifact 内容哈希。 */
TZrUInt64 ZrCore_HotPatch_ComputePolicyHash(const SZrHotPatchValidationInput *in) {
    TZrUInt64 h = UINT64_C(1469598103934665603);
    if (!in || !in->manifest) return 0u;
/* 固定宽度和字节顺序使不同 host 对同一策略得出同一身份。 */
#define HP_MIX(v) do { TZrUInt64 x=(TZrUInt64)(v); for(TZrUInt32 i=0;i<8u;i++){h^=(TZrByte)(x>>(i*8u));h*=UINT64_C(1099511628211);}} while(0)
    HP_MIX(in->manifest->schemaVersion); HP_MIX(in->manifest->flags);
    HP_MIX(in->manifest->targetAbiVersion); HP_MIX(in->manifest->targetProfile);
    HP_MIX(in->hostAbiVersion); HP_MIX(in->hostProfile); HP_MIX(in->hostAllowedCapabilities);
    HP_MIX(in->loadedBaseModuleHash); HP_MIX(in->loadedPublicContractHash);
#undef HP_MIX
    return h;
}

/* 部署前核对内容、基模块、公开契约、ABI、profile 和 host 授权；
 * validated 仍借用 artifact/manifest，但发布路径使用本次验证捕获的标量身份。 */
EZrHotPatchCapabilityStatus ZrCore_HotPatch_Validate(
        const SZrHotPatchValidationInput *in,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatch *validated,
        SZrHotPatchDiagnostic *diagnostic) {
    const SZrHotPatchCapabilityManifest *m;
    const TZrByte *contentBytes = ZR_NULL;
    TZrUInt32 contentLength = 0u;
    TZrUInt64 contentHash = 0u;
    TZrUInt64 required = 0u;
    SZrValidatedHotPatch candidate;
    if (validated) memset(validated, 0, sizeof(*validated));
    if (!in || !in->artifact || !in->manifest || !validated) return hp_fail(diagnostic,ZR_HOT_PATCH_INVALID_ARGUMENT,0,0,0,0);
    m = in->manifest;
    if (m->requirementCount > ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS) {
        return hp_fail(diagnostic, ZR_HOT_PATCH_LIMIT, 0u, 0u,
                       ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS,
                       m->requirementCount);
    }
    if (m->schemaVersion != ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION ||
        (m->flags & ~ZR_HOT_PATCH_FLAG_KNOWN_MASK) != 0u ||
        (m->requirementCount && !m->requirements) || m->patchId == 0u ||
        m->contentHash == 0u || m->baseModuleHash == 0u ||
        m->publicContractHash == 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_INVALID_ARGUMENT,0,0,0,0);
    if (in->artifact->buffer == ZR_NULL || in->artifact->bufferLength == 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_ARTIFACT_INVALID,0,0,0,0);
    /* Capture the exact borrowed range before invoking the verifier callback. */
    contentBytes = in->artifact->buffer;
    contentLength = in->artifact->bufferLength;
    contentHash = ZrCore_ArtifactExecIr_HashBytes(contentBytes,contentLength);
    if (contentHash != m->contentHash || (in->expectedContentHash && contentHash != in->expectedContentHash)) return hp_fail(diagnostic,ZR_HOT_PATCH_BASE_MISMATCH,0,0,m->contentHash,contentHash);
    if (m->baseModuleHash != in->loadedBaseModuleHash || in->artifact->moduleHash != in->loadedBaseModuleHash) return hp_fail(diagnostic,ZR_HOT_PATCH_BASE_MISMATCH,0,0,in->loadedBaseModuleHash,m->baseModuleHash);
    if (m->publicContractHash != in->loadedPublicContractHash || (m->flags & (ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT|ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE))) return hp_fail(diagnostic,ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,0,0,in->loadedPublicContractHash,m->publicContractHash);
    if (m->targetAbiVersion != in->hostAbiVersion || in->artifact->abiVersion != in->hostAbiVersion) return hp_fail(diagnostic,ZR_HOT_PATCH_ABI_MISMATCH,0,0,in->hostAbiVersion,m->targetAbiVersion);
    if (m->targetProfile != in->hostProfile) return hp_fail(diagnostic,ZR_HOT_PATCH_PROFILE_MISMATCH,0,0,in->hostProfile,m->targetProfile);
    if (m->flags & ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE) return hp_fail(diagnostic,ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN,0,0,0,m->flags);
    if (m->flags & ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT) return hp_fail(diagnostic,ZR_HOT_PATCH_IMPORT_FORBIDDEN,0,0,0,m->flags);
    if (in->expectedPatchId && in->expectedPatchId != m->patchId) return hp_fail(diagnostic,ZR_HOT_PATCH_BASE_MISMATCH,0,0,in->expectedPatchId,m->patchId);
    if (!verifySignature || !verifySignature(contentBytes,contentLength,in->signature,in->signatureLength,userData)) return hp_fail(diagnostic,ZR_HOT_PATCH_SIGNATURE_REJECTED,0,0,0,0);
    for (TZrUInt32 i=0u;i<m->requirementCount;i++) {
        const SZrHotPatchCapabilityRequirement *r=&m->requirements[i];
        if (!r->token || r->reserved || !r->requiredBits) return hp_fail(diagnostic,ZR_HOT_PATCH_INVALID_ARGUMENT,r->token,r->sourceOffset,0,0);
        if ((r->requiredBits & ~in->hostAllowedCapabilities) != 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_CAPABILITY_ESCALATION,r->token,r->sourceOffset,in->hostAllowedCapabilities,r->requiredBits);
        required |= r->requiredBits;
    }
    if ((m->requiredCapabilities & ~in->hostAllowedCapabilities) != 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_CAPABILITY_ESCALATION,0,0,in->hostAllowedCapabilities,m->requiredCapabilities);
    required |= m->requiredCapabilities;
    candidate.artifact=in->artifact; candidate.manifest=m; candidate.contentHash=contentHash;
    candidate.contentBytes=contentBytes; candidate.contentLength=contentLength;
    candidate.patchId=m->patchId; candidate.publicContractHash=m->publicContractHash;
    candidate.requiredCapabilities=required; candidate.validationPolicyHash=ZrCore_HotPatch_ComputePolicyHash(in);
    candidate.targetProfile=m->targetProfile; candidate.signatureVerified=ZR_TRUE; candidate.immutableContent=ZR_TRUE;
    *validated = candidate;
    return hp_fail(diagnostic,ZR_HOT_PATCH_OK,0,0,0,0);
}

/* Validates the signed canonical container and its selected ExecIR entry.
 * All policy data needed after the callback is copied or reduced to scalars
 * before the callback can mutate its caller-owned inputs. */
EZrHotPatchCapabilityStatus ZrCore_HotPatch_ValidateZraf(
        const SZrHotPatchZrafValidationInput *input,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatchZraf *validated,
        SZrHotPatchDiagnostic *diagnostic) {
    SZrHotPatchZrafValidationInput inputSnapshot;
    SZrHotPatchCapabilityManifest manifestSnapshot;
    SZrArtifactPublicIdentity expectedIdentitySnapshot;
    SZrHotPatchValidationInput policyInput;
    SZrHotPatchDiagnostic policyDiagnostic;
    SZrArtifactDiagnostic artifactDiagnostic;
    SZrExecIrDiagnostic execIrDiagnostic;
    SZrArtifactView outerView;
    SZrExecIrModule module;
    SZrValidatedHotPatchZraf candidate;
    EZrHotPatchCapabilityStatus status = ZR_HOT_PATCH_INVALID_ARGUMENT;
    EZrArtifactStatus artifactStatus;
    TZrUInt64 outerHash = 0u;
    TZrUInt64 postCallbackHash = 0u;
    TZrUInt64 requiredCapabilities = 0u;
    TZrUInt64 tokenSignatureHash = 0u;
    TZrUInt32 tokenMatchCount = 0u;
    TZrUInt32 exactMatchCount = 0u;
    TZrUInt32 outerLength32 = 0u;
    TZrBool sawToken = ZR_FALSE;
    TZrBool moduleInitialized = ZR_FALSE;
    TZrBool callbackAccepted;

    if (validated != ZR_NULL) memset(validated, 0, sizeof(*validated));
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    memset(&inputSnapshot, 0, sizeof(inputSnapshot));
    memset(&manifestSnapshot, 0, sizeof(manifestSnapshot));
    memset(&expectedIdentitySnapshot, 0, sizeof(expectedIdentitySnapshot));
    memset(&policyInput, 0, sizeof(policyInput));
    memset(&policyDiagnostic, 0, sizeof(policyDiagnostic));
    memset(&artifactDiagnostic, 0, sizeof(artifactDiagnostic));
    memset(&execIrDiagnostic, 0, sizeof(execIrDiagnostic));
    memset(&outerView, 0, sizeof(outerView));
    memset(&candidate, 0, sizeof(candidate));

    if (input == ZR_NULL || validated == ZR_NULL ||
        input->manifest == ZR_NULL ||
        input->expectedPublicIdentity == ZR_NULL) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT,
                         0u, 0u, 0u, 0u);
        goto cleanup;
    }

    /* Snapshot every by-value policy input and root identity before calling
     * any host callback. Requirement rows are consumed into a scalar closure
     * before the callback and never read again. */
    inputSnapshot = *input;
    manifestSnapshot = *input->manifest;
    expectedIdentitySnapshot = *input->expectedPublicIdentity;
    inputSnapshot.manifest = &manifestSnapshot;
    inputSnapshot.expectedPublicIdentity = &expectedIdentitySnapshot;

    if (inputSnapshot.outerBytes == ZR_NULL ||
        inputSnapshot.outerLength == 0u ||
        inputSnapshot.expectedContentHash == 0u ||
        inputSnapshot.entryFunctionToken == 0u ||
        inputSnapshot.entrySignatureHash == 0u) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT,
                         inputSnapshot.entryFunctionToken, 0u,
                         1u, inputSnapshot.outerLength);
        goto cleanup;
    }
    if (inputSnapshot.outerLength >
                (TZrSize)ZR_ARTIFACT_MAX_BYTE_LENGTH ||
        inputSnapshot.outerLength > (TZrSize)UINT32_MAX) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_LIMIT,
                         0u, 0u, ZR_ARTIFACT_MAX_BYTE_LENGTH,
                         inputSnapshot.outerLength);
        goto cleanup;
    }
    /* Hashing and the host callback use UInt32; artifact readers retain size. */
    outerLength32 = (TZrUInt32)inputSnapshot.outerLength;
    if (manifestSnapshot.requirementCount >
        ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_LIMIT, 0u, 0u,
                         ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS,
                         manifestSnapshot.requirementCount);
        goto cleanup;
    }
    if (manifestSnapshot.schemaVersion !=
                ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION ||
        (manifestSnapshot.flags & ~ZR_HOT_PATCH_FLAG_KNOWN_MASK) != 0u ||
        (manifestSnapshot.requirementCount != 0u &&
         manifestSnapshot.requirements == ZR_NULL) ||
        manifestSnapshot.patchId == 0u ||
        manifestSnapshot.contentHash == 0u ||
        manifestSnapshot.baseModuleHash == 0u ||
        manifestSnapshot.publicContractHash == 0u) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT,
                         0u, 0u, ZR_HOT_PATCH_FLAG_KNOWN_MASK,
                         manifestSnapshot.flags);
        goto cleanup;
    }
    if (inputSnapshot.hostAbiVersion == 0u ||
        manifestSnapshot.targetAbiVersion != inputSnapshot.hostAbiVersion) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_ABI_MISMATCH,
                         0u, 0u, inputSnapshot.hostAbiVersion,
                         manifestSnapshot.targetAbiVersion);
        goto cleanup;
    }
    if (manifestSnapshot.targetProfile != inputSnapshot.hostProfile) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_PROFILE_MISMATCH,
                         0u, 0u, inputSnapshot.hostProfile,
                         manifestSnapshot.targetProfile);
        goto cleanup;
    }
    if ((manifestSnapshot.flags & ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE) != 0u) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN,
                         0u, 0u, 0u, manifestSnapshot.flags);
        goto cleanup;
    }
    if ((manifestSnapshot.flags & ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT) != 0u) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_IMPORT_FORBIDDEN,
                         0u, 0u, 0u, manifestSnapshot.flags);
        goto cleanup;
    }
    if (manifestSnapshot.baseModuleHash !=
                inputSnapshot.loadedBaseModuleHash) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_BASE_MISMATCH,
                         0u, 0u, inputSnapshot.loadedBaseModuleHash,
                         manifestSnapshot.baseModuleHash);
        goto cleanup;
    }
    if (expectedIdentitySnapshot.moduleHash !=
                inputSnapshot.loadedBaseModuleHash) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_BASE_MISMATCH,
                         0u, 0u, inputSnapshot.loadedBaseModuleHash,
                         expectedIdentitySnapshot.moduleHash);
        goto cleanup;
    }
    if (manifestSnapshot.publicContractHash !=
                inputSnapshot.loadedPublicContractHash) {
        status = hp_fail(diagnostic,
                         ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,
                         0u, 0u, inputSnapshot.loadedPublicContractHash,
                         manifestSnapshot.publicContractHash);
        goto cleanup;
    }
    if (expectedIdentitySnapshot.callableContractHash !=
                manifestSnapshot.publicContractHash) {
        status = hp_fail(diagnostic,
                         ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,
                         0u, 0u, manifestSnapshot.publicContractHash,
                         expectedIdentitySnapshot.callableContractHash);
        goto cleanup;
    }
    if ((manifestSnapshot.flags &
         (ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT |
          ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE)) != 0u) {
        status = hp_fail(diagnostic,
                         ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,
                         0u, 0u, 0u, manifestSnapshot.flags);
        goto cleanup;
    }
    if (inputSnapshot.expectedPatchId != 0u &&
        inputSnapshot.expectedPatchId != manifestSnapshot.patchId) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_BASE_MISMATCH,
                         0u, 0u, inputSnapshot.expectedPatchId,
                         manifestSnapshot.patchId);
        goto cleanup;
    }

    outerHash = ZrCore_ArtifactExecIr_HashBytes(
            inputSnapshot.outerBytes, outerLength32);
    if (outerHash == 0u || outerHash != manifestSnapshot.contentHash) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_BASE_MISMATCH,
                         0u, 0u, manifestSnapshot.contentHash, outerHash);
        goto cleanup;
    }
    if (outerHash != inputSnapshot.expectedContentHash) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_BASE_MISMATCH,
                         0u, 0u, inputSnapshot.expectedContentHash,
                         outerHash);
        goto cleanup;
    }

    /* Reuse the shared capability closure without changing legacy Validate's
     * callback/error ordering. This new API consumes the closure before its
     * callback so a callback cannot change requirement rows after admission. */
    status = ZrCore_HotPatch_ComputeRequiredCapabilities(
            &manifestSnapshot, inputSnapshot.hostAllowedCapabilities,
            &requiredCapabilities, &policyDiagnostic);
    if (status != ZR_HOT_PATCH_OK) {
        status = hp_fail(diagnostic, status, policyDiagnostic.token,
                         policyDiagnostic.sourceOffset,
                         policyDiagnostic.expected,
                         policyDiagnostic.actual);
        goto cleanup;
    }

    policyInput.manifest = &manifestSnapshot;
    policyInput.loadedBaseModuleHash =
            inputSnapshot.loadedBaseModuleHash;
    policyInput.loadedPublicContractHash =
            inputSnapshot.loadedPublicContractHash;
    policyInput.hostAbiVersion = inputSnapshot.hostAbiVersion;
    policyInput.hostProfile = inputSnapshot.hostProfile;
    policyInput.hostAllowedCapabilities =
            inputSnapshot.hostAllowedCapabilities;
    candidate.validationPolicyHash =
            ZrCore_HotPatch_ComputePolicyHash(&policyInput);

    /* The canonical container and nested graph are checked only after the
     * host authenticates this bounded, complete byte span. */
    callbackAccepted = verifySignature != ZR_NULL &&
            verifySignature(inputSnapshot.outerBytes,
                            outerLength32,
                            inputSnapshot.signature,
                            inputSnapshot.signatureLength,
                            userData);
    postCallbackHash = ZrCore_ArtifactExecIr_HashBytes(
            inputSnapshot.outerBytes, outerLength32);
    if (postCallbackHash != outerHash) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_CONTENT_CHANGED,
                         0u, 0u, outerHash, postCallbackHash);
        goto cleanup;
    }
    if (!callbackAccepted) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_SIGNATURE_REJECTED,
                         0u, 0u, 0u, 0u);
        goto cleanup;
    }

    artifactStatus = ZrCore_Artifact_Read(
            inputSnapshot.outerBytes, inputSnapshot.outerLength,
            &outerView, &artifactDiagnostic);
    if (artifactStatus != ZR_ARTIFACT_STATUS_OK) {
        status = hp_zraf_artifact_fail(diagnostic, &artifactDiagnostic);
        goto cleanup;
    }
    if (outerView.kind != ZR_ARTIFACT_KIND_ZRO) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_ARTIFACT_INVALID,
                         0u, 0u, ZR_ARTIFACT_KIND_ZRO, outerView.kind);
        goto cleanup;
    }
    artifactStatus = ZrCore_Artifact_ValidatePublicIdentity(
            &outerView, &expectedIdentitySnapshot, &artifactDiagnostic);
    if (artifactStatus != ZR_ARTIFACT_STATUS_OK) {
        status = hp_zraf_artifact_fail(diagnostic, &artifactDiagnostic);
        goto cleanup;
    }

    ZrCore_ExecIr_ModuleInit(&module);
    moduleInitialized = ZR_TRUE;
    artifactStatus = ZrCore_Module_OpenExecIrArtifact(
            inputSnapshot.outerBytes, inputSnapshot.outerLength,
            &expectedIdentitySnapshot, &module, &artifactDiagnostic);
    if (artifactStatus != ZR_ARTIFACT_STATUS_OK) {
        status = hp_zraf_artifact_fail(diagnostic, &artifactDiagnostic);
        goto cleanup;
    }
    if (module.moduleHash != inputSnapshot.loadedBaseModuleHash ||
        module.contract.abiVersion != inputSnapshot.hostAbiVersion) {
        status = hp_fail(diagnostic,
                         module.contract.abiVersion !=
                                 inputSnapshot.hostAbiVersion
                                 ? ZR_HOT_PATCH_ABI_MISMATCH
                                 : ZR_HOT_PATCH_BASE_MISMATCH,
                         0u, 0u,
                         module.contract.abiVersion !=
                                 inputSnapshot.hostAbiVersion
                                 ? inputSnapshot.hostAbiVersion
                                 : inputSnapshot.loadedBaseModuleHash,
                         module.contract.abiVersion !=
                                 inputSnapshot.hostAbiVersion
                                 ? module.contract.abiVersion
                                 : module.moduleHash);
        goto cleanup;
    }
    if (!ZrCore_ExecIr_VerifyModule(&module, &execIrDiagnostic)) {
        status = hp_fail(diagnostic, ZR_HOT_PATCH_ARTIFACT_INVALID,
                         0u, execIrDiagnostic.instructionId,
                         0u, (TZrUInt64)execIrDiagnostic.code);
        goto cleanup;
    }

    for (TZrUInt32 index = 0u; index < module.functionCount; ++index) {
        const SZrExecIrFunction *function = &module.functions[index];
        if (function->functionToken != inputSnapshot.entryFunctionToken) {
            continue;
        }
        sawToken = ZR_TRUE;
        tokenMatchCount++;
        tokenSignatureHash = function->signatureHash;
        if (function->signatureHash == inputSnapshot.entrySignatureHash) {
            exactMatchCount++;
        }
    }
    if (exactMatchCount != 1u) {
        if (sawToken && tokenMatchCount == 1u) {
            status = hp_zraf_selector_fail(
                    diagnostic, inputSnapshot.entryFunctionToken,
                    inputSnapshot.entrySignatureHash, tokenSignatureHash);
        } else if (sawToken) {
            status = hp_fail(diagnostic, ZR_HOT_PATCH_ARTIFACT_INVALID,
                             inputSnapshot.entryFunctionToken, 0u,
                             1u, tokenMatchCount);
        } else {
            status = hp_zraf_selector_fail(
                    diagnostic, inputSnapshot.entryFunctionToken,
                    inputSnapshot.entryFunctionToken, 0u);
        }
        goto cleanup;
    }

    candidate.outerBytes = inputSnapshot.outerBytes;
    candidate.outerLength = inputSnapshot.outerLength;
    candidate.outerContentHash = outerHash;
    candidate.publicIdentity = outerView.identity;
    candidate.entryFunctionToken = inputSnapshot.entryFunctionToken;
    candidate.entrySignatureHash = inputSnapshot.entrySignatureHash;
    candidate.patchId = manifestSnapshot.patchId;
    candidate.publicContractHash = manifestSnapshot.publicContractHash;
    candidate.requiredCapabilities = requiredCapabilities;
    candidate.targetAbiVersion = manifestSnapshot.targetAbiVersion;
    candidate.targetProfile = manifestSnapshot.targetProfile;
    candidate.signatureVerified = ZR_TRUE;
    candidate.canonicalExecIrVerified = ZR_TRUE;
    status = ZR_HOT_PATCH_OK;

cleanup:
    if (moduleInitialized) {
        ZrCore_ExecIr_FreeModule(&module);
    }
    if (status == ZR_HOT_PATCH_OK) {
        *validated = candidate;
        return hp_fail(diagnostic, ZR_HOT_PATCH_OK, 0u, 0u, 0u, 0u);
    }
    if (validated != ZR_NULL) memset(validated, 0, sizeof(*validated));
    return status;
}

EZrHotPatchCapabilityStatus ZrCore_HotPatch_RecheckZrafContent(
        const SZrValidatedHotPatchZraf *validated,
        SZrHotPatchDiagnostic *diagnostic) {
    TZrUInt64 actualHash;
    TZrUInt32 outerLength32;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (validated == ZR_NULL || validated->outerBytes == ZR_NULL ||
        validated->outerLength == 0u ||
        validated->outerLength > (TZrSize)ZR_ARTIFACT_MAX_BYTE_LENGTH ||
        validated->outerLength > (TZrSize)UINT32_MAX ||
        validated->outerContentHash == 0u ||
        !validated->signatureVerified ||
        !validated->canonicalExecIrVerified) {
        return hp_fail(diagnostic, ZR_HOT_PATCH_INVALID_ARGUMENT,
                       0u, 0u, 0u, 0u);
    }
    outerLength32 = (TZrUInt32)validated->outerLength;
    actualHash = ZrCore_ArtifactExecIr_HashBytes(
            validated->outerBytes, outerLength32);
    if (actualHash != validated->outerContentHash) {
        return hp_fail(diagnostic, ZR_HOT_PATCH_CONTENT_CHANGED,
                       validated->entryFunctionToken, 0u,
                       validated->outerContentHash, actualHash);
    }
    return hp_fail(diagnostic, ZR_HOT_PATCH_OK, 0u, 0u, 0u, 0u);
}

/* 供拒绝日志展示，机器判定继续使用状态枚举与结构化诊断。 */
const TZrChar *ZrCore_HotPatch_StatusName(EZrHotPatchCapabilityStatus s) { switch(s){case ZR_HOT_PATCH_OK:return "ok";case ZR_HOT_PATCH_SIGNATURE_REJECTED:return "signature-rejected";case ZR_HOT_PATCH_BASE_MISMATCH:return "base-mismatch";case ZR_HOT_PATCH_CAPABILITY_ESCALATION:return "capability-escalation";case ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE:return "public-contract-change";case ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN:return "machine-code-forbidden";case ZR_HOT_PATCH_IMPORT_FORBIDDEN:return "import-forbidden";case ZR_HOT_PATCH_LIMIT:return "limit";case ZR_HOT_PATCH_CONTENT_CHANGED:return "content-changed";default:return "invalid-hotpatch";} }
