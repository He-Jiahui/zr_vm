#include "semantic/semantic_analyzer_internal.h"

#include <string.h>

static TZrBool semantic_analysis_ranges_equal(const SZrFileRange *left,
                                               const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset == right->start.offset &&
           left->end.offset == right->end.offset;
}

static TZrSize semantic_analysis_cache_hash(SZrAstNode *ast, SZrAstNode *scopeRoot) {
    TZrSize astHash = ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(ast);

    if (scopeRoot == ast) {
        return astHash;
    }
    return astHash * (TZrSize)31u +
           ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(scopeRoot);
}

static TZrBool semantic_analysis_root_is_valid(SZrAstNode *ast, SZrAstNode *scopeRoot) {
    if (ast == ZR_NULL || scopeRoot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (scopeRoot == ast) {
        return ZR_TRUE;
    }
    return scopeRoot->location.end.offset >= scopeRoot->location.start.offset &&
           ZrLanguageServer_SemanticAnalyzer_IsAnalysisRoot(ast, scopeRoot);
}

void ZrLanguageServer_SemanticAnalyzer_ClearCachedDiagnosticRefs(
        SZrSemanticAnalyzer *analyzer) {
    if (analyzer == ZR_NULL || analyzer->cache == ZR_NULL ||
        !analyzer->cache->cachedDiagnostics.isValid) {
        return;
    }
    analyzer->cache->cachedDiagnostics.length = 0;
}

void ZrLanguageServer_SemanticAnalyzer_ReleaseDiagnostics(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        TZrBool resetStorage) {
    TZrSize capacity;
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL || !analyzer->diagnostics.isValid) {
        return;
    }

    for (index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        SZrDiagnostic *diagnostic =
                diagnosticPtr != ZR_NULL ? *diagnosticPtr : ZR_NULL;
        TZrBool alreadyReleased = ZR_FALSE;
        TZrSize previousIndex;

        if (diagnostic == ZR_NULL) {
            continue;
        }
        for (previousIndex = 0; previousIndex < index; previousIndex++) {
            SZrDiagnostic **previousPtr =
                    (SZrDiagnostic **)ZrCore_Array_Get(
                            &analyzer->diagnostics,
                            previousIndex);
            if (previousPtr != ZR_NULL && *previousPtr == diagnostic) {
                alreadyReleased = ZR_TRUE;
                break;
            }
        }
        if (!alreadyReleased) {
            ZrLanguageServer_Diagnostic_Free(state, diagnostic);
        }
        if (diagnosticPtr != ZR_NULL) {
            *diagnosticPtr = ZR_NULL;
        }
    }

    ZrLanguageServer_SemanticAnalyzer_ClearCachedDiagnosticRefs(analyzer);
    if (resetStorage) {
        capacity = analyzer->diagnostics.capacity > 0
                           ? analyzer->diagnostics.capacity
                           : ZR_LSP_ARRAY_INITIAL_CAPACITY;
        ZrCore_Array_Free(state, &analyzer->diagnostics);
        ZrCore_Array_Init(
                state,
                &analyzer->diagnostics,
                sizeof(SZrDiagnostic *),
                capacity);
        return;
    }
    analyzer->diagnostics.length = 0;
}

void ZrLanguageServer_SemanticAnalyzer_GetMetrics(
        const SZrSemanticAnalyzer *analyzer,
        SZrSemanticAnalysisMetrics *outMetrics) {
    if (outMetrics == ZR_NULL) {
        return;
    }
    if (analyzer == ZR_NULL) {
        memset(outMetrics, 0, sizeof(*outMetrics));
        return;
    }
    *outMetrics = analyzer->metrics;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ast,
        SZrAstNode *scopeRoot) {
    SZrAstNode *previousAst;
    TZrSize analysisHash = 0;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        !semantic_analysis_root_is_valid(ast, scopeRoot)) {
        return ZR_FALSE;
    }

    analyzer->metrics.requestCount++;

    previousAst = analyzer->ast;
    if (previousAst != ZR_NULL && previousAst != ast) {
        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
    }
    analyzer->ast = ast;
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analysisHash = semantic_analysis_cache_hash(ast, scopeRoot);
        if (previousAst == ast && analyzer->cache->isValid &&
            analyzer->cache->astHash == analysisHash &&
            semantic_analysis_ranges_equal(
                &analyzer->cache->cacheRange,
                &scopeRoot->location)) {
            analyzer->metrics.cacheHitCount++;
            return ZR_TRUE;
        }
        analyzer->cache->isValid = ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_ReleaseDiagnostics(state, analyzer, ZR_TRUE);
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(state, analyzer, ast)) {
        return ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, ast);
    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(
            state,
            analyzer,
            scopeRoot);
    if (analyzer->compilerState != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(
                state,
                analyzer,
                scopeRoot);
    }
    ZrLanguageServer_SemanticAnalyzer_AppendSemanticQueryDiagnostics(state, analyzer);

    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = analysisHash;
        analyzer->cache->cacheRange = scopeRoot->location;
        analyzer->cache->isValid = ZR_TRUE;
        analyzer->cache->cachedDiagnostics.length = 0;
    }
    analyzer->metrics.executionCount++;
    analyzer->metrics.lastExecutionRange = scopeRoot->location;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrAstNode *ast) {
    return ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
            state,
            analyzer,
            ast,
            ast);
}
