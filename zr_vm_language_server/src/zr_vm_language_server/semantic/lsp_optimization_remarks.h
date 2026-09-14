#ifndef ZR_VM_LANGUAGE_SERVER_LSP_OPTIMIZATION_REMARKS_H
#define ZR_VM_LANGUAGE_SERVER_LSP_OPTIMIZATION_REMARKS_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/optimization_remark.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef TZrBool (*FZrLspOptimizationRemarkCancellationCheck)(void *userData);

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

typedef struct SZrLspOptimizationRemarkCache {
    TZrUInt64 documentVersion;
    TZrUInt64 generation;
    TZrBool valid;
} SZrLspOptimizationRemarkCache;

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkRequest_Init(
        SZrLspOptimizationRemarkRequest *request);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkPage_Free(
        SZrLspOptimizationRemarkPage *page);
ZR_LANGUAGE_SERVER_API EZrLspOptimizationRemarkResult
ZrLanguageServer_LspOptimizationRemark_Project(
        const SZrOptimizationRemark *remark,
        TZrUInt64 requestedDocumentVersion,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspOptimizationRemark *outRemark,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/* Version-explicit variant.  Unlike the legacy zero-means-wildcard helper,
 * this preserves a legitimate document version of zero. */
ZR_LANGUAGE_SERVER_API EZrLspOptimizationRemarkResult
ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
        const SZrOptimizationRemark *remark,
        TZrBool hasDocumentVersion,
        TZrUInt64 requestedDocumentVersion,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspOptimizationRemark *outRemark,
        SZrOptimizationRemarkDiagnostic *diagnostic);
ZR_LANGUAGE_SERVER_API EZrLspOptimizationRemarkResult
ZrLanguageServer_LspOptimizationRemarks_Query(
        const SZrOptimizationRemarkStore *store,
        const SZrLspOptimizationRemarkRequest *request,
        SZrLspOptimizationRemarkPage *page,
        SZrOptimizationRemarkDiagnostic *diagnostic);

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkCache_Init(
        SZrLspOptimizationRemarkCache *cache);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkCache_Begin(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_Accept(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspOptimizationRemarkCache_Invalidate(
        SZrLspOptimizationRemarkCache *cache);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_Publish(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_PublishGeneration(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion,
        TZrUInt64 generation);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspOptimizationRemarkCache_AcceptGeneration(
        const SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion,
        TZrUInt64 generation);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_OPTIMIZATION_REMARKS_H */
