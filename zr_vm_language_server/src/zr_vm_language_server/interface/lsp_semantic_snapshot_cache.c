#include "interface/lsp_interface_internal.h"
#include "interface/lsp_semantic_cache_lru.h"
#include "semantic/semantic_analyzer_internal.h"

#include <string.h>

typedef struct SZrLspSemanticSnapshotEntry {
    SZrString *uri;
    TZrSize version;
    TZrSize contentGeneration;
    TZrSize insertionOrder;
    SZrSemanticAnalyzer *analyzer;
} SZrLspSemanticSnapshotEntry;

struct SZrLspSemanticSnapshotCache {
    SZrArray entries;
    TZrSize nextInsertionOrder;
};

static TZrBool snapshot_entry_matches_uri(
        const SZrLspSemanticSnapshotEntry *entry,
        const SZrString *uri) {
    return entry != ZR_NULL && entry->uri != ZR_NULL && uri != ZR_NULL &&
           ZrLanguageServer_Lsp_StringsEqual(entry->uri, (SZrString *)uri);
}

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

    memset(&entry, 0, sizeof(entry));
    entry.uri = uri;
    entry.version = previousContent->version;
    entry.contentGeneration = previousContent->contentBlock->contentGeneration;
    entry.insertionOrder = ++cache->nextInsertionOrder;
    entry.analyzer = snapshotAnalyzer;
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
