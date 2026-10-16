#include "zr_vm_core/canonical_consumer.h"

#include <string.h>

/* 每次公开查询从空诊断开始，允许调用方复用同一个 diagnostic。 */
static void canonical_consumer_clear_diagnostic(SZrArtifactDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

/* 将失败状态及 section/row 位置集中写入可选诊断对象。 */
static EZrArtifactStatus canonical_consumer_fail(SZrArtifactDiagnostic *diagnostic,
                                                 EZrArtifactStatus status,
                                                 TZrUInt32 sectionKind,
                                                 TZrUInt32 rowIndex) {
    canonical_consumer_clear_diagnostic(diagnostic);
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->sectionKind = sectionKind;
        diagnostic->rowIndex = rowIndex;
    }
    return status;
}

/* 根签名哈希不匹配时保留预期与实际值，供 VM/AOT 报告同一错误。 */
static EZrArtifactStatus canonical_consumer_hash_fail(SZrArtifactDiagnostic *diagnostic,
                                                      TZrUInt64 expected,
                                                      TZrUInt64 actual) {
    canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
                            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, 0u);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH;
}

/* 调度器可由本模块 TypeDef 或跨模块 TypeRef 标识，RID 必须非零。 */
static TZrBool canonical_consumer_scheduler_type_token_is_valid(
        TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_RID(token) != 0u &&
                     (table == ZR_METADATA_TABLE_TYPE_DEF ||
                      table == ZR_METADATA_TABLE_TYPE_REF));
}

