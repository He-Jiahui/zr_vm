#include "zr_vm_language_server/lsp_semantic_snapshot.h"

static TZrBool analysis_external_references_have_generation(
        SZrState *state,
        const SZrSemanticAnalyzer *analyzer,
        TZrUInt64 generation) {
    SZrArray references = {0};
    TZrBool valid;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    valid = ZrParser_SemanticQuery_ExternalReferences(
            analyzer->semanticContext, ZR_NULL, &references) &&
            references.length > 0U &&
            analyzer->semanticContext->externalProviderGeneration == generation;
    for (TZrSize index = 0U; valid && index < references.length; index++) {
        const SZrParserSemanticExternalReferenceQuery *reference =
                (const SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                        &references, index);
        valid = reference != ZR_NULL &&
                reference->externalProviderGeneration == generation;
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    return valid;
}

static void test_analysis_provider_generation_invalidates_same_ast_cache(
        SZrState *state) {
    const TZrChar *summary = "LSP Provider Generation Invalidates Same AST Caches";
    const TZrChar *content =
            "var api = import(\"zr.system\");\nreturn api.console;\n";
    TZrChar uriText[] = "file:///analysis_provider_generation.zr";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrSemanticAnalyzer *scoped;
    SZrAstNode *ast;
    SZrSemanticAnalysisMetrics before;
    TZrUInt64 generation;
    const TZrChar *failure = "fixture analysis";
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U)) {
        goto cleanup;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        goto cleanup;
    }
    ast = analyzer->ast;
    generation = context->semanticSnapshotProviderGeneration;
    scoped = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state, analyzer);
    failure = "warm whole-document and scoped caches";
    if (scoped == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(state, scoped, ast, ast)) {
        goto cleanup;
    }
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &before);
    if (!ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) ||
        analyzer->metrics.executionCount != before.executionCount ||
        analyzer->metrics.cacheHitCount != before.cacheHitCount + 1U ||
        analyzer->scopedQueryAnalyzer != scoped) {
        goto cleanup;
    }

    ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(context);
    failure = "analyzer lookup must hide the previous provider facts";
    if (ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri) != analyzer ||
        analyzer->semanticContext != ZR_NULL || analyzer->scopedQueryAnalyzer != ZR_NULL) {
        goto cleanup;
    }
    failure = "changed provider generation must reanalyze the same AST";
    if (generation == 0U || context->semanticSnapshotProviderGeneration == generation ||
        !ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast) ||
        analyzer->metrics.executionCount != before.executionCount + 1U ||
        analyzer->scopedQueryAnalyzer != ZR_NULL ||
        !analysis_external_references_have_generation(
                state, analyzer, context->semanticSnapshotProviderGeneration)) {
        goto cleanup;
    }

    failure = "new scoped analysis must inherit the new provider generation";
    scoped = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state, analyzer);
    if (scoped == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(state, scoped, ast, ast) ||
        !analysis_external_references_have_generation(
                state, scoped, context->semanticSnapshotProviderGeneration)) {
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}
