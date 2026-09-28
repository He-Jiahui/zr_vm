#include "interface/lsp_semantic_cache_lru.h"

#include "interface/lsp_interface_internal.h"
#include "interface/lsp_semantic_snapshot_cache.h"

/* 只统计可丢弃的分析缓存；AST 与语义上下文由分析器另行持有，不属于此预算。 */
struct SZrLspSemanticCacheLru {
    TZrSize limitBytes;
    TZrSize peakStorageBytes;
    TZrSize evictionCount;
    TZrSize releasedBytes;
    TZrSize nextAccessOrder;
};

/* 一次扫描同时给预算检查和状态查询提供占用量与最久未使用候选。 */
typedef struct SZrLspSemanticCacheLruScan {
    TZrSize storageBytes;
    SZrSemanticAnalyzer *oldestAnalyzer;
    TZrSize oldestAccessOrder;
} SZrLspSemanticCacheLruScan;

/* 总量与累计释放量采用饱和记账，防止规模溢出变成较小的预算值。 */
static TZrSize lsp_semantic_cache_lru_add_clamped(
        TZrSize total,
        TZrSize value) {
    if (value > ZR_MAX_SIZE - total) {
        return ZR_MAX_SIZE;
    }
    return total + value;
}

/* 当前及历史分析器由不同容器持有，但都参与同一逐出顺序。 */
static void lsp_semantic_cache_lru_consider_analyzer(
        SZrSemanticAnalyzer *analyzer,
        void *userData) {
    SZrLspSemanticCacheLruScan *scan =
            (SZrLspSemanticCacheLruScan *)userData;
    TZrSize storageBytes;

    if (scan == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }
    storageBytes = ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(
            analyzer);
    scan->storageBytes = lsp_semantic_cache_lru_add_clamped(
            scan->storageBytes,
            storageBytes);
    if (storageBytes == 0U) {
        return;
    }
    if (scan->oldestAnalyzer == ZR_NULL ||
        analyzer->cacheStorageAccessOrder < scan->oldestAccessOrder) {
        scan->oldestAnalyzer = analyzer;
        scan->oldestAccessOrder = analyzer->cacheStorageAccessOrder;
    }
}

/* 主分析器按 URI 保存在上下文哈希表，遍历时只借用其中的原生指针。 */
static void lsp_semantic_cache_lru_visit_primary_analyzers(
        const SZrLspContext *context,
        TZrLspSemanticSnapshotAnalyzerVisitor visitor,
        void *userData) {
    if (context == ZR_NULL || visitor == ZR_NULL ||
        !context->uriToAnalyzerMap.isValid ||
        context->uriToAnalyzerMap.buckets == ZR_NULL) {
        return;
    }
    for (TZrSize bucketIndex = 0U;
         bucketIndex < context->uriToAnalyzerMap.capacity;
         bucketIndex++) {
        SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                visitor(
                        (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer,
                        userData);
            }
            pair = pair->next;
        }
    }
}

/* 将当前分析器与历史快照合并扫描，避免历史版本绕过工作区统一预算。 */
static SZrLspSemanticCacheLruScan lsp_semantic_cache_lru_scan(
        const SZrLspContext *context) {
    SZrLspSemanticCacheLruScan scan;

    scan.storageBytes = 0U;
    scan.oldestAnalyzer = ZR_NULL;
    scan.oldestAccessOrder = ZR_MAX_SIZE;
    lsp_semantic_cache_lru_visit_primary_analyzers(
            context,
            lsp_semantic_cache_lru_consider_analyzer,
            &scan);
    ZrLanguageServer_LspSemanticSnapshotCache_VisitAnalyzers(
            context,
            lsp_semantic_cache_lru_consider_analyzer,
            &scan);
    return scan;
}

/* 访问序号耗尽时清除旧序号；随后第一次 Touch 重新建立顺序起点。 */
static void lsp_semantic_cache_lru_clear_access_order(
        SZrSemanticAnalyzer *analyzer,
        void *userData) {
    (void)userData;
    if (analyzer != ZR_NULL) {
        analyzer->cacheStorageAccessOrder = 0U;
    }
}

/* 重置必须同时覆盖主表和历史快照，避免不同容器的序号失去可比性。 */
static void lsp_semantic_cache_lru_reset_access_orders(
        SZrLspContext *context) {
    lsp_semantic_cache_lru_visit_primary_analyzers(
            context,
            lsp_semantic_cache_lru_clear_access_order,
            ZR_NULL);
    ZrLanguageServer_LspSemanticSnapshotCache_VisitAnalyzers(
            context,
            lsp_semantic_cache_lru_clear_access_order,
            ZR_NULL);
}

