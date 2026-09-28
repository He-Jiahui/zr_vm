#include "interface/lsp_interface_internal.h"
#include "interface/lsp_semantic_cache_lru.h"
#include "semantic/semantic_analyzer_internal.h"

#include <string.h>

/* 历史语义版本与文件内容历史并行；analyzer 独占旧 AST，uri 指针由调用方保证存活。 */
typedef struct SZrLspSemanticSnapshotEntry {
    SZrString *uri;
    TZrSize version;
    TZrSize contentGeneration;
    TZrSize insertionOrder;
    SZrSemanticAnalyzer *analyzer;
} SZrLspSemanticSnapshotEntry;

/* entries 为跨 URI 共用数组，每个 URI 通过 insertionOrder 独立选择最旧版本。 */
struct SZrLspSemanticSnapshotCache {
    SZrArray entries;
    TZrSize nextInsertionOrder;
};

/* 等价 URI 可能由不同字符串对象进入上下文，因此用文本身份匹配历史项。 */
static TZrBool snapshot_entry_matches_uri(
        const SZrLspSemanticSnapshotEntry *entry,
        const SZrString *uri) {
    return entry != ZR_NULL && entry->uri != ZR_NULL && uri != ZR_NULL &&
           ZrLanguageServer_Lsp_StringsEqual(entry->uri, (SZrString *)uri);
}

/* 回收旧 AST 前先使仍借用它的当前作用域查询分析器失效。 */
static void snapshot_cache_release_entry(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticSnapshotEntry *entry) {
    SZrSemanticAnalyzer *currentAnalyzer;

    if (state == ZR_NULL || context == ZR_NULL || entry == ZR_NULL ||
        entry->analyzer == ZR_NULL) {
        return;
    }

    currentAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(
            state,
            context,
            entry->uri);
    if (currentAnalyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzerBorrowingAst(
                state,
                currentAnalyzer,
                entry->analyzer->ownedAst);
    }
    ZrLanguageServer_SemanticAnalyzer_Free(state, entry->analyzer);
    memset(entry, 0, sizeof(*entry));
}

/* 删除采用尾元素补位；版本顺序另由 insertionOrder 维护。 */
static void snapshot_cache_remove_entry_at(
        SZrState *state,
        SZrLspContext *context,
        TZrSize index) {
    SZrLspSemanticSnapshotCache *cache;
    SZrLspSemanticSnapshotEntry *entry;
    SZrLspSemanticSnapshotEntry *last;

    if (context == ZR_NULL || context->semanticSnapshotCache == ZR_NULL) {
        return;
    }
    cache = context->semanticSnapshotCache;
    if (!cache->entries.isValid || index >= cache->entries.length) {
        return;
    }
    entry = (SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
            &cache->entries,
            index);
    snapshot_cache_release_entry(state, context, entry);
    if (index + 1U < cache->entries.length) {
        last = (SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
                &cache->entries,
                cache->entries.length - 1U);
        if (entry != ZR_NULL && last != ZR_NULL) {
            *entry = *last;
        }
    }
    (void)ZrCore_Array_Pop(&cache->entries);
}

/* 容量按 URI 单独限制，而非跨整个工作区共用两个历史槽。 */
static TZrSize snapshot_cache_count_uri(
        const SZrLspSemanticSnapshotCache *cache,
        const SZrString *uri) {
    TZrSize count = 0U;

    if (cache == ZR_NULL || !cache->entries.isValid) {
        return 0U;
    }
    for (TZrSize index = 0U; index < cache->entries.length; index++) {
        const SZrLspSemanticSnapshotEntry *entry =
                (const SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
                        (SZrArray *)&cache->entries,
                        index);
        if (snapshot_entry_matches_uri(entry, uri)) {
            count++;
        }
    }
    return count;
}

