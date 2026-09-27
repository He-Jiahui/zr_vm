#ifndef ZR_VM_CORE_CONTAINER_STORAGE_CONTRACT_H
#define ZR_VM_CORE_CONTAINER_STORAGE_CONTRACT_H

/*
 * map/string 存储优化的可缓存见证值。记录只含标量，可由 ExecIR 规划器复制到
 * 计划或产物中，不延长托管对象、回调及借用缓冲区的生命周期。校验通过仅表示
 * 可以考虑该策略；事实缺失时走通用路径，哈希命中后仍须执行语言相等性。
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 版本和类型标记用于拒绝跨格式复用；OFFSET_NONE 仅表示布局没有 next 槽。 */
#define ZR_CONTAINER_STORAGE_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_COMPACT_MAP_CONTRACT_MAGIC ((TZrUInt32)0x314d5043u) /* CPM1 */
#define ZR_STRING_STORAGE_CONTRACT_MAGIC ((TZrUInt32)0x31545343u) /* CST1 */
#define ZR_COMPACT_MAP_OFFSET_NONE ((TZrUInt32)UINT32_MAX)
#define ZR_STRING_STORAGE_DEFAULT_SHORT_LIMIT ((TZrUInt64)ZR_VM_SHORT_STRING_MAX)

/** @brief 拒绝优化见证值的稳定原因类别；调用方可据此回退通用路径。 */
typedef enum EZrContainerStorageDiagnosticCode {
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_NONE = 0,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_MAGIC,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNKNOWN_FLAGS,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INVALID_LAYOUT,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_DOMAIN,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_OWNERSHIP,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ITERATION,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_TOMBSTONE,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_IDENTITY_OBSERVED,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_INTERN_RETENTION,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_SHORT_STRING_LIMIT,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_EFFECT_MISMATCH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ROPE_UNMEASURED,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_BUDGET,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_DEPTH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_SEGMENTS,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_OVERFLOW,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSUPPORTED,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_MISMATCH,
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_COUNT
} EZrContainerStorageDiagnosticCode;

/* Compatibility spellings used by callers that describe the reason first. */
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_HASH_UNSTABLE \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_HASH
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_EQUALITY_UNSTABLE \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_CUSTOM_EQUALITY \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_CUSTOM_EQUALITY \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_UNSTABLE_EQUALITY
#define ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE_OBSERVED \
    ZR_CONTAINER_STORAGE_DIAGNOSTIC_ESCAPE

