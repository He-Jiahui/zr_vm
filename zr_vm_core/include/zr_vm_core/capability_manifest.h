#ifndef ZR_VM_CORE_CAPABILITY_MANIFEST_H
#define ZR_VM_CORE_CAPABILITY_MANIFEST_H

#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/artifact_schema.h"

#define ZR_HOT_PATCH_CAPABILITY_SCHEMA_VERSION ((TZrUInt32)1u)

/** @brief manifest 与 host 准入失败的机器可读类别；部署方应按枚举而非状态名决策。 */
typedef enum EZrHotPatchCapabilityStatus {
    ZR_HOT_PATCH_OK = 0,
    ZR_HOT_PATCH_INVALID_ARGUMENT,
    ZR_HOT_PATCH_ARTIFACT_INVALID,
    ZR_HOT_PATCH_SIGNATURE_REJECTED,
    ZR_HOT_PATCH_BASE_MISMATCH,
    ZR_HOT_PATCH_ABI_MISMATCH,
    ZR_HOT_PATCH_CAPABILITY_ESCALATION,
    ZR_HOT_PATCH_PUBLIC_CONTRACT_CHANGE,
    ZR_HOT_PATCH_MACHINE_CODE_FORBIDDEN,
    ZR_HOT_PATCH_IMPORT_FORBIDDEN,
    ZR_HOT_PATCH_PROFILE_MISMATCH,
    ZR_HOT_PATCH_LIMIT,
    ZR_HOT_PATCH_CONTENT_CHANGED
} EZrHotPatchCapabilityStatus;

/** @brief 把单个元数据 token 的能力需求及来源位置带入授权闭包。 */
typedef struct SZrHotPatchCapabilityRequirement {
    TZrUInt32 token;
    TZrUInt64 requiredBits;
    TZrUInt32 sourceOffset;
    TZrUInt32 reserved;
} SZrHotPatchCapabilityRequirement;

/** @brief 候选身份、目标环境与能力需求的策略输入；requirements 由调用方持有。 */
typedef struct SZrHotPatchCapabilityManifest {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt64 patchId;
    TZrUInt64 contentHash;
    TZrUInt64 baseModuleHash;
    TZrUInt64 publicContractHash;
    TZrUInt64 requiredCapabilities;
    TZrUInt32 targetAbiVersion;
    TZrUInt32 targetProfile;
    TZrUInt32 requirementCount;
    const SZrHotPatchCapabilityRequirement *requirements;
} SZrHotPatchCapabilityManifest;

/* 主验证器拒绝新增机器码、原生导入及公开布局/签名变动；未知 flag 也不放行。 */
#define ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE ((TZrUInt32)1u << 0u)
#define ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT ((TZrUInt32)1u << 1u)
#define ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT ((TZrUInt32)1u << 2u)
#define ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE ((TZrUInt32)1u << 3u)
#define ZR_HOT_PATCH_FLAG_KNOWN_MASK \
    (ZR_HOT_PATCH_FLAG_HAS_MACHINE_CODE | ZR_HOT_PATCH_FLAG_ADDS_NATIVE_IMPORT | \
     ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_LAYOUT | ZR_HOT_PATCH_FLAG_CHANGES_PUBLIC_SIGNATURE)

/** @brief host 的当前模块身份与准入策略；artifact/manifest 及签名在验证调用期间保持有效。 */
typedef struct SZrHotPatchValidationInput {
    const SZrArtifactExecIrView *artifact;
    const SZrHotPatchCapabilityManifest *manifest;
    TZrUInt64 loadedBaseModuleHash;
    TZrUInt64 loadedPublicContractHash;
    TZrUInt32 hostAbiVersion;
    TZrUInt32 hostProfile;
    TZrUInt64 hostAllowedCapabilities;
    TZrUInt64 expectedPatchId;
    TZrUInt64 expectedContentHash;
    const TZrByte *signature;
    TZrUInt32 signatureLength;
} SZrHotPatchValidationInput;

/** @brief 同步验签回调，读取借用的 artifact 字节和调用方传入的签名，不接管其所有权。 */
typedef TZrBool (*FZrHotPatchVerifySignature)(const TZrByte *content,
                                               TZrUInt32 contentLength,
                                               const TZrByte *signature,
                                               TZrUInt32 signatureLength,
                                               TZrPtr userData);

/** @brief 验证一个完整 canonical ZRAF，并显式选择唯一的 ExecIR 入口函数。
 * @note outerLength 保持 TZrSize 宽度；超过 artifact 最大字节数时在窄化前拒绝。
 *       outerBytes、signature、manifest 和 expectedPublicIdentity 均由调用方借用；
 *       入口在调用回调前快照策略标量、根身份和能力需求闭包；通过验签并复核回调后哈希后，
 *       才 Read/Open/Verify 外层结构并检查入口。回调收到完整外层字节跨度。
 *       manifest 与签名是宿主提供的独立策略/认证输入，不包含在外层哈希的认证声明内。 */