/* 为版本滚动寻找此 URI 最早捕获的语义状态。 */
static TZrSize snapshot_cache_oldest_uri_index(
        const SZrLspSemanticSnapshotCache *cache,
        const SZrString *uri) {
    TZrSize oldestIndex = ZR_MAX_SIZE;
    TZrSize oldestOrder = ZR_MAX_SIZE;

    if (cache == ZR_NULL || !cache->entries.isValid) {
        return ZR_MAX_SIZE;
    }
    for (TZrSize index = 0U; index < cache->entries.length; index++) {
        const SZrLspSemanticSnapshotEntry *entry =
                (const SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
                        (SZrArray *)&cache->entries,
                        index);
        if (snapshot_entry_matches_uri(entry, uri) &&
            entry->insertionOrder < oldestOrder) {
            oldestIndex = index;
            oldestOrder = entry->insertionOrder;
        }
    }
    return oldestIndex;
}

SZrLspSemanticSnapshotCache *ZrLanguageServer_LspSemanticSnapshotCache_New(
        SZrState *state) {
    SZrLspSemanticSnapshotCache *cache;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    cache = (SZrLspSemanticSnapshotCache *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrLspSemanticSnapshotCache));
    if (cache == ZR_NULL) {
        return ZR_NULL;
    }
    /* BUG: Array_Init 分配失败时仍把 entries 标为有效；此构造函数会返回
     * 非空缓存，首次 Capture 的 Array_Push 随即向空缓冲区复制。 */
    ZrCore_Array_Init(
            state,
            &cache->entries,
            sizeof(SZrLspSemanticSnapshotEntry),
            ZR_LSP_HISTORICAL_SEMANTIC_SNAPSHOT_CAPACITY);
    cache->nextInsertionOrder = 0U;
    return cache;
}

void ZrLanguageServer_LspSemanticSnapshotCache_Free(
        SZrState *state,
        SZrLspContext *context) {
    SZrLspSemanticSnapshotCache *cache;

    if (state == ZR_NULL || context == ZR_NULL ||
        context->semanticSnapshotCache == ZR_NULL) {
        return;
    }
    cache = context->semanticSnapshotCache;
    while (cache->entries.isValid && cache->entries.length > 0U) {
        snapshot_cache_remove_entry_at(state, context, cache->entries.length - 1U);
    }
    ZrCore_Array_Free(state, &cache->entries);
    ZrCore_Memory_RawFree(
            state->global,
            cache,
            sizeof(SZrLspSemanticSnapshotCache));
    context->semanticSnapshotCache = ZR_NULL;
}

void ZrLanguageServer_LspSemanticSnapshotCache_RemoveUri(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri) {
    SZrLspSemanticSnapshotCache *cache;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        context->semanticSnapshotCache == ZR_NULL) {
        return;
    }
    cache = context->semanticSnapshotCache;
    for (TZrSize index = cache->entries.length; index > 0U; index--) {
        SZrLspSemanticSnapshotEntry *entry =
                (SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
                        &cache->entries,
                        index - 1U);
        if (snapshot_entry_matches_uri(entry, uri)) {
            snapshot_cache_remove_entry_at(state, context, index - 1U);
        }
    }
}