/* 以 token、版本和哈希筛选布局；零表示该维度不限制，返回行的值副本。 */
static TZrBool canonical_consumer_find_layout(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        TZrUInt32 version,
        TZrUInt64 hash,
        SZrArtifactLayoutRow *outLayout) {
    TZrUInt32 index;

    for (index = 0u; index < projection->layouts.elementCount; ++index) {
        SZrArtifactLayoutRow row;
        if (ZrCore_Artifact_ReadLayoutRow(&projection->layouts, index, &row, ZR_NULL) !=
            ZR_ARTIFACT_STATUS_OK) {
            continue;
        }
        if ((typeToken == 0u || row.typeToken == typeToken) &&
            (version == 0u || row.version == version) &&
            (hash == 0u || row.layoutHash == hash)) {
            *outLayout = row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 以签名 token 和可选哈希筛选 callable 契约，供根类型及 ABI 查询共用。 */
static TZrBool canonical_consumer_find_contract(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken signatureToken,
        TZrUInt64 hash,
        SZrArtifactContractRow *outContract) {
    TZrUInt32 index;

    for (index = 0u; index < projection->contracts.elementCount; ++index) {
        SZrArtifactContractRow row;
        if (ZrCore_Artifact_ReadContractRow(&projection->contracts, index, &row, ZR_NULL) !=
            ZR_ARTIFACT_STATUS_OK) {
            continue;
        }
        if ((signatureToken == 0u || row.signatureToken == signatureToken) &&
            (hash == 0u || row.contractHash == hash)) {
            *outContract = row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 可选域传递表按类型 token 定位；缺表或缺行统一返回假。 */
static TZrBool canonical_consumer_find_domain_transfer(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactDomainTransferRow *outContract) {
    TZrUInt32 index;

    if (projection == ZR_NULL || outContract == ZR_NULL || typeToken == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < projection->domainTransfers.elementCount; ++index) {
        SZrArtifactDomainTransferRow row;
        if (ZrCore_Artifact_ReadDomainTransferRow(
                    &projection->domainTransfers, index, &row, ZR_NULL) ==
                    ZR_ARTIFACT_STATUS_OK &&
            row.typeToken == typeToken) {
            *outContract = row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 调度器表按精确 token 定位，避免仅凭名称或 canonical ID 误绑定。 */
static TZrBool canonical_consumer_find_scheduler_contract(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken schedulerTypeToken,
        SZrArtifactSchedulerContractRow *outContract) {
    TZrUInt32 index;

    if (projection == ZR_NULL || outContract == ZR_NULL || schedulerTypeToken == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < projection->schedulerContracts.elementCount; ++index) {
        SZrArtifactSchedulerContractRow row;
        if (ZrCore_Artifact_ReadSchedulerContractRow(
                    &projection->schedulerContracts, index, &row, ZR_NULL) ==
                    ZR_ARTIFACT_STATUS_OK &&
            row.schedulerTypeToken == schedulerTypeToken) {
            *outContract = row;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* TypeRef/TypeSpec 的域传递契约可由同 canonical ID 的本地 TypeDef 承载。 */
static TZrMetadataToken canonical_consumer_find_type_def_token_by_id(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 canonicalTypeId) {
    TZrUInt32 index;

    if (projection == ZR_NULL || canonicalTypeId == 0u) {
        return 0u;
    }
    for (index = 0u; index < projection->typeDefs.elementCount; ++index) {
        SZrArtifactTypeDefRow row;
        if (ZrCore_Artifact_ReadTypeDefRow(
                    &projection->typeDefs, index, &row, ZR_NULL) ==
                    ZR_ARTIFACT_STATUS_OK &&
            row.canonicalTypeId == canonicalTypeId) {
            return row.token;
        }
    }
    return 0u;
}

/* 优先采用当前 token 的契约；TypeRef/TypeSpec 缺行时按 canonical ID 回退 TypeDef。 */
static void canonical_consumer_project_domain_transfer(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        TZrUInt32 canonicalTypeId,
        SZrCanonicalTypeProjection *outType) {
    TZrMetadataToken definitionToken;

    outType->hasDomainTransfer = canonical_consumer_find_domain_transfer(
            projection, typeToken, &outType->domainTransfer);
    if (outType->hasDomainTransfer ||
        ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_DEF) {
        return;
    }
    definitionToken = canonical_consumer_find_type_def_token_by_id(
            projection, canonicalTypeId);
    if (definitionToken != 0u) {
        outType->hasDomainTransfer = canonical_consumer_find_domain_transfer(
                projection, definitionToken, &outType->domainTransfer);
    }
}

/* 将根函数签名中的 effect/参数约束与契约表逐项对齐。 */
static EZrArtifactStatus canonical_consumer_validate_callable_contract(
        const SZrCanonicalTypeProjection *type,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrUInt32 effectMask = ZR_ARTIFACT_CONTRACT_FLAG_THROWS |
                                 ZR_ARTIFACT_CONTRACT_FLAG_ASYNC |
                                 ZR_ARTIFACT_CONTRACT_FLAG_GENERATOR;
    SZrArtifactCallableSignatureSummary summary;
    EZrArtifactStatus status;

    if (type == ZR_NULL || !type->hasContract || type->signatureLength == 0U ||
        type->signatureData[0] != ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION) {
        return ZR_ARTIFACT_STATUS_OK;
    }
    status = ZrCore_Artifact_ReadCallableSignatureSummary(
            type->signatureData,
            type->signatureLength,
            &summary,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if (type->contract.receiverEffect != (TZrUInt32)summary.receiverEffect ||
        type->contract.refExportEffect != (TZrUInt32)summary.refExportEffect ||
        (type->contract.flags & effectMask) != summary.effectFlags ||
        ((type->contract.flags & ZR_ARTIFACT_CONTRACT_FLAG_SCOPED) != 0U) !=
                (summary.hasScopedParameter != ZR_FALSE) ||
        type->contract.parameterCount != summary.parameterCount) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                0U);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

/* 从 TypeRef/TypeSpec 行投影签名视图，并核根身份哈希及关联布局/契约。 */
static EZrArtifactStatus canonical_consumer_project_identity_row(
        const SZrCanonicalConsumerProjection *projection,
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactTypeIdentityRow row;
    TZrUInt64 actualHash;

    if (ZrCore_Artifact_ReadTypeIdentityRow(section, rowIndex, &row, diagnostic) !=
        ZR_ARTIFACT_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status : ZR_ARTIFACT_STATUS_INVALID_SECTION;
    }
    if ((TZrUInt64)row.signatureOffset + row.signatureLength > projection->signatures.byteLength) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
                                       section->kind, rowIndex);
    }

    memset(outType, 0, sizeof(*outType));
    outType->canonicalTypeId = row.canonicalTypeId;
    outType->typeToken = row.token;
    outType->signatureToken = row.signatureToken;
    outType->capabilityFlags = row.flags;
    /* signatureData 仍指向调用方 buffer；局部投影不复制签名字节。 */
    outType->signatureData = projection->signatures.data + row.signatureOffset;
    outType->signatureLength = row.signatureLength;
    actualHash = ZrCore_Artifact_HashBytes(outType->signatureData, outType->signatureLength);
    outType->signatureHash = actualHash;
    if (row.canonicalTypeId == projection->artifact.identity.canonicalTypeId &&
        actualHash != projection->artifact.identity.signatureHash) {
        return canonical_consumer_hash_fail(diagnostic,
                                            projection->artifact.identity.signatureHash,
                                            actualHash);
    }
    outType->hasLayout = canonical_consumer_find_layout(projection,
                                                        row.token,
                                                        row.layoutVersion,
                                                        row.layoutHash,
                                                        &outType->layout);
    /* 根身份与 TypeDef 的 token 可不同；根布局按版本和哈希跨 token 匹配。 */
    if (!outType->hasLayout && row.canonicalTypeId == projection->artifact.identity.canonicalTypeId) {
        outType->hasLayout = canonical_consumer_find_layout(
                projection, 0u, row.layoutVersion, row.layoutHash, &outType->layout);
    }
    outType->hasContract = canonical_consumer_find_contract(
            projection, row.signatureToken, 0u, &outType->contract);
    canonical_consumer_project_domain_transfer(
            projection, row.token, row.canonicalTypeId, outType);
    return ZR_ARTIFACT_STATUS_OK;
}

/* TypeDef 没有独立签名字节片段，投影时使用行内哈希与构造签名 token。 */
static EZrArtifactStatus canonical_consumer_project_type_def(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 rowIndex,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactTypeDefRow row;

    if (ZrCore_Artifact_ReadTypeDefRow(&projection->typeDefs, rowIndex, &row, diagnostic) !=
        ZR_ARTIFACT_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status : ZR_ARTIFACT_STATUS_INVALID_SECTION;
    }
    memset(outType, 0, sizeof(*outType));
    outType->canonicalTypeId = row.canonicalTypeId;
    outType->typeToken = row.token;
    outType->signatureToken = row.constructorSignatureToken;
    outType->capabilityFlags = row.flags;
    outType->signatureHash = row.typeSignatureHash;
    outType->hasLayout = canonical_consumer_find_layout(
            projection, row.token, 0u, 0u, &outType->layout);
    if (row.constructorSignatureToken != 0u) {
        outType->hasContract = canonical_consumer_find_contract(
                projection, row.constructorSignatureToken, row.constructorContractHash,
                &outType->contract);
    }
    canonical_consumer_project_domain_transfer(
            projection, row.token, row.canonicalTypeId, outType);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 先按 token 的表号选择 TypeDef/TypeRef/TypeSpec，再复用各行投影逻辑。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveTypeToken(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 index;
    const SZrArtifactSectionView *section;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || outType == ZR_NULL || typeToken == 0u) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    memset(outType, 0, sizeof(*outType));
    if (ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_DEF) {
        for (index = 0u; index < projection->typeDefs.elementCount; ++index) {
            SZrArtifactTypeDefRow row;
            ZrCore_Artifact_ReadTypeDefRow(&projection->typeDefs, index, &row, ZR_NULL);
            if (row.token == typeToken) {
                return canonical_consumer_project_type_def(projection, index, outType, diagnostic);
            }
        }
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION,
                                       ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, 0u);
    }
    section = ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_REF
                      ? &projection->typeRefs
                      : ZR_METADATA_TOKEN_TABLE(typeToken) == ZR_METADATA_TABLE_TYPE_SPEC
                                ? &projection->typeSpecs
                                : ZR_NULL;
    if (section == ZR_NULL) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN, 0u, 0u);
    }
    for (index = 0u; index < section->elementCount; ++index) {
        SZrArtifactTypeIdentityRow row;
        ZrCore_Artifact_ReadTypeIdentityRow(section, index, &row, ZR_NULL);
        if (row.token == typeToken) {
            return canonical_consumer_project_identity_row(
                    projection, section, index, outType, diagnostic);
        }
    }
    return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION,
                                   section->kind, 0u);
}

/* 根 ID 优先返回已验证投影；其余查询 TypeSpec 再 TypeDef。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveTypeId(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 canonicalTypeId,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic) {
    TZrUInt32 index;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || outType == ZR_NULL || canonicalTypeId == 0u) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    if (projection->rootType.canonicalTypeId == canonicalTypeId) {
        *outType = projection->rootType;
        return ZR_ARTIFACT_STATUS_OK;
    }
    for (index = 0u; index < projection->typeSpecs.elementCount; ++index) {
        SZrArtifactTypeIdentityRow row;
        ZrCore_Artifact_ReadTypeIdentityRow(&projection->typeSpecs, index, &row, ZR_NULL);
        if (row.canonicalTypeId == canonicalTypeId) {
            return canonical_consumer_project_identity_row(
                    projection, &projection->typeSpecs, index, outType, diagnostic);
        }
    }
    for (index = 0u; index < projection->typeDefs.elementCount; ++index) {
        SZrArtifactTypeDefRow row;
        ZrCore_Artifact_ReadTypeDefRow(&projection->typeDefs, index, &row, ZR_NULL);
        if (row.canonicalTypeId == canonicalTypeId) {
            return canonical_consumer_project_type_def(projection, index, outType, diagnostic);
        }
    }
    return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u);
}

/* 独立布局查询要求精确类型 token；未找到时保留空输出和 section 诊断。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveLayout(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactLayoutRow *outLayout,
        SZrArtifactDiagnostic *diagnostic) {
    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || outLayout == ZR_NULL || typeToken == 0u) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    memset(outLayout, 0, sizeof(*outLayout));
    if (!canonical_consumer_find_layout(projection, typeToken, 0u, 0u, outLayout)) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION,
                                       ZR_ARTIFACT_SECTION_LAYOUT_TABLE, 0u);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

/* 经类型投影取得域传递契约，包含 TypeRef/TypeSpec 到 TypeDef 的 canonical ID 回退。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveDomainTransfer(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactDomainTransferRow *outContract,
        SZrArtifactDiagnostic *diagnostic) {
    SZrCanonicalTypeProjection type;
    EZrArtifactStatus status;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || outContract == ZR_NULL || typeToken == 0u) {
        return canonical_consumer_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    memset(outContract, 0, sizeof(*outContract));
    status = ZrCore_CanonicalConsumer_ResolveTypeToken(
            projection, typeToken, &type, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if (!type.hasDomainTransfer) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
                0u);
    }
    *outContract = type.domainTransfer;
    return ZR_ARTIFACT_STATUS_OK;
}

/* 先验证调度器类型 token 的解析结果，再读取同 token 的契约行。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveSchedulerContract(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken schedulerTypeToken,
        SZrArtifactSchedulerContractRow *outContract,
        SZrArtifactDiagnostic *diagnostic) {
    SZrCanonicalTypeProjection schedulerType;
    EZrArtifactStatus status;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || outContract == ZR_NULL ||
        !canonical_consumer_scheduler_type_token_is_valid(schedulerTypeToken)) {
        return canonical_consumer_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    memset(outContract, 0, sizeof(*outContract));
    status = ZrCore_CanonicalConsumer_ResolveTypeToken(
            projection, schedulerTypeToken, &schedulerType, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || schedulerType.typeToken != schedulerTypeToken) {
        return status != ZR_ARTIFACT_STATUS_OK
                       ? status
                       : canonical_consumer_fail(
                                 diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u);
    }
    if (!canonical_consumer_find_scheduler_contract(
                projection, schedulerTypeToken, outContract)) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

/* 以精确 token 和策略位筛选契约，分别报告 ABI、策略、要求位及哈希不匹配。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ValidateSchedulerContract(
        const SZrCanonicalConsumerProjection *projection,
        const SZrCanonicalSchedulerContractExpectation *expected,
        SZrArtifactDiagnostic *diagnostic) {
    SZrArtifactSchedulerContractRow contract;
    TZrUInt32 actualRequirements;
    EZrArtifactStatus status;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || expected == ZR_NULL ||
        !canonical_consumer_scheduler_type_token_is_valid(expected->schedulerTypeToken) ||
        expected->taskTypeToken == 0u || expected->jobTypeToken == 0u ||
        expected->abiVersion == 0u ||
        (expected->policy != ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN &&
         expected->policy != ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN) ||
        (expected->requirementFlags &
         ~ZR_ARTIFACT_SCHEDULER_REQUIREMENT_KNOWN_MASK) != 0u ||
        expected->transportContractHash == 0u ||
        expected->schedulerContractHash == 0u) {
        return canonical_consumer_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    status = ZrCore_CanonicalConsumer_ResolveSchedulerContract(
            projection, expected->schedulerTypeToken, &contract, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if (contract.taskTypeToken != expected->taskTypeToken ||
        contract.jobTypeToken != expected->jobTypeToken) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
    }
    if (contract.abiVersion != expected->abiVersion) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_SCHEDULER_ABI_MISMATCH,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = expected->abiVersion;
            diagnostic->actualVersion = contract.abiVersion;
        }
        return ZR_ARTIFACT_STATUS_SCHEDULER_ABI_MISMATCH;
    }
    if ((contract.policyMask & expected->policy) == 0u) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = expected->policy;
            diagnostic->actualVersion = contract.policyMask;
        }
        return ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH;
    }
    /* 要求位取当前选择的域策略对应字段，不混用 attached 与 isolated。 */
    actualRequirements = expected->policy == ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN
                                 ? contract.attachedRequirementFlags
                                 : contract.isolatedRequirementFlags;
    if (actualRequirements != expected->requirementFlags) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_SCHEDULER_REQUIREMENT_MISMATCH,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = expected->requirementFlags;
            diagnostic->actualVersion = actualRequirements;
        }
        return ZR_ARTIFACT_STATUS_SCHEDULER_REQUIREMENT_MISMATCH;
    }
    if (contract.transportContractHash != expected->transportContractHash) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expected->transportContractHash;
            diagnostic->actualHash = contract.transportContractHash;
        }
        return ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH;
    }
    if (contract.schedulerContractHash != expected->schedulerContractHash) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_SCHEDULER_CONTRACT_MISMATCH,
                ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expected->schedulerContractHash;
            diagnostic->actualHash = contract.schedulerContractHash;
        }
        return ZR_ARTIFACT_STATUS_SCHEDULER_CONTRACT_MISMATCH;
    }
    return ZR_ARTIFACT_STATUS_OK;
}

