#ifndef ZR_VM_CORE_ARTIFACT_SCHEMA_H
#define ZR_VM_CORE_ARTIFACT_SCHEMA_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/call_binding.h"

/* ZRS/ZRI/ZRO 共享的小端线格式版本、头/目录宽度与硬上限。 */
#define ZR_ARTIFACT_SCHEMA_VERSION ((TZrUInt16)5u)
#define ZR_ARTIFACT_HEADER_ENCODED_SIZE ((TZrUInt32)112u)
#define ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE ((TZrUInt32)24u)
#define ZR_ARTIFACT_HEADER_SECTION_COUNT_OFFSET ((TZrUInt32)16u)
#define ZR_ARTIFACT_MAX_SECTION_COUNT ((TZrUInt32)32u)
#define ZR_ARTIFACT_MAX_ROW_COUNT ((TZrUInt32)1048576u)
#define ZR_ARTIFACT_MAX_BYTE_LENGTH ((TZrUInt32)67108864u)
#define ZR_ARTIFACT_SIGNATURE_MAX_DEPTH ((TZrUInt32)128u)
#define ZR_ARTIFACT_SIGNATURE_MAX_CHILD_COUNT ((TZrUInt32)1048576u)

/* 下列宽度是线格式行大小，不等同于可能含填充的 C 结构大小。 */
#define ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE ((TZrUInt32)40u)
#define ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE ((TZrUInt32)40u)
#define ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE ((TZrUInt32)40u)
#define ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE ((TZrUInt32)96u)
#define ZR_ARTIFACT_DOMAIN_TRANSFER_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_SCHEDULER_CONTRACT_ROW_ENCODED_SIZE ((TZrUInt32)48u)
#define ZR_ARTIFACT_METADATA_STATE_ROW_ENCODED_SIZE ((TZrUInt32)64u)
#define ZR_ARTIFACT_METADATA_RECORD_ROW_ENCODED_SIZE ((TZrUInt32)40u)
/* 独立 layout map 堆的编码版本及头部宽度。 */
#define ZR_ARTIFACT_LAYOUT_MAP_VERSION ((TZrUInt32)1u)
#define ZR_ARTIFACT_LAYOUT_MAP_HEADER_ENCODED_SIZE ((TZrUInt32)16u)

/** @brief 区分源、接口和对象产物；节种类许可由此决定。 */
typedef enum EZrArtifactKind {
    ZR_ARTIFACT_KIND_ZRS = 1,
    ZR_ARTIFACT_KIND_ZRI = 2,
    ZR_ARTIFACT_KIND_ZRO = 3
} EZrArtifactKind;

/** @brief 目录中的稳定节 ID；读端可跳过未知的可选节。 */
typedef enum EZrArtifactSectionKind {
    ZR_ARTIFACT_SECTION_INVALID = 0,
    ZR_ARTIFACT_SECTION_STRING_HEAP = 1,
    ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE = 2,
    ZR_ARTIFACT_SECTION_TYPE_REF_TABLE = 3,
    ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE = 4,
    ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE = 5,
    ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE = 6,
    ZR_ARTIFACT_SECTION_SIGNATURE_HEAP = 7,
    ZR_ARTIFACT_SECTION_CONTRACT_TABLE = 8,
    ZR_ARTIFACT_SECTION_LAYOUT_TABLE = 9,
    ZR_ARTIFACT_SECTION_CODE_TABLE = 10,
    ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE = 11,
    ZR_ARTIFACT_SECTION_DEBUG_MAP = 12,
    ZR_ARTIFACT_SECTION_SYNTAX_TREE = 13,
    ZR_ARTIFACT_SECTION_SEMANTIC_IR = 14,
    ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE = 15,
    ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE = 16,
    ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE = 17,
    ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE = 18,
    ZR_ARTIFACT_SECTION_METADATA_BLOB_HEAP = 19,
    ZR_ARTIFACT_SECTION_LAYOUT_MAP_HEAP = 20,
    ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE = 21
} EZrArtifactSectionKind;

