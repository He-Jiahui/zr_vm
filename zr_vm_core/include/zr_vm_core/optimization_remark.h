#ifndef ZR_VM_CORE_OPTIMIZATION_REMARK_H
#define ZR_VM_CORE_OPTIMIZATION_REMARK_H

/* 优化备注保存生产者给出的源事实；CLI/JSON/LSP 消费者按字段投影，不重新推断优化结果。
 * 固定数组和标量使记录可按值复制，但跨进程格式由显式投影决定。 */
/* TODO: ExecIR pass 已写 remark sink，但 ImportExecIr 目前只由测试调用，CLI
 * explain 路径还创建空 store；核对编译/AOT 发布入口及持久化消费者的接线契约。 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX ((TZrSize)48u)
#define ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX ((TZrSize)64u)

/** @brief 规范源快照的半开字节区间，供 LSP 投影为行列位置。 */
typedef struct SZrOptimizationRemarkSourceRange {
    TZrUInt32 startOffset;
    TZrUInt32 endOffset;
} SZrOptimizationRemarkSourceRange;

/** @brief 区分优化成功、未触发与明确阻断；后两者必须附原因。 */
typedef enum EZrOptimizationRemarkStatus {
    ZR_OPTIMIZATION_REMARK_SUCCESS = 0,
    ZR_OPTIMIZATION_REMARK_MISSED,
    ZR_OPTIMIZATION_REMARK_BLOCKED,
    ZR_OPTIMIZATION_REMARK_STATUS_COUNT
} EZrOptimizationRemarkStatus;

/** @brief 跨生产者共享的原因码；数值同时用于查询位掩码。 */
typedef enum EZrOptimizationRemarkReason {
    ZR_OPTIMIZATION_REMARK_REASON_NONE = 0,
    ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN,
    ZR_OPTIMIZATION_REMARK_REASON_ESCAPES,
    ZR_OPTIMIZATION_REMARK_REASON_ABI_VISIBLE,
    ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER,
    ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET,
    ZR_OPTIMIZATION_REMARK_REASON_PROFILE_STALE,
    ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED,
    ZR_OPTIMIZATION_REMARK_REASON_CAPABILITY_DENIED,
    ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE,
    ZR_OPTIMIZATION_REMARK_REASON_BOXING,
    ZR_OPTIMIZATION_REMARK_REASON_BOUNDS,
    ZR_OPTIMIZATION_REMARK_REASON_VECTOR,
    ZR_OPTIMIZATION_REMARK_REASON_BARRIER,
    ZR_OPTIMIZATION_REMARK_REASON_INLINING,
    ZR_OPTIMIZATION_REMARK_REASON_AOT,
    ZR_OPTIMIZATION_REMARK_REASON_LAYOUT,
    ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION,
    ZR_OPTIMIZATION_REMARK_REASON_DEOPT,
    ZR_OPTIMIZATION_REMARK_REASON_SOFTWARE_IC_MISS,
    ZR_OPTIMIZATION_REMARK_REASON_HARDWARE_CACHE_MISS,
    ZR_OPTIMIZATION_REMARK_REASON_BOUNDS_UNKNOWN,
    ZR_OPTIMIZATION_REMARK_REASON_CANCELLED,
    ZR_OPTIMIZATION_REMARK_REASON_TRUNCATED,
    ZR_OPTIMIZATION_REMARK_REASON_COUNT
} EZrOptimizationRemarkReason;

/* 旧拼写仅作同值源码别名，不产生新的原因码。 */
#define ZR_OPTIMIZATION_REMARK_REASON_BOXING_REQUIRED \
    ZR_OPTIMIZATION_REMARK_REASON_BOXING
#define ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION_REQUIRED \
    ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION
#define ZR_OPTIMIZATION_REMARK_REASON_DEOPT_REQUIRED \
    ZR_OPTIMIZATION_REMARK_REASON_DEOPT