/* 公共 ref-like ABI 先核 TypeRef 身份，再核布局版本/哈希与 callable escape/lowering。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
        const SZrCanonicalConsumerProjection *projection,
        const SZrCanonicalPublicRefLikeAbiExpectation *expected,
        SZrArtifactDiagnostic *diagnostic) {
    SZrCanonicalTypeProjection type;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactContractRow contract;
    EZrArtifactStatus status;
    TZrBool foundTypeRef = ZR_FALSE;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (projection == ZR_NULL || expected == ZR_NULL ||
        expected->typeToken == 0u || expected->callableSignatureToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(expected->typeToken) != ZR_METADATA_TABLE_TYPE_REF ||
        expected->typeRefHash == 0u ||
        expected->layoutVersion == 0u || expected->layoutHash == 0u ||
        (expected->typeFlags & ZR_ARTIFACT_TYPE_FLAG_REF_LIKE) == 0u ||
        (expected->typeFlags & ~ZR_ARTIFACT_TYPE_FLAG_KNOWN_MASK) != 0u ||
        (expected->callableEscapeFlags &
         ~ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_KNOWN_MASK) != 0u ||
        expected->abiLoweringKind <= ZR_ARTIFACT_ABI_LOWERING_NONE ||
        expected->abiLoweringKind > ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT) {
        return canonical_consumer_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }

    for (TZrUInt32 index = 0u;
         index < projection->typeRefs.elementCount;
         index++) {
        if (ZrCore_Artifact_ReadTypeIdentityRow(
                    &projection->typeRefs, index, &typeRef, ZR_NULL) ==
                    ZR_ARTIFACT_STATUS_OK &&
            typeRef.token == expected->typeToken) {
            foundTypeRef = ZR_TRUE;
            break;
        }
    }
    if (!foundTypeRef) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
                0u);
    }
    if (typeRef.signatureHash != expected->typeRefHash) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
                ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expected->typeRefHash;
            diagnostic->actualHash = typeRef.signatureHash;
        }
        return ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH;
    }

    status = ZrCore_CanonicalConsumer_ResolveTypeToken(
            projection, expected->typeToken, &type, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    if ((type.capabilityFlags & ZR_ARTIFACT_TYPE_FLAG_KNOWN_MASK) !=
        expected->typeFlags) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                ZR_METADATA_TOKEN_TABLE(expected->typeToken) ==
                                ZR_METADATA_TABLE_TYPE_REF
                        ? ZR_ARTIFACT_SECTION_TYPE_REF_TABLE
                        : ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
                0u);
    }
    if (!type.hasLayout) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                0u);
    }
    if (type.layout.version != expected->layoutVersion) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedVersion = expected->layoutVersion;
            diagnostic->actualVersion = type.layout.version;
        }
        return ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH;
    }
    if (type.layout.layoutHash != expected->layoutHash) {
        canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
                ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
                0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expected->layoutHash;
            diagnostic->actualHash = type.layout.layoutHash;
        }
        return ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH;
    }
    if (!canonical_consumer_find_contract(
                projection,
                expected->callableSignatureToken,
                0u,
                &contract)) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SECTION,
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                0u);
    }
    /* NATIVE_DIRECT 即使与预期一致仍不属于此公共 ref-like 边界允许的 lowering。 */
    if (contract.escapeFlags != expected->callableEscapeFlags ||
        contract.abiLoweringKind != expected->abiLoweringKind ||
        contract.abiLoweringKind == ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT) {
        return canonical_consumer_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
                0u);
    }
    return ZR_ARTIFACT_STATUS_OK;
}