/* 目录仅识别 OPTIONAL 位；未知必需节不能交给旧 reader。 */
#define ZR_ARTIFACT_SECTION_FLAG_MANDATORY ((TZrUInt32)0u)
#define ZR_ARTIFACT_SECTION_FLAG_OPTIONAL ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_SECTION_FLAG_KNOWN_MASK ZR_ARTIFACT_SECTION_FLAG_OPTIONAL

/** @brief 编码、读取、身份和跨节校验的公开状态码。 */
typedef enum EZrArtifactStatus {
    ZR_ARTIFACT_STATUS_OK = 0,
    ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
    ZR_ARTIFACT_STATUS_BAD_MAGIC,
    ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION,
    ZR_ARTIFACT_STATUS_INVALID_KIND,
    ZR_ARTIFACT_STATUS_TRUNCATED,
    ZR_ARTIFACT_STATUS_COUNT_LIMIT,
    ZR_ARTIFACT_STATUS_UNKNOWN_MANDATORY_SECTION,
    ZR_ARTIFACT_STATUS_DUPLICATE_SECTION,
    ZR_ARTIFACT_STATUS_FORBIDDEN_SECTION,
    ZR_ARTIFACT_STATUS_INVALID_SECTION,
    ZR_ARTIFACT_STATUS_SECTION_OVERLAP,
    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
    ZR_ARTIFACT_STATUS_TRUNCATED_BLOB,
    ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
    ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_TYPE_SPEC_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
    ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_CONTRACT_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH,
    ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL,
    ZR_ARTIFACT_STATUS_INVALID_TEXT,
    ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH,
    ZR_ARTIFACT_STATUS_SCHEDULER_REQUIREMENT_MISMATCH,
    ZR_ARTIFACT_STATUS_SCHEDULER_ABI_MISMATCH,
    ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH,
    ZR_ARTIFACT_STATUS_SCHEDULER_CONTRACT_MISMATCH
} EZrArtifactStatus;

/** @brief 签名字节流的节点标签，深度及子节点数受上方限额约束。 */
typedef enum EZrArtifactSignatureNode {
    ZR_ARTIFACT_SIGNATURE_NODE_INVALID = 0,
    ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE = 1,
    ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF = 2,
    ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_PARAMETER = 3,
    ZR_ARTIFACT_SIGNATURE_NODE_GENERIC_INSTANCE = 4,
    ZR_ARTIFACT_SIGNATURE_NODE_ARRAY = 5,
    ZR_ARTIFACT_SIGNATURE_NODE_TUPLE = 6,
    ZR_ARTIFACT_SIGNATURE_NODE_UNION = 7,
    ZR_ARTIFACT_SIGNATURE_NODE_NULLABLE = 8,
    ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION = 9,
    ZR_ARTIFACT_SIGNATURE_NODE_REF = 10,
    ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW = 11,
    ZR_ARTIFACT_SIGNATURE_NODE_OWNER = 12,
    ZR_ARTIFACT_SIGNATURE_NODE_NEVER = 13,
    ZR_ARTIFACT_SIGNATURE_NODE_ERROR = 14,
    ZR_ARTIFACT_SIGNATURE_NODE_CONST_INT = 15,
    ZR_ARTIFACT_SIGNATURE_NODE_CONST_PARAMETER = 16
} EZrArtifactSignatureNode;

/** @brief 签名中 ref 的写入权限。 */
typedef enum EZrArtifactRefAccess {
    ZR_ARTIFACT_REF_WRITABLE = 0,
    ZR_ARTIFACT_REF_READONLY = 1
} EZrArtifactRefAccess;

/** @brief owner 签名中的所有权类别；跨进程只保存枚举值。 */
typedef enum EZrArtifactOwnerKind {
    ZR_ARTIFACT_OWNER_UNIQUE = 0,
    ZR_ARTIFACT_OWNER_SHARED = 1,
    ZR_ARTIFACT_OWNER_WEAK = 2,
    ZR_ARTIFACT_OWNER_ATOMIC_SHARED = 3
} EZrArtifactOwnerKind;

/** @brief 函数参数在签名中的值、借用或输出传递形式。 */
typedef enum EZrArtifactPassingForm {
    ZR_ARTIFACT_PASSING_VALUE = 0,
    ZR_ARTIFACT_PASSING_IN = 1,
    ZR_ARTIFACT_PASSING_REF = 2,
    ZR_ARTIFACT_PASSING_REF_READONLY = 3,
    ZR_ARTIFACT_PASSING_OUT = 4
} EZrArtifactPassingForm;

