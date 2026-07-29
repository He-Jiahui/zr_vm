#ifndef ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SCOPE_CASES_H
#define ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SCOPE_CASES_H

static SZrAstNode *local_scope_test_function_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->nodes == ZR_NULL ||
        index >= ast->data.script.statements->count) {
        return ZR_NULL;
    }
    return ast->data.script.statements->nodes[index];
}

static SZrAstNode *local_scope_test_return_expression(SZrAstNode *functionNode) {
    SZrAstNode *body;
    SZrAstNode *statement;

    if (functionNode == ZR_NULL || functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_NULL;
    }
    body = functionNode->data.functionDeclaration.body;
    if (body == ZR_NULL || body->type != ZR_AST_BLOCK ||
        body->data.block.body == ZR_NULL ||
        body->data.block.body->nodes == ZR_NULL ||
        body->data.block.body->count == 0) {
        return ZR_NULL;
    }
    statement = body->data.block.body->nodes[0];
    return statement != ZR_NULL && statement->type == ZR_AST_RETURN_STATEMENT
               ? statement->data.returnStatement.expr
               : ZR_NULL;
}

static TZrBool local_scope_test_has_symbol_for_node(const SZrSemanticContext *context,
                                                    const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *record =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols,
                        index);
        if (record != ZR_NULL && record->astNode == node) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_analysis_root_resolver_rejects_non_callable_wrappers(
        SZrState *state) {
    const TZrChar *summary = "Analysis Root Resolver Rejects Non Callable Wrappers";
    SZrTestTimer timer;
    SZrAstNode propertyNode;
    SZrAstNode compileTimeNode;
    SZrFileRange position;

    ZR_UNUSED_PARAMETER(state);
    TEST_START(summary);
    memset(&propertyNode, 0, sizeof(propertyNode));
    memset(&compileTimeNode, 0, sizeof(compileTimeNode));
    memset(&position, 0, sizeof(position));

    propertyNode.type = ZR_AST_CLASS_PROPERTY;
    propertyNode.location.start.offset = 10;
    propertyNode.location.end.offset = 20;
    compileTimeNode.type = ZR_AST_COMPILE_TIME_DECLARATION;
    compileTimeNode.location.start.offset = 30;
    compileTimeNode.location.end.offset = 40;

    position.start.offset = 15;
    position.end.offset = 15;
    if (ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                &propertyNode,
                position) != ZR_NULL) {
        TEST_FAIL(timer, summary, "Property wrapper without an accessor is not an analysis root");
        return;
    }

    position.start.offset = 35;
    position.end.offset = 35;
    if (ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                &compileTimeNode,
                position) != ZR_NULL) {
        TEST_FAIL(timer, summary, "Compile-time wrapper without a callable is not an analysis root");
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_scoped_semantic_analysis_limits_body_facts_and_reuses_scope_cache(
        SZrState *state) {
    const TZrChar *summary = "Scoped Semantic Analysis Limits Body Facts And Reuses Scope Cache";
    const TZrChar *uriText = "file:///local_scoped_semantic_analysis.zr";
    const TZrChar *content =
            "fn first(): int {\n"
            "    return 1 + 2;\n"
            "}\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrAstNode *ast;
    SZrAstNode *firstFunction;
    SZrAstNode *secondFunction;
    SZrAstNode *firstExpression;
    SZrAstNode *secondExpression;
    SZrAstNode *resolvedFirstRoot;
    const SZrSemanticNumericFact *numericFact;
    SZrSemanticContext *cachedContext;
    TZrBool firstAnalyzeSuccess;
    TZrChar reason[512];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
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
        TEST_FAIL(timer, summary, "Failed to prepare scoped semantic fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ast = fileVersion != ZR_NULL ? fileVersion->ast : ZR_NULL;
    firstFunction = local_scope_test_function_at(ast, 0);
    secondFunction = local_scope_test_function_at(ast, 1);
    firstExpression = local_scope_test_return_expression(firstFunction);
    secondExpression = local_scope_test_return_expression(secondFunction);
    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    resolvedFirstRoot = firstExpression != ZR_NULL
                                ? ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                                      ast,
                                      firstExpression->location)
                                : ZR_NULL;
    firstAnalyzeSuccess = analyzer != ZR_NULL && firstFunction != ZR_NULL
                                  ? ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                                        state,
                                        analyzer,
                                        ast,
                                        firstFunction)
                                  : ZR_FALSE;
    if (firstExpression == ZR_NULL || secondExpression == ZR_NULL || analyzer == ZR_NULL ||
        resolvedFirstRoot != firstFunction || !firstAnalyzeSuccess) {
        snprintf(reason,
                 sizeof(reason),
                 "Fixture/scope failure: ast=%p first=%p type=%d second=%p type=%d firstExpr=%p secondExpr=%p resolved=%p analyze=%d",
                 (void *)ast,
                 (void *)firstFunction,
                 firstFunction != ZR_NULL ? (int)firstFunction->type : -1,
                 (void *)secondFunction,
                 secondFunction != ZR_NULL ? (int)secondFunction->type : -1,
                 (void *)firstExpression,
                 (void *)secondExpression,
                 (void *)resolvedFirstRoot,
                 (int)firstAnalyzeSuccess);
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(
            analyzer->semanticContext,
            firstExpression);
    if (!local_scope_test_has_symbol_for_node(analyzer->semanticContext, firstFunction) ||
        !local_scope_test_has_symbol_for_node(analyzer->semanticContext, secondFunction) ||
        ZrParser_SemanticFacts_FindExpressionByNode(
                analyzer->semanticContext,
                firstExpression) == ZR_NULL ||
        numericFact == ZR_NULL || !numericFact->hasRange ||
        numericFact->minValue != 3 || numericFact->maxValue != 3 ||
        ZrParser_SemanticFacts_FindExpressionByNode(
                analyzer->semanticContext,
                secondExpression) != ZR_NULL ||
        ZrParser_SemanticFacts_FindNumericByNode(
                analyzer->semanticContext,
                secondExpression) != ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected module declarations plus first-scope facts only; firstNumeric=%p secondExpression=%p secondNumeric=%p",
                 (void *)numericFact,
                 (void *)ZrParser_SemanticFacts_FindExpressionByNode(
                         analyzer->semanticContext,
                         secondExpression),
                 (void *)ZrParser_SemanticFacts_FindNumericByNode(
                         analyzer->semanticContext,
                         secondExpression));
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    cachedContext = analyzer->semanticContext;
    if (!ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                analyzer,
                ast,
                firstFunction) ||
        analyzer->semanticContext != cachedContext ||
        analyzer->cache == ZR_NULL || !analyzer->cache->isValid ||
        analyzer->cache->cacheRange.start.offset != firstFunction->location.start.offset ||
        analyzer->cache->cacheRange.end.offset != firstFunction->location.end.offset) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Repeated scoped analysis did not reuse the first-scope cache");
        return;
    }

    if (!ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                analyzer,
                ast,
                secondFunction) ||
        ZrParser_SemanticFacts_FindExpressionByNode(
                analyzer->semanticContext,
                firstExpression) != ZR_NULL ||
        ZrParser_SemanticFacts_FindExpressionByNode(
                analyzer->semanticContext,
                secondExpression) == ZR_NULL ||
        ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                ast,
                secondExpression->location) != secondFunction) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Switching scopes did not replace body facts with the second function");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_scoped_query_analyzer_cache_reuses_scope_and_invalidates_on_edit(
        SZrState *state) {
    const TZrChar *summary =
            "Scoped Query Analyzer Cache Reuses Scope And Invalidates On Edit";
    const TZrChar *uriText = "file:///scoped_query_analyzer_cache.zr";
    const TZrChar *initialContent =
            "fn first(): int {\n"
            "    return 1 + 2;\n"
            "}\n"
            "// old note\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *tokenEquivalentContent =
            "fn first(): int {\n"
            "    return 1 + 2;\n"
            "}\n"
            "// new note\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *updatedContent =
            "fn first(): int {\n"
            "    return 1 + 3;\n"
            "}\n"
            "// new note\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalyzer *ownerAfterEdit;
    SZrAstNode *initialAst;
    SZrAstNode *analysisRoot;
    SZrSemanticAnalysisMetrics metrics;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
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
        TEST_FAIL(timer, summary, "Failed to prepare scoped query cache fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    analysisRoot = fileVersion != ZR_NULL
                           ? local_scope_test_function_at(fileVersion->ast, 0)
                           : ZR_NULL;
    initialAst = fileVersion != ZR_NULL ? fileVersion->ast : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || analysisRoot == ZR_NULL || scopedAnalyzer == ZR_NULL ||
        owner->scopedQueryAnalyzer != scopedAnalyzer ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                analysisRoot) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                analysisRoot)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to prepare and repeat scoped query analysis");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &metrics);
    if (metrics.requestCount != 2 || metrics.executionCount != 1 ||
        metrics.cacheHitCount != 1) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Repeated scoped query did not hit its isolated cache");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                tokenEquivalentContent,
                strlen(tokenEquivalentContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply token-equivalent cache update");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ownerAfterEdit = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    analysisRoot = fileVersion != ZR_NULL
                           ? local_scope_test_function_at(fileVersion->ast, 0)
                           : ZR_NULL;
    if (ownerAfterEdit != owner || owner->scopedQueryAnalyzer != scopedAnalyzer ||
        fileVersion == ZR_NULL || fileVersion->ast != initialAst ||
        analysisRoot == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                analysisRoot)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Token-equivalent update discarded the scoped query cache");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &metrics);
    if (metrics.requestCount != 3 || metrics.executionCount != 1 ||
        metrics.cacheHitCount != 2) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Token-equivalent update did not reuse scoped query work");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                3)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply scoped query invalidation edit");
        return;
    }

    ownerAfterEdit = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    analysisRoot = fileVersion != ZR_NULL
                           ? local_scope_test_function_at(fileVersion->ast, 0)
                           : ZR_NULL;
    if (ownerAfterEdit != owner || owner->scopedQueryAnalyzer != ZR_NULL ||
        fileVersion == ZR_NULL || analysisRoot == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Real edit did not invalidate the prior scoped query analyzer");
        return;
    }

    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                analysisRoot)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to analyze the edited scoped query snapshot");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &metrics);
    if (metrics.requestCount != 1 || metrics.executionCount != 1 ||
        metrics.cacheHitCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Edited snapshot reused stale scoped query metrics");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_body_edit_preserves_unaffected_scoped_query_cache(
        SZrState *state) {
    const TZrChar *summary =
            "Body Edit Preserves Unaffected Scoped Query Cache";
    const TZrChar *uriText = "file:///unaffected_scoped_query_cache.zr";
    const TZrChar *initialContent =
            "fn first(): int {\n"
            "    return 1 + 2;\n"
            "}\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *updatedContent =
            "fn first(): int {\n"
            "    return 1 + 3;\n"
            "}\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *secondStableContent =
            "fn first(): int {\n"
            "    return 1 + 4;\n"
            "}\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *shiftedContent =
            "fn first(): int {\n"
            "    return 1 +\n"
            "4;\n"
            "}\n"
            "fn second(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticContext *cachedContext;
    SZrAstNode *initialAst;
    SZrAstNode *secondFunction;
    SZrAstNode *secondExpression;
    SZrAstNode *cachedExpression;
    SZrAstNode *currentSecondFunction;
    const SZrSemanticNumericFact *numericFact;
    SZrFileRange cachedRange;
    SZrSemanticAnalysisMetrics metrics;
    SZrSemanticAnalysisMetrics ownerMetrics;
    TZrChar reason[512];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
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
        TEST_FAIL(timer, summary, "Failed to prepare unaffected scope fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    initialAst = fileVersion != ZR_NULL ? fileVersion->ast : ZR_NULL;
    secondFunction = local_scope_test_function_at(initialAst, 1);
    cachedExpression = local_scope_test_return_expression(secondFunction);
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (fileVersion == ZR_NULL || owner == ZR_NULL || secondFunction == ZR_NULL ||
        cachedExpression == ZR_NULL ||
        scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                initialAst,
                secondFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                initialAst,
                secondFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to prime unaffected scope cache");
        return;
    }
    cachedContext = scopedAnalyzer->semanticContext;
    cachedRange = scopedAnalyzer->cache->cacheRange;

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply body-local edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    secondFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1)
                             : ZR_NULL;
    if (fileVersion == ZR_NULL || fileVersion->ast == initialAst ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        owner->scopedQueryAnalyzer != scopedAnalyzer || secondFunction == ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Unaffected scope cache was discarded: astChanged=%d impact=%d ownerCache=%p expected=%p",
                 fileVersion != ZR_NULL && fileVersion->ast != initialAst,
                 fileVersion != ZR_NULL ? (int)fileVersion->lastChangeInfo.impact : -1,
                 (void *)owner->scopedQueryAnalyzer,
                 (void *)scopedAnalyzer);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    secondExpression = local_scope_test_return_expression(secondFunction);
    if (secondExpression == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                secondFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to query preserved unaffected scope");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &metrics);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(
            scopedAnalyzer->semanticContext,
            cachedExpression);
    if (scopedAnalyzer->semanticContext != cachedContext ||
        metrics.requestCount != 3 || metrics.executionCount != 1 ||
        metrics.cacheHitCount != 2 ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        ownerMetrics.scopedCacheInvalidationCount != 0 ||
        numericFact == ZR_NULL ||
        !numericFact->hasRange || numericFact->minValue != 30 ||
        numericFact->maxValue != 30) {
        snprintf(reason,
                 sizeof(reason),
                 "Preserved scope cache mismatch: contextSame=%d requests=%zu executions=%zu hits=%zu preserved=%zu invalidated=%zu numeric=%p",
                 scopedAnalyzer->semanticContext == cachedContext,
                 (size_t)metrics.requestCount,
                 (size_t)metrics.executionCount,
                 (size_t)metrics.cacheHitCount,
                 (size_t)ownerMetrics.scopedCachePreservationCount,
                 (size_t)ownerMetrics.scopedCacheInvalidationCount,
                 (void *)numericFact);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                secondStableContent,
                strlen(secondStableContent),
                3)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply repeated stable body edit");
        return;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    secondFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1)
                             : ZR_NULL;
    if (fileVersion == ZR_NULL || secondFunction == ZR_NULL ||
        owner->scopedQueryAnalyzer != scopedAnalyzer ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                secondFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Repeated stable edit discarded the original scope cache");
        return;
    }
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &metrics);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (scopedAnalyzer->semanticContext != cachedContext ||
        metrics.requestCount != 4 || metrics.executionCount != 1 ||
        metrics.cacheHitCount != 3 ||
        ownerMetrics.scopedCachePreservationCount != 2 ||
        ownerMetrics.scopedCacheInvalidationCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Repeated stable edit did not reuse the original semantic snapshot");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                shiftedContent,
                strlen(shiftedContent),
                4)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply equal-length coordinate shift");
        return;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    currentSecondFunction = fileVersion != ZR_NULL
                                    ? local_scope_test_function_at(
                                          fileVersion->ast,
                                          1)
                                    : ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        owner->scopedQueryAnalyzer != ZR_NULL ||
        ownerMetrics.scopedCachePreservationCount != 2 ||
        ownerMetrics.scopedCacheInvalidationCount != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Coordinate-shifted scope cache was not invalidated: impact=%d cachedLines=%d-%d currentLines=%d-%d cache=%p preserved=%zu invalidated=%zu",
                 fileVersion != ZR_NULL ? (int)fileVersion->lastChangeInfo.impact : -1,
                 cachedRange.start.line,
                 cachedRange.end.line,
                 currentSecondFunction != ZR_NULL
                         ? currentSecondFunction->location.start.line
                         : -1,
                 currentSecondFunction != ZR_NULL
                         ? currentSecondFunction->location.end.line
                         : -1,
                 (void *)owner->scopedQueryAnalyzer,
                 (size_t)ownerMetrics.scopedCachePreservationCount,
                 (size_t)ownerMetrics.scopedCacheInvalidationCount);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_completion_fallback_reuses_scoped_query_analyzer_cache(
        SZrState *state) {
    const TZrChar *summary =
            "Completion Fallback Reuses Scoped Query Analyzer Cache";
    const TZrChar *uriText = "file:///completion_scope_cache.zr";
    const TZrChar *content = "\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSymbolTable *originalSymbolTable;
    SZrSymbolTable *emptySymbolTable;
    SZrSemanticAnalysisMetrics metrics;
    SZrLspPosition position;
    SZrArray completions;
    TZrSize requestIndex;
    TZrChar reason[256];

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
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
        TEST_FAIL(timer, summary, "Failed to prepare empty completion fixture");
        return;
    }

    position.line = 0;
    position.character = 0;
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    emptySymbolTable = ZrLanguageServer_SymbolTable_New(state);
    if (owner == ZR_NULL || emptySymbolTable == ZR_NULL) {
        if (emptySymbolTable != ZR_NULL) {
            ZrLanguageServer_SymbolTable_Free(state, emptySymbolTable);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to isolate the primary completion provider");
        return;
    }
    originalSymbolTable = owner->symbolTable;
    owner->symbolTable = emptySymbolTable;

    for (requestIndex = 0; requestIndex < 2; requestIndex++) {
        ZrCore_Array_Init(
                state,
                &completions,
                sizeof(SZrLspCompletionItem *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        if (!ZrLanguageServer_Lsp_GetCompletion(
                    state,
                    context,
                    uri,
                    position,
                    &completions)) {
            ZrCore_Array_Free(state, &completions);
            owner->symbolTable = originalSymbolTable;
            ZrLanguageServer_SymbolTable_Free(state, emptySymbolTable);
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer, summary, "Completion fallback request failed");
            return;
        }
        ZrCore_Array_Free(state, &completions);
    }

    scopedAnalyzer = owner->scopedQueryAnalyzer;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &metrics);
    if (scopedAnalyzer == ZR_NULL || metrics.requestCount != 2 ||
        metrics.executionCount != 1 || metrics.cacheHitCount != 1) {
        snprintf(reason,
                 sizeof(reason),
                 "Repeated fallback cache mismatch: owner=%p scoped=%p requests=%zu executions=%zu hits=%zu",
                 (void *)owner,
                 (void *)scopedAnalyzer,
                 (size_t)metrics.requestCount,
                 (size_t)metrics.executionCount,
                 (size_t)metrics.cacheHitCount);
        owner->symbolTable = originalSymbolTable;
        ZrLanguageServer_SymbolTable_Free(state, emptySymbolTable);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    owner->symbolTable = originalSymbolTable;
    ZrLanguageServer_SymbolTable_Free(state, emptySymbolTable);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif // ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SCOPE_CASES_H
