#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_language_server/incremental_parser.h"

static TZrSize scoped_cache_range_length(const SZrFileRange *range) {
    return range != ZR_NULL && range->end.offset >= range->start.offset
                   ? range->end.offset - range->start.offset
                   : 0;
}

static TZrBool scoped_cache_ranges_overlap(const SZrFileRange *left,
                                           const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset < right->end.offset &&
           right->start.offset < left->end.offset;
}

static TZrBool scoped_cache_ranges_equal(const SZrFileRange *left,
                                         const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset == right->start.offset &&
           left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.offset == right->end.offset &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

SZrSemanticAnalyzer *
ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    if (analyzer->scopedQueryAnalyzer == ZR_NULL) {
        analyzer->scopedQueryAnalyzer =
                ZrLanguageServer_SemanticAnalyzer_New(state);
        if (analyzer->scopedQueryAnalyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(
                    analyzer->scopedQueryAnalyzer,
                    analyzer->enableCache);
        }
    }

    return analyzer->scopedQueryAnalyzer;
}

void ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    SZrSemanticAnalyzer *scopedQueryAnalyzer;

    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    analyzer->preserveScopedQueryAnalyzerOnNextAstChange = ZR_FALSE;
    if (analyzer->scopedQueryAnalyzer == ZR_NULL) {
        return;
    }
    scopedQueryAnalyzer = analyzer->scopedQueryAnalyzer;
    analyzer->scopedQueryAnalyzer = ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_Free(state, scopedQueryAnalyzer);
}

TZrBool ZrLanguageServer_SemanticAnalyzer_PrepareScopedQueryCacheForChange(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *currentAst,
        const SZrFileChangeInfo *changeInfo,
        TZrBool *retainCurrentAst) {
    SZrSemanticAnalyzer *scopedAnalyzer;
    TZrBool canPreserve;

    if (retainCurrentAst != ZR_NULL) {
        *retainCurrentAst = ZR_FALSE;
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || changeInfo == ZR_NULL ||
        retainCurrentAst == ZR_NULL || analyzer->scopedQueryAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    scopedAnalyzer = analyzer->scopedQueryAnalyzer;
    canPreserve = changeInfo->impact == ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY &&
                  changeInfo->hasDeclaration &&
                  currentAst != ZR_NULL && scopedAnalyzer->enableCache &&
                  scopedAnalyzer->cache != ZR_NULL &&
                  scopedAnalyzer->cache->isValid &&
                  scoped_cache_range_length(&changeInfo->oldRange) ==
                          scoped_cache_range_length(&changeInfo->newRange) &&
                  !scoped_cache_ranges_overlap(
                          &scopedAnalyzer->cache->cacheRange,
                          &changeInfo->declarationRange);
    if (!canPreserve) {
        analyzer->metrics.scopedCacheInvalidationCount++;
        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
        return ZR_FALSE;
    }

    *retainCurrentAst = scopedAnalyzer->ast == currentAst &&
                        scopedAnalyzer->ownedAst == ZR_NULL;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_CommitScopedQueryCachePreservation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *newAst,
        SZrAstNode *retainedAst) {
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrAstNode *newScopeRoot;
    TZrBool canCommit;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (analyzer == ZR_NULL) {
        if (retainedAst != ZR_NULL) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }
    if (newAst == ZR_NULL || analyzer->scopedQueryAnalyzer == ZR_NULL) {
        TZrBool cacheOwnsRetainedAst =
                analyzer->scopedQueryAnalyzer != ZR_NULL &&
                analyzer->scopedQueryAnalyzer->ownedAst == retainedAst;

        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
        if (retainedAst != ZR_NULL && !cacheOwnsRetainedAst) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }

    scopedAnalyzer = analyzer->scopedQueryAnalyzer;
    newScopeRoot = scopedAnalyzer->cache != ZR_NULL
                           ? ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                                 newAst,
                                 scopedAnalyzer->cache->cacheRange)
                           : ZR_NULL;
    canCommit = scopedAnalyzer->cache != ZR_NULL &&
                scopedAnalyzer->cache->isValid && newScopeRoot != ZR_NULL &&
                scoped_cache_ranges_equal(
                        &scopedAnalyzer->cache->cacheRange,
                        &newScopeRoot->location) &&
                scopedAnalyzer->cache->scopeAstHash ==
                        ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(
                                newScopeRoot) &&
                (retainedAst == ZR_NULL ||
                 (scopedAnalyzer->ast == retainedAst &&
                  scopedAnalyzer->ownedAst == ZR_NULL));
    if (!canCommit) {
        TZrBool cacheOwnsRetainedAst = scopedAnalyzer->ownedAst == retainedAst;

        analyzer->metrics.scopedCacheInvalidationCount++;
        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
        if (retainedAst != ZR_NULL && !cacheOwnsRetainedAst) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }

    if (retainedAst != ZR_NULL) {
        scopedAnalyzer->ownedAst = retainedAst;
    }
    analyzer->metrics.scopedCachePreservationCount++;
    analyzer->preserveScopedQueryAnalyzerOnNextAstChange = ZR_TRUE;
    return ZR_TRUE;
}