/** @brief 签名宣告的借用逃逸上界，供契约检查而非运行时指针保存。 */
typedef enum EZrArtifactEscapeUpperBound {
    ZR_ARTIFACT_ESCAPE_BLOCK = 0,
    ZR_ARTIFACT_ESCAPE_FUNCTION = 1,
    ZR_ARTIFACT_ESCAPE_CALLER = 2,
    ZR_ARTIFACT_ESCAPE_HEAP_STATIC = 3,
    ZR_ARTIFACT_ESCAPE_UNKNOWN = 4
} EZrArtifactEscapeUpperBound;

/** @brief 输出参数进入调用时的初始化状态。 */
typedef enum EZrArtifactEntryInitialization {
    ZR_ARTIFACT_ENTRY_INITIALIZED = 0,
    ZR_ARTIFACT_ENTRY_UNINITIALIZED = 1
} EZrArtifactEntryInitialization;

/** @brief 输出参数离开调用时的确定初始化保证。 */
typedef enum EZrArtifactExitInitialization {
    ZR_ARTIFACT_EXIT_UNCHANGED = 0,
    ZR_ARTIFACT_EXIT_DEFINITELY_INITIALIZED = 1
} EZrArtifactExitInitialization;

/** @brief 调用点对 ref/out 形参的显式标记。 */
typedef enum EZrArtifactCallSiteMarker {
    ZR_ARTIFACT_CALL_SITE_NONE = 0,
    ZR_ARTIFACT_CALL_SITE_REF = 1,
    ZR_ARTIFACT_CALL_SITE_OUT = 2
} EZrArtifactCallSiteMarker;

/** @brief callable 对接收者的只读或可变作用级别。 */
typedef enum EZrArtifactReceiverEffect {
    ZR_ARTIFACT_RECEIVER_NONE = 0,
    ZR_ARTIFACT_RECEIVER_READONLY = 1,
    ZR_ARTIFACT_RECEIVER_MUTABLE = 2
} EZrArtifactReceiverEffect;

/** @brief callable 返回借用引用时承诺的访问权限。 */
typedef enum EZrArtifactRefExportEffect {
    ZR_ARTIFACT_REF_EXPORT_NONE = 0,
    ZR_ARTIFACT_REF_EXPORT_READONLY = 1,
    ZR_ARTIFACT_REF_EXPORT_WRITABLE = 2
} EZrArtifactRefExportEffect;

/** @brief 已验证函数签名的简要投影；由签名摘要读取 API 填值，不持有签名字节。 */
typedef struct SZrArtifactCallableSignatureSummary {
    EZrArtifactReceiverEffect receiverEffect;
    EZrArtifactRefExportEffect refExportEffect;
    TZrUInt32 effectFlags;
    TZrUInt32 parameterCount;
    TZrBool hasScopedParameter;
} SZrArtifactCallableSignatureSummary;

/** @brief layout 行声明的 GC 扫描策略，需和 ownership map 协同解释。 */
typedef enum EZrArtifactGcScanKind {
    ZR_ARTIFACT_GC_SCAN_FREE = 0,
    ZR_ARTIFACT_GC_SCAN_MAPPED = 1,
    ZR_ARTIFACT_GC_SCAN_BARRIERED = 2
} EZrArtifactGcScanKind;

/** @brief 跨域传输的禁止、复制、克隆、句柄和资源移动策略。 */
typedef enum EZrArtifactDomainTransferKind {
    ZR_ARTIFACT_DOMAIN_TRANSFER_FORBIDDEN = 0,
    ZR_ARTIFACT_DOMAIN_TRANSFER_VALUE_COPY = 1,
    ZR_ARTIFACT_DOMAIN_TRANSFER_STRUCTURED_CLONE = 2,
    ZR_ARTIFACT_DOMAIN_TRANSFER_IMMUTABLE_HANDLE = 3,
    ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE = 4
} EZrArtifactDomainTransferKind;

/* 传输失败时的资源处置位；编码器拒绝未知位。 */
#define ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_KNOWN_MASK \
    ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE

/* 调度域模式与附着/隔离域所需的 send、sync 能力分开编码。 */
#define ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_SCHEDULER_POLICY_KNOWN_MASK \
    (ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN | \
     ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN)

#define ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SYNC ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_SCHEDULER_REQUIREMENT_KNOWN_MASK \
    (ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND | \
     ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SYNC)

/* TypeDef 能力位组成固定掩码，读写端必须拒绝未来未知位。 */
#define ZR_ARTIFACT_TYPE_FLAG_VALUE ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_TYPE_FLAG_GC ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_TYPE_FLAG_RESOURCE ((TZrUInt32)1u << 2u)
#define ZR_ARTIFACT_TYPE_FLAG_READONLY ((TZrUInt32)1u << 3u)
#define ZR_ARTIFACT_TYPE_FLAG_REF_LIKE ((TZrUInt32)1u << 4u)
#define ZR_ARTIFACT_TYPE_FLAG_DROP ((TZrUInt32)1u << 5u)
#define ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE ((TZrUInt32)1u << 6u)
#define ZR_ARTIFACT_TYPE_FLAG_INTERFACE ((TZrUInt32)1u << 7u)
#define ZR_ARTIFACT_TYPE_FLAG_ABSTRACT ((TZrUInt32)1u << 8u)
#define ZR_ARTIFACT_TYPE_FLAG_ENUM ((TZrUInt32)1u << 9u)
#define ZR_ARTIFACT_TYPE_FLAG_KNOWN_MASK ((TZrUInt32)0x3ffu)

/* callable、property 和逃逸位在独立行中使用各自掩码。 */
#define ZR_ARTIFACT_CONTRACT_FLAG_SCOPED ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_CONTRACT_FLAG_THROWS ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_CONTRACT_FLAG_ASYNC ((TZrUInt32)1u << 2u)
#define ZR_ARTIFACT_CONTRACT_FLAG_GENERATOR ((TZrUInt32)1u << 3u)
#define ZR_ARTIFACT_CONTRACT_FLAG_KNOWN_MASK ((TZrUInt32)0x0fu)
#define ZR_ARTIFACT_PROPERTY_FLAG_STATIC ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_PROPERTY_FLAG_ABSTRACT ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_PROPERTY_FLAG_VIRTUAL ((TZrUInt32)1u << 2u)
#define ZR_ARTIFACT_PROPERTY_FLAG_OVERRIDE ((TZrUInt32)1u << 3u)
#define ZR_ARTIFACT_PROPERTY_FLAG_REF_RETURN ((TZrUInt32)1u << 4u)
#define ZR_ARTIFACT_PROPERTY_FLAG_KNOWN_MASK ((TZrUInt32)0x1fu)

#define ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_SCOPED_INPUT ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_BORROWED_RETURN ((TZrUInt32)1u << 1u)
#define ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_KNOWN_MASK \
    (ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_SCOPED_INPUT | \
     ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_BORROWED_RETURN)

/** @brief callable 的 ABI 降低方式；与签名和契约哈希一起验证。 */
typedef enum EZrArtifactAbiLoweringKind {
    ZR_ARTIFACT_ABI_LOWERING_NONE = 0,
    ZR_ARTIFACT_ABI_LOWERING_ZR_VALUE_FRAME = 1,
    ZR_ARTIFACT_ABI_LOWERING_NATIVE_MARSHALLED = 2,
    ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT = 3
} EZrArtifactAbiLoweringKind;

/* layout 行仅识别稳定槽来源能力位。 */
#define ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE ((TZrUInt32)1u << 0u)
#define ZR_ARTIFACT_LAYOUT_CAPABILITY_KNOWN_MASK ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE

/** @brief metadata 保留等级，从身份到完整成员信息递增。 */
typedef enum EZrArtifactMetadataPreservationState {
    ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY = 1,
    ZR_ARTIFACT_METADATA_PRESERVATION_MEMBERS = 2,
    ZR_ARTIFACT_METADATA_PRESERVATION_FULL = 3
} EZrArtifactMetadataPreservationState;

