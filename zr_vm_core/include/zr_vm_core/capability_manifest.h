#ifndef ZR_VM_CORE_CAPABILITY_MANIFEST_H
#define ZR_VM_CORE_CAPABILITY_MANIFEST_H

#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/artifact_schema.h"

/** @brief 宿主提供的能力清单版本；主验证入口仅接受此版本，不作旧版本转换。 */
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

/** @brief 宿主提供的逐 token 授权声明；非零需求与清单总需求合并后必须属于宿主许可集合。
 * @note token/sourceOffset 用于拒绝诊断，不在这里解析元数据或证明产物实际需求；reserved 必须为零。 */
typedef struct SZrHotPatchCapabilityRequirement {
    /** @brief 非零需求 token；仅作授权项身份及诊断定位，不由本接口解引用。 */
    TZrUInt32 token;
    /** @brief 非零能力位集合；必须全部属于宿主允许集合。 */
    TZrUInt64 requiredBits;
    /** @brief 需求来源位置；诊断原样传递，不校验对应源文件。 */
    TZrUInt32 sourceOffset;
    /** @brief 保留为零，非零会拒绝此需求。 */
    TZrUInt32 reserved;
} SZrHotPatchCapabilityRequirement;

/** @brief 宿主提供的候选身份、目标环境与能力需求策略；requirements 由调用方持有。
 * @note 此结构是含指针的进程内输入，不是可直接落盘的 ZRAF 线格式。内容哈希跨度由所选验证入口决定；
 *       宿主负责清单和认证策略的可信来源，验证器不从产物推导这里的授权位。 */
