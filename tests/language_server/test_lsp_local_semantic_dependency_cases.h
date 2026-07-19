#ifndef ZR_VM_TEST_LSP_LOCAL_SEMANTIC_DEPENDENCY_CASES_H
#define ZR_VM_TEST_LSP_LOCAL_SEMANTIC_DEPENDENCY_CASES_H

static TZrBool local_dependency_ranges_equal(const SZrFileRange *left,
                                              const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset == right->start.offset &&
           left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.offset == right->end.offset &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static TZrBool local_dependency_has_resolved_call(
        const SZrSemanticContext *semanticContext,
        const SZrFileRange *declarationRange) {
    TZrSize index;

    if (semanticContext == ZR_NULL || declarationRange == ZR_NULL ||
        !semanticContext->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < semanticContext->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&semanticContext->referenceFacts,
                        index);
        if (fact != ZR_NULL && fact->isResolved &&
            fact->kind == ZR_SEMANTIC_REFERENCE_CALL &&
            local_dependency_ranges_equal(
                    &fact->declarationRange,
                    declarationRange)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool local_dependency_has_closure_identifier(
        const SZrSemanticContext *semanticContext) {
    TZrSize index;

    if (semanticContext == ZR_NULL ||
        !semanticContext->expressionFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < semanticContext->expressionFacts.length; index++) {
        const SZrSemanticExpressionFact *fact =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        (SZrArray *)&semanticContext->expressionFacts,
                        index);
        if (fact != ZR_NULL && fact->node != ZR_NULL &&
            fact->node->type == ZR_AST_IDENTIFIER_LITERAL &&
            fact->inferredType.baseType == ZR_VALUE_TYPE_CLOSURE) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool local_dependency_has_resolved_reference(
        const SZrSemanticContext *semanticContext,
        const SZrFileRange *declarationRange) {
    TZrSize index;

    if (semanticContext == ZR_NULL || declarationRange == ZR_NULL ||
        !semanticContext->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < semanticContext->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&semanticContext->referenceFacts,
                        index);
        if (fact != ZR_NULL && fact->isResolved &&
            local_dependency_ranges_equal(
                    &fact->declarationRange,
                    declarationRange)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_generic_signature_edit_invalidates_only_changed_and_direct_caller_scopes(
        SZrState *state) {
    const TZrChar *summary =
            "Generic Signature Edit Invalidates Only Changed And Direct Caller Scopes";
    const TZrChar *uriText = "file:///generic_signature_direct_caller_cache.zr";
    const TZrChar *initialContent =
            "identity<T>(value: T): T {\n"
            "    return value;\n"
            "}\n"
            "caller(): int {\n"
            "    return identity<int>(1);\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *firstSignatureContent =
            "identity<U>(value: U): U {\n"
            "    return value;\n"
            "}\n"
            "caller(): int {\n"
            "    return identity<int>(1);\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *secondSignatureContent =
            "identity<V>(value: V): V {\n"
            "    return value;\n"
            "}\n"
            "caller(): int {\n"
            "    return identity<int>(1);\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *thirdSignatureContent =
            "identity<W>(value: W): W {\n"
            "    return value;\n"
            "}\n"
            "caller(): int {\n"
            "    return identity<int>(1);\n"
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
    SZrSemanticAnalysisMetrics scopedMetrics;
    SZrAstNode *identityFunction;
    SZrAstNode *callerFunction;
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
        TEST_FAIL(timer, summary, "Failed to prepare generic direct-caller fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    unrelatedFunction = fileVersion != ZR_NULL
                                ? local_scope_test_function_at(fileVersion->ast, 2)
                                : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || unrelatedFunction == ZR_NULL || scopedAnalyzer == ZR_NULL ||
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
        TEST_FAIL(timer, summary, "Failed to cache the unrelated function scope");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                firstSignatureContent,
                strlen(firstSignatureContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply the first generic signature edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    unrelatedFunction = fileVersion != ZR_NULL
                                ? local_scope_test_function_at(fileVersion->ast, 2)
                                : ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE ||
        owner->scopedQueryAnalyzer != scopedAnalyzer ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        ownerMetrics.scopedCacheInvalidationCount != 0 ||
        unrelatedFunction == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Generic signature edit did not preserve the unrelated scope cache");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &scopedMetrics);
    if (scopedMetrics.requestCount != 3 || scopedMetrics.executionCount != 1 ||
        scopedMetrics.cacheHitCount != 2) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Unrelated scope was recomputed after a stable generic signature edit");
        return;
    }

    identityFunction = local_scope_test_function_at(fileVersion->ast, 0);
    callerFunction = local_scope_test_function_at(fileVersion->ast, 1);
    if (identityFunction == ZR_NULL || callerFunction == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                callerFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                callerFunction) ||
        !local_dependency_has_resolved_call(
                scopedAnalyzer->semanticContext,
                &identityFunction->data.functionDeclaration.nameLocation)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Caller scope did not expose a resolved dependency on identity");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                secondSignatureContent,
                strlen(secondSignatureContent),
                3)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply the second generic signature edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE ||
        owner->scopedQueryAnalyzer != ZR_NULL ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        ownerMetrics.scopedCacheInvalidationCount != 1 ||
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 1 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Generic signature edit did not invalidate its direct caller cache");
        return;
    }

    callerFunction = local_scope_test_function_at(fileVersion->ast, 1);
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (callerFunction == ZR_NULL || scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                callerFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to recompute the invalidated caller scope");
        return;
    }
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &scopedMetrics);
    if (scopedMetrics.requestCount != 1 || scopedMetrics.executionCount != 1 ||
        scopedMetrics.cacheHitCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Direct caller reused stale scoped semantic work");
        return;
    }

    identityFunction = local_scope_test_function_at(fileVersion->ast, 0);
    if (identityFunction == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                identityFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                identityFunction) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                thirdSignatureContent,
                strlen(thirdSignatureContent),
                4)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache or edit the changed declaration scope");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE ||
        owner->scopedQueryAnalyzer != ZR_NULL ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        ownerMetrics.scopedCacheInvalidationCount != 2) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Changed declaration scope survived its own generic signature edit");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_inferred_signature_body_edit_invalidates_direct_caller_scope(
        SZrState *state) {
    const TZrChar *summary =
            "Inferred Signature Body Edit Invalidates Direct Caller Scope";
    const TZrChar *uriText = "file:///inferred_signature_direct_caller_cache.zr";
    const TZrChar *initialContent =
            "inferred() {\n"
            "    return 1.0;\n"
            "}\n"
            "caller() {\n"
            "    return inferred();\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *changedTypeContent =
            "inferred() {\n"
            "    return \"a\";\n"
            "}\n"
            "caller() {\n"
            "    return inferred();\n"
            "}\n"
            "unrelated(): int {\n"
            "    return 10 + 20;\n"
            "}\n";
    const TZrChar *stableTypeContent =
            "inferred() {\n"
            "    return \"b\";\n"
            "}\n"
            "caller() {\n"
            "    return inferred();\n"
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
    SZrSemanticAnalysisMetrics scopedMetrics;
    SZrAstNode *inferredFunction;
    SZrAstNode *callerFunction;
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
        TEST_FAIL(timer, summary, "Failed to prepare inferred-signature fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    inferredFunction = fileVersion != ZR_NULL
                               ? local_scope_test_function_at(fileVersion->ast, 0)
                               : ZR_NULL;
    callerFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1)
                             : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || inferredFunction == ZR_NULL || callerFunction == ZR_NULL ||
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
                callerFunction) ||
        !local_dependency_has_resolved_call(
                scopedAnalyzer->semanticContext,
                &inferredFunction->data.functionDeclaration.nameLocation)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache the inferred function's direct caller");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                changedTypeContent,
                strlen(changedTypeContent),
                2)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to apply inferred return-type body edit");
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
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 1 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 0) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Inferred return-type edit preserved a stale direct caller cache");
        return;
    }

    unrelatedFunction = local_scope_test_function_at(fileVersion->ast, 2);
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (unrelatedFunction == ZR_NULL || scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction) ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                stableTypeContent,
                strlen(stableTypeContent),
                3)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache unrelated scope or apply stable inferred edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    unrelatedFunction = fileVersion != ZR_NULL
                                ? local_scope_test_function_at(fileVersion->ast, 2)
                                : ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        owner->scopedQueryAnalyzer != scopedAnalyzer ||
        ownerMetrics.scopedCacheInvalidationCount != 1 ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        unrelatedFunction == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                unrelatedFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Inferred body edit did not preserve an unrelated scope cache");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &scopedMetrics);
    if (scopedMetrics.requestCount != 3 || scopedMetrics.executionCount != 1 ||
        scopedMetrics.cacheHitCount != 2) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Unrelated scope recomputed after an inferred body edit");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_explicit_signature_body_edit_preserves_direct_caller_scope(
        SZrState *state) {
    const TZrChar *summary =
            "Explicit Signature Body Edit Preserves Direct Caller Scope";
    const TZrChar *uriText = "file:///explicit_signature_direct_caller_cache.zr";
    const TZrChar *initialContent =
            "answer(): int {\n"
            "    return 1;\n"
            "}\n"
            "caller(): int {\n"
            "    return answer();\n"
            "}\n";
    const TZrChar *updatedContent =
            "answer(): int {\n"
            "    return 2;\n"
            "}\n"
            "caller(): int {\n"
            "    return answer();\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics ownerMetrics;
    SZrSemanticAnalysisMetrics scopedMetrics;
    SZrAstNode *answerFunction;
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
                1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare explicit-signature fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    answerFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 0)
                             : ZR_NULL;
    callerFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1)
                             : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || answerFunction == ZR_NULL || callerFunction == ZR_NULL ||
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
                callerFunction) ||
        !local_dependency_has_resolved_call(
                scopedAnalyzer->semanticContext,
                &answerFunction->data.functionDeclaration.nameLocation)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache the explicit function's direct caller");
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
        TEST_FAIL(timer, summary, "Failed to apply explicit return-type body edit");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    callerFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1)
                             : ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_GetMetrics(owner, &ownerMetrics);
    if (fileVersion == ZR_NULL ||
        fileVersion->lastChangeInfo.impact !=
                ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY ||
        owner->scopedQueryAnalyzer != scopedAnalyzer ||
        ownerMetrics.scopedCacheInvalidationCount != 0 ||
        ownerMetrics.scopedCachePreservationCount != 1 ||
        callerFunction == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                callerFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Explicit-signature body edit discarded a stable caller cache");
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_GetMetrics(scopedAnalyzer, &scopedMetrics);
    if (scopedMetrics.requestCount != 3 || scopedMetrics.executionCount != 1 ||
        scopedMetrics.cacheHitCount != 2) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Stable direct caller was recomputed after a body-only edit");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_resolved_function_value_dependency_invalidates_directly(
        SZrState *state) {
    const TZrChar *summary =
            "Resolved Function Value Dependency Invalidates Directly";
    const TZrChar *uriText = "file:///resolved_function_value_dependency_cache.zr";
    const TZrChar *initialContent =
            "identity<T>(value: T): T {\n"
            "    return value;\n"
            "}\n"
            "holder() {\n"
            "    return identity;\n"
            "}\n";
    const TZrChar *updatedContent =
            "identity<U>(value: U): U {\n"
            "    return value;\n"
            "}\n"
            "holder() {\n"
            "    return identity;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics ownerMetrics;
    SZrAstNode *identityFunction;
    SZrAstNode *holderFunction;
    TZrChar metricDetail[192];

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
        TEST_FAIL(timer, summary, "Failed to prepare function-value dependency fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    identityFunction = fileVersion != ZR_NULL
                               ? local_scope_test_function_at(fileVersion->ast, 0)
                               : ZR_NULL;
    holderFunction = fileVersion != ZR_NULL
                             ? local_scope_test_function_at(fileVersion->ast, 1)
                             : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || identityFunction == ZR_NULL ||
        holderFunction == ZR_NULL || scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                holderFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                holderFunction) ||
        !local_dependency_has_closure_identifier(
                scopedAnalyzer->semanticContext) ||
        !local_dependency_has_resolved_reference(
                scopedAnalyzer->semanticContext,
                &identityFunction->data.functionDeclaration.nameLocation)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Fixture did not expose a resolved function-value edge");
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
        TEST_FAIL(timer, summary, "Failed to apply signature edit beside function value");
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
        ownerMetrics.scopedCacheDirectDependencyInvalidationCount != 1 ||
        ownerMetrics.scopedCacheConservativeInvalidationCount != 0) {
        snprintf(metricDetail,
                 sizeof(metricDetail),
                 "Resolved function-value counters total=%zu preserve=%zu direct=%zu conservative=%zu",
                 (size_t)ownerMetrics.scopedCacheInvalidationCount,
                 (size_t)ownerMetrics.scopedCachePreservationCount,
                 (size_t)ownerMetrics.scopedCacheDirectDependencyInvalidationCount,
                 (size_t)ownerMetrics.scopedCacheConservativeInvalidationCount);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, metricDetail);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_poisoned_scope_invalidates_conservatively_on_signature_edit(
        SZrState *state) {
    const TZrChar *summary =
            "Poisoned Scope Invalidates Conservatively On Signature Edit";
    const TZrChar *uriText = "file:///poisoned_signature_dependency_cache.zr";
    const TZrChar *initialContent =
            "identity<T>(value: T): T {\n"
            "    return value;\n"
            "}\n"
            "poisoned(): int {\n"
            "    return missing();\n"
            "}\n";
    const TZrChar *updatedContent =
            "identity<U>(value: U): U {\n"
            "    return value;\n"
            "}\n"
            "poisoned(): int {\n"
            "    return missing();\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *owner;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics ownerMetrics;
    SZrAstNode *poisonedFunction;
    TZrBool hasUnavailableFacts;

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
        TEST_FAIL(timer, summary, "Failed to prepare poisoned dependency fixture");
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    owner = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    poisonedFunction = fileVersion != ZR_NULL
                               ? local_scope_test_function_at(fileVersion->ast, 1)
                               : ZR_NULL;
    scopedAnalyzer = ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
            state,
            owner);
    if (owner == ZR_NULL || poisonedFunction == ZR_NULL || scopedAnalyzer == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                poisonedFunction) ||
        !ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                state,
                scopedAnalyzer,
                fileVersion->ast,
                poisonedFunction)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Failed to cache the poisoned caller scope");
        return;
    }

    hasUnavailableFacts = scopedAnalyzer->diagnostics.length > 0 ||
                          (scopedAnalyzer->semanticContext != ZR_NULL &&
                           scopedAnalyzer->semanticContext->queryDiagnostics.isValid &&
                           scopedAnalyzer->semanticContext->queryDiagnostics.length > 0);
    if (!hasUnavailableFacts) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Fixture did not expose poisoned or unavailable semantic facts");
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
        TEST_FAIL(timer, summary, "Failed to apply signature edit beside poisoned scope");
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
        TEST_FAIL(timer, summary, "Poisoned dependency scope was reused without a proven edge set");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
