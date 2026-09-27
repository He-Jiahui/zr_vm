#include "zr_vm_core/artifact_schema.h"

#include "artifact_schema_internal.h"

/* 身份边界的哈希失败保留预期值和实际值，供调用方识别不兼容字段。 */
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

/* Canonical consumer 在完整解码后核对外部预期的根身份；这里只比较描述符，不复验原始字节。 */
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
    /* TODO: 根 ID 或 token 失配只给 ILLEGAL_TOKEN；需确认诊断是否应填 expectedToken/actualToken。 */
    if (expected->canonicalTypeId != view->identity.canonicalTypeId ||
        expected->typeRefToken != view->identity.typeRefToken ||
        expected->typeSpecToken != view->identity.typeSpecToken ||
        expected->signatureToken != view->identity.signatureToken)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN, 0u, 0u, 0u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 签名和传输指纹共享的确定性字节哈希；调用方须提供有效字节范围，空指针配正长度返回零。 */
/* TODO: scheduler 指纹调用方直接哈希 C 结构；跨 ABI 复用产物前需核对字段布局、填充和端序。 */
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

/* 元数据摘要按线格式字段顺序吸收整数，避免依赖主机端序。 */
static void artifact_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 shift;

    for (shift = 0u; shift < 32u; shift += 8u) {
        *hash ^= (TZrByte)((value >> shift) & 0xffu);
        *hash *= 1099511628211ULL;
    }
}

/* 与 32 位字段使用同一摘要规则，供元数据状态吸收各身份哈希。 */
static void artifact_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 shift;

    for (shift = 0u; shift < 64u; shift += 8u) {
        *hash ^= (TZrByte)((value >> shift) & 0xffu);
        *hash *= 1099511628211ULL;
    }
}

/* 投影方产生状态摘要，图校验方复算；排除摘要字段本身，以零表示无输入。 */
TZrUInt64 ZrCore_Artifact_ComputeMetadataStateHash(
        const SZrArtifactMetadataStateRow *state) {
    TZrUInt64 hash = 1469598103934665603ULL;

    if (state == ZR_NULL) return 0u;
    artifact_hash_u32(&hash, state->typeToken);
    artifact_hash_u32(&hash, (TZrUInt32)state->preservationState);
    artifact_hash_u32(&hash, (TZrUInt32)state->category);
    artifact_hash_u32(&hash, state->metadataGeneration);
    artifact_hash_u32(&hash, state->retainedMemberCount);
    artifact_hash_u32(&hash, state->retainedPropertyCount);
    artifact_hash_u32(&hash, state->retainedMetaRecordCount);
    artifact_hash_u32(&hash, state->flags);
    artifact_hash_u64(&hash, state->typeSignatureHash);
    artifact_hash_u64(&hash, state->layoutHash);
    artifact_hash_u64(&hash, state->callableContractHash);
    return hash;
}

/* 将记录身份、blob 内偏移和载荷绑定，供 metadata graph 检测被移动或改写的记录。 */
/* 调用方保持 payload 在调用期可读；长度不匹配或非空长度配空指针时返回零。 */
TZrUInt64 ZrCore_Artifact_ComputeMetadataRecordHash(
        const SZrArtifactMetadataRecordRow *record,
        const TZrByte *payload,
        TZrSize payloadLength) {
    TZrUInt64 hash = 1469598103934665603ULL;
    TZrSize index;

    if (record == ZR_NULL || payloadLength != record->payloadLength ||
        (payload == ZR_NULL && payloadLength > 0u)) {
        return 0u;
    }
    artifact_hash_u32(&hash, record->ownerToken);
    artifact_hash_u32(&hash, (TZrUInt32)record->kind);
    artifact_hash_u32(&hash, (TZrUInt32)record->retention);
    artifact_hash_u32(&hash, record->flags);
    artifact_hash_u32(&hash, record->payloadOffset);
    artifact_hash_u32(&hash, record->payloadLength);
    artifact_hash_u32(&hash, record->metadataGeneration);
    artifact_hash_u32(&hash, record->reserved0);
    for (index = 0u; index < payloadLength; ++index) {
        hash ^= payload[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* 将公开状态映射为稳定的静态诊断文本；仓内尚无直接调用，外部调用者可查询任意状态。 */
/* BUG: 新增的五种 scheduler/transport 失配状态在此表缺席，合法状态会被报告为 unknown。 */
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

/* 文本产物展示已知节名；未知可选节保留 unknown，不影响二进制解码。 */
const TZrChar *ZrCore_Artifact_SectionName(TZrUInt32 sectionKind) {
    static const TZrChar *names[] = {
            "invalid", "string-heap", "type-def-table", "type-ref-table", "type-spec-table",
            "member-def-table", "property-def-table", "signature-heap", "contract-table",
            "layout-table", "code-table", "relocation-binding-table", "debug-map",
            "syntax-tree", "semantic-ir", "domain-transfer-table", "scheduler-contract-table",
            "metadata-state-table", "metadata-record-table", "metadata-blob-heap",
            "layout-map-heap", "call-binding-table"};
    if (sectionKind >= (TZrUInt32)(sizeof(names) / sizeof(names[0]))) return "unknown";
    return names[sectionKind];
}
