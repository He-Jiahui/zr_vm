#ifndef ZR_VM_TEST_LSP_SIGNATURE_SNAPSHOT_FACT_CASES_H
#define ZR_VM_TEST_LSP_SIGNATURE_SNAPSHOT_FACT_CASES_H

static void test_signature_does_not_materialize_missing_snapshot_facts(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Signature Does Not Materialize Missing Snapshot Facts";
    const TZrChar *uriText = "file:///signature_snapshot_facts.zr";
    const TZrChar *content =
            "fn pick(value: int): int {\n"
            "    return value;\n"
            "}\n"
            "fn calc(): int {\n"
            "    return pick(1 + 2);\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition position;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrSemanticExpressionFact *expressionFact;
    SZrAstNode *savedFactNode;
    TZrSize factCountBefore;
    TZrSize factCountAfter;
    SZrLspSignatureHelp *help = ZR_NULL;
    TZrBool requestSucceeded;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "1 + 2", 0U, 2, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare signature snapshot fixture");
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    expressionFact = analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL
            ? (SZrSemanticExpressionFact *)ZrParser_SemanticFacts_FindExpressionAtPosition(
                    analyzer->semanticContext, fileRange)
            : ZR_NULL;
    if (expressionFact == ZR_NULL || expressionFact->node == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Expected compiler-published argument expression fact");
        return;
    }

    factCountBefore = analyzer->semanticContext->expressionFacts.length;
    savedFactNode = expressionFact->node;
    expressionFact->node = ZR_NULL;
    requestSucceeded = ZrLanguageServer_Lsp_GetSignatureHelp(
            state, context, uri, position, &help);
    factCountAfter = analyzer->semanticContext->expressionFacts.length;
    expressionFact->node = savedFactNode;

    if (!requestSucceeded || help == ZR_NULL || factCountAfter != factCountBefore) {
        snprintf(reason,
                 sizeof(reason),
                 "Signature must consume an immutable snapshot; success=%d help=%p facts=%llu->%llu",
                 requestSucceeded,
                 (void *)help,
                 (unsigned long long)factCountBefore,
                 (unsigned long long)factCountAfter);
        if (help != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, help);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

#endif