/* 两种类型名共享同一枚举，避免出现不同布局的兼容结构。 */
typedef EZrOptimizationRemarkReason ZrOptimizationRemarkReason;

/** @brief 事实依据类别；只有 MEASURED 可携带测量计数器。 */
typedef enum EZrOptimizationRemarkEvidence {
    ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN = 0,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_UNAVAILABLE,
    ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT
} EZrOptimizationRemarkEvidence;

/* backendMask 可合并多个后端；Validate 拒绝零值与未知位。 */
#define ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_AOT ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_JIT ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_LLVM ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_INTERPRETER ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_REMARK_BACKEND_KNOWN_MASK \
    (ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC | \
     ZR_OPTIMIZATION_REMARK_BACKEND_AOT | \
     ZR_OPTIMIZATION_REMARK_BACKEND_JIT | \
     ZR_OPTIMIZATION_REMARK_BACKEND_LLVM | \
     ZR_OPTIMIZATION_REMARK_BACKEND_INTERPRETER)

/* measuredCounterMask 只标记实际可用的计数器，未标记值投影为 null。 */
#define ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_HARDWARE_CACHE ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_BRANCH ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_ALLOCATIONS ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_DEOPT ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_REMARK_COUNTER_KNOWN_MASK \
    (ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC | \
     ZR_OPTIMIZATION_REMARK_COUNTER_HARDWARE_CACHE | \
     ZR_OPTIMIZATION_REMARK_COUNTER_BRANCH | \
     ZR_OPTIMIZATION_REMARK_COUNTER_ALLOCATIONS | \
     ZR_OPTIMIZATION_REMARK_COUNTER_DEOPT)

/* 位筛选宏可能对参数求值两次；传入无副作用的枚举值。 */
#define ZR_OPTIMIZATION_REMARK_REASON_MASK(reason) \
    ((TZrUInt32)(TZrUInt32)(reason) < 32u \
         ? ((TZrUInt32)1u << (TZrUInt32)(reason)) : 0u)

#define ZR_OPTIMIZATION_REMARK_STATUS_MASK(status) \
    ((TZrUInt32)(TZrUInt32)(status) < 32u \
         ? ((TZrUInt32)1u << (TZrUInt32)(status)) : 0u)

#define ZR_OPTIMIZATION_REMARK_EVIDENCE_MASK(evidence) \
    ((TZrUInt32)(TZrUInt32)(evidence) < 32u \
         ? ((TZrUInt32)1u << (TZrUInt32)(evidence)) : 0u)

/** @brief 备注查询/校验诊断独立于 ExecIR 编译错误。 */
typedef enum EZrOptimizationRemarkDiagnosticCode {
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NONE = 0,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_SCHEMA,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_RANGE,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_STATUS,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_EVIDENCE,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_BACKEND,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_COUNTER,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NOT_FOUND,
    ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNT
} EZrOptimizationRemarkDiagnosticCode;