typedef struct SZrHotPatchCapabilityManifest {
    /** @brief 清单契约版本，须与当前主验证器版本一致。 */
    TZrUInt32 schemaVersion;
    /** @brief 声明候选变更类别；未知位与当前禁止的变更均拒绝。 */
    TZrUInt32 flags;
    /** @brief 补丁幂等身份，主验证入口要求非零；后续登记使用捕获值。 */
    TZrUInt64 patchId;
    /** @brief 所选入口内容跨度的预期哈希；旧入口为视图 buffer，ZRAF 入口为完整外层。 */
    TZrUInt64 contentHash;
    /** @brief 清单期望替换的基模块身份，必须匹配宿主已加载身份。 */
    TZrUInt64 baseModuleHash;
    /** @brief 公开调用契约身份，必须匹配已加载契约；不准借补丁改变公开布局/签名。 */
    TZrUInt64 publicContractHash;
    /** @brief 清单总需求，与逐项需求取并集后受宿主许可集合限制。 */
    TZrUInt64 requiredCapabilities;
    /** @brief 目标 ABI，主验证器要求与宿主 ABI 相同。 */
    TZrUInt32 targetAbiVersion;
    /** @brief 目标部署 profile；主入口检查与宿主相等，不在此选择执行后端。 */
    TZrUInt32 targetProfile;
    /** @brief requirements 的有效项数；主入口和闭包入口均先检查共享上限。 */
    TZrUInt32 requirementCount;
    /** @brief 调用方持有的需求数组；非零项数要求非空且可读。 */
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

/** @brief 旧 ExecIR 视图入口的宿主身份与准入策略；输入及其指向存储须在调用期间有效、稳定。
 * @note 该入口核对视图标量及字节哈希，不执行完整 ZRAF 的 Read/Open/Verify；需要结构校验时使用 ValidateZraf。
 *       预期补丁 ID/内容哈希为零表示不附加对应匹配限制，宿主许可位仍限制能力闭包。 */
typedef struct SZrHotPatchValidationInput {
    /** @brief 调用方持有的旧 ExecIR 视图；成功结果借用该视图且另捕获原字节跨度。 */
    const SZrArtifactExecIrView *artifact;
    /** @brief 调用方持有的策略清单；验证令牌另捕获后续使用的策略标量。 */
    const SZrHotPatchCapabilityManifest *manifest;
    /** @brief 宿主当前加载的基模块身份，用于拒绝错基补丁。 */
    TZrUInt64 loadedBaseModuleHash;
    /** @brief 宿主当前公开契约身份，用于维持既有调用方契约。 */
    TZrUInt64 loadedPublicContractHash;
    /** @brief 宿主 ABI 比较值；ZRAF 入口同时要求非零。 */
    TZrUInt32 hostAbiVersion;
    /** @brief 宿主部署 profile 比较值；不是后端注册或切换操作。 */
    TZrUInt32 hostProfile;
    /** @brief 宿主授权上界；验证器只能在此集合内合并声明需求。 */
    TZrUInt64 hostAllowedCapabilities;
    /** @brief 可选额外 ID 限制；零跳过对应匹配检查。 */
    TZrUInt64 expectedPatchId;
    /** @brief 宿主预期内容哈希；旧入口可为零，ZRAF 入口必须非零且覆盖完整外层。 */
    TZrUInt64 expectedContentHash;
    /** @brief 宿主认证输入，借用至同步回调返回；格式由回调策略决定。 */
    const TZrByte *signature;
    /** @brief 交给宿主回调的签名长度；验证器不自行解释签名格式。 */
    TZrUInt32 signatureLength;
} SZrHotPatchValidationInput;

/** @brief 宿主同步验签回调，读取借用内容和签名；返回真才认可认证，不接管存储或 userData。
 * @note 不得释放内容存储；宿主负责验签算法、信任根及回调上下文有效期。
 * TODO: 仓内正向调用使用测试验签桩；真实宿主的算法/密钥与清单绑定策略须从本回调接入处核查。 */
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
    /** @brief 借用的完整外层 ZRAF 字节；调用方保持存活并同步写入者。 */
    const TZrByte *outerBytes;
    /** @brief 完整外层跨度长度；先以 TZrSize 检查上限，再窄化给哈希/回调。 */
    TZrSize outerLength;
    /** @brief 宿主预期公开身份；ZRAF 验证在回调前按值快照。 */
    const SZrArtifactPublicIdentity *expectedPublicIdentity;
    /** @brief 调用方持有的策略清单；验证令牌另捕获后续使用的策略标量。 */
    const SZrHotPatchCapabilityManifest *manifest;
    /** @brief 显式入口 token，须非零并与图内函数选择器匹配。 */
    TZrMetadataToken entryFunctionToken;
    /** @brief 显式入口签名哈希，须非零且 token/签名组合恰有一个匹配。 */
    TZrUInt64 entrySignatureHash;
    /** @brief 宿主当前加载的基模块身份，用于拒绝错基补丁。 */
    TZrUInt64 loadedBaseModuleHash;
    /** @brief 宿主当前公开契约身份，用于维持既有调用方契约。 */
    TZrUInt64 loadedPublicContractHash;
    /** @brief 宿主 ABI 比较值；ZRAF 入口同时要求非零。 */
    TZrUInt32 hostAbiVersion;
    /** @brief 宿主部署 profile 比较值；不是后端注册或切换操作。 */
    TZrUInt32 hostProfile;
    /** @brief 宿主授权上界；验证器只能在此集合内合并声明需求。 */
    TZrUInt64 hostAllowedCapabilities;
    /** @brief 可选额外 ID 限制；零跳过对应匹配检查。 */
    TZrUInt64 expectedPatchId;
    /** @brief 宿主预期内容哈希；旧入口可为零，ZRAF 入口必须非零且覆盖完整外层。 */
    TZrUInt64 expectedContentHash;
    /** @brief 宿主认证输入，借用至同步回调返回；格式由回调策略决定。 */
    const TZrByte *signature;
    /** @brief 交给宿主回调的签名长度；验证器不自行解释签名格式。 */
    TZrUInt32 signatureLength;
} SZrHotPatchZrafValidationInput;

/** @brief 验证后交给 Prepare/Apply 的令牌。
 * artifact/manifest 与 contentBytes 的存储仍由调用方持有；Apply/Prepare 使用验证时
 * 捕获的标量、字节地址和长度快照。字节不复制或固定，调用方须保持其生命周期。 */
