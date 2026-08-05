#ifndef ZR_VM_TEST_LSP_SNAPSHOT_CACHE_CASES_H
#define ZR_VM_TEST_LSP_SNAPSHOT_CACHE_CASES_H

static void test_lsp_identical_content_update_reuses_snapshot_and_semantic_cache(
        SZrState *state) {
    const TZrChar *summary = "LSP Identical Content Update Reuses Snapshot And Semantic Cache";
    const TZrChar *content =
            "fn compute(): int {\n"
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
            "fn compute(): int {\n"
            "    return 1 + 2;\n"
            "}\n";
    const TZrChar *updatedContent =
            "fn compute(): int {\n"
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
        fileVersion->lastChangeInfo.isTokenEquivalent ||
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

static void test_lsp_token_equivalent_comment_edit_reuses_semantic_snapshot(
        SZrState *state) {
    const TZrChar *summary = "LSP Token Equivalent Comment Edit Reuses Semantic Snapshot";
    const TZrChar *initialContent =
            "fn compute(): int {\n"
            "    // old note\n"
            "    return 1 + 2;\n"
            "}\n";
    const TZrChar *updatedContent =
            "fn compute(): int {\n"
            "    // new note\n"
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
            "file:///token_equivalent_comment.zr",
            strlen("file:///token_equivalent_comment.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        strlen(initialContent) != strlen(updatedContent) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare token-equivalent comment content");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = find_test_analyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->textBlock == ZR_NULL ||
        fileVersion->ast == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial comment snapshot was incomplete");
        return;
    }

    textBlock = fileVersion->textBlock;
    contentGeneration = textBlock->contentGeneration;
    ast = fileVersion->ast;
    semanticContext = analyzer->semanticContext;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &beforeMetrics);

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, updatedContent, strlen(updatedContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Token-equivalent comment update failed");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &afterMetrics);
    if (!fileVersion->lastChangeInfo.isTokenEquivalent ||
        fileVersion->lastChangeInfo.impact != ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        fileVersion->textBlock == textBlock ||
        fileVersion->textBlock->contentGeneration != contentGeneration + 1 ||
        strstr(fileVersion->textBlock->content, "new note") == ZR_NULL ||
        fileVersion->ast != ast ||
        analyzer->semanticContext != semanticContext ||
        afterMetrics.requestCount != beforeMetrics.requestCount + 1 ||
        afterMetrics.executionCount != beforeMetrics.executionCount ||
        afterMetrics.cacheHitCount != beforeMetrics.cacheHitCount + 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected token-equivalent reuse; equivalent=%d impact=%d blockChanged=%d generation=%zu/%zu astSame=%d contextSame=%d requests=%zu/%zu executions=%zu/%zu hits=%zu/%zu",
                 fileVersion->lastChangeInfo.isTokenEquivalent,
                 (int)fileVersion->lastChangeInfo.impact,
                 fileVersion->textBlock != textBlock,
                 (size_t)fileVersion->textBlock->contentGeneration,
                 (size_t)(contentGeneration + 1),
                 fileVersion->ast == ast,
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

static void test_lsp_token_coordinate_change_invalidates_semantic_snapshot(
        SZrState *state) {
    const TZrChar *summary = "LSP Token Coordinate Change Invalidates Semantic Snapshot";
    const TZrChar *initialContent = "fn compute(): int { return 1; }\n";
    const TZrChar *updatedContent = "fn compute(): int {\nreturn 1; }\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;
    SZrSemanticAnalysisMetrics beforeMetrics;
    SZrSemanticAnalysisMetrics afterMetrics;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///token_coordinate_change.zr",
            strlen("file:///token_coordinate_change.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        strlen(initialContent) != strlen(updatedContent) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare coordinate-change content");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = find_test_analyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL || analyzer == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial coordinate snapshot was incomplete");
        return;
    }
    ast = fileVersion->ast;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &beforeMetrics);

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, updatedContent, strlen(updatedContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Coordinate-changing update failed");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &afterMetrics);
    if (fileVersion->lastChangeInfo.isTokenEquivalent ||
        fileVersion->ast == ast ||
        afterMetrics.requestCount != beforeMetrics.requestCount + 1 ||
        afterMetrics.executionCount != beforeMetrics.executionCount + 1 ||
        afterMetrics.cacheHitCount != beforeMetrics.cacheHitCount) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Token coordinate movement reused a stale semantic snapshot");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_token_value_change_invalidates_semantic_snapshot(
        SZrState *state) {
    const TZrChar *summary = "LSP Token Value Change Invalidates Semantic Snapshot";
    const TZrChar *initialContent = "fn compute(): int { return 1; }\n";
    const TZrChar *updatedContent = "fn compute(): int { return 2; }\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;
    SZrSemanticAnalysisMetrics beforeMetrics;
    SZrSemanticAnalysisMetrics afterMetrics;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///token_value_change.zr",
            strlen("file:///token_value_change.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        strlen(initialContent) != strlen(updatedContent) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare token-value content");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = find_test_analyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL || analyzer == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial token-value snapshot was incomplete");
        return;
    }
    ast = fileVersion->ast;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &beforeMetrics);

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, updatedContent, strlen(updatedContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Token-value update failed");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &afterMetrics);
    if (fileVersion->lastChangeInfo.isTokenEquivalent ||
        fileVersion->ast == ast ||
        afterMetrics.requestCount != beforeMetrics.requestCount + 1 ||
        afterMetrics.executionCount != beforeMetrics.executionCount + 1 ||
        afterMetrics.cacheHitCount != beforeMetrics.cacheHitCount) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "A changed token value reused a stale semantic snapshot");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_body_edit_records_minimal_change_and_declaration_scope(
        SZrState *state) {
    const TZrChar *summary = "LSP Body Edit Records Minimal Change And Declaration Scope";
    const TZrChar *initialContent =
            "fn alpha(): int {\n"
            "    return 1 + 2;\n"
            "}\n"
            "fn beta(): int {\n"
            "    return 3;\n"
            "}\n";
    const TZrChar *updatedContent =
            "fn alpha(): int {\n"
            "    return 10 + 20;\n"
            "}\n"
            "fn beta(): int {\n"
            "    return 3;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrAstNode *root;
    SZrFileRange rootRange;
    SZrFileRange position;
    const TZrChar *oldExpression;
    const TZrChar *newExpression;
    TZrSize expectedStart;
    TZrChar reason[512];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///minimal_body_change.zr",
            strlen("file:///minimal_body_change.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare the body-edit snapshot");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    oldExpression = strstr(initialContent, "1 + 2");
    newExpression = strstr(updatedContent, "10 + 20");
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL ||
        oldExpression == ZR_NULL || newExpression == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial body-edit state is incomplete");
        return;
    }

    position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(
                    (TZrSize)(oldExpression - initialContent), 0, 0),
            ZrParser_FilePosition_Create(
                    (TZrSize)(oldExpression - initialContent), 0, 0),
            uri);
    root = ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
            fileVersion->ast, position);
    if (root == ZR_NULL || root->type != ZR_AST_FUNCTION_DECLARATION) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to resolve the old owning function");
        return;
    }
    rootRange = root->location;
    expectedStart = (TZrSize)(oldExpression - initialContent) + 1;

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, updatedContent, strlen(updatedContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Body edit update failed");
        return;
    }

    if (fileVersion->lastChangeInfo.impact != ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        !fileVersion->lastChangeInfo.hasDeclaration ||
        fileVersion->lastChangeInfo.declarationType != ZR_AST_FUNCTION_DECLARATION ||
        fileVersion->lastChangeInfo.declarationRange.start.offset != rootRange.start.offset ||
        fileVersion->lastChangeInfo.declarationRange.end.offset != rootRange.end.offset ||
        fileVersion->lastChangeInfo.oldRange.start.offset != expectedStart ||
        fileVersion->lastChangeInfo.oldRange.end.offset !=
                (TZrSize)(oldExpression - initialContent) + strlen("1 + 2") ||
        fileVersion->lastChangeInfo.newRange.start.offset != expectedStart ||
        fileVersion->lastChangeInfo.newRange.end.offset !=
                (TZrSize)(newExpression - updatedContent) + strlen("10 + 20") ||
        fileVersion->lastChangeRange.start.offset !=
                fileVersion->lastChangeInfo.newRange.start.offset ||
        fileVersion->lastChangeRange.end.offset !=
                fileVersion->lastChangeInfo.newRange.end.offset) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected body range old=%zu..%zu new=%zu..%zu impact=%d declaration=%d/%d root=%zu..%zu actualRoot=%zu..%zu",
                 (size_t)expectedStart,
                 (size_t)((oldExpression - initialContent) + strlen("1 + 2")),
                 (size_t)expectedStart,
                 (size_t)((newExpression - updatedContent) + strlen("10 + 20")),
                 (int)fileVersion->lastChangeInfo.impact,
                 fileVersion->lastChangeInfo.hasDeclaration,
                 (int)fileVersion->lastChangeInfo.declarationType,
                 (size_t)rootRange.start.offset,
                 (size_t)rootRange.end.offset,
                 (size_t)fileVersion->lastChangeInfo.declarationRange.start.offset,
                 (size_t)fileVersion->lastChangeInfo.declarationRange.end.offset);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_signature_edit_records_minimal_change_and_declaration_scope(
        SZrState *state) {
    const TZrChar *summary = "LSP Signature Edit Records Minimal Change And Declaration Scope";
    const TZrChar *initialContent =
            "fn compute(value: int): int {\n"
            "    return value;\n"
            "}\n";
    const TZrChar *updatedContent =
            "fn compute(value: float): int {\n"
            "    return value;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrAstNode *root;
    SZrFileRange rootRange;
    SZrFileRange position;
    const TZrChar *oldType;
    const TZrChar *newType;
    TZrSize oldStart;
    TZrSize newStart;
    TZrChar reason[512];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///minimal_signature_change.zr",
            strlen("file:///minimal_signature_change.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare the signature-edit snapshot");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    oldType = strstr(initialContent, "int): int");
    newType = strstr(updatedContent, "float): int");
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL ||
        oldType == ZR_NULL || newType == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial signature-edit state is incomplete");
        return;
    }
    oldStart = (TZrSize)(oldType - initialContent);
    newStart = (TZrSize)(newType - updatedContent);
    position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(oldStart, 0, 0),
            ZrParser_FilePosition_Create(oldStart, 0, 0),
            uri);
    root = ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
            fileVersion->ast, position);
    if (root == ZR_NULL || root->type != ZR_AST_FUNCTION_DECLARATION) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to resolve the old signature owner");
        return;
    }
    rootRange = root->location;

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, updatedContent, strlen(updatedContent), 2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Signature edit update failed");
        return;
    }

    if (fileVersion->lastChangeInfo.impact != ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE ||
        !fileVersion->lastChangeInfo.hasDeclaration ||
        fileVersion->lastChangeInfo.declarationType != ZR_AST_FUNCTION_DECLARATION ||
        fileVersion->lastChangeInfo.declarationRange.start.offset != rootRange.start.offset ||
        fileVersion->lastChangeInfo.declarationRange.end.offset != rootRange.end.offset ||
        fileVersion->lastChangeInfo.oldRange.start.offset != oldStart ||
        fileVersion->lastChangeInfo.oldRange.end.offset != oldStart + strlen("in") ||
        fileVersion->lastChangeInfo.newRange.start.offset != newStart ||
        fileVersion->lastChangeInfo.newRange.end.offset != newStart + strlen("floa")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected signature range old=%zu..%zu new=%zu..%zu impact=%d declaration=%d/%d root=%zu..%zu actualRoot=%zu..%zu",
                 (size_t)oldStart,
                 (size_t)(oldStart + strlen("in")),
                 (size_t)newStart,
                 (size_t)(newStart + strlen("floa")),
                 (int)fileVersion->lastChangeInfo.impact,
                 fileVersion->lastChangeInfo.hasDeclaration,
                 (int)fileVersion->lastChangeInfo.declarationType,
                 (size_t)rootRange.start.offset,
                 (size_t)rootRange.end.offset,
                 (size_t)fileVersion->lastChangeInfo.declarationRange.start.offset,
                 (size_t)fileVersion->lastChangeInfo.declarationRange.end.offset);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_top_level_insertion_records_module_change(
        SZrState *state) {
    const TZrChar *summary = "LSP Top Level Insertion Records Module Change";
    const TZrChar *initialContent =
            "fn compute(): int {\n"
            "    return 1;\n"
            "}\n";
    const TZrChar *insertedDeclaration = "var added = 0;\n";
    const TZrChar *updatedContent =
            "var added = 0;\n"
            "fn compute(): int {\n"
            "    return 1;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    TZrChar reason[384];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///minimal_module_change.zr",
            strlen("file:///minimal_module_change.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare the module-edit snapshot");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, updatedContent, strlen(updatedContent), 2)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Top-level insertion update failed");
        return;
    }

    if (fileVersion->lastChangeInfo.impact != ZR_FILE_CHANGE_IMPACT_MODULE ||
        fileVersion->lastChangeInfo.hasDeclaration ||
        fileVersion->lastChangeInfo.oldRange.start.offset != 0 ||
        fileVersion->lastChangeInfo.oldRange.end.offset != 0 ||
        fileVersion->lastChangeInfo.newRange.start.offset != 0 ||
        fileVersion->lastChangeInfo.newRange.end.offset != strlen(insertedDeclaration)) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected module insertion old=%zu..%zu new=%zu..%zu impact=%d declaration=%d",
                 (size_t)fileVersion->lastChangeInfo.oldRange.start.offset,
                 (size_t)fileVersion->lastChangeInfo.oldRange.end.offset,
                 (size_t)fileVersion->lastChangeInfo.newRange.start.offset,
                 (size_t)fileVersion->lastChangeInfo.newRange.end.offset,
                 (int)fileVersion->lastChangeInfo.impact,
                 fileVersion->lastChangeInfo.hasDeclaration);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    fileVersion->lastChangeInfo.hasDeclaration = ZR_TRUE;
    fileVersion->lastChangeInfo.declarationType = ZR_AST_FUNCTION_DECLARATION;
    fileVersion->lastChangeInfo.declarationRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10, 0, 0),
            ZrParser_FilePosition_Create(20, 0, 0),
            uri);
    ZrLanguageServer_SemanticAnalyzer_ClassifyFileChange(
            ZR_NULL,
            &fileVersion->lastChangeInfo);
    if (fileVersion->lastChangeInfo.hasDeclaration ||
        fileVersion->lastChangeInfo.declarationType != (EZrAstNodeType)0 ||
        fileVersion->lastChangeInfo.declarationRange.start.offset != 0 ||
        fileVersion->lastChangeInfo.declarationRange.end.offset != 0 ||
        fileVersion->lastChangeInfo.declarationRange.source != uri) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Module reclassification retained stale declaration metadata");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_fallback_ast_change_remains_module_scoped(
        SZrState *state) {
    const TZrChar *summary = "LSP Fallback AST Change Remains Module Scoped";
    const TZrChar *initialContent =
            "fn compute(): int {\n"
            "    return 1;\n"
            "}\n";
    const TZrChar *invalidContent =
            "fn compute(): int {\n"
            "    return 1 +;\n"
            "}\n";
    const TZrChar *recoveredContent =
            "fn compute(): int {\n"
            "    return 1 + 2;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///fallback_ast_change.zr",
            strlen("file:///fallback_ast_change.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 1) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, invalidContent, strlen(invalidContent), 2)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare a last-good fallback AST");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || !fileVersion->usesFallbackAst ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                recoveredContent,
                strlen(recoveredContent),
                3)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Fallback AST state or recovery update was unavailable");
        return;
    }

    if (fileVersion->lastChangeInfo.impact != ZR_FILE_CHANGE_IMPACT_MODULE ||
        fileVersion->lastChangeInfo.hasDeclaration) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "A stale fallback AST classified an immediate-text declaration change");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_non_monotonic_versions_are_rejected_before_semantic_work(
        SZrState *state) {
    const TZrChar *summary = "LSP Non-Monotonic Versions Are Rejected Before Semantic Work";
    const TZrChar *initialContent =
            "fn compute(): int {\n"
            "    return 1;\n"
            "}\n";
    const TZrChar *staleContent =
            "fn compute(): int {\n"
            "    return 2;\n"
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
    TZrSize generation;
    TZrBool sameVersionAccepted;
    TZrBool staleVersionAccepted;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///non_monotonic_version.zr",
            strlen("file:///non_monotonic_version.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, initialContent, strlen(initialContent), 5)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare versioned semantic state");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analyzer = find_test_analyzer(state, context, uri);
    if (fileVersion == ZR_NULL || fileVersion->textBlock == ZR_NULL ||
        fileVersion->ast == ZR_NULL || analyzer == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Initial versioned semantic state is incomplete");
        return;
    }
    textBlock = fileVersion->textBlock;
    generation = textBlock->contentGeneration;
    ast = fileVersion->ast;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &beforeMetrics);

    sameVersionAccepted = ZrLanguageServer_Lsp_UpdateDocument(
            state, context, uri, initialContent, strlen(initialContent), 5);
    staleVersionAccepted = ZrLanguageServer_Lsp_UpdateDocument(
            state, context, uri, staleContent, strlen(staleContent), 4);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(analyzer, &afterMetrics);

    if (sameVersionAccepted || staleVersionAccepted ||
        fileVersion->version != 5 ||
        fileVersion->textBlock != textBlock ||
        fileVersion->textBlock->contentGeneration != generation ||
        fileVersion->ast != ast ||
        afterMetrics.requestCount != beforeMetrics.requestCount ||
        afterMetrics.executionCount != beforeMetrics.executionCount ||
        afterMetrics.cacheHitCount != beforeMetrics.cacheHitCount) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Rejected version reached snapshot or semantic work");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TEST_LSP_SNAPSHOT_CACHE_CASES_H
