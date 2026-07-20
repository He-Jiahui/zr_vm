#ifndef ZR_VM_TEST_LSP_LOCAL_SEMANTIC_RECEIVER_DEPENDENCY_CASES_H
#define ZR_VM_TEST_LSP_LOCAL_SEMANTIC_RECEIVER_DEPENDENCY_CASES_H

static void test_receiver_signature_edit_preserves_unrelated_scope_with_target_fact(
        SZrState *state) {
    const TZrChar *summary =
            "Receiver Signature Edit Preserves Unrelated Scope With Target Fact";
    const TZrChar *uriText = "file:///receiver_signature_dependency_cache.zr";
    const TZrChar *initialContent =
            "class Counter {\n"
            "    const fn read(): int { return 1; }\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *updatedContent =
            "class Counter {\n"
            "    fn       read(): int { return 1; }\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics ownerMetrics;
    SZrAstNode *unrelatedFunction;

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
        TEST_FAIL(timer, summary, "Failed to prepare receiver dependency fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    unrelatedFunction = fileVersion != ZR_NULL
                                ? local_scope_test_function_at(fileVersion->ast, 1)
                                : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || unrelatedFunction == ZR_NULL ||
        unrelatedFunction->type != ZR_AST_FUNCTION_DECLARATION ||
        scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache unrelated scope beside receiver declaration");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply receiver-effect signature edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE ||
        owner->scopedQueryAnalyzer == ZR_NULL ||
        ownerMetrics.scopedCacheInvalidationCount != 0 ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 0 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Resolved receiver target did not preserve the unrelated scope");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_inferred_method_body_edit_preserves_unrelated_scope(
        SZrState *state) {
    const TZrChar *summary =
            "Inferred Method Body Edit Preserves Unrelated Scope";
    const TZrChar *uriText = "file:///inferred_method_body_dependency_cache.zr";
    const TZrChar *initialContent =
            "class Counter {\n"
            "    fn inferred() { return 1.0; }\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *updatedContent =
            "class Counter {\n"
            "    fn inferred() { return \"a\"; }\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics ownerMetrics;
    SZrAstNode *unrelatedFunction;

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
        TEST_FAIL(timer, summary, "Failed to prepare inferred-method fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    unrelatedFunction = fileVersion != ZR_NULL
                                ? local_scope_test_function_at(fileVersion->ast, 1)
                                : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || unrelatedFunction == ZR_NULL ||
        unrelatedFunction->type != ZR_AST_FUNCTION_DECLARATION ||
        scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache unrelated scope beside inferred method");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply inferred method body edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        owner->scopedQueryAnalyzer == ZR_NULL ||
        ownerMetrics.scopedCacheInvalidationCount != 0 ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 0 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Resolved method target did not preserve the unrelated scope");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_inferred_method_body_edit_invalidates_resolved_direct_caller(
        SZrState *state) {
    const TZrChar *summary =
            "Inferred Method Body Edit Invalidates Resolved Direct Caller";
    const TZrChar *uriText = "file:///inferred_method_direct_dependency_cache.zr";
    const TZrChar *initialContent =
            "class Counter {\n"
            "    fn inferred() { return 1.0; }\n"
            "}\n"
            "run(counter: Counter): object {\n"
            "    return counter.inferred();\n"
            "}\n";
    const TZrChar *updatedContent =
            "class Counter {\n"
            "    fn inferred() { return \"a\"; }\n"
            "}\n"
            "run(counter: Counter): object {\n"
            "    return counter.inferred();\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics ownerMetrics;
    SZrAstNode *callerFunction;

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
                1U)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare direct receiver dependency fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    callerFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1U)
                             : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || callerFunction == ZR_NULL ||
        callerFunction->type != ZR_AST_FUNCTION_DECLARATION ||
        scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                callerFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                callerFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache the receiver direct caller scope");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2U)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply inferred method body edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        owner->scopedQueryAnalyzer != ZR_NULL ||
        ownerMetrics.scopedCacheInvalidationCount != 1U ||
        ownerMetrics.scopedCachePreservationCount != 0U ||
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 1U ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 0U) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(
                timer,
                summary,
                "Receiver caller was not classified as a resolved direct dependency");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