typedef struct SZrValidatedHotPatch {
    /** @brief 借用的验证输入身份；Apply 仅要求非空，不从它重取发布标量或内容跨度。 */
    const SZrArtifactExecIrView *artifact;
    /** @brief 借用的验证输入身份；Apply 仅要求非空，不从它重取发布标量或内容跨度。 */
    const SZrHotPatchCapabilityManifest *manifest;
    /** @brief 旧入口捕获的原跨度哈希；Apply/Prepare 使用此快照。 */
    TZrUInt64 contentHash;
    /** @brief 验证时捕获的借用地址；Apply 按此地址复核，不从后来改写的视图重新取地址。 */
    const TZrByte *contentBytes;
    /** @brief 验证时捕获的字节长度；调用方须保留整个可读跨度。 */
    TZrUInt32 contentLength;
    /** @brief 补丁幂等身份，主验证入口要求非零；后续登记使用捕获值。 */
    TZrUInt64 patchId;
    /** @brief 公开调用契约身份，必须匹配已加载契约；不准借补丁改变公开布局/签名。 */
    TZrUInt64 publicContractHash;
    /** @brief 清单总需求，与逐项需求取并集后受宿主许可集合限制。 */
    TZrUInt64 requiredCapabilities;
    /** @brief 有限策略字段的比较身份，不是完整清单认证或内容哈希。 */
    TZrUInt64 validationPolicyHash;
    /** @brief 目标部署 profile；主入口检查与宿主相等，不在此选择执行后端。 */
    TZrUInt32 targetProfile;
    /** @brief 验证器成功验签后置真；消费入口检查标记，不再次调用宿主验签。 */
    TZrBool signatureVerified;
    /** @brief 验证器置真的准入标记；Apply 仍重算借用跨度的哈希，不固定存储或防止并发写入。 */
    TZrBool immutableContent;
} SZrValidatedHotPatch;

/** @brief 当前成功或首个拒绝点的结构化信息；token 可为零、需求或入口/产物定位，sourceOffset 随检查点表示需求位置、产物字节偏移或指令 ID。 */
typedef struct SZrHotPatchDiagnostic {
    /** @brief 本次校验结果；成功或首个拒绝类别。 */
    EZrHotPatchCapabilityStatus status;
    /** @brief 逐项需求或入口错误的定位 token；其他检查可能为零。 */
    TZrUInt32 token;
    /** @brief 逐项需求来源位置或产物诊断位置；并非统一的源文件字节偏移。 */
    TZrUInt32 sourceOffset;
    /** @brief 拒绝点所期望的值/允许集合/上限；含义随 status 及检查点变化。 */
    TZrUInt64 expected;
    /** @brief 拒绝点实际观察值；不能单凭此字段反推所有校验历史。 */
    TZrUInt64 actual;
} SZrHotPatchDiagnostic;