/** @brief 反射消费者可见的类型类别，由 metadata 图约束。 */
typedef enum EZrArtifactReflectionCategory {
    ZR_ARTIFACT_REFLECTION_CATEGORY_ERASED = 0,
    ZR_ARTIFACT_REFLECTION_CATEGORY_CLASS = 1,
    ZR_ARTIFACT_REFLECTION_CATEGORY_CONCRETE_CLASS = 2,
    ZR_ARTIFACT_REFLECTION_CATEGORY_INSTANCE_CLASS = 3,
    ZR_ARTIFACT_REFLECTION_CATEGORY_STRUCT = 4,
    ZR_ARTIFACT_REFLECTION_CATEGORY_INTERFACE = 5,
    ZR_ARTIFACT_REFLECTION_CATEGORY_RESOURCE_CLASS = 6,
    ZR_ARTIFACT_REFLECTION_CATEGORY_REF_STRUCT = 7,
    ZR_ARTIFACT_REFLECTION_CATEGORY_ENUM = 8
} EZrArtifactReflectionCategory;

/** @brief metadata blob 中记录的语义类别。 */
typedef enum EZrArtifactMetadataRecordKind {
    ZR_ARTIFACT_METADATA_RECORD_ATTRIBUTE_DATA = 1,
    ZR_ARTIFACT_METADATA_RECORD_USER_DATA = 2,
    ZR_ARTIFACT_METADATA_RECORD_SOURCE_IDENTITY = 3,
    ZR_ARTIFACT_METADATA_RECORD_DECLARATION_FLAG = 4
} EZrArtifactMetadataRecordKind;

/** @brief metadata 记录面向运行时、测试或编译工具的保留用途。 */
typedef enum EZrArtifactMetadataRetention {
    ZR_ARTIFACT_METADATA_RETENTION_RUNTIME = 1,
    ZR_ARTIFACT_METADATA_RETENTION_TEST = 2,
    ZR_ARTIFACT_METADATA_RETENTION_COMPILE_TOOL = 3
} EZrArtifactMetadataRetention;

/* 当前 metadata 状态和记录均无可接受的扩展 flag。 */
#define ZR_ARTIFACT_METADATA_STATE_FLAG_KNOWN_MASK ((TZrUInt32)0u)
#define ZR_ARTIFACT_METADATA_RECORD_FLAG_KNOWN_MASK ((TZrUInt32)0u)

/** @brief 可选的解码/校验诊断；哈希、版本和 token 字段只在对应失配路径有效。 */
typedef struct SZrArtifactDiagnostic {
    EZrArtifactStatus status;
    TZrUInt32 sectionKind;
    TZrUInt32 rowIndex;
    TZrUInt32 byteOffset;
    TZrMetadataToken expectedToken;
    TZrMetadataToken actualToken;
    TZrUInt32 expectedVersion;
    TZrUInt32 actualVersion;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrArtifactDiagnostic;

/** @brief 产物根身份；token 与各摘要共同约束类型、布局、callable 和模块。
 * @note reserved0 在线格式写为零，不应以 C 结构内存直接编码。 */
typedef struct SZrArtifactPublicIdentity {
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken typeRefToken;
    TZrMetadataToken typeSpecToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 typeRefHash;
    TZrUInt64 typeSpecHash;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
    TZrUInt64 callableContractHash;
    TZrUInt64 moduleHash;
} SZrArtifactPublicIdentity;

/** @brief TypeDef 行保存类型和构造器身份；token 与哈希由整文档校验关联。 */
typedef struct SZrArtifactTypeDefRow {
    TZrMetadataToken token;
    TZrUInt32 flags;
    TZrUInt32 canonicalTypeId;
    TZrMetadataToken constructorToken;
    TZrMetadataToken constructorSignatureToken;
    TZrUInt32 reserved0;
    TZrUInt64 typeSignatureHash;
    TZrUInt64 constructorContractHash;
    TZrUInt64 reserved1;
} SZrArtifactTypeDefRow;

/** @brief TypeRef/TypeSpec 共用的身份行；签名偏移和长度索引独立 signature heap。 */
typedef struct SZrArtifactTypeIdentityRow {
    TZrMetadataToken token;
    TZrMetadataToken signatureToken;
    TZrUInt32 canonicalTypeId;
    TZrUInt32 flags;
    TZrUInt32 signatureOffset;
    TZrUInt32 signatureLength;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
} SZrArtifactTypeIdentityRow;

/** @brief 成员的 owner、名称和 callable 身份；名称偏移索引 string heap。 */
typedef struct SZrArtifactMemberDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 flags;
    TZrUInt32 nameStringOffset;
    TZrUInt32 reserved0;
    TZrUInt64 signatureHash;
    TZrUInt64 contractHash;
} SZrArtifactMemberDefRow;

