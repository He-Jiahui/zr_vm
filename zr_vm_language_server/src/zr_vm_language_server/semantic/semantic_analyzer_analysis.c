#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_calls.h"
#include "zr_vm_parser/semantic_relations.h"

#include <string.h>

static TZrBool semantic_analysis_ranges_equal(const SZrFileRange *left,
                                               const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset == right->start.offset &&
           left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.offset == right->end.offset &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
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

/** 缓存只借用诊断指针；重做分析或释放诊断前必须先撤销这些别名。 */
void ZrLanguageServer_SemanticAnalyzer_ClearCachedDiagnosticRefs(
        SZrSemanticAnalyzer *analyzer) {
    if (analyzer == ZR_NULL || analyzer->cache == ZR_NULL ||
        !analyzer->cache->cachedDiagnostics.isValid) {
        return;
    }
    analyzer->cache->cachedDiagnostics.length = 0;
}

/**
 * @brief 结束当前诊断快照，并同步清除缓存中的借用引用。
 * @note 分析重置需要重建数组存储；析构路径只清空元素，再由外层释放数组。
 */
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

/**
 * @brief 为 LSP 局部查询生成符号、类型和诊断事实；全量分析也经由同一入口。
 * @pre scopeRoot 必须是 ast 本身或其中受支持的声明根；传入 AST 的寿命由调用方或快照持有。
 * @note 跨 AST 命中缓存时保留旧事实及其旧 AST；文档更新链先决定是否保留旧 AST，不能单凭相同位置复用。
 */
TZrBool ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ast,
        SZrAstNode *scopeRoot) {
    SZrAstNode *previousAst;
    TZrSize analysisHash = 0;
    TZrSize scopeAstHash = 0;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        !semantic_analysis_root_is_valid(ast, scopeRoot)) {
        return ZR_FALSE;
    }

    analyzer->metrics.requestCount++;

    previousAst = analyzer->ast;
    if (previousAst != ZR_NULL && previousAst != ast) {
        if (analyzer->preserveScopedQueryAnalyzerOnNextAstChange) {
            analyzer->preserveScopedQueryAnalyzerOnNextAstChange = ZR_FALSE;
        } else {
            ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                    state,
                    analyzer);
        }
    }
    if (analyzer->enableCache && analyzer->cache == ZR_NULL) {
        (void)ZrLanguageServer_SemanticAnalyzer_EnsureCacheStorage(state, analyzer);
    }
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analysisHash = semantic_analysis_cache_hash(ast, scopeRoot);
        scopeAstHash = ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(scopeRoot);
        if (previousAst == ast && analyzer->cache->isValid &&
            analyzer->cache->astHash == analysisHash &&
            semantic_analysis_ranges_equal(
                &analyzer->cache->cacheRange,
                &scopeRoot->location)) {
            analyzer->metrics.cacheHitCount++;
            return ZR_TRUE;
        }
        if (previousAst != ast &&
            (analyzer->ownedAst == previousAst ||
             analyzer->borrowedAst == previousAst) &&
            analyzer->cache->isValid &&
            analyzer->cache->scopeAstHash == scopeAstHash &&
            semantic_analysis_ranges_equal(
                    &analyzer->cache->cacheRange,
                    &scopeRoot->location)) {
            analyzer->metrics.cacheHitCount++;
            return ZR_TRUE;
        }
        analyzer->cache->isValid = ZR_FALSE;
    }

    analyzer->ast = ast;
    ZrLanguageServer_SemanticAnalyzer_ReleaseDiagnostics(state, analyzer, ZR_TRUE);
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(state, analyzer, ast)) {
        analyzer->ast = previousAst;
        return ZR_FALSE;
    }
    if (analyzer->ownedAst != ZR_NULL && analyzer->ownedAst != ast) {
        ZrParser_Ast_Free(state, analyzer->ownedAst);
        analyzer->ownedAst = ZR_NULL;
    }
    if (analyzer->borrowedAst != ZR_NULL && analyzer->borrowedAst != ast) {
        analyzer->borrowedAst = ZR_NULL;
    }

    ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, ast);
    if (analyzer->compilerState != ZR_NULL) {
        (void)ZrParser_SemanticRelations_PublishCompilerContracts(
                analyzer->compilerState);
    }
    if (analyzer->compilerState != ZR_NULL) {
        if (!ZrParser_Compiler_ValidateReferenceEscapes(
                    analyzer->compilerState,
                    scopeRoot) &&
            analyzer->compilerState->hasError) {
            ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                    state,
                    analyzer,
                    scopeRoot->location);
        }
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(
                state,
                analyzer,
                scopeRoot);
    }
    if (!ZrParser_SemanticCalls_PublishSource(
                analyzer->semanticContext, ast)) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticRelations_PublishImportOrigins(
                analyzer->semanticContext)) {
        return ZR_FALSE;
    }
    ZrLanguageServer_SemanticAnalyzer_AppendSemanticQueryDiagnostics(state, analyzer);

    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = analysisHash;
        analyzer->cache->scopeAstHash = scopeAstHash;
        analyzer->cache->cacheRange = scopeRoot->location;
        analyzer->cache->isValid = ZR_TRUE;
        analyzer->cache->cachedDiagnostics.length = 0;
    }
    analyzer->metrics.executionCount++;
    analyzer->metrics.lastExecutionRange = scopeRoot->location;
    return ZR_TRUE;
}

/** 项目索引与打开文档的全量分析入口，借用传入 AST 并复用局部分析的事实发布流程。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrAstNode *ast) {
    return ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
            state,
            analyzer,
            ast,
            ast);
}
