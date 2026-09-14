#include "semantic/lsp_optimization_remarks.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void lsp_remark_diag_clear(SZrOptimizationRemarkDiagnostic *diagnostic) {
    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
}

static TZrSize lsp_remark_max_count_for_item_size(TZrSize itemSize) {
    if (itemSize == 0u) return (TZrSize)-1;
    return ((TZrSize)-1) / itemSize;
}

static void lsp_remark_cache_bump_generation(
        SZrLspOptimizationRemarkCache *cache) {
    if (cache->generation != UINT64_MAX) cache->generation++;
}

static void lsp_remark_position(const TZrChar *content,
                                TZrSize contentLength,
                                TZrUInt32 offset,
                                SZrLspPosition *position) {
    TZrSize index = 0u;
    TZrUInt32 utf16Column = 0u;
    TZrBool previousWasCr = ZR_FALSE;

    position->line = 0;
    position->character = 0;
    if (content == ZR_NULL) {
        position->character = offset > (TZrUInt32)INT32_MAX
                              ? INT32_MAX
                              : (TZrInt32)offset;
        return;
    }
    /* Compare in the widest contract type so 32-bit size_t toolchains do not
     * diagnose an always-false TZrSize-vs- UINT32_MAX comparison. */
    if ((TZrUInt64)contentLength > (TZrUInt64)UINT32_MAX) {
        contentLength = (TZrSize)UINT32_MAX;
    }
    if ((TZrSize)offset > contentLength) offset = (TZrUInt32)contentLength;
    while (index < (TZrSize)offset) {
        unsigned char byte = (unsigned char)content[index];
        TZrUInt32 codePoint = byte;
        TZrSize width = 1u;
        if (byte == '\n') {
            if (!previousWasCr && position->line < INT32_MAX) {
                position->line++;
            }
            utf16Column = 0u;
            previousWasCr = ZR_FALSE;
            index++;
            continue;
        }
        if (byte == '\r') {
            if (position->line < INT32_MAX) position->line++;
            utf16Column = 0u;
            previousWasCr = ZR_TRUE;
            index++;
            continue;
        }
        previousWasCr = ZR_FALSE;
        if (byte >= 0xc2u && byte <= 0xdfu && index + 1u < contentLength &&
            (((unsigned char)content[index + 1u] & 0xc0u) == 0x80u)) {
            codePoint = ((TZrUInt32)byte & 0x1fu) << 6u;
            codePoint |= (TZrUInt32)((unsigned char)content[index + 1u] & 0x3fu);
            width = 2u;
        } else if (byte >= 0xe0u && byte <= 0xefu && index + 2u < contentLength &&
                   (((unsigned char)content[index + 1u] & 0xc0u) == 0x80u) &&
                   (((unsigned char)content[index + 2u] & 0xc0u) == 0x80u)) {
            codePoint = ((TZrUInt32)byte & 0x0fu) << 12u;
            codePoint |= ((TZrUInt32)((unsigned char)content[index + 1u] & 0x3fu) << 6u);
            codePoint |= (TZrUInt32)((unsigned char)content[index + 2u] & 0x3fu);
            width = 3u;
        } else if (byte >= 0xf0u && byte <= 0xf4u && index + 3u < contentLength &&
                   (((unsigned char)content[index + 1u] & 0xc0u) == 0x80u) &&
                   (((unsigned char)content[index + 2u] & 0xc0u) == 0x80u) &&
                   (((unsigned char)content[index + 3u] & 0xc0u) == 0x80u)) {
            codePoint = ((TZrUInt32)byte & 0x07u) << 18u;
            codePoint |= ((TZrUInt32)((unsigned char)content[index + 1u] & 0x3fu) << 12u);
            codePoint |= ((TZrUInt32)((unsigned char)content[index + 2u] & 0x3fu) << 6u);
            codePoint |= (TZrUInt32)((unsigned char)content[index + 3u] & 0x3fu);
            width = 4u;
        }
        utf16Column += codePoint > 0xffffu ? 2u : 1u;
        index += width;
    }
    position->character = utf16Column > (TZrUInt32)INT32_MAX
                          ? INT32_MAX
                          : (TZrInt32)utf16Column;
}

static TZrBool lsp_remark_fixed_string_is_terminated(
        const TZrChar *text, TZrSize capacity) {
    if (text == ZR_NULL) return ZR_FALSE;
    return memchr(text, '\0', capacity) != ZR_NULL;
}

