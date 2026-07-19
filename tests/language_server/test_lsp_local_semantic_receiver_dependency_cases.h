#ifndef ZR_VM_TEST_LSP_LOCAL_SEMANTIC_RECEIVER_DEPENDENCY_CASES_H
#define ZR_VM_TEST_LSP_LOCAL_SEMANTIC_RECEIVER_DEPENDENCY_CASES_H

static void test_receiver_signature_edit_invalidates_conservatively_without_target_fact(
        SZrState *state) {
    const TZrChar *summary =
            "Receiver Signature Edit Invalidates Conservatively Without Target Fact";
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
        owner->scopedQueryAnalyzer != ZR_NULL ||
        ownerMetrics.scopedCacheInvalidationCount != 1 ||
        ownerMetrics.scopedCachePreservationCount != 0 ||
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 0 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 1) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Receiver edit bypassed the conservative dependency fallback");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_inferred_method_body_edit_invalidates_conservatively(
        SZrState *state) {
    const TZrChar *summary =
            "Inferred Method Body Edit Invalidates Conservatively";
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
        owner->scopedQueryAnalyzer != ZR_NULL ||
        ownerMetrics.scopedCacheInvalidationCount != 1 ||
        ownerMetrics.scopedCachePreservationCount != 0 ||
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 0 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 1) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Inferred method body bypassed conservative invalidation");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