SZrLspSemanticCacheLru *ZrLanguageServer_LspSemanticCacheLru_New(
        SZrState *state) {
    SZrLspSemanticCacheLru *lru;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    lru = (SZrLspSemanticCacheLru *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrLspSemanticCacheLru));
    if (lru == ZR_NULL) {
        return ZR_NULL;
    }
    lru->limitBytes = ZR_LSP_SEMANTIC_CACHE_DEFAULT_LIMIT_BYTES;
    lru->peakStorageBytes = 0U;
    lru->evictionCount = 0U;
    lru->releasedBytes = 0U;
    lru->nextAccessOrder = 0U;
    return lru;
}

void ZrLanguageServer_LspSemanticCacheLru_Free(
        SZrState *state,
        SZrLspContext *context) {
    SZrLspSemanticCacheLru *lru;

    if (state == ZR_NULL || state->global == ZR_NULL || context == ZR_NULL ||
        context->semanticCacheLru == ZR_NULL) {
        return;
    }
    lru = context->semanticCacheLru;
    ZrCore_Memory_RawFree(
            state->global,
            lru,
            sizeof(SZrLspSemanticCacheLru));
    context->semanticCacheLru = ZR_NULL;
}

void ZrLanguageServer_LspSemanticCacheLru_Touch(
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer) {
    SZrLspSemanticCacheLru *lru;

    if (context == ZR_NULL || analyzer == ZR_NULL ||
        context->semanticCacheLru == ZR_NULL) {
        return;
    }
    lru = context->semanticCacheLru;
    if (lru->nextAccessOrder == ZR_MAX_SIZE) {
        lsp_semantic_cache_lru_reset_access_orders(context);
        lru->nextAccessOrder = 0U;
    }
    analyzer->cacheStorageAccessOrder = ++lru->nextAccessOrder;
}

void ZrLanguageServer_LspSemanticCacheLru_Enforce(
        SZrState *state,
        SZrLspContext *context) {
    SZrLspSemanticCacheLru *lru;
    SZrLspSemanticCacheLruScan scan;

    if (state == ZR_NULL || context == ZR_NULL ||
        context->semanticCacheLru == ZR_NULL) {
        return;
    }
    lru = context->semanticCacheLru;
    /* 每次释放后重算占用量；一个分析器可能同时持有普通与作用域查询缓存。 */
    for (;;) {
        TZrSize releasedBytes;

        scan = lsp_semantic_cache_lru_scan(context);
        if (scan.storageBytes > lru->peakStorageBytes) {
            lru->peakStorageBytes = scan.storageBytes;
        }
        if (scan.storageBytes <= lru->limitBytes ||
            scan.oldestAnalyzer == ZR_NULL) {
            return;
        }
        releasedBytes = ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(
                scan.oldestAnalyzer);
        ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage(
                state,
                scan.oldestAnalyzer);
        lru->releasedBytes = lsp_semantic_cache_lru_add_clamped(
                lru->releasedBytes,
                releasedBytes);
        if (lru->evictionCount < ZR_MAX_SIZE) {
            lru->evictionCount++;
        }
    }
}

TZrBool ZrLanguageServer_LspSemanticCacheLru_SetLimit(
        SZrState *state,
        SZrLspContext *context,
        TZrSize limitBytes) {
    if (state == ZR_NULL || context == ZR_NULL ||
        context->semanticCacheLru == ZR_NULL) {
        return ZR_FALSE;
    }
    context->semanticCacheLru->limitBytes = limitBytes;
    ZrLanguageServer_LspSemanticCacheLru_Enforce(state, context);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticCacheLru_GetInfo(
        const SZrLspContext *context,
        SZrLspSemanticCacheStorageInfo *outInfo) {
    SZrLspSemanticCacheLruScan scan;
    const SZrLspSemanticCacheLru *lru;

    if (outInfo != ZR_NULL) {
        outInfo->limitBytes = 0U;
        outInfo->storageBytes = 0U;
        outInfo->peakStorageBytes = 0U;
        outInfo->evictionCount = 0U;
        outInfo->releasedBytes = 0U;
    }
    if (context == ZR_NULL || outInfo == ZR_NULL ||
        context->semanticCacheLru == ZR_NULL) {
        return ZR_FALSE;
    }
    lru = context->semanticCacheLru;
    scan = lsp_semantic_cache_lru_scan(context);
    outInfo->limitBytes = lru->limitBytes;
    outInfo->storageBytes = scan.storageBytes;
    outInfo->peakStorageBytes = lru->peakStorageBytes;
    outInfo->evictionCount = lru->evictionCount;
    outInfo->releasedBytes = lru->releasedBytes;
    return ZR_TRUE;
}