/** @brief 属性与访问器、初始化器的 token 关系；关联有效性由 metadata 图复核。 */
typedef struct SZrArtifactPropertyDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrMetadataToken getterToken;
    TZrMetadataToken setterToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 flags;
    TZrUInt64 signatureHash;
    TZrUInt64 contractHash;
    TZrMetadataToken initializerToken;
    TZrUInt32 nameStringOffset;
} SZrArtifactPropertyDefRow;

/** @brief callable 的参数、receiver/ref 作用和 ABI 合同；哈希不代替字段校验。 */
typedef struct SZrArtifactContractRow {
    TZrMetadataToken memberToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 parameterCount;
    TZrUInt32 flags;
    TZrUInt32 receiverEffect;
    TZrUInt32 refExportEffect;
    TZrUInt32 escapeFlags;
    EZrArtifactAbiLoweringKind abiLoweringKind;
    TZrUInt64 contractHash;
} SZrArtifactContractRow;

/** @brief 类型大小、对齐、GC 和稳定槽布局；ownership map 由偏移/长度定位。 */
typedef struct SZrArtifactLayoutRow {
    TZrMetadataToken typeToken;
    TZrUInt32 version;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlignment;
    TZrUInt32 gcScanKind;
    TZrUInt32 capabilityFlags;
    TZrUInt32 ownershipMapOffset;
    TZrUInt32 ownershipMapLength;
    TZrUInt64 layoutHash;
    TZrUInt64 stableSlotContractHash;
} SZrArtifactLayoutRow;

/** @brief 反射保留状态及三个计数；metadataHash 绑定除自身外的状态字段。 */
typedef struct SZrArtifactMetadataStateRow {
    TZrMetadataToken typeToken;
    EZrArtifactMetadataPreservationState preservationState;
    EZrArtifactReflectionCategory category;
    TZrUInt32 metadataGeneration;
    TZrUInt32 retainedMemberCount;
    TZrUInt32 retainedPropertyCount;
    TZrUInt32 retainedMetaRecordCount;
    TZrUInt32 flags;
    TZrUInt64 typeSignatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 callableContractHash;
    TZrUInt64 metadataHash;
} SZrArtifactMetadataStateRow;

/** @brief metadata blob 的记录坐标；payload 由独立 heap 持有，recordHash 绑定内容。 */
typedef struct SZrArtifactMetadataRecordRow {
    TZrMetadataToken ownerToken;
    EZrArtifactMetadataRecordKind kind;
    EZrArtifactMetadataRetention retention;
    TZrUInt32 flags;
    TZrUInt32 payloadOffset;
    TZrUInt32 payloadLength;
    TZrUInt32 metadataGeneration;
    TZrUInt32 reserved0;
    TZrUInt64 recordHash;
} SZrArtifactMetadataRecordRow;

/** @brief 类型的跨域传输方式、schema 窗口和 provider 身份。
 * TODO: provider 身份目前仅核 token 形态与非零合同哈希；需核定哪个 consumer 验证实体对应。 */
typedef struct SZrArtifactDomainTransferRow {
    TZrMetadataToken typeToken;
    EZrArtifactDomainTransferKind kind;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 schemaOffset;
    TZrUInt32 schemaLength;
    TZrMetadataToken providerToken;
    TZrUInt32 reserved0;
    TZrUInt64 schemaHash;
    TZrUInt64 providerContractHash;
} SZrArtifactDomainTransferRow;

/** @brief 调度域 ABI、能力要求及传输/调度合同哈希。 */
typedef struct SZrArtifactSchedulerContractRow {
    TZrMetadataToken schedulerTypeToken;
    TZrMetadataToken taskTypeToken;
    TZrMetadataToken jobTypeToken;
    TZrUInt32 abiVersion;
    TZrUInt32 policyMask;
    TZrUInt32 attachedRequirementFlags;
    TZrUInt32 isolatedRequirementFlags;
    TZrUInt32 reserved0;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
} SZrArtifactSchedulerContractRow;

