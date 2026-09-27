#ifndef ZR_VM_CORE_CANONICAL_CONSUMER_H
#define ZR_VM_CORE_CANONICAL_CONSUMER_H

#include "zr_vm_core/artifact_schema.h"

/** @brief 按 canonical ID/token 投影一个类型的身份及可用契约。
 * signatureData 借用 artifact 字节，layout/contract/domainTransfer 行按值保存；has* 决定对应行是否有效。
 */
typedef struct SZrCanonicalTypeProjection {
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken typeToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 capabilityFlags;
    const TZrByte *signatureData;
    TZrUInt32 signatureLength;
    TZrUInt64 signatureHash;
    TZrBool hasLayout;
    SZrArtifactLayoutRow layout;
    TZrBool hasContract;
    SZrArtifactContractRow contract;
    TZrBool hasDomainTransfer;
    SZrArtifactDomainTransferRow domainTransfer;
} SZrCanonicalTypeProjection;

/** @brief ZRO 产物的只读视图，供模块、反射、调试、LSP 与 AOT 消费统一身份。
 * 所有 section 及 rootType.signatureData 都借用 Open 的 buffer；使用期间不得释放或改写该 buffer。
 */
typedef struct SZrCanonicalConsumerProjection {
    SZrArtifactView artifact;
    SZrArtifactSectionView typeDefs;
    SZrArtifactSectionView typeRefs;
    SZrArtifactSectionView typeSpecs;
    SZrArtifactSectionView signatures;
    SZrArtifactSectionView contracts;
    SZrArtifactSectionView layouts;
    SZrArtifactSectionView domainTransfers;
    SZrArtifactSectionView schedulerContracts;
    SZrArtifactSectionView callBindings;
    SZrCanonicalTypeProjection rootType;
} SZrCanonicalConsumerProjection;

/** @brief 调用方要求的调度器 token、ABI、域策略与传输哈希。 */
typedef struct SZrCanonicalSchedulerContractExpectation {
    TZrMetadataToken schedulerTypeToken;
    TZrMetadataToken taskTypeToken;
    TZrMetadataToken jobTypeToken;
    TZrUInt32 abiVersion;
    TZrUInt32 policy;
    TZrUInt32 requirementFlags;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
} SZrCanonicalSchedulerContractExpectation;

/** @brief 公共 ref-like 类型的引用、布局与可调用 ABI 预期。 */
typedef struct SZrCanonicalPublicRefLikeAbiExpectation {
    TZrMetadataToken typeToken;
    TZrMetadataToken callableSignatureToken;
    TZrUInt64 typeRefHash;
    TZrUInt32 typeFlags;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    TZrUInt32 callableEscapeFlags;
    EZrArtifactAbiLoweringKind abiLoweringKind;
} SZrCanonicalPublicRefLikeAbiExpectation;

/** @brief 解码 ZRO 并校验公开身份、根类型签名及 callable 契约。
 * @param expectedIdentity 可为空；非空时要求产物公开身份与之匹配。
 * @param outProjection 成功后持有借用视图；失败时可能已有部分字段，仅成功结果可供查询。
 * @return Artifact 状态码；diagnostic 可为空，非空时报告失败位置或哈希差异。
 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_Open(
        const TZrByte *buffer,
        TZrSize bufferLength,
        const SZrArtifactPublicIdentity *expectedIdentity,
        SZrCanonicalConsumerProjection *outProjection,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 按元数据 token 查询 TypeDef、TypeRef 或 TypeSpec 投影。
 * @pre projection 来自成功 Open，原 buffer 仍存活且未改写。
 * @return 成功时 outType 有效；合法但找不到的 token 返回 INVALID_SECTION。
 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveTypeToken(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 按 canonical ID 查询根类型、TypeSpec 或 TypeDef 投影。
 * @pre projection 来自成功 Open，原 buffer 仍存活且未改写。
 * @note 根类型直接复用 Open 的投影；其余按表顺序查找。
 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveTypeId(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 canonicalTypeId,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 按类型 token 查询 layout 行；不存在时报告 LAYOUT_TABLE 的 INVALID_SECTION。 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveLayout(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactLayoutRow *outLayout,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 查询类型的域传递契约；TypeRef/TypeSpec 可回退到同 canonical ID 的 TypeDef。 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveDomainTransfer(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken typeToken,
        SZrArtifactDomainTransferRow *outContract,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 按调度器 TypeDef/TypeRef token 查询契约，要求 token 能解析为同一类型。 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ResolveSchedulerContract(
        const SZrCanonicalConsumerProjection *projection,
        TZrMetadataToken schedulerTypeToken,
        SZrArtifactSchedulerContractRow *outContract,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 比较调度器的任务/工作 token、ABI 版本、域策略、要求位和两个契约哈希。
 * @return 不匹配时返回相应状态；ABI、策略、要求位或哈希不符时还记录预期/实际值。
 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ValidateSchedulerContract(
        const SZrCanonicalConsumerProjection *projection,
        const SZrCanonicalSchedulerContractExpectation *expected,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 验证公共 ref-like TypeRef 的身份、布局和可调用 ABI。
 * @note 当前公开边界拒绝 NATIVE_DIRECT lowering，即使预期字段指定该枚举值。
 */
ZR_CORE_API EZrArtifactStatus ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
        const SZrCanonicalConsumerProjection *projection,
        const SZrCanonicalPublicRefLikeAbiExpectation *expected,
        SZrArtifactDiagnostic *diagnostic);

#endif /* ZR_VM_CORE_CANONICAL_CONSUMER_H */
