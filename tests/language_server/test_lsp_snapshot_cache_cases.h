#ifndef ZR_VM_TEST_LSP_SNAPSHOT_CACHE_CASES_H
#define ZR_VM_TEST_LSP_SNAPSHOT_CACHE_CASES_H

static void test_lsp_identical_content_update_reuses_snapshot_and_semantic_cache(
        SZrState *state) {
    const TZrChar *summary = "LSP Identical Content Update Reuses Snapshot And Semantic Cache";
    const TZrChar *content =
            "compute(): int {\n"
            "    return 1 + 2;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *textBlock;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;
    SZrSemanticContext *semanticContext;
    SZrSemanticAnalysisMetrics beforeMetrics;
    SZrSemanticAnalysisMetrics afterMetrics;
    TZrSize contentGeneration;
    TZrChar reason[512];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///snapshot_cache_reuse.zr",
            strlen("file:///snapshot_cache_reuse.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                content,
                strlen(content),
                1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare the initial semantic snapshot");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = find_test_analyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->textBlock == ZR_NULL ||
        fileVersion->ast == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial snapshot did not publish parser and semantic state");
        return;
    }

    textBlock = fileVersion->textBlock;
    contentGeneration = textBlock->contentGeneration;
    ast = fileVersion->ast;
    semanticContext = analyzer->semanticContext;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &beforeMetrics);

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                content,
                strlen(content),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Identical-content version update failed");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &afterMetrics);
    if (fileVersion->version != 2 ||
        fileVersion->textBlock != textBlock ||
        fileVersion->textBlock->contentGeneration != contentGeneration ||
        fileVersion->ast != ast ||
        analyzer != find_test_analyzer(state, context, uri) ||
        analyzer->semanticContext != semanticContext ||
        afterMetrics.requestCount != beforeMetrics.requestCount + 1 ||
        afterMetrics.executionCount != beforeMetrics.executionCount ||
        afterMetrics.cacheHitCount != beforeMetrics.cacheHitCount + 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected snapshot/cache reuse; version=%zu blockSame=%d generation=%zu/%zu astSame=%d analyzerSame=%d contextSame=%d requests=%zu/%zu executions=%zu/%zu hits=%zu/%zu",
                 (size_t)fileVersion->version,
                 fileVersion->textBlock == textBlock,
                 (size_t)fileVersion->textBlock->contentGeneration,
                 (size_t)contentGeneration,
                 fileVersion->ast == ast,
                 analyzer == find_test_analyzer(state, context, uri),
                 analyzer->semanticContext == semanticContext,
                 (size_t)afterMetrics.requestCount,
                 (size_t)beforeMetrics.requestCount,
                 (size_t)afterMetrics.executionCount,
                 (size_t)beforeMetrics.executionCount,
                 (size_t)afterMetrics.cacheHitCount,
                 (size_t)beforeMetrics.cacheHitCount);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_changed_content_invalidates_snapshot_and_semantic_cache(
        SZrState *state) {
    const TZrChar *summary = "LSP Changed Content Invalidates Snapshot And Semantic Cache";
    const TZrChar *initialContent =
            "compute(): int {\n"
            "    return 1 + 2;\n"
            "}\n";
    const TZrChar *updatedContent =
            "compute(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentBlock *textBlock;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;
    SZrSemanticAnalysisMetrics beforeMetrics;
    SZrSemanticAnalysisMetrics afterMetrics;
    TZrSize contentGeneration;
    TZrChar reason[512];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///snapshot_cache_invalidation.zr",
            strlen("file:///snapshot_cache_invalidation.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                initialContent,
                strlen(initialContent),
                1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare the initial semantic snapshot");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = find_test_analyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->textBlock == ZR_NULL ||
        fileVersion->ast == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial snapshot did not publish parser and semantic state");
        return;
    }

    textBlock = fileVersion->textBlock;
    contentGeneration = textBlock->contentGeneration;
    ast = fileVersion->ast;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &beforeMetrics);

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Changed-content version update failed");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &afterMetrics);
    if (fileVersion->version != 2 ||
        fileVersion->textBlock == textBlock ||
        fileVersion->textBlock->contentGeneration != contentGeneration + 1 ||
        fileVersion->ast == ast ||
        analyzer != find_test_analyzer(state, context, uri) ||
        afterMetrics.requestCount != beforeMetrics.requestCount + 1 ||
        afterMetrics.executionCount != beforeMetrics.executionCount + 1 ||
        afterMetrics.cacheHitCount != beforeMetrics.cacheHitCount) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected snapshot/cache invalidation; version=%zu blockChanged=%d generation=%zu/%zu astChanged=%d analyzerSame=%d requests=%zu/%zu executions=%zu/%zu hits=%zu/%zu",
                 (size_t)fileVersion->version,
                 fileVersion->textBlock != textBlock,
                 (size_t)fileVersion->textBlock->contentGeneration,
                 (size_t)(contentGeneration + 1),
                 fileVersion->ast != ast,
                 analyzer == find_test_analyzer(state, context, uri),
                 (size_t)afterMetrics.requestCount,
                 (size_t)beforeMetrics.requestCount,
                 (size_t)afterMetrics.executionCount,
                 (size_t)beforeMetrics.executionCount,
                 (size_t)afterMetrics.cacheHitCount,
                 (size_t)beforeMetrics.cacheHitCount);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TEST_LSP_SNAPSHOT_CACHE_CASES_H
