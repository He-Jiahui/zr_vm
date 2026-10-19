#ifndef ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SCOPE_CASES_H
#define ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SCOPE_CASES_H

#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

/* 从文档 AST 按序取得顶层节点，供缓存测试绑定作用域；调用者须另核对函数类型，指针由文件版本持有。 */
static SZrAstNode *local_scope_test_function_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->nodes == ZR_NULL ||
        index >= ast->data.script.statements->count) {
        return ZR_NULL;
    }
    return ast->data.script.statements->nodes[index];
}

/* 只取 fixture 函数首个 return 的表达式节点，供事实范围与作用域边界比较。 */
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

/* 按 AST 节点身份检查符号是否属于当前分析快照，避免只靠同名文本误判缓存复用。 */
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

/* 非 callable 的 property/编译期包装不能成为局部重算根，防止把容器误当独立执行作用域。 */
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

/* 分析一个函数体时应保留全局声明却只产出该函数的局部事实；重复请求命中同一作用域缓存。 */
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

/* token 等价更新保留作用域快照，真实编辑必须失效；指标区分请求、执行和缓存命中。 */
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

/* 相邻函数体的等长修改不应丢弃无关作用域，坐标偏移改变时则必须使旧事实失效。 */
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

static void local_scope_test_free_protocol_completions(
        SZrState *state, SZrArray *items) {
    if (items == ZR_NULL || !items->isValid) {
        return;
    }
    for (TZrSize index = 0U; index < items->length; index++) {
        SZrLspCompletionItem **item =
                (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (item != ZR_NULL && *item != ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, *item, sizeof(SZrLspCompletionItem));
        }
    }
    ZrCore_Array_Free(state, items);
}

static void test_completion_reads_canonical_snapshot_twice(SZrState *state) {
    const char *summary = "Completion Reads Canonical Snapshot Twice";
    const char *uriText = "file:///completion_canonical_snapshot.zr";
    const char *content =
            "fn make(seed: int): int {\n"
            "    return seed + 0;\n"
            "}\n"
            "fn main(): void {\n"
            "    var canonicalLocal: int = 1;\n"
            "    make(1);\n"
            "}\n";
    const char *failure = "Fixture prerequisite: canonical completion setup failed";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrSemanticAnalyzer *owner = ZR_NULL;
    SZrSemanticAnalyzer *currentOwner;
    SZrSemanticContext *semanticContext = ZR_NULL;
    SZrFileVersion *version = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrLspPosition position;
    SZrFilePosition filePosition;
    SZrFileRange queryRange;
    SZrParserSemanticVisibleSymbolOptions options = {0};
    SZrArray visibleSymbols = {0};
    SZrArray completions = {0};
    TZrSymbolId expectedSymbolId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId expectedTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrSize symbolCount = 0U;
    TZrSize expressionCount = 0U;
    TZrSize referenceCount = 0U;
    TZrBool passed = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !lsp_find_position_for_substring(content, "make(1)", 0U, 0, &position)) {
        goto cleanup;
    }
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    version = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (owner == ZR_NULL || owner->semanticContext == ZR_NULL ||
        version == ZR_NULL || version->version != 1U ||
        version->usesFallbackAst || version->isDesynchronized ||
        version->ast == ZR_NULL || version->ast->ownership == ZR_NULL) {
        failure = "Fixture prerequisite: current owned semantic snapshot is required";
        goto cleanup;
    }
    semanticContext = owner->semanticContext;
    ast = version->ast;
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    queryRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    ZrCore_Array_Construct(&visibleSymbols);
    if (!ZrParser_SemanticQuery_VisibleSymbols(
                semanticContext, queryRange, ZR_NULL, &options, &visibleSymbols)) {
        failure = "Fixture prerequisite: canonical visible-symbol query failed";
        goto cleanup;
    }
    for (TZrSize index = 0U; index < visibleSymbols.length; index++) {
        const SZrParserSemanticSymbolQuery *symbol =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        &visibleSymbols, index);
        const char *name = symbol != ZR_NULL && symbol->displayName != ZR_NULL
                ? ZrCore_String_GetNativeString(symbol->displayName) : ZR_NULL;
        if (name != ZR_NULL && strcmp(name, "canonicalLocal") == 0 &&
            symbol->declarationNode != ZR_NULL &&
            symbol->declarationNode->ownership == ast->ownership &&
            symbol->declarationRange.source != ZR_NULL) {
            expectedSymbolId = symbol->symbolId;
            expectedTypeId = symbol->typeId;
            break;
        }
    }
    ZrCore_Array_Free(state, &visibleSymbols);
    if (expectedSymbolId == ZR_SEMANTIC_ID_INVALID ||
        expectedTypeId == ZR_SEMANTIC_ID_INVALID) {
        failure = "Fixture prerequisite: real typed local must have canonical visible identity";
        goto cleanup;
    }
    symbolCount = semanticContext->symbols.length;
    expressionCount = semanticContext->expressionFacts.length;
    referenceCount = semanticContext->referenceFacts.length;

    for (TZrSize request = 0U; request < 2U; request++) {
        TZrBool found = ZR_FALSE;

        ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8U);
        if (!completions.isValid ||
            !ZrLanguageServer_Lsp_GetCompletion(
                    state, context, uri, position, &completions)) {
            failure = "Canonical completion request failed";
            goto cleanup;
        }
        for (TZrSize index = 0U; index < completions.length; index++) {
            SZrLspCompletionItem **item =
                    (SZrLspCompletionItem **)ZrCore_Array_Get(&completions, index);
            if (item != ZR_NULL && *item != ZR_NULL && (*item)->label != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString((*item)->label), "canonicalLocal") == 0) {
                found = (*item)->kind == ZR_LSP_COMPLETION_ITEM_KIND_VARIABLE && (*item)->detail != ZR_NULL &&
                        strncmp(ZrCore_String_GetNativeString((*item)->detail),
                                "int\nSemantic facts: ",
                                sizeof("int\nSemantic facts: ") - 1U) == 0;
                if (!found) {
                    fprintf(stderr, "Completion request %zu: label=%s kind=%d detail=%s\n",
                            (size_t)request, ZrCore_String_GetNativeString((*item)->label),
                            (int)(*item)->kind, (*item)->detail != ZR_NULL
                                    ? ZrCore_String_GetNativeString((*item)->detail) : "<null>");
                }
                break;
            }
        }
        local_scope_test_free_protocol_completions(state, &completions);
        if (!found) {
            fprintf(stderr, "Completion request %zu did not match the canonical typed local\n",
                    (size_t)request);
            failure = "Repeated completion must return the canonical typed local";
            goto cleanup;
        }
    }

    currentOwner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    version = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (currentOwner == owner && currentOwner->semanticContext == semanticContext &&
        version != ZR_NULL && version->version == 1U && version->ast == ast &&
        semanticContext->symbols.length == symbolCount &&
        semanticContext->expressionFacts.length == expressionCount &&
        semanticContext->referenceFacts.length == referenceCount) {
        const SZrSemanticSymbolRecord *symbol =
                ZrParser_Semantic_FindSymbolById(semanticContext, expectedSymbolId);
        passed = symbol != ZR_NULL && symbol->typeId == expectedTypeId;
    }
    failure = "Repeated completion must consume the same canonical snapshot";