/* 共享解码器校验格式与元数据图；此处建立 ZRO 专用借用视图及根身份合同。 */
EZrArtifactStatus ZrCore_CanonicalConsumer_Open(
        const TZrByte *buffer,
        TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrCanonicalConsumerProjection *outProjection,
        SZrArtifactDiagnostic *diagnostic) {
    EZrArtifactStatus status;
    SZrCanonicalTypeProjection rootRef;
    TZrUInt32 typeDefIndex;

    canonical_consumer_clear_diagnostic(diagnostic);
    if (outProjection != ZR_NULL) {
        memset(outProjection, 0, sizeof(*outProjection));
    }
    if (buffer == ZR_NULL || outProjection == ZR_NULL) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    status = ZrCore_Artifact_Read(buffer, bufferLength, &outProjection->artifact, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (outProjection->artifact.kind != ZR_ARTIFACT_KIND_ZRO) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_KIND, 0u, 0u);
    }
    if (expectedIdentity != ZR_NULL) {
        status = ZrCore_Artifact_ValidatePublicIdentity(
                &outProjection->artifact, expectedIdentity, diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
    }
    /* 核心类型/签名/契约/布局表必须存在，缺任一表即沿 FindSection 状态退出。 */
#define ZR_CANONICAL_CONSUMER_SECTION(KIND, FIELD) \
    status = ZrCore_Artifact_FindSection(&outProjection->artifact, KIND, \
                                         &outProjection->FIELD, diagnostic); \
    if (status != ZR_ARTIFACT_STATUS_OK) return status
    ZR_CANONICAL_CONSUMER_SECTION(ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, typeDefs);
    ZR_CANONICAL_CONSUMER_SECTION(ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, typeRefs);
    ZR_CANONICAL_CONSUMER_SECTION(ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, typeSpecs);
    ZR_CANONICAL_CONSUMER_SECTION(ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, signatures);
    ZR_CANONICAL_CONSUMER_SECTION(ZR_ARTIFACT_SECTION_CONTRACT_TABLE, contracts);
    ZR_CANONICAL_CONSUMER_SECTION(ZR_ARTIFACT_SECTION_LAYOUT_TABLE, layouts);
#undef ZR_CANONICAL_CONSUMER_SECTION
    /* 扩展表允许缺席；置空视图使后续查询自然走“未找到”状态。 */
    status = ZrCore_Artifact_FindSection(
            &outProjection->artifact,
            ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
            &outProjection->domainTransfers,
            ZR_NULL);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        memset(&outProjection->domainTransfers, 0,
               sizeof(outProjection->domainTransfers));
    }
    status = ZrCore_Artifact_FindSection(
            &outProjection->artifact,
            ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
            &outProjection->schedulerContracts,
            ZR_NULL);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        memset(&outProjection->schedulerContracts, 0,
               sizeof(outProjection->schedulerContracts));
    }
    status = ZrCore_Artifact_FindSection(
            &outProjection->artifact,
            ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE,
            &outProjection->callBindings,
            ZR_NULL);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        memset(&outProjection->callBindings, 0, sizeof(outProjection->callBindings));
    }
    /* 根 TypeSpec 与 TypeRef 必须编码相同签名字节，然后核布局及 callable 契约。 */
    status = ZrCore_CanonicalConsumer_ResolveTypeToken(
            outProjection,
            outProjection->artifact.identity.typeSpecToken,
            &outProjection->rootType,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    status = ZrCore_CanonicalConsumer_ResolveTypeToken(
            outProjection,
            outProjection->artifact.identity.typeRefToken,
            &rootRef,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    if (rootRef.signatureLength != outProjection->rootType.signatureLength ||
        memcmp(rootRef.signatureData,
               outProjection->rootType.signatureData,
               rootRef.signatureLength) != 0) {
        return canonical_consumer_fail(diagnostic,
                                       ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
                                       ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
                                       0u);
    }
    for (typeDefIndex = 0u; typeDefIndex < outProjection->typeDefs.elementCount; ++typeDefIndex) {
        SZrArtifactTypeDefRow typeDef;
        ZrCore_Artifact_ReadTypeDefRow(&outProjection->typeDefs, typeDefIndex, &typeDef, ZR_NULL);
        if (typeDef.canonicalTypeId == outProjection->artifact.identity.canonicalTypeId &&
            typeDef.typeSignatureHash != outProjection->artifact.identity.signatureHash) {
            return canonical_consumer_hash_fail(diagnostic,
                                                outProjection->artifact.identity.signatureHash,
                                                typeDef.typeSignatureHash);
        }
    }
    if (!outProjection->rootType.hasLayout ||
        !canonical_consumer_find_contract(
                outProjection,
                outProjection->artifact.identity.signatureToken,
                outProjection->artifact.identity.callableContractHash,
                &outProjection->rootType.contract)) {
        return canonical_consumer_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION, 0u, 0u);
    }
    outProjection->rootType.hasContract = ZR_TRUE;
    return canonical_consumer_validate_callable_contract(
            &outProjection->rootType, diagnostic);
}
