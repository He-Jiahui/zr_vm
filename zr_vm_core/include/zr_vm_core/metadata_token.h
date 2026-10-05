#ifndef ZR_VM_CORE_METADATA_TOKEN_H
#define ZR_VM_CORE_METADATA_TOKEN_H

#include "zr_vm_core/conf.h"

struct SZrString;

/** @brief 表标记和 1 起始 RID 组成的 32 位元数据身份；零值表示缺席。 */
typedef TZrUInt32 TZrMetadataToken;

/** @brief 供编译器、模块绑定和反射共同解释 token 高字节的表种类。 */
typedef enum EZrMetadataTableTag {
    ZR_METADATA_TABLE_MODULE = 1,
    ZR_METADATA_TABLE_TYPE_DEF = 2,
    ZR_METADATA_TABLE_MEMBER_DEF = 3,
    ZR_METADATA_TABLE_ASSEMBLY_REF = 4,
    ZR_METADATA_TABLE_TYPE_REF = 5,
    ZR_METADATA_TABLE_MEMBER_REF = 6,
    ZR_METADATA_TABLE_TYPE_SPEC = 7,
    ZR_METADATA_TABLE_SIGNATURE = 8
} EZrMetadataTableTag;

/* token 的高 8 位是表标记，低 24 位是 RID；MAKE 会截断越界输入。 */
/* TODO: AOT 紧缩重映射以保留行数加一生成新 RID，未见该入口检查 24 位上限；
 * 需核裁剪前后的最大行数约束，并用边界输入验证 MAKE 不产生碰撞。 */
#define ZR_METADATA_TOKEN_TABLE_SHIFT 24U
#define ZR_METADATA_TOKEN_RID_MASK ((TZrUInt32)0x00FFFFFFu)
#define ZR_METADATA_TOKEN_TABLE_MASK ((TZrUInt32)0xFFu)
#define ZR_METADATA_TOKEN_MAKE(TABLE, RID)                                                                            \
    ((((TZrUInt32)(TABLE) & ZR_METADATA_TOKEN_TABLE_MASK) << ZR_METADATA_TOKEN_TABLE_SHIFT) |                         \
     ((TZrUInt32)(RID) & ZR_METADATA_TOKEN_RID_MASK))
#define ZR_METADATA_TOKEN_TABLE(TOKEN)                                                                                \
    (((TZrUInt32)(TOKEN) >> ZR_METADATA_TOKEN_TABLE_SHIFT) & ZR_METADATA_TOKEN_TABLE_MASK)
#define ZR_METADATA_TOKEN_RID(TOKEN) ((TZrUInt32)(TOKEN) & ZR_METADATA_TOKEN_RID_MASK)

/** @brief 签名 blob 的节点标记；校验器按根节点种类递归解释后续字节。 */
typedef enum EZrMetadataSignatureNode {
    ZR_METADATA_SIGNATURE_NODE_INVALID = 0,
    ZR_METADATA_SIGNATURE_NODE_PRIMITIVE = 1,
    ZR_METADATA_SIGNATURE_NODE_TYPE_REF = 2,
    ZR_METADATA_SIGNATURE_NODE_TYPE_DEF = 3,
    ZR_METADATA_SIGNATURE_NODE_ARRAY = 4,
    ZR_METADATA_SIGNATURE_NODE_TUPLE = 5,
    ZR_METADATA_SIGNATURE_NODE_FUNC = 6,
    ZR_METADATA_SIGNATURE_NODE_GENERIC_INST = 7,
    ZR_METADATA_SIGNATURE_NODE_OWNERSHIP = 8,
    ZR_METADATA_SIGNATURE_NODE_UNION = 9,
    ZR_METADATA_SIGNATURE_NODE_NULLABLE = 10,
    ZR_METADATA_SIGNATURE_NODE_MEMBER_REF = 11,
    ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF = 12,
    ZR_METADATA_SIGNATURE_NODE_METHOD_SIG = 13,
    ZR_METADATA_SIGNATURE_NODE_FIELD_SIG = 14,
    ZR_METADATA_SIGNATURE_NODE_MODULE = 15
} EZrMetadataSignatureNode;

/** @brief 函数及模块的 token 关系与签名/布局身份记录。
 * 字符串指针是进程内 GC 对象引用；source 文件按字段序列化并重建，不能直接持久化指针。
 * related/owner/target token 与签名哈希共同约束模块解析结果。
 */
/* TODO: AOT 修剪器按 native sizeof 复制 tokenRecords，包括三个字符串指针；
 * 需核发布产物是否将非空宿主地址写入 blob，以及跨进程读取的字段约束。 */
