#ifndef ZR_VM_LANGUAGE_SERVER_LSP_OPTIMIZATION_REMARKS_H
#define ZR_VM_LANGUAGE_SERVER_LSP_OPTIMIZATION_REMARKS_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/optimization_remark.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 投影结果把数据错误、请求取消与过期版本分开交给 LSP 层处理。 */
typedef enum EZrLspOptimizationRemarkResult {
    ZR_LSP_OPTIMIZATION_REMARK_OK = 0,
    ZR_LSP_OPTIMIZATION_REMARK_STALE,
    ZR_LSP_OPTIMIZATION_REMARK_CANCELLED,
    ZR_LSP_OPTIMIZATION_REMARK_INVALID
} EZrLspOptimizationRemarkResult;

/* LSP-facing output owns no core pointers.  Fixed strings make a result safe
 * to retain after the source store or parser snapshot is released. */
typedef struct SZrLspOptimizationRemark {
    SZrLspRange range;
    TZrUInt64 siteKey;
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 moduleVersion;
    TZrUInt64 irVersion;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 backendMask;
    TZrUInt32 measuredCounterMask;
    TZrUInt32 beforeRepresentation;
    TZrUInt32 afterRepresentation;
    TZrUInt32 boxingFlags;
    TZrUInt32 allocationFlags;
    TZrUInt32 cacheFlags;
    TZrUInt32 deoptFlags;
    TZrUInt64 proofId;
    TZrUInt64 profileCount;
    TZrUInt64 estimatedCost;
    TZrUInt64 softwareIcMisses;
    TZrUInt64 hardwareCacheMisses;
    TZrUInt64 branchMisses;
    TZrUInt64 allocations;
    TZrUInt64 deopts;
    TZrBool stale;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
    char status[16];
    char reason[32];
    char evidenceKind[16];
} SZrLspOptimizationRemark;

/**
 * 一次筛选页的独立副本；items 由查询方通过 Page_Free 释放。
 * count 为本页数量，totalMatches 为分页前命中数，两者不能互换。
 */
typedef struct SZrLspOptimizationRemarkPage {
    SZrLspOptimizationRemark *items;
    TZrUInt32 count;
    TZrUInt32 totalMatches;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrUInt64 droppedCount;
    TZrBool truncated;
    TZrBool stale;
    TZrBool cancelled;
} SZrLspOptimizationRemarkPage;

/** 长查询在记录投影期间轮询此回调；回调不得释放请求或 store。 */
typedef TZrBool (*FZrLspOptimizationRemarkCancellationCheck)(void *userData);

/**
 * 从编辑器过滤条件到 core store 的借用请求视图。
 * content 与取消回调的 userData 在 Query 返回前必须有效；hasDocumentVersion 允许明确查询版本 0。
 */
typedef struct SZrLspOptimizationRemarkRequest {
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 documentVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reasonMask;
    TZrUInt32 statusMask;
    TZrUInt32 backendMask;
    TZrUInt32 evidenceMask;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrBool hasModuleHash;
    TZrBool hasIrHash;
    TZrBool hasDocumentVersion;
    TZrBool hasSourceId;
    TZrBool hasSourceRange;
    TZrBool hasPass;
    TZrBool hasModule;
    TZrBool reserved;
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
    const TZrChar *content;
    TZrSize contentLength;
    FZrLspOptimizationRemarkCancellationCheck isCancelled;
    void *cancellationUserData;
} SZrLspOptimizationRemarkRequest;

/** 异步发布门闩；文档版本和 generation 必须同时匹配才能接纳旧请求结果。 */
typedef struct SZrLspOptimizationRemarkCache {
    TZrUInt64 documentVersion;
    TZrUInt64 generation;
    TZrBool valid;
} SZrLspOptimizationRemarkCache;

/** @brief 为直接查询 core store 的调用方建立空筛选和默认分页上限；当前仓内消费点是投影测试。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkRequest_Init(
        SZrLspOptimizationRemarkRequest *request);
/** @brief 释放 Query 分配的页副本；重用 page 前须先调用此函数，允许清理部分失败结果。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkPage_Free(
        SZrLspOptimizationRemarkPage *page);
/**
 * @brief 无显式版本零筛选的投影入口。
 * TODO: 仓内未发现调用点；接入外部消费者前确认“版本 0 代表不过滤”的约定是否仍必要。
 */
ZR_LANGUAGE_SERVER_API EZrLspOptimizationRemarkResult
ZrLanguageServer_LspOptimizationRemark_Project(
        const SZrOptimizationRemark *remark,
        TZrUInt64 requestedDocumentVersion,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspOptimizationRemark *outRemark,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/**
 * @brief 将 core remark 复制为不借用 store 指针的 LSP 结果。
 * @pre content 若非空，必须对应 remark 的 sourceVersion；hasDocumentVersion 区分版本 0 与无筛选。
 */
ZR_LANGUAGE_SERVER_API EZrLspOptimizationRemarkResult
ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
        const SZrOptimizationRemark *remark,
        TZrBool hasDocumentVersion,
        TZrUInt64 requestedDocumentVersion,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspOptimizationRemark *outRemark,
        SZrOptimizationRemarkDiagnostic *diagnostic);
/**
 * @brief 在 core store 上筛选、分页并投影 remark，供编辑器请求使用。
 * @pre page 首次使用或已由 Page_Free 释放；content 若非空须匹配请求的源版本。
 * @note 成功后的 page.items 由调用方释放；过期或取消由返回码和 page 标志共同表达。取消只在查询前和逐项投影时轮询。
 * TODO: 当前仓内仅测试直接调用，接入实际 LSP 请求时需核对 store 的快照锁和取消时序。
 */
ZR_LANGUAGE_SERVER_API EZrLspOptimizationRemarkResult
ZrLanguageServer_LspOptimizationRemarks_Query(
        const SZrOptimizationRemarkStore *store,
        const SZrLspOptimizationRemarkRequest *request,
        SZrLspOptimizationRemarkPage *page,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/** @brief 初始化增量请求的发布门闩；首次 Begin 前不可接纳任何结果，当前仓内仅测试使用。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkCache_Init(
        SZrLspOptimizationRemarkCache *cache);
/** @brief 新请求开始时更换代数并撤销旧版本的已发布状态。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkCache_Begin(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion);
/** @brief 仅按文档版本读取同步缓存；异步结果应使用 AcceptGeneration。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_Accept(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion);
/** @brief 文档或 store 改变时撤销结果并推进代数。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkCache_Invalidate(
        SZrLspOptimizationRemarkCache *cache);
/** @brief 发布同步结果；异步请求必须同时核对 generation。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_Publish(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion);
/** @brief 仅当请求捕获的版本及代数仍是当前值时发布结果。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_PublishGeneration(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion,
        TZrUInt64 generation);
/** @brief 防止旧异步投影覆盖后来文档版本的已发布结果。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_AcceptGeneration(
        const SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion,
        TZrUInt64 generation);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_OPTIMIZATION_REMARKS_H */