/** @brief 可选的值型诊断；field 是本契约的字段类别，不持有源代码或对象指针。 */
typedef struct SZrContainerStorageDiagnostic {
    EZrContainerStorageDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrContainerStorageDiagnostic;

/* 键的不可变性、哈希域及相等性必须由事实提供方证明；未知或自定义相等性
 * 不能从类型名推断为纯操作，KNOWN_MASK 用于拒绝未来版本的未知标志。 */
#define ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY ((TZrUInt32)1u << 0u)
#define ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE ((TZrUInt32)1u << 1u)
#define ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE ((TZrUInt32)1u << 2u)
#define ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL ((TZrUInt32)1u << 3u)
#define ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY ((TZrUInt32)1u << 4u)
#define ZR_COMPACT_MAP_HASH_FLAG_IDENTITY_OBSERVED ((TZrUInt32)1u << 5u)
#define ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE ((TZrUInt32)1u << 6u)
#define ZR_COMPACT_MAP_HASH_FLAG_KNOWN_MASK \
    (ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY | \
     ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE | \
     ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE | \
     ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL | \
     ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY | \
     ZR_COMPACT_MAP_HASH_FLAG_IDENTITY_OBSERVED | \
     ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE)

/** @brief map 键哈希的域见证；seed/domain/version 同时参与字节哈希，不能跨域比较。 */
typedef struct SZrCompactMapHash {
    TZrUInt64 seed;
    TZrUInt64 domain;
    TZrUInt32 version;
    TZrUInt32 flags;
} SZrCompactMapHash;

/** @brief 删除槽位的语义；保留插入顺序时不能用回移删除。 */
typedef enum EZrCompactMapTombstonePolicy {
    ZR_COMPACT_MAP_TOMBSTONE_NONE = 0,
    ZR_COMPACT_MAP_TOMBSTONE_MARK = 1,
    ZR_COMPACT_MAP_TOMBSTONE_BACKSHIFT = 2
} EZrCompactMapTombstonePolicy;

/** @brief 迭代可观察顺序；PRESERVE_ITERATION 要求 INSERTION_ORDER。 */
typedef enum EZrCompactMapIterationPolicy {
    ZR_COMPACT_MAP_ITERATION_UNSPECIFIED = 0,
    ZR_COMPACT_MAP_ITERATION_BUCKET_ORDER = 1,
    ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER = 2
} EZrCompactMapIterationPolicy;

/** @brief 候选布局对条目引用的持有方式；GC_ROOTED 必须与 GC_ROOTS 同步。 */
typedef enum EZrCompactMapOwnershipPolicy {
    ZR_COMPACT_MAP_OWNERSHIP_NONE = 0,
    ZR_COMPACT_MAP_OWNERSHIP_BORROWED = 1,
    ZR_COMPACT_MAP_OWNERSHIP_OWNED = 2,
    ZR_COMPACT_MAP_OWNERSHIP_GC_ROOTED = 3
} EZrCompactMapOwnershipPolicy;

/** @brief 条目布局的标量描述，不含条目数据或任何对象所有权。
 * @note 偏移和槽位只由候选校验器检查；实际分配和访问仍由存储实现负责。
 */
typedef struct SZrCompactMapLayout {
    TZrUInt32 entrySize;
    TZrUInt32 entryAlignment;
    TZrUInt32 bucketCount;
    TZrUInt32 maxLoadNumerator;
    TZrUInt32 maxLoadDenominator;
    TZrUInt32 hashOffset;
    TZrUInt32 keyOffset;
    TZrUInt32 valueOffset;
    TZrUInt32 nextOffset;
    EZrCompactMapTombstonePolicy tombstonePolicy;
    EZrCompactMapIterationPolicy iterationPolicy;
    EZrCompactMapOwnershipPolicy ownershipPolicy;
} SZrCompactMapLayout;

/* 候选策略承诺保留顺序、GC 根和缓存哈希；设置标志须同时满足布局与哈希前提。 */
#define ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION ((TZrUInt32)1u << 0u)
#define ZR_COMPACT_MAP_FLAG_GC_ROOTS ((TZrUInt32)1u << 1u)
#define ZR_COMPACT_MAP_FLAG_CACHE_HASH ((TZrUInt32)1u << 2u)
/* TODO: 此位目前只参与已知位校验及摘要，校验器和 ExecIR 未按它改变策略；
 * 核实它是实体布局承诺还是预留位，再决定设置和消费条件。 */
#define ZR_COMPACT_MAP_FLAG_COMPACT_LAYOUT ((TZrUInt32)1u << 3u)
#define ZR_COMPACT_MAP_FLAG_KNOWN_MASK \
    (ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION | ZR_COMPACT_MAP_FLAG_GC_ROOTS | \
     ZR_COMPACT_MAP_FLAG_CACHE_HASH | ZR_COMPACT_MAP_FLAG_COMPACT_LAYOUT)

/** @brief 可复制的 map 特化候选；Finalize 生成双层哈希，消费前须重新 Validate。
 * @note 哈希只检测记录是否改动，不证明语言键的相等性，也不授权读取条目。
 */
typedef struct SZrCompactMapCandidate {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    SZrCompactMapHash hash;
    SZrCompactMapLayout layout;
    TZrUInt32 flags;
    TZrUInt32 reserved;
    TZrUInt64 layoutHash;
    TZrUInt64 candidateHash;
} SZrCompactMapCandidate;

/** @brief 将可选诊断置为“无错误”；NULL 表示调用方不接收细节。 */
ZR_CORE_API void ZrCore_ContainerStorage_DiagnosticClear(
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 返回静态诊断名；未知代码返回静态的 unknown，不转移字符串所有权。 */
ZR_CORE_API const TZrChar *ZrCore_ContainerStorage_DiagnosticName(
        EZrContainerStorageDiagnosticCode code);

/** @brief 初始化待填写的候选，包括格式标记与无 next 槽的哨兵值。 */
ZR_CORE_API void ZrCore_CompactMapCandidate_Init(
        SZrCompactMapCandidate *candidate);
/** @brief 校验提供方填写的布局、哈希和可观察语义后封存候选。
 * @return 失败时不更新旧摘要，调用方不得使用该候选；成功后才写入两个摘要。
 */
ZR_CORE_API TZrBool ZrCore_CompactMapCandidate_Finalize(
        SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 消费缓存或跨层复制的 map 候选前复验格式、前提及摘要。
 * @note 不改变候选；摘要不是防恶意篡改的认证，也不能替代键相等性。
 */
ZR_CORE_API TZrBool ZrCore_CompactMapCandidate_Validate(
        const SZrCompactMapCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 判断是否可跨探测复用键哈希；要求不可变键及纯、全定义的相等性。 */
ZR_CORE_API TZrBool ZrCore_CompactMap_CanCacheHash(
        const SZrCompactMapHash *hash);
/** @brief 按见证中的 seed/domain/version 对字节串生成稳定的非零探测哈希。
 * @pre length 非零时 data 指向至少 length 个可读字节。
 * @return 参数或见证无效时返回零；结果只是桶定位提示，不是相等性证明。
 */
ZR_CORE_API TZrUInt64 ZrCore_CompactMap_HashBytes(
        const SZrCompactMapHash *hash,
        const TZrByte *data,
        TZrSize length);
/** @brief 比较两个已成形哈希见证是否属于同一缓存域；不比较实际键。 */
ZR_CORE_API TZrBool ZrCore_CompactMap_HashWitnessEqual(
        const SZrCompactMapHash *left,
        const SZrCompactMapHash *right);
/** @brief 匹配哈希仅允许进入语言相等性比较，包括自定义或有副作用的比较。
 * @note 调用方仍须检查哈希本身是否有效，并在返回真后调用语言比较操作。
 */
ZR_CORE_API TZrBool ZrCore_CompactMap_ProbeNeedsEquality(
        TZrUInt64 storedHash,
        TZrUInt64 incomingHash);
/** @brief 计算二次幂桶数的索引；无效桶数返回零，调用方须自行拒绝该布局。 */
ZR_CORE_API TZrSize ZrCore_CompactMap_BucketIndex(
        TZrUInt64 hash,
        TZrSize bucketCount);

/* 字符串策略事实只含标量；身份、逃逸、异常顺序、驻留域及预算由事实提供方证明，
 * KNOWN_MASK 拒绝本版本不理解的位，不能把缺少证据解释为优化许可。 */
#define ZR_STRING_STORAGE_FLAG_IMMUTABLE ((TZrUInt32)1u << 0u)
/* TODO: 此位目前仅参与已知位校验和候选摘要，策略门槛未读取；核实
 * 字节串特化是否需要区分已验证 UTF-8 与任意字节序列。 */
#define ZR_STRING_STORAGE_FLAG_UTF8_VALID ((TZrUInt32)1u << 1u)
#define ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED ((TZrUInt32)1u << 2u)
#define ZR_STRING_STORAGE_FLAG_ESCAPES ((TZrUInt32)1u << 3u)
#define ZR_STRING_STORAGE_FLAG_FFI_VISIBLE ((TZrUInt32)1u << 4u)
#define ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE ((TZrUInt32)1u << 5u)
#define ZR_STRING_STORAGE_FLAG_INTERN_WEAK ((TZrUInt32)1u << 6u)
#define ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE ((TZrUInt32)1u << 7u)
#define ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT ((TZrUInt32)1u << 8u)
#define ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT ((TZrUInt32)1u << 9u)
#define ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN ((TZrUInt32)1u << 10u)
#define ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY ((TZrUInt32)1u << 11u)
#define ZR_STRING_STORAGE_FLAG_ADDRESS_ESCAPES ((TZrUInt32)1u << 12u)
#define ZR_STRING_STORAGE_FLAG_KNOWN_MASK \
    (ZR_STRING_STORAGE_FLAG_IMMUTABLE | ZR_STRING_STORAGE_FLAG_UTF8_VALID | \
     ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED | ZR_STRING_STORAGE_FLAG_ESCAPES | \
     ZR_STRING_STORAGE_FLAG_FFI_VISIBLE | \
     ZR_STRING_STORAGE_FLAG_EXCEPTION_ORDER_VISIBLE | \
     ZR_STRING_STORAGE_FLAG_INTERN_WEAK | \
     ZR_STRING_STORAGE_FLAG_INTERN_DOMAIN_STABLE | \
     ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT | \
     ZR_STRING_STORAGE_FLAG_ROPE_MEASURED_BENEFIT | \
     ZR_STRING_STORAGE_FLAG_ROPE_BOUNDED_FLATTEN | \
     ZR_STRING_STORAGE_FLAG_CUSTOM_EQUALITY | \
     ZR_STRING_STORAGE_FLAG_ADDRESS_ESCAPES)

/** @brief 调用方证明的字符串可观察性和资源预算事实。
 * @note storageGeneration 属于上层事实身份；本层候选不持有原字符串或缓冲区。
 */
typedef struct SZrStringStorageFacts {
    TZrUInt32 flags;
    TZrUInt32 reserved;
    TZrUInt64 byteLength;
    TZrUInt64 shortStringLimit;
    TZrUInt64 internDomain;
    TZrUInt64 storageGeneration;
    TZrUInt64 declaredEffects;
    TZrUInt64 replacementEffects;
    TZrUInt64 intermediateCount;
    TZrUInt64 maxBuilderBytes;
    TZrUInt64 ropeDepth;
    TZrUInt64 segmentCount;
    TZrUInt64 ropeMaxDepth;
    TZrUInt64 ropeMaxSegments;
    TZrUInt64 measuredCopyBytes;
    TZrUInt64 projectedRopeBytes;
    TZrUInt64 flattenBudgetBytes;
    TZrUInt64 flattenBudgetMicros;
} SZrStringStorageFacts;

/** @brief 计划中的存储选择；GENERIC 表示未获得特化许可。 */
typedef enum EZrStringStorageStrategy {
    ZR_STRING_STORAGE_STRATEGY_GENERIC = 0,
    ZR_STRING_STORAGE_STRATEGY_SSO = 1,
    ZR_STRING_STORAGE_STRATEGY_INTERN = 2,
    ZR_STRING_STORAGE_STRATEGY_BUILDER = 3,
    ZR_STRING_STORAGE_STRATEGY_ROPE = 4
} EZrStringStorageStrategy;

/* Common spelling aliases keep the contract readable to C callers that use
 * the all-caps abbreviation for short-string optimisation. */
#define ZR_STRING_STORAGE_STRATEGY_SHORT_STRING ZR_STRING_STORAGE_STRATEGY_SSO
#define ZrCore_StringStorage_CanUseSSO ZrCore_StringStorage_CanUseSso

/** @brief 从事实选出的可复制候选；candidateHash 覆盖其标量字段。
 * @note 使用前重新 Validate，并由上层把候选与当前事实、代次和计划绑定。
 */
typedef struct SZrStringStorageCandidate {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrStringStorageStrategy strategy;
    TZrUInt32 flags;
    TZrUInt64 byteLength;
    TZrUInt64 shortStringLimit;
    TZrUInt64 internDomain;
    TZrUInt64 ropeDepth;
    TZrUInt64 segmentCount;
    TZrUInt64 ropeMaxDepth;
    TZrUInt64 ropeMaxSegments;
    TZrUInt64 measuredCopyBytes;
    TZrUInt64 projectedRopeBytes;
    TZrUInt64 flattenBudgetBytes;
    TZrUInt64 flattenBudgetMicros;
    TZrUInt64 declaredEffects;
    TZrUInt64 replacementEffects;
    TZrUInt64 intermediateCount;
    TZrUInt64 maxBuilderBytes;
    TZrUInt64 candidateHash;
} SZrStringStorageCandidate;

/** @brief 为事实提供保守起点和运行时短串上限；其余许可需由提供方填写。 */
ZR_CORE_API void ZrCore_StringStorageFacts_Init(
        SZrStringStorageFacts *facts);
/** @brief 验证事实的格式、已知标志与预算基本约束，不等于某策略可用。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_Validate(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 确认短串可内联且替换存储不会改变身份、逃逸或相等性。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseSso(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 确认短串可在稳定域内弱驻留，不改变保留期和可观察身份。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseIntern(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 确认合并中间字符串不会改变效应顺序，且结果在构造预算内。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseBuilder(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 确认 rope 有实测收益，并受深度、片段数和扁平化预算约束。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_CanUseRope(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 按驻留、短串、构造器、rope 的保守优先级选策略；全失败则回退。
 * @note 返回 GENERIC 时诊断保存最后一次拒绝原因，不保证是所有原因的汇总。
 */
ZR_CORE_API EZrStringStorageStrategy ZrCore_StringStorage_SelectStrategy(
        const SZrStringStorageFacts *facts,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 从事实构造带摘要的非通用候选；失败时将非空输出候选清零。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_BuildCandidate(
        const SZrStringStorageFacts *facts,
        SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);
/** @brief 消费前复验候选策略、资源约束和摘要；不修改也不持有候选。 */
ZR_CORE_API TZrBool ZrCore_StringStorage_ValidateCandidate(
        const SZrStringStorageCandidate *candidate,
        SZrContainerStorageDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_CONTAINER_STORAGE_CONTRACT_H */