/** @brief 可选的失败详情；成功入口先清空，失败填写字段及期望/实际值。 */
typedef struct SZrOptimizationRemarkDiagnostic {
    EZrOptimizationRemarkDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrOptimizationRemarkDiagnostic;

/** @brief 运行时采样值；有效字段由记录的 measuredCounterMask 指定。 */
typedef struct SZrOptimizationRemarkCounters {
    TZrUInt64 softwareIcMisses;
    TZrUInt64 hardwareCacheMisses;
    TZrUInt64 branchMisses;
    TZrUInt64 allocations;
    TZrUInt64 deopts;
} SZrOptimizationRemarkCounters;

/** @brief 无指针的备注值；store 按值复制，CLI/LSP 按字段序列化或投影。
 * @note pass/module 属于记录本身；sourceRange 相对于同一 sourceVersion 的源快照。
 */
/* TODO: 记录含编译器决定宽度的 enum 与匿名 union，仓内未见原始结构的 artifact
 * 读写；核对是否需要跨工具链原始二进制 ABI，并为该路径定义显式编码或布局约束。 */
typedef struct SZrOptimizationRemark {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 moduleVersion;
    TZrUInt64 irVersion;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reserved0;
    /* Stable profile/runtime site identity.  It is a scalar key (never a
     * code address) and remains distinct from the proof witness below. */
    TZrUInt64 siteKey;
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
    EZrOptimizationRemarkStatus status;
    union {
        EZrOptimizationRemarkReason reason;
        EZrOptimizationRemarkReason reasonCode;
    };
    union {
        TZrUInt32 backendMask;
        TZrUInt32 backend;
    };
    union {
        EZrOptimizationRemarkEvidence evidence;
        EZrOptimizationRemarkEvidence evidenceKind;
    };
    TZrUInt32 reserved1;
    union {
        TZrUInt64 proofId;
        TZrUInt64 proof;
    };
    union {
        TZrUInt64 profileCount;
        TZrUInt64 profile;
    };
    union {
        TZrUInt64 estimatedCost;
        TZrUInt64 cost;
    };
    union {
        TZrUInt32 before;
        TZrUInt32 beforeRepresentation;
    };
    union {
        TZrUInt32 after;
        TZrUInt32 afterRepresentation;
    };
    TZrUInt32 measuredCounterMask;
    union {
        TZrUInt32 boxingFlags;
        TZrUInt32 boxing;
    };
    union {
        TZrUInt32 allocationFlags;
        TZrUInt32 allocation;
    };
    union {
        TZrUInt32 cacheFlags;
        TZrUInt32 cache;
    };
    union {
        TZrUInt32 deoptFlags;
        TZrUInt32 deopt;
    };
    union {
        SZrOptimizationRemarkCounters measuredCounters;
        SZrOptimizationRemarkCounters counters;
        struct {
            TZrUInt64 softwareIcMisses;
            TZrUInt64 hardwareCacheMisses;
            TZrUInt64 branchMisses;
            TZrUInt64 allocations;
            TZrUInt64 deopts;
        };
    };
} SZrOptimizationRemark;

/** @brief 持有 items 缓冲；容量上限后的丢弃累计到 droppedCount/truncated。 */
typedef struct SZrOptimizationRemarkStore {
    SZrOptimizationRemark *items;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 maxRecords;
    TZrUInt32 reserved;
    TZrUInt64 droppedCount;
    TZrBool truncated;
} SZrOptimizationRemarkStore;

/** @brief 零值默认不过滤；has* 允许精确选择零哈希、版本和 sourceId。
 * @note pageLimit 为零时返回偏移之后的全部匹配项。 */
typedef struct SZrOptimizationRemarkQuery {
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reasonMask;
    TZrUInt32 statusMask;
    TZrUInt32 backendMask;
    TZrUInt32 evidenceMask;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrBool hasModuleHash;
    TZrBool hasIrHash;
    TZrBool hasSourceVersion;
    TZrBool hasSourceId;
    TZrBool hasSourceRange;
    TZrBool hasPass;
    TZrBool hasModule;
    TZrBool reservedFlags[1];
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
} SZrOptimizationRemarkQuery;

/** @brief 查询页持有 items 副本；PageFree 释放后才可重用同一个 page。 */
typedef struct SZrOptimizationRemarkPage {
    SZrOptimizationRemark *items;
    TZrUInt32 count;
    TZrUInt32 totalMatches;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrUInt64 droppedCount;
    TZrBool truncated;
} SZrOptimizationRemarkPage;

/** @brief 初始化待填充记录的 schema 与默认事实状态。
 * @pre remark 指向可写记录，现有内容无需释放。 */
ZR_CORE_API void ZrCore_OptimizationRemark_Init(SZrOptimizationRemark *remark);
/** @brief 清空可选诊断，使后续失败可覆盖同一对象。 */
ZR_CORE_API void ZrCore_OptimizationRemarkDiagnostic_Clear(
        SZrOptimizationRemarkDiagnostic *diagnostic);
/** @brief 返回稳定的诊断码名称，未知值返回 unknown。 */
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_DiagnosticName(
        EZrOptimizationRemarkDiagnosticCode code);
/** @brief 返回状态的显示名称，未知值返回 unknown。 */
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_StatusName(
        EZrOptimizationRemarkStatus status);
/** @brief 返回原因码的显示名称，供 CLI 参数和 JSON 复用。 */
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_ReasonName(
        EZrOptimizationRemarkReason reason);
/** @brief 返回证据类别的显示名称，未知值返回 unknown。 */
ZR_CORE_API const TZrChar *ZrCore_OptimizationRemark_EvidenceName(
        EZrOptimizationRemarkEvidence evidence);
/** @brief 拒绝损坏的范围、枚举、掩码及证据与计数器组合。
 * @return 合法时为真；失败时可选 diagnostic 描述首个错误。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemark_Validate(
        const SZrOptimizationRemark *remark,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/** @brief 初始化首次使用或已释放的 store，并设置默认记录上限。
 * @pre 不能在仍持有 items 的 store 上重复初始化。 */
ZR_CORE_API void ZrCore_OptimizationRemarks_StoreInit(
        SZrOptimizationRemarkStore *store);
/** @brief 释放 store 的记录数组并清零状态；记录内没有独立堆资源。 */
ZR_CORE_API void ZrCore_OptimizationRemarks_StoreFree(
        SZrOptimizationRemarkStore *store);
/** @brief 校验后按值追加；达到上限时成功返回并累计丢弃数。
 * @return 分配或校验失败时为假，store 中既有记录仍由调用方持有。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_Append(
        SZrOptimizationRemarkStore *store,
        const SZrOptimizationRemark *remark,
        SZrOptimizationRemarkDiagnostic *diagnostic);
/** @brief 初始化全通配查询，调用方再按字段设置筛选条件。 */
ZR_CORE_API void ZrCore_OptimizationRemarkQuery_Init(
        SZrOptimizationRemarkQuery *query);
/** @brief 释放 Query 生成的页缓冲并清零分页元数据。 */
ZR_CORE_API void ZrCore_OptimizationRemarks_PageFree(
        SZrOptimizationRemarkPage *page);
/** @brief 校验公开 store，排序匹配记录后按偏移返回独立副本。
 * @pre page 为首次使用的对象，或已先调用 PageFree；同一 store 查询期间不被修改。
 * @return 失败时诊断可选；成功页须由 PageFree 释放。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_Query(
        const SZrOptimizationRemarkStore *store,
        const SZrOptimizationRemarkQuery *query,
        SZrOptimizationRemarkPage *page,
        SZrOptimizationRemarkDiagnostic *diagnostic);
/** @brief 移除同一模块及源版本的记录，保留其他记录相对顺序。
 * @return 没有匹配行时为假并报告 NOT_FOUND。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_InvalidateSourceVersion(
        SZrOptimizationRemarkStore *store,
        TZrUInt64 moduleHash,
        TZrUInt64 sourceVersion,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/* 两次投影共用写入契约：空 buffer/零容量可先测长度；outWrittenSize 不含 NUL。 */
/** @brief 将单条已校验备注按字段写成确定性 JSON。
 * @return 缓冲不足时为假，并通过 outWrittenSize 返回所需正文长度。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemark_WriteJson(
        const SZrOptimizationRemark *remark,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic);
/** @brief 将已校验查询页写为 JSON，包含分页与丢弃元数据。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemarks_PageWriteJson(
        const SZrOptimizationRemarkPage *page,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic);
/** @brief 将单条备注写成 CLI 可读文本，同样支持先测长度。 */
ZR_CORE_API TZrBool ZrCore_OptimizationRemark_WriteText(
        const SZrOptimizationRemark *remark,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_OPTIMIZATION_REMARK_H */
