#ifndef ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SNAPSHOT_CASES_H
#define ZR_VM_TEST_LSP_LOCAL_SEMANTIC_SNAPSHOT_CASES_H

static void test_local_expression_query_does_not_materialize_snapshot_facts(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Local Expression Query Does Not Materialize Snapshot Facts";
    const TZrChar *uriText = "file:///local_query_snapshot_facts.zr";
    const TZrChar *content =
            "fn calc(): int {\n"
            "    return 1 + 2;\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition position;
    SZrFilePosition filePosition;
    SZrFileRange queryRange;
    SZrSemanticExpressionFact *expressionFact;
    SZrAstNode *savedNode;
    SZrFileRange savedRange;
    TZrSize factCountBefore;
    TZrSize factCountAfter;
    SZrLspLocalSemanticQueryResult query;
    TZrBool querySucceeded;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "+", 0U, 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare local query snapshot fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    queryRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    expressionFact = analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
            ? (SZrSemanticExpressionFact *)ZrParser_SemanticFacts_FindExpressionAtPosition(
                    analyzer->semanticContext, queryRange)
            : ZR_NULL;
    if (expressionFact == ZR_NULL || expressionFact->node == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected compiler-published expression fact");
        return;
    }

    factCountBefore = analyzer->semanticContext->expressionFacts.length;
    savedNode = expressionFact->node;
    savedRange = expressionFact->range;
    expressionFact->node = ZR_NULL;
    expressionFact->range.start.offset += 100000U;
    expressionFact->range.end.offset += 100000U;
    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    querySucceeded = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(
            state, context, uri, position, &query);
    factCountAfter = analyzer->semanticContext->expressionFacts.length;
    expressionFact->node = savedNode;
    expressionFact->range = savedRange;

    if (!querySucceeded || factCountAfter != factCountBefore) {
        snprintf(reason,
                 sizeof(reason),
                 "Local query must consume an immutable snapshot; success=%d facts=%llu->%llu",
                 querySucceeded,
                 (unsigned long long)factCountBefore,
                 (unsigned long long)factCountAfter);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
