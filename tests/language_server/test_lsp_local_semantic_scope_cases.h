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
            "first(): int {\n"
            "    return 1 + 2;\n"
            "}\n"
            "second(): int {\n"
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

#endif // ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SCOPE_CASES_H