/** @brief 代码内偏移与稳定目标身份；装载后才解析为进程内索引。 */
typedef struct SZrArtifactRelocationRow {
    TZrUInt32 codeOffset;
    TZrUInt32 kind;
    TZrMetadataToken targetToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 expectedSignatureHash;
    TZrUInt64 expectedContractHash;
    TZrUInt64 expectedModuleHash;
} SZrArtifactRelocationRow;

/** @brief AOT 调用点与绑定合同、位置的固定行；节内按函数和缓存索引排序。 */
typedef struct SZrArtifactCallBindingRow {
    TZrUInt32 schemaVersion;
    TZrUInt32 functionIndex;
    TZrUInt32 cacheIndex;
    TZrUInt32 instructionIndex;
    SZrCallBindingContract contract;
    SZrCallBindingLocation location;
} SZrArtifactCallBindingRow;

/** @brief 写端借用的节数据；字节宽度由 kind 决定，data 在写入期间保持有效。 */
typedef struct SZrArtifactSectionInput {
    EZrArtifactSectionKind kind;
    TZrUInt32 flags;
    TZrUInt32 elementCount;
    const void *data;
} SZrArtifactSectionInput;

/** @brief 写前组装的 ZRS/ZRI/ZRO 文档；sections 数组及行/heap 数据由调用方持有。 */
typedef struct SZrArtifactDocument {
    EZrArtifactKind kind;
    TZrUInt32 flags;
    SZrArtifactPublicIdentity identity;
    TZrUInt32 sectionCount;
    const SZrArtifactSectionInput *sections;
} SZrArtifactDocument;

/** @brief 从已验证目录取得的节窗口；data 借用原始 artifact buffer。
 * @note 固定行读取期望对应 kind 且完整 byteLength；直接构造的视图须自行保证。 */
typedef struct SZrArtifactSectionView {
    TZrUInt32 kind;
    TZrUInt32 flags;
    TZrUInt32 byteOffset;
    TZrUInt32 byteLength;
    TZrUInt32 elementCount;
    TZrUInt32 elementSize;
    const TZrByte *data;
} SZrArtifactSectionView;

/** @brief 成功读取后的根视图；只借用 buffer，节视图须经 FindSection 取得。
 * @note Read 失败后可能保留部分已解码字段，调用方仅在 OK 后使用。 */
typedef struct SZrArtifactView {
    EZrArtifactKind kind;
    TZrUInt32 flags;
    SZrArtifactPublicIdentity identity;
    TZrUInt32 sectionCount;
    const TZrByte *buffer;
    TZrSize bufferLength;
} SZrArtifactView;

/** @brief 预检文档种类、身份、节及跨节关系，再计算总编码长度。
 * @note 失败时 outSize 置零；诊断可为空。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_GetEncodedSize(
        const SZrArtifactDocument *document,
        TZrSize *outSize,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 将通过同一预检的文档编码到调用方缓冲区。
 * @pre 节 data 不与目标 buffer 重叠；写入前会清零目标区域。
 * @note 失败时 outWrittenSize 置零。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_Write(
        const SZrArtifactDocument *document,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 验证线格式、目录、行和跨节身份后发布借用字节的根视图。
 * @note 只在 OK 后消费 outView；失败可能保留部分 header/身份字段。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_Read(
        const TZrByte *buffer,
        TZrSize bufferLength,
        SZrArtifactView *outView,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 在成功读取的视图中取指定节的借用窗口；缺失时清空 outSection。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_FindSection(
        const SZrArtifactView *view,
        EZrArtifactSectionKind kind,
        SZrArtifactSectionView *outSection,
        SZrArtifactDiagnostic *diagnostic);

/* BUG: 下列十一种固定行读取共用的定位器未核对节 kind、data 和完整 byteLength；
 * 直接传入伪造节视图可误解码或越界，且成功时可能保留旧诊断。 */