typedef struct SZrHotPatchZrafValidationInput {
    const TZrByte *outerBytes;
    TZrSize outerLength;
    const SZrArtifactPublicIdentity *expectedPublicIdentity;
    const SZrHotPatchCapabilityManifest *manifest;
    TZrMetadataToken entryFunctionToken;
    TZrUInt64 entrySignatureHash;
    TZrUInt64 loadedBaseModuleHash;
    TZrUInt64 loadedPublicContractHash;
    TZrUInt32 hostAbiVersion;
    TZrUInt32 hostProfile;
    TZrUInt64 hostAllowedCapabilities;
    TZrUInt64 expectedPatchId;
    TZrUInt64 expectedContentHash;
    const TZrByte *signature;
    TZrUInt32 signatureLength;
} SZrHotPatchZrafValidationInput;

/** @brief 验证后交给 Prepare/Apply 的令牌。
 * artifact/manifest 与 contentBytes 的存储仍由调用方持有；Apply/Prepare 使用验证时
 * 捕获的标量、字节地址和长度快照。字节不复制或固定，调用方须保持其生命周期。 */
typedef struct SZrValidatedHotPatch {
    const SZrArtifactExecIrView *artifact;
    const SZrHotPatchCapabilityManifest *manifest;
    TZrUInt64 contentHash;
    const TZrByte *contentBytes;
    TZrUInt32 contentLength;
    TZrUInt64 patchId;
    TZrUInt64 publicContractHash;
    TZrUInt64 requiredCapabilities;
    TZrUInt64 validationPolicyHash;
    TZrUInt32 targetProfile;
    TZrBool signatureVerified;
    /* Apply rechecks the borrowed contentBytes range; this does not pin storage. */
    TZrBool immutableContent;
} SZrValidatedHotPatch;

/** @brief 首个拒绝点的结构化信息；token/sourceOffset 为逐需求错误定位。 */
typedef struct SZrHotPatchDiagnostic {
    EZrHotPatchCapabilityStatus status;
    TZrUInt32 token;
    TZrUInt32 sourceOffset;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrHotPatchDiagnostic;

/** @brief 完整 ZRAF 验证后的只读借用令牌；字节仍由调用方持有且须保持存活。 */
typedef struct SZrValidatedHotPatchZraf {
    const TZrByte *outerBytes;
    TZrSize outerLength;
    TZrUInt64 outerContentHash;
    SZrArtifactPublicIdentity publicIdentity;
    TZrMetadataToken entryFunctionToken;
    TZrUInt64 entrySignatureHash;
    TZrUInt64 patchId;
    TZrUInt64 publicContractHash;
    TZrUInt64 requiredCapabilities;
    TZrUInt64 validationPolicyHash;
    TZrUInt32 targetAbiVersion;
    TZrUInt32 targetProfile;
    TZrBool signatureVerified;
    TZrBool canonicalExecIrVerified;
} SZrValidatedHotPatchZraf;

/** @brief 部署前同时核对字节哈希、基模块、ABI/profile、授权集合与 host 验签。
 * @pre verifySignature 可调用；输入指针指向的存储在本次验证期间保持有效且稳定。
 * @return 成功才填充 validated；失败时清零输出并通过可选 diagnostic 报告原因。
 * @note requirementCount 不得超过 4096；超限会在
 * 调用签名回调和读取逐项需求前返回 ZR_HOT_PATCH_LIMIT，并填充 expected/actual。
 * 成功 token 捕获原始字节地址与长度；调用方须保持字节存储到 Apply 完成。 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_Validate(
        const SZrHotPatchValidationInput *input,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatch *validated,
        SZrHotPatchDiagnostic *diagnostic);
/** @brief Validate the complete outer ZRAF, public identity, canonical ExecIR,
 * and entry selector.
 * @pre Keep outerBytes alive and unchanged throughout this call, including signature
 *      verification and structural decoding. The caller must externally synchronize
 *      all writers. The callback receives a borrowed read-only span and must not modify it.
 * @return Publish the scalar snapshot only on success; clear validated on rejection.
 * @note outerLength is not truncated and must not exceed ZR_ARTIFACT_MAX_BYTE_LENGTH. Both
 *       manifest.contentHash and expectedContentHash cover the complete outerBytes span. The
 *       post-callback rehash rejects a differing hash; it cannot observe a callback mutation
 *       restored before return or make concurrent writes atomic. The token borrows outerBytes;
 *       the caller must keep the storage alive. Recheck observes only changes visible
 *       to its hash while the storage remains live; it does not own the storage.
 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_ValidateZraf(
        const SZrHotPatchZrafValidationInput *input,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatchZraf *validated,
        SZrHotPatchDiagnostic *diagnostic);
/** @brief Recompute the hash of the validated token's borrowed outer ZRAF span.
 * @pre Keep the borrowed storage alive and externally synchronize all writers during
 *      this call.
 * @note Recheck observes only changes visible to its hash; it does not own or pin
 *       storage and does not provide concurrent-write atomicity.
 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_RecheckZrafContent(
        const SZrValidatedHotPatchZraf *validated,
        SZrHotPatchDiagnostic *diagnostic);
/** @brief 为同一 host/base/manifest 策略计算稳定身份；结果不代表签名或内容完整性。 */
ZR_CORE_API TZrUInt64 ZrCore_HotPatch_ComputePolicyHash(
        const SZrHotPatchValidationInput *input);
/** @brief 返回适合诊断展示的状态名；调用方仍以状态枚举判断失败类别。 */
ZR_CORE_API const TZrChar *ZrCore_HotPatch_StatusName(
        EZrHotPatchCapabilityStatus status);

#endif