void ZrLanguageServer_LspOptimizationRemarkRequest_Init(
        SZrLspOptimizationRemarkRequest *request) {
    if (request == ZR_NULL) return;
    memset(request, 0, sizeof(*request));
    request->pageLimit = 64u;
}

void ZrLanguageServer_LspOptimizationRemarkPage_Free(
        SZrLspOptimizationRemarkPage *page) {
    if (page == ZR_NULL) return;
    free(page->items);
    memset(page, 0, sizeof(*page));
}

EZrLspOptimizationRemarkResult ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
        const SZrOptimizationRemark *remark,
        TZrBool hasDocumentVersion,
        TZrUInt64 requestedDocumentVersion,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspOptimizationRemark *outRemark,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    lsp_remark_diag_clear(diagnostic);
    if (outRemark != ZR_NULL) memset(outRemark, 0, sizeof(*outRemark));
    if (remark == ZR_NULL || outRemark == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
    }
    if (!ZrCore_OptimizationRemark_Validate(remark, diagnostic)) {
        return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
    }
    if (hasDocumentVersion &&
        requestedDocumentVersion != remark->sourceVersion) {
        outRemark->stale = ZR_TRUE;
        return ZR_LSP_OPTIMIZATION_REMARK_STALE;
    }
    outRemark->moduleHash = remark->moduleHash;
    outRemark->siteKey = remark->siteKey;
    outRemark->irHash = remark->irHash;
    outRemark->moduleVersion = remark->moduleVersion;
    outRemark->irVersion = remark->irVersion;
    outRemark->sourceVersion = remark->sourceVersion;
    outRemark->sourceId = remark->sourceId;
    outRemark->backendMask = remark->backendMask;
    outRemark->measuredCounterMask = remark->measuredCounterMask;
    outRemark->beforeRepresentation = remark->before;
    outRemark->afterRepresentation = remark->after;
    outRemark->boxingFlags = remark->boxingFlags;
    outRemark->allocationFlags = remark->allocationFlags;
    outRemark->cacheFlags = remark->cacheFlags;
    outRemark->deoptFlags = remark->deoptFlags;
    outRemark->proofId = remark->proofId;
    outRemark->profileCount = remark->profileCount;
    outRemark->estimatedCost = remark->estimatedCost;
    outRemark->softwareIcMisses = remark->softwareIcMisses;
    outRemark->hardwareCacheMisses = remark->hardwareCacheMisses;
    outRemark->branchMisses = remark->branchMisses;
    outRemark->allocations = remark->allocations;
    outRemark->deopts = remark->deopts;
    outRemark->range.start.line = 0;
    outRemark->range.start.character = 0;
    outRemark->range.end.line = 0;
    outRemark->range.end.character = 0;
    lsp_remark_position(content, contentLength, remark->sourceRange.startOffset,
                        &outRemark->range.start);
    lsp_remark_position(content, contentLength, remark->sourceRange.endOffset,
                        &outRemark->range.end);
    memcpy(outRemark->pass, remark->pass, sizeof(outRemark->pass));
    memcpy(outRemark->module, remark->module, sizeof(outRemark->module));
    (void)snprintf(outRemark->status, sizeof(outRemark->status), "%s",
                   ZrCore_OptimizationRemark_StatusName(remark->status));
    (void)snprintf(outRemark->reason, sizeof(outRemark->reason), "%s",
                   ZrCore_OptimizationRemark_ReasonName(remark->reason));
    (void)snprintf(outRemark->evidenceKind, sizeof(outRemark->evidenceKind), "%s",
                   ZrCore_OptimizationRemark_EvidenceName(remark->evidence));
    return ZR_LSP_OPTIMIZATION_REMARK_OK;
}

EZrLspOptimizationRemarkResult ZrLanguageServer_LspOptimizationRemark_Project(
        const SZrOptimizationRemark *remark,
        TZrUInt64 requestedDocumentVersion,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspOptimizationRemark *outRemark,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    return ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
            remark, requestedDocumentVersion != 0u, requestedDocumentVersion,
            content, contentLength, outRemark, diagnostic);
}