/** @brief 读取 TypeDef 类型和构造器身份；推荐使用 FindSection 的对应节。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadTypeDefRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactTypeDefRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取 TypeRef 或 TypeSpec 的签名窗口及布局身份。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadTypeIdentityRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactTypeIdentityRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取成员 owner、名称偏移和 callable 身份。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadMemberDefRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactMemberDefRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取属性访问器、初始化器和名称坐标。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadPropertyDefRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactPropertyDefRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取 callable 的逃逸、receiver/ref 和 ABI 合同。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadContractRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactContractRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取类型布局与 GC/ownership map 坐标。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadLayoutRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactLayoutRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取反射保留状态及摘要；记录关联由完整文档校验。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadMetadataStateRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactMetadataStateRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取 metadata 记录及 blob 坐标；不复制或持有 blob。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadMetadataRecordRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactMetadataRecordRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取跨域传输策略、provider 与 schema 窗口。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadDomainTransferRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactDomainTransferRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取调度域 ABI、需求位和合同哈希。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadSchedulerContractRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactSchedulerContractRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读取代码位置及目标 token/哈希，供装载端稍后解析。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadRelocationRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactRelocationRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 单独校验并编码一条固定 96 字节的 call binding 行。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_WriteCallBindingRow(
        const SZrArtifactCallBindingRow *row,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 从匹配的节中读取并校验 call binding 行；失败时 outRow 清零。
 * BUG: 合同解码失败后的错误分类会丢失非法 token 细节。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadCallBindingRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactCallBindingRow *outRow,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 在完整读取后对照外部预期的根身份；哈希/版本失配填诊断。
 * TODO: token 失配当前未填 expectedToken/actualToken，需确认调用方是否需要。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ValidatePublicIdentity(
        const SZrArtifactView *view,
        const SZrArtifactPublicIdentity *expected,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 验证签名字节恰好构成一个完整根节点；诊断偏移相对本片段。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ValidateSignature(
        const TZrByte *signature,
        TZrSize signatureLength,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 从已验证的函数签名提取作用和参数摘要；失败时输出清零。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadCallableSignatureSummary(
        const TZrByte *signature,
        TZrSize signatureLength,
        SZrArtifactCallableSignatureSummary *outSummary,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 对借用字节计算确定性哈希；空指针配正长度返回零。 */
ZR_CORE_API TZrUInt64 ZrCore_Artifact_HashBytes(const TZrByte *bytes, TZrSize byteLength);

/** @brief 按线格式字段顺序计算状态摘要，不把 metadataHash 自身纳入输入。 */
ZR_CORE_API TZrUInt64 ZrCore_Artifact_ComputeMetadataStateHash(
        const SZrArtifactMetadataStateRow *state);

/** @brief 将记录字段和借用的 payload 合成摘要；长度不符或数据缺失返回零。 */
ZR_CORE_API TZrUInt64 ZrCore_Artifact_ComputeMetadataRecordHash(
        const SZrArtifactMetadataRecordRow *record,
        const TZrByte *payload,
        TZrSize payloadLength);

/** @brief 将已验证二进制产物投影为可往返文本；输出由调用方持有。
 * @note 失败时已写文本前缀可能保留，但 outWrittenSize 为零。
 * TODO: 需核定文本目标与 view->buffer 的重叠契约。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_WriteText(
        const SZrArtifactView *view,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 从文本最后一个 payload-hex 还原二进制，再调用 Read 完整验证。
 * @note 失败时目标 buffer 可能含部分字节，outWrittenSize 为零。
 * TODO: 需核定 text 与二进制目标 buffer 的重叠契约。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Artifact_ReadText(
        const TZrChar *text,
        TZrSize textLength,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 返回静态状态名，调用方不释放。
 * BUG: 五个 scheduler/transport 失配状态缺少名称，被映射为 unknown。 */
ZR_CORE_API const TZrChar *ZrCore_Artifact_StatusName(EZrArtifactStatus status);
/** @brief 返回已知节的静态名称；未知节显示 unknown。 */
ZR_CORE_API const TZrChar *ZrCore_Artifact_SectionName(TZrUInt32 sectionKind);

#endif // ZR_VM_CORE_ARTIFACT_SCHEMA_H