cleanup:
    local_scope_test_free_protocol_completions(state, &completions);
    if (visibleSymbols.isValid) {
        ZrCore_Array_Free(state, &visibleSymbols);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (passed) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}



/* Completion must use canonical facts even when the legacy symbol table is empty. */
static void test_completion_reuses_published_snapshot_without_reanalysis(
        SZrState *state) {
    const TZrChar *summary =
            "Completion Reuses Published Snapshot Without Reanalysis";
    const TZrChar *uriText = "file:///completion_scope_cache.zr";
    const TZrChar *content = "fn offer(): int { return 7; }\n\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri;
    SZrSemanticAnalyzer *owner;
    SZrSemanticContext *semanticContext;
    SZrSemanticAnalysisMetrics before;
    TZrSize referenceCount;
    TZrSize expressionCount;
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
        TEST_FAIL(timer, summary, "Failed to prepare snapshot completion fixture");
        return;
    }

    position.line = 1;
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
    /* An empty legacy table must still use the published canonical snapshot. */
    originalSymbolTable = owner->symbolTable;
    owner->symbolTable = emptySymbolTable;

    semanticContext = owner->semanticContext;
    referenceCount = semanticContext->referenceFacts.length;
    expressionCount = semanticContext->expressionFacts.length;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &before);

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
        TZrBool found = ZR_FALSE;
        for (TZrSize index = 0; index < completions.length; index++) {
            SZrLspCompletionItem *item = *(SZrLspCompletionItem **)ZrCore_Array_Get(&completions, index);
            if (item->label != ZR_NULL && ZrCore_String_GetByteLength(item->label) == 5 &&
                memcmp(ZrCore_String_GetNativeString(item->label), "offer", 5) == 0) found = ZR_TRUE;
            ZrCore_Memory_RawFree(state->global, item, sizeof(*item));
        }
        ZrCore_Array_Free(state, &completions);
        if (!found) {
            owner->symbolTable = originalSymbolTable;
            ZrLanguageServer_SymbolTable_Free(state, emptySymbolTable);
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer, summary, "Published function is missing from snapshot completion");
            return;
        }
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &metrics);
    if (owner->scopedQueryAnalyzer != ZR_NULL || owner->semanticContext != semanticContext ||
        metrics.executionCount != before.executionCount ||
        semanticContext->referenceFacts.length != referenceCount ||
        semanticContext->expressionFacts.length != expressionCount) {
        snprintf(reason,
                 sizeof(reason),
                 "Snapshot completion changed analysis: owner=%p scoped=%p requests=%zu executions=%zu hits=%zu",
                 (void *)owner,
                 (void *)owner->scopedQueryAnalyzer,
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