static void lsp_remark_fill_query(
        const SZrLspOptimizationRemarkRequest *request,
        TZrBool includeDocumentVersion,
        SZrOptimizationRemarkQuery *query) {
    ZrCore_OptimizationRemarkQuery_Init(query);
    query->moduleHash = request->moduleHash;
    query->irHash = request->irHash;
    query->sourceVersion = includeDocumentVersion ? request->documentVersion : 0u;
    query->sourceId = request->sourceId;
    query->reasonMask = request->reasonMask;
    query->statusMask = request->statusMask;
    query->backendMask = request->backendMask;
    query->evidenceMask = request->evidenceMask;
    query->pageOffset = request->pageOffset;
    query->pageLimit = request->pageLimit;
    query->hasModuleHash = request->hasModuleHash || request->moduleHash != 0u;
    query->hasIrHash = request->hasIrHash || request->irHash != 0u;
    query->hasSourceVersion = includeDocumentVersion;
    query->hasSourceId = request->hasSourceId || request->sourceId != 0u;
    query->hasSourceRange = request->hasSourceRange;
    query->hasPass = request->hasPass;
    query->hasModule = request->hasModule;
    query->sourceRange = request->sourceRange;
    if (query->hasPass) {
        memcpy(query->pass, request->pass, sizeof(query->pass));
        query->pass[sizeof(query->pass) - 1u] = '\0';
    }
    if (query->hasModule) {
        memcpy(query->module, request->module, sizeof(query->module));
        query->module[sizeof(query->module) - 1u] = '\0';
    }
}

EZrLspOptimizationRemarkResult ZrLanguageServer_LspOptimizationRemarks_Query(
        const SZrOptimizationRemarkStore *store,
        const SZrLspOptimizationRemarkRequest *request,
        SZrLspOptimizationRemarkPage *page,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    SZrOptimizationRemarkQuery query;
    SZrOptimizationRemarkPage corePage;
    SZrOptimizationRemarkPage stalePage;
    TZrUInt32 index;
    TZrBool hasDocumentVersion;
    EZrLspOptimizationRemarkResult result = ZR_LSP_OPTIMIZATION_REMARK_OK;

    lsp_remark_diag_clear(diagnostic);
    if (page != ZR_NULL) memset(page, 0, sizeof(*page));
    if (store == ZR_NULL || request == ZR_NULL || page == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
    }
    if (request->isCancelled != ZR_NULL &&
        request->isCancelled(request->cancellationUserData)) {
        page->cancelled = ZR_TRUE;
        return ZR_LSP_OPTIMIZATION_REMARK_CANCELLED;
    }
    /* Requests arrive over JSON-RPC but are represented by fixed arrays at
     * this boundary.  Reject unterminated selectors instead of silently
     * truncating them while copying into the core query. */
    if ((request->hasPass &&
         !lsp_remark_fixed_string_is_terminated(
                 request->pass, sizeof(request->pass))) ||
        (request->hasModule &&
         !lsp_remark_fixed_string_is_terminated(
                 request->module, sizeof(request->module)))) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY;
            diagnostic->field = 6u;
        }
        return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
    }
    hasDocumentVersion = request->hasDocumentVersion || request->documentVersion != 0u;
    lsp_remark_fill_query(request, hasDocumentVersion, &query);
    memset(&corePage, 0, sizeof(corePage));
    if (!ZrCore_OptimizationRemarks_Query(store, &query, &corePage, diagnostic)) {
        return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
    }

    /* A versioned request is stale only when the same query would produce
     * records for another version.  This avoids reporting an unrelated stale
     * module/pass as stale when the requested filter has no candidates. */
    if (hasDocumentVersion && corePage.totalMatches == 0u) {
        memset(&stalePage, 0, sizeof(stalePage));
        lsp_remark_fill_query(request, ZR_FALSE, &query);
        if (!ZrCore_OptimizationRemarks_Query(store, &query, &stalePage, diagnostic)) {
            ZrCore_OptimizationRemarks_PageFree(&corePage);
            return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
        }
        if (stalePage.totalMatches != 0u) {
            page->stale = ZR_TRUE;
            ZrCore_OptimizationRemarks_PageFree(&stalePage);
            ZrCore_OptimizationRemarks_PageFree(&corePage);
            return ZR_LSP_OPTIMIZATION_REMARK_STALE;
        }
        ZrCore_OptimizationRemarks_PageFree(&stalePage);
        /* Keep the empty current-version page metadata. */
        page->pageOffset = corePage.pageOffset;
        page->pageLimit = corePage.pageLimit;
        page->totalMatches = corePage.totalMatches;
        page->droppedCount = corePage.droppedCount;
        page->truncated = corePage.truncated;
        ZrCore_OptimizationRemarks_PageFree(&corePage);
        return ZR_LSP_OPTIMIZATION_REMARK_OK;
    }

    page->totalMatches = corePage.totalMatches;
    page->pageOffset = corePage.pageOffset;
    page->pageLimit = corePage.pageLimit;
    page->droppedCount = corePage.droppedCount;
    page->truncated = corePage.truncated;
    if (corePage.count != 0u) {
        if ((TZrSize)corePage.count >
            lsp_remark_max_count_for_item_size((TZrSize)sizeof(*page->items))) {
            ZrCore_OptimizationRemarks_PageFree(&corePage);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY;
            }
            return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
        }
        page->items = (SZrLspOptimizationRemark *)calloc(
                corePage.count, sizeof(*page->items));
        if (page->items == ZR_NULL) {
            ZrCore_OptimizationRemarks_PageFree(&corePage);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY;
            }
            return ZR_LSP_OPTIMIZATION_REMARK_INVALID;
        }
    }
    for (index = 0u; index < corePage.count; index++) {
        if (request->isCancelled != ZR_NULL &&
            request->isCancelled(request->cancellationUserData)) {
            page->cancelled = ZR_TRUE;
            free(page->items);
            page->items = ZR_NULL;
            ZrCore_OptimizationRemarks_PageFree(&corePage);
            return ZR_LSP_OPTIMIZATION_REMARK_CANCELLED;
        }
        result = ZrLanguageServer_LspOptimizationRemark_ProjectVersioned(
                &corePage.items[index], hasDocumentVersion,
                request->documentVersion, request->content, request->contentLength,
                &page->items[index], diagnostic);
        if (result != ZR_LSP_OPTIMIZATION_REMARK_OK) {
            if (result == ZR_LSP_OPTIMIZATION_REMARK_STALE) page->stale = ZR_TRUE;
            page->count = 0u;
            free(page->items);
            page->items = ZR_NULL;
            ZrCore_OptimizationRemarks_PageFree(&corePage);
            return result;
        }
    }
    page->count = corePage.count;
    ZrCore_OptimizationRemarks_PageFree(&corePage);
    return result;
}