typedef struct SZrMetadataTokenRecord {
    TZrMetadataToken token;
    TZrMetadataToken relatedToken;
    TZrMetadataToken ownerToken;
    TZrUInt32 ownerIndex;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 targetSignatureHash;
    TZrUInt64 targetModuleSignatureHash;
    struct SZrString *requestedModuleVersion;
    struct SZrString *minModuleVersionInclusive;
    struct SZrString *maxModuleVersionExclusive;
} SZrMetadataTokenRecord;

/* reserved0 标记可调用记录种类；ownerIndex 随种类解释，签名记录使用无槽哨兵。 */
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT ((TZrUInt32)1u)
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER ((TZrUInt32)2u)
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_MODULE ((TZrUInt32)3u)
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_SIGNATURE ((TZrUInt32)4u)
#define ZR_METADATA_TOKEN_RECORD_CALLABLE_CHILD ((TZrUInt32)5u)
#define ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY ((TZrUInt32)6u)

/** @brief 编译期稳定排序的字符串堆条目；value 由 GC 管理，表仅借用。 */
typedef struct SZrMetadataStringHeapEntry {
    TZrUInt32 stringIndex;
    struct SZrString *value;
} SZrMetadataStringHeapEntry;

/** @brief 调用方期望身份与绑定后实际身份的成对快照。
 * refToken 指向被解析引用，expected/resolved 字段供运行时比对签名及布局兼容性。
 */
typedef struct SZrMetadataTokenBinding {
    TZrMetadataToken refToken;
    TZrMetadataToken refSignatureToken;
    TZrUInt64 refSignatureHash;
    TZrMetadataToken expectedMetadataToken;
    TZrMetadataToken expectedSignatureToken;
    TZrUInt64 expectedSignatureHash;
    TZrUInt64 expectedModuleSignatureHash;
    TZrUInt32 expectedLayoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 expectedLayoutHash;
    TZrMetadataToken resolvedMetadataToken;
    TZrMetadataToken resolvedSignatureToken;
    TZrUInt64 resolvedSignatureHash;
    TZrUInt64 resolvedModuleSignatureHash;
    TZrUInt32 resolvedLayoutVersion;
    TZrUInt32 reserved1;
    TZrUInt64 resolvedLayoutHash;
} SZrMetadataTokenBinding;

/** @brief TypeSpec 绑定统计与首个失败样本；未出现的失败类别对应字段保持零值。 */
typedef struct SZrMetadataTypeSpecBindStatus {
    TZrUInt32 callerTypeSpecCount;
    TZrUInt32 matchedTypeSpecCount;
    TZrUInt32 unmatchedTypeSpecCount;
    TZrUInt32 definitionMismatchCount;
    TZrUInt32 layoutMismatchCount;
    TZrMetadataToken firstUnmatchedTypeSpecToken;
    TZrUInt64 firstUnmatchedSignatureHash;
    TZrMetadataToken firstDefinitionMismatchTypeSpecToken;
    TZrUInt64 firstExpectedDefinitionSignatureHash;
    TZrUInt64 firstActualDefinitionSignatureHash;
    TZrMetadataToken firstLayoutMismatchTypeSpecToken;
    TZrUInt32 firstExpectedLayoutVersion;
    TZrUInt32 firstActualLayoutVersion;
    TZrUInt64 firstExpectedLayoutHash;
    TZrUInt64 firstActualLayoutHash;
} SZrMetadataTypeSpecBindStatus;

/** @brief TypeRef 绑定统计与首个失败样本；供模块链接诊断展示差异。 */
typedef struct SZrMetadataTypeRefBindStatus {
    TZrUInt32 callerTypeRefCount;
    TZrUInt32 matchedTypeRefCount;
    TZrUInt32 unmatchedTypeRefCount;
    TZrUInt32 definitionMismatchCount;
    TZrUInt32 layoutMismatchCount;
    TZrMetadataToken firstUnmatchedTypeRefToken;
    TZrUInt64 firstUnmatchedSignatureHash;
    TZrMetadataToken firstDefinitionMismatchTypeRefToken;
    TZrUInt64 firstExpectedDefinitionSignatureHash;
    TZrUInt64 firstActualDefinitionSignatureHash;
    TZrMetadataToken firstLayoutMismatchTypeRefToken;
    TZrUInt32 firstExpectedLayoutVersion;
    TZrUInt32 firstActualLayoutVersion;
    TZrUInt64 firstExpectedLayoutHash;
    TZrUInt64 firstActualLayoutHash;
} SZrMetadataTypeRefBindStatus;

#endif // ZR_VM_CORE_METADATA_TOKEN_H