/** @brief 完整 ZRAF 验证后的只读借用令牌；字节仍由调用方持有且须保持存活。 */
typedef struct SZrValidatedHotPatchZraf {
    /** @brief 借用的完整外层 ZRAF 字节；调用方保持存活并同步写入者。 */
    const TZrByte *outerBytes;
    /** @brief 完整外层跨度长度；先以 TZrSize 检查上限，再窄化给哈希/回调。 */
    TZrSize outerLength;
    /** @brief 验证时完整外层哈希；Recheck 比较仍存活的同一跨度。 */
    TZrUInt64 outerContentHash;
    /** @brief 解码后按值捕获的公开身份，不借用临时解码图。 */
    SZrArtifactPublicIdentity publicIdentity;
    /** @brief 完整 ZRAF 验证成功时捕获的entryFunctionToken 标量；不是指向临时图的借用引用。 */
    TZrMetadataToken entryFunctionToken;
    /** @brief 完整 ZRAF 验证成功时捕获的entrySignatureHash 标量；不是指向临时图的借用引用。 */
    TZrUInt64 entrySignatureHash;
    /** @brief 完整 ZRAF 验证成功时捕获的patchId 标量；不是指向临时图的借用引用。 */
    TZrUInt64 patchId;
    /** @brief 完整 ZRAF 验证成功时捕获的publicContractHash 标量；不是指向临时图的借用引用。 */
    TZrUInt64 publicContractHash;
    /** @brief 完整 ZRAF 验证成功时捕获的requiredCapabilities 标量；不是指向临时图的借用引用。 */
    TZrUInt64 requiredCapabilities;
    /** @brief 有限策略字段的比较身份，不是完整清单认证或内容哈希。 */
    TZrUInt64 validationPolicyHash;
    /** @brief 完整 ZRAF 验证成功时捕获的targetAbiVersion 标量；不是指向临时图的借用引用。 */
    TZrUInt32 targetAbiVersion;
    /** @brief 完整 ZRAF 验证成功时捕获的targetProfile 标量；不是指向临时图的借用引用。 */
    TZrUInt32 targetProfile;
    /** @brief 验证器成功验签后置真；消费入口检查标记，不再次调用宿主验签。 */
    TZrBool signatureVerified;
    /** @brief 支持的图解码/验证及入口匹配已通过的标记；Recheck 只检查标记并重算字节哈希。 */
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
/** @brief 核对完整外层 ZRAF、公开身份、支持的 canonical ExecIR 图及唯一匹配的入口选择器。
 * @pre outerBytes 在验签和结构解码期间保持存活且不变；调用方须同步所有写入者，回调不得改写该跨度。
 * @return 成功才发布标量快照；拒绝时清零可写的 validated，diagnostic 可为空。
 * @note outerLength 在窄化前检查最大长度；manifest.contentHash 与非零 expectedContentHash 均覆盖完整外层跨度。
 *       策略与能力闭包在回调前捕获；回调后重算哈希只发现当时可见的变化，不能发现已恢复的改写或提供并发原子性。
 *       令牌借用 outerBytes，调用方继续负责存储有效期；此入口不发布补丁或转移字节所有权。
 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_ValidateZraf(
        const SZrHotPatchZrafValidationInput *input,
        FZrHotPatchVerifySignature verifySignature,
        TZrPtr userData,
        SZrValidatedHotPatchZraf *validated,
        SZrHotPatchDiagnostic *diagnostic);
/** @brief 复核成功令牌借用的完整外层跨度是否仍具有原内容哈希。
 * @pre 存储在调用期间保持存活，所有写入者由调用方同步；令牌标量不可作为任意地址可读的证明。
 * @return 标记/跨度非法返回 INVALID_ARGUMENT；可见哈希改变返回 CONTENT_CHANGED，不再次验签或解析结构。
 * @note 不接管或固定存储，不保证并发写入原子性；哈希相同也不代表已重新认证宿主策略。
 */
ZR_CORE_API EZrHotPatchCapabilityStatus ZrCore_HotPatch_RecheckZrafContent(
        const SZrValidatedHotPatchZraf *validated,
        SZrHotPatchDiagnostic *diagnostic);
/** @brief 为版本/标记、目标 ABI/profile 与当前宿主/base 策略计算稳定比较身份。
 * @return input 或 manifest 为空时为零；其余输入须有效。
 * @note 不纳入补丁 ID、内容哈希或逐项需求，不代表签名、完整清单身份或内容完整性。 */
ZR_CORE_API TZrUInt64 ZrCore_HotPatch_ComputePolicyHash(
        const SZrHotPatchValidationInput *input);
/** @brief 返回借用的静态诊断字符串，无需释放；调用方以状态枚举判断失败类别。
 * @note 部分枚举及未知值共用兜底名称，字符串不是一一对应的决策接口。 */
ZR_CORE_API const TZrChar *ZrCore_HotPatch_StatusName(
        EZrHotPatchCapabilityStatus status);

#endif