void ZrLanguageServer_LspOptimizationRemarkCache_Init(
        SZrLspOptimizationRemarkCache *cache) {
    if (cache == ZR_NULL) return;
    memset(cache, 0, sizeof(*cache));
}

void ZrLanguageServer_LspOptimizationRemarkCache_Begin(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion) {
    if (cache == ZR_NULL) return;
    cache->documentVersion = documentVersion;
    lsp_remark_cache_bump_generation(cache);
    cache->valid = ZR_FALSE;
}

TZrBool ZrLanguageServer_LspOptimizationRemarkCache_Accept(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion) {
    if (cache == ZR_NULL || !cache->valid ||
        cache->documentVersion != documentVersion) return ZR_FALSE;
    return ZR_TRUE;
}

void ZrLanguageServer_LspOptimizationRemarkCache_Invalidate(
        SZrLspOptimizationRemarkCache *cache) {
    if (cache == ZR_NULL) return;
    lsp_remark_cache_bump_generation(cache);
    cache->valid = ZR_FALSE;
}

TZrBool ZrLanguageServer_LspOptimizationRemarkCache_Publish(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion) {
    if (cache == ZR_NULL || cache->documentVersion != documentVersion) {
        return ZR_FALSE;
    }
    cache->valid = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspOptimizationRemarkCache_PublishGeneration(
        SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion,
        TZrUInt64 generation) {
    if (cache == ZR_NULL || cache->documentVersion != documentVersion ||
        cache->generation != generation) return ZR_FALSE;
    cache->valid = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspOptimizationRemarkCache_AcceptGeneration(
        const SZrLspOptimizationRemarkCache *cache,
        TZrUInt64 documentVersion,
        TZrUInt64 generation) {
    if (cache == ZR_NULL || !cache->valid ||
        cache->documentVersion != documentVersion ||
        cache->generation != generation) return ZR_FALSE;
    return ZR_TRUE;
}
