#include "zr_vm_core/artifact_schema.h"

#include "artifact_schema_internal.h"

static EZrArtifactStatus artifact_identity_hash_mismatch(SZrArtifactDiagnostic *diagnostic,
                                                         EZrArtifactStatus status,
                                                         TZrUInt64 expected,
                                                         TZrUInt64 actual) {
    zr_artifact_fail(diagnostic, status, 0u, 0u, 0u);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return status;
}

EZrArtifactStatus ZrCore_Artifact_ValidatePublicIdentity(const SZrArtifactView *view,
                                                         const SZrArtifactPublicIdentity *expected,
                                                         SZrArtifactDiagnostic *diagnostic) {
    zr_artifact_diagnostic_clear(diagnostic);
    if (view == ZR_NULL || expected == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u);
    if (expected->typeRefHash != view->identity.typeRefHash)
        return artifact_identity_hash_mismatch(diagnostic, ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
                                               expected->typeRefHash, view->identity.typeRefHash);
    if (expected->typeSpecHash != view->identity.typeSpecHash)
        return artifact_identity_hash_mismatch(diagnostic, ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH,
                                               expected->typeSpecHash, view->identity.typeSpecHash);
    if (expected->signatureHash != view->identity.signatureHash)
        return artifact_identity_hash_mismatch(diagnostic, ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
                                               expected->signatureHash, view->identity.signatureHash);
    if (expected->layoutVersion != view->identity.layoutVersion) {
        zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH, 0u, 0u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = expected->layoutVersion;
            diagnostic->actualVersion = view->identity.layoutVersion;
        }
        return ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH;
    }
    if (expected->layoutHash != view->identity.layoutHash)
        return artifact_identity_hash_mismatch(diagnostic, ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
                                               expected->layoutHash, view->identity.layoutHash);
    if (expected->callableContractHash != view->identity.callableContractHash)
        return artifact_identity_hash_mismatch(diagnostic, ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
                                               expected->callableContractHash,
                                               view->identity.callableContractHash);
    if (expected->moduleHash != view->identity.moduleHash)
        return artifact_identity_hash_mismatch(diagnostic, ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH,
                                               expected->moduleHash, view->identity.moduleHash);
    if (expected->canonicalTypeId != view->identity.canonicalTypeId ||
        expected->typeRefToken != view->identity.typeRefToken ||
        expected->typeSpecToken != view->identity.typeSpecToken ||
        expected->signatureToken != view->identity.signatureToken)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN, 0u, 0u, 0u);
    return ZR_ARTIFACT_STATUS_OK;
}

TZrUInt64 ZrCore_Artifact_HashBytes(const TZrByte *bytes, TZrSize byteLength) {
    TZrUInt64 hash = 1469598103934665603ULL;
    TZrSize index;
    if (bytes == ZR_NULL && byteLength > 0u) return 0u;
    for (index = 0u; index < byteLength; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

const TZrChar *ZrCore_Artifact_StatusName(EZrArtifactStatus status) {
    static const TZrChar *names[] = {
            "ok", "invalid-argument", "bad-magic", "unsupported-version", "invalid-kind",
            "truncated", "count-limit", "unknown-mandatory-section", "duplicate-section",
            "forbidden-section", "invalid-section", "section-overlap", "illegal-token",
            "truncated-blob", "invalid-signature", "type-ref-hash-mismatch",
            "type-spec-hash-mismatch", "signature-hash-mismatch", "layout-version-mismatch",
            "layout-hash-mismatch", "contract-hash-mismatch", "module-hash-mismatch",
            "buffer-too-small", "invalid-text"};
    if ((TZrUInt32)status >= (TZrUInt32)(sizeof(names) / sizeof(names[0]))) return "unknown";
    return names[status];
}

const TZrChar *ZrCore_Artifact_SectionName(TZrUInt32 sectionKind) {
    static const TZrChar *names[] = {
            "invalid", "string-heap", "type-def-table", "type-ref-table", "type-spec-table",
            "member-def-table", "property-def-table", "signature-heap", "contract-table",
            "layout-table", "code-table", "relocation-binding-table", "debug-map",
            "syntax-tree", "semantic-ir"};
    if (sectionKind >= (TZrUInt32)(sizeof(names) / sizeof(names[0]))) return "unknown";
    return names[sectionKind];
}
