#include "zr_vm_core/capability_manifest.h"

#include <string.h>

static EZrHotPatchCapabilityStatus hp_fail(SZrHotPatchDiagnostic *d,
                                             EZrHotPatchCapabilityStatus s,
                                             TZrUInt32 token, TZrUInt32 source,
                                             TZrUInt64 expected, TZrUInt64 actual) {
    if (d) { d->status=s; d->token=token; d->sourceOffset=source; d->expected=expected; d->actual=actual; }
    return s;
}

TZrUInt64 ZrCore_HotPatch_ComputePolicyHash(const SZrHotPatchValidationInput *in) {
    TZrUInt64 h = UINT64_C(1469598103934665603);
    if (!in || !in->manifest) return 0u;
#define HP_MIX(v) do { TZrUInt64 x=(TZrUInt64)(v); for(TZrUInt32 i=0;i<8u;i++){h^=(TZrByte)(x>>(i*8u));h*=UINT64_C(1099511628211);}} while(0)
    HP_MIX(in->manifest->schemaVersion); HP_MIX(in->manifest->flags);
    HP_MIX(in->manifest->targetAbiVersion); HP_MIX(in->manifest->targetProfile);
    HP_MIX(in->hostAbiVersion); HP_MIX(in->hostProfile); HP_MIX(in->hostAllowedCapabilities);
    HP_MIX(in->loadedBaseModuleHash); HP_MIX(in->loadedPublicContractHash);
#undef HP_MIX
    return h;
}

EZrHotPatchCapabilityStatus ZrCore_HotPatch_Validate(
        const SZrHotPatchValidationInput *in,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatch *validated,
        SZrHotPatchDiagnostic *diagnostic) {
    const SZrHotPatchCapabilityManifest *m;
    TZrUInt64 contentHash = 0u;
    TZrUInt64 required = 0u;
    SZrValidatedHotPatch candidate;
    if (validated) memset(validated, 0, sizeof(*validated));
    if (!in || !in->artifact || !in->manifest || !validated) return hp_fail(diagnostic,ZR_HOT_PATCH_INVALID_ARGUMENT,0,0,0,0);
    m = in->manifest;
    if (m->schemaVersion != ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION ||
        (m->flags & ~ZR_HOT_PATCH_FLAG_KNOWN_MASK) != 0u ||
        m->requirementCount > 1048576u ||
        (m->requirementCount && !m->requirements) || m->patchId == 0u ||
        m->contentHash == 0u || m->baseModuleHash == 0u ||
        m->publicContractHash == 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_INVALID_ARGUMENT,0,0,0,0);
    if (in->artifact->buffer == ZR_NULL || in->artifact->bufferLength == 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_ARTIFACT_INVALID,0,0,0,0);
    contentHash = ZrCore_ArtifactExecIr_HashBytes(in->artifact->buffer,in->artifact->bufferLength);
    if (contentHash != m->contentHash || (in->expectedContentHash && contentHash != in->expectedContentHash)) return hp_fail(diagnostic,ZR_HOT_PATCH_BASE_MISMATCH,0,0,m->contentHash,contentHash);
    if (m->baseModuleHash != in->loadedBaseModuleHash || in->artifact->moduleHash != in->loadedBaseModuleHash) return hp_fail(diagnostic,ZR_HOT_PATCH_BASE_MISMATCH,0,0,in->loadedBaseModuleHash,m->baseModuleHash);
    if (m->publicContractHash != in->loadedPublicContractHash || (m->flags & (ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT|ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE))) return hp_fail(diagnostic,ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,0,0,in->loadedPublicContractHash,m->publicContractHash);
    if (m->targetAbiVersion != in->hostAbiVersion || in->artifact->abiVersion != in->hostAbiVersion) return hp_fail(diagnostic,ZR_HOT_PATCH_ABI_MISMATCH,0,0,in->hostAbiVersion,m->targetAbiVersion);
    if (m->targetProfile != in->hostProfile) return hp_fail(diagnostic,ZR_HOT_PATCH_PROFILE_MISMATCH,0,0,in->hostProfile,m->targetProfile);
    if (m->flags & ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE) return hp_fail(diagnostic,ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN,0,0,0,m->flags);
    if (m->flags & ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT) return hp_fail(diagnostic,ZR_HOT_PATCH_IMPORT_FORBIDDEN,0,0,0,m->flags);
    if (in->expectedPatchId && in->expectedPatchId != m->patchId) return hp_fail(diagnostic,ZR_HOT_PATCH_BASE_MISMATCH,0,0,in->expectedPatchId,m->patchId);
    if (!verifySignature || !verifySignature(in->artifact->buffer,in->artifact->bufferLength,in->signature,in->signatureLength,userData)) return hp_fail(diagnostic,ZR_HOT_PATCH_SIGNATURE_REJECTED,0,0,0,0);
    for (TZrUInt32 i=0u;i<m->requirementCount;i++) {
        const SZrHotPatchCapabilityRequirement *r=&m->requirements[i];
        if (!r->token || r->reserved || !r->requiredBits) return hp_fail(diagnostic,ZR_HOT_PATCH_INVALID_ARGUMENT,r->token,r->sourceOffset,0,0);
        if ((r->requiredBits & ~in->hostAllowedCapabilities) != 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_CAPABILITY_ESCALATION,r->token,r->sourceOffset,in->hostAllowedCapabilities,r->requiredBits);
        required |= r->requiredBits;
    }
    if ((m->requiredCapabilities & ~in->hostAllowedCapabilities) != 0u) return hp_fail(diagnostic,ZR_HOT_PATCH_CAPABILITY_ESCALATION,0,0,in->hostAllowedCapabilities,m->requiredCapabilities);
    required |= m->requiredCapabilities;
    candidate.artifact=in->artifact; candidate.manifest=m; candidate.contentHash=contentHash; candidate.requiredCapabilities=required; candidate.validationPolicyHash=ZrCore_HotPatch_ComputePolicyHash(in); candidate.targetProfile=m->targetProfile; candidate.signatureVerified=ZR_TRUE; candidate.immutableContent=ZR_TRUE;
    *validated = candidate;
    return hp_fail(diagnostic,ZR_HOT_PATCH_OK,0,0,0,0);
}

const TZrChar *ZrCore_HotPatch_StatusName(EZrHotPatchCapabilityStatus s) { switch(s){case ZR_HOT_PATCH_OK:return "ok";case ZR_HOT_PATCH_SIGNATURE_REJECTED:return "signature-rejected";case ZR_HOT_PATCH_BASE_MISMATCH:return "base-mismatch";case ZR_HOT_PATCH_CAPABILITY_ESCALATION:return "capability-escalation";case ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE:return "public-contract-change";case ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN:return "machine-code-forbidden";case ZR_HOT_PATCH_IMPORT_FORBIDDEN:return "import-forbidden";default:return "invalid-hotpatch";} }