TZrBool ZrLanguageServer_LspSemanticSnapshotCache_Capture(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrFileVersion *fileVersion,
        SZrSemanticAnalyzer *currentAnalyzer,
        SZrAstNode *retainedAst,
        TZrBool preserveScopedQueryAnalyzer) {
    SZrLspSemanticSnapshotCache *cache;
    SZrLspSemanticSnapshotEntry entry;
    const SZrFileVersionHistoricalContent *previousContent;
    SZrSemanticAnalyzer *snapshotAnalyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        fileVersion == ZR_NULL || currentAnalyzer == ZR_NULL ||
        retainedAst == ZR_NULL || context->semanticSnapshotCache == ZR_NULL ||
        fileVersion->historicalContentCount == 0U) {
        return ZR_FALSE;
    }
    previousContent = &fileVersion->historicalContent[0];
    if (previousContent->contentBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    snapshotAnalyzer =
            ZrLanguageServer_SemanticAnalyzer_DetachCurrentStateForSnapshot(
                    state,
                    currentAnalyzer,
                    retainedAst,
                    preserveScopedQueryAnalyzer);
    if (snapshotAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    cache = context->semanticSnapshotCache;
    while (snapshot_cache_count_uri(cache, uri) >=
           ZR_LSP_HISTORICAL_SEMANTIC_SNAPSHOT_CAPACITY) {
        TZrSize oldestIndex = snapshot_cache_oldest_uri_index(cache, uri);
        if (oldestIndex == ZR_MAX_SIZE) {
            break;
        }
        snapshot_cache_remove_entry_at(state, context, oldestIndex);
    }

    /* 记录的是解析器刚保留的前一版内容；调用方随后在原分析器上分析新 AST。 */
    memset(&entry, 0, sizeof(entry));
    entry.uri = uri;
    entry.version = previousContent->version;
    entry.contentGeneration = previousContent->contentBlock->contentGeneration;
    entry.insertionOrder = ++cache->nextInsertionOrder;
    entry.analyzer = snapshotAnalyzer;
    /* BUG: 第三个跨 URI 快照等触发 entries 扩容时，Array_Push 的分配失败路径
     * 会覆盖数组缓冲区指针并继续复制，导致崩溃；这里也无法接收失败状态。 */
    ZrCore_Array_Push(state, &cache->entries, &entry);
    return ZR_TRUE;
}

void ZrLanguageServer_LspSemanticSnapshotCache_VisitAnalyzers(
        const SZrLspContext *context,
        TZrLspSemanticSnapshotAnalyzerVisitor visitor,
        void *userData) {
    const SZrLspSemanticSnapshotCache *cache;

    if (context == ZR_NULL || visitor == ZR_NULL ||
        context->semanticSnapshotCache == ZR_NULL) {
        return;
    }
    cache = context->semanticSnapshotCache;
    /* LRU 访问者只在遍历期间借用指针，不转移快照所有权。 */
    for (TZrSize index = 0U; index < cache->entries.length; index++) {
        const SZrLspSemanticSnapshotEntry *entry =
                (const SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
                        (SZrArray *)&cache->entries,
                        index);
        if (entry != ZR_NULL && entry->analyzer != ZR_NULL) {
            visitor(entry->analyzer, userData);
        }
    }
}

/* 对外只借出分析器和版本身份；索引与文件内容历史一致，0 为最近一次更新前的版本。 */
TZrBool ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(
        const SZrLspContext *context,
        const SZrString *uri,
        TZrSize historyIndex,
        SZrLspHistoricalSemanticSnapshot *outSnapshot) {
    const SZrLspSemanticSnapshotCache *cache;
    const SZrLspSemanticSnapshotEntry *newest = ZR_NULL;
    const SZrLspSemanticSnapshotEntry *nextNewest = ZR_NULL;

    if (outSnapshot != ZR_NULL) {
        memset(outSnapshot, 0, sizeof(*outSnapshot));
    }
    if (context == ZR_NULL || uri == ZR_NULL || outSnapshot == ZR_NULL ||
        historyIndex >= ZR_LSP_HISTORICAL_SEMANTIC_SNAPSHOT_CAPACITY ||
        context->semanticSnapshotCache == ZR_NULL) {
        return ZR_FALSE;
    }
    cache = context->semanticSnapshotCache;
    for (TZrSize index = 0U; index < cache->entries.length; index++) {
        const SZrLspSemanticSnapshotEntry *entry =
                (const SZrLspSemanticSnapshotEntry *)ZrCore_Array_Get(
                        (SZrArray *)&cache->entries,
                        index);
        if (!snapshot_entry_matches_uri(entry, uri)) {
            continue;
        }
        if (newest == ZR_NULL || entry->insertionOrder > newest->insertionOrder) {
            nextNewest = newest;
            newest = entry;
        } else if (nextNewest == ZR_NULL ||
                   entry->insertionOrder > nextNewest->insertionOrder) {
            nextNewest = entry;
        }
    }
    newest = historyIndex == 0U ? newest : nextNewest;
    if (newest == ZR_NULL || newest->analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    outSnapshot->version = newest->version;
    outSnapshot->contentGeneration = newest->contentGeneration;
    outSnapshot->analyzer = newest->analyzer;
    ZrLanguageServer_LspSemanticCacheLru_Touch(
            (SZrLspContext *)context,
            newest->analyzer);
    return ZR_TRUE;
}
