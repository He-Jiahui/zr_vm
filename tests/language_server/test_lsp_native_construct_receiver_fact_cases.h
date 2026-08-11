static void test_lsp_native_construct_receiver_fails_closed_without_expression_fact(
        SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition receiverEndPosition;
    SZrLspPosition memberPosition;
    SZrFilePosition receiverEndFilePosition;
    SZrFileRange receiverEndRange;
    SZrAstNode *receiverExpressionNode;
    SZrSemanticExpressionFact *receiverFact = ZR_NULL;
    SZrLspSemanticQuery query;
    const TZrChar *content =
        "var math = import(\"zr.math\");\n"
        "fn runImpl() {\n"
        "    return init math.Vector3(4.0, 5.0, 6.0).y;\n"
        "}\n";
    const TZrChar *restoredContent =
        "var math = import(\"zr.math\");\n"
        "fn runImpl() {\n"
        "    return init math.Vector3(4.0, 5.0, 6.0) .y;\n"
        "}\n";

    TEST_START("LSP Native Construct Receiver Fails Closed Without Expression Fact");
    TEST_INFO("Native construct receiver canonical fact",
              "A native member receiver constructed in source must not be re-inferred from its AST after its exact expression fact is unavailable.");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///native_construct_receiver_fact.zr",
                               strlen("file:///native_construct_receiver_fact.zr"));
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, ").y", 0, 0, &receiverEndPosition) ||
        !lsp_find_position_for_substring(content, ").y", 0, 2, &memberPosition) ||
        (analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri)) == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Receiver Fails Closed Without Expression Fact",
                  "Failed to prepare the native construct receiver fixture");
        return;
    }

    receiverEndFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, receiverEndPosition);
    receiverEndRange = ZrParser_FileRange_Create(receiverEndFilePosition,
                                                  receiverEndFilePosition,
                                                  uri);
    receiverExpressionNode = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
            analyzer->ast, receiverEndRange);
    if (receiverExpressionNode != ZR_NULL &&
        receiverExpressionNode->type == ZR_AST_PRIMARY_EXPRESSION) {
        receiverExpressionNode = receiverExpressionNode->data.primaryExpression.property;
    }
    receiverFact = (SZrSemanticExpressionFact *)ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext, receiverExpressionNode);
    if (receiverExpressionNode == ZR_NULL ||
        (receiverExpressionNode->type != ZR_AST_CONSTRUCT_EXPRESSION &&
         receiverExpressionNode->type != ZR_AST_STRUCT_INIT_EXPRESSION) ||
        receiverFact == ZR_NULL ||
        receiverFact->exactness != ZR_SEMANTIC_FACT_EXACT ||
        receiverFact->typeId == ZR_SEMANTIC_ID_INVALID) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Receiver Fails Closed Without Expression Fact",
                  "The native construct receiver must publish an exact canonical expression fact before projection");
        return;
    }

    receiverFact->exactness = ZR_SEMANTIC_FACT_UNKNOWN;

    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
            state, context, uri, memberPosition, &query)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Receiver Fails Closed Without Expression Fact",
                  "Native member resolution recovered Vector3 from AST/type text after its canonical expression fact became unavailable");
        return;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state,
                                              context,
                                              uri,
                                              restoredContent,
                                              strlen(restoredContent),
                                              2) ||
        !lsp_find_position_for_substring(restoredContent, ") .y", 0, 0, &receiverEndPosition) ||
        (analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri)) == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Receiver Fails Closed Without Expression Fact",
                  "Failed to restore the native construct receiver semantic fact for the invalid TypeId boundary");
        return;
    }

    receiverEndFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, receiverEndPosition);
    receiverEndRange = ZrParser_FileRange_Create(receiverEndFilePosition,
                                                  receiverEndFilePosition,
                                                  uri);
    receiverExpressionNode = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
            analyzer->ast, receiverEndRange);
    if (receiverExpressionNode != ZR_NULL &&
        receiverExpressionNode->type == ZR_AST_PRIMARY_EXPRESSION) {
        receiverExpressionNode = receiverExpressionNode->data.primaryExpression.property;
    }
    receiverFact = (SZrSemanticExpressionFact *)ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext, receiverExpressionNode);
    if (receiverFact == ZR_NULL ||
        receiverFact->exactness != ZR_SEMANTIC_FACT_EXACT ||
        receiverFact->typeId == ZR_SEMANTIC_ID_INVALID) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Receiver Fails Closed Without Expression Fact",
                  "The restored native construct receiver must republish an exact canonical expression fact");
        return;
    }

    receiverFact->typeId = ZR_SEMANTIC_ID_INVALID;
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
            state, context, uri, memberPosition, &query)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Receiver Fails Closed Without Expression Fact",
                  "Native member resolution accepted an exact receiver fact with an invalid canonical TypeId");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Construct Receiver Fails Closed Without Expression Fact");
}

static void test_lsp_native_construct_member_chain_fails_closed_without_expression_fact(
        SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrSemanticAnalyzer *analyzer;
    SZrLspPosition receiverEndPosition;
    SZrLspPosition memberPosition;
    SZrFilePosition receiverEndFilePosition;
    SZrFileRange receiverEndRange;
    SZrAstNode *primaryNode;
    SZrAstNode *receiverExpressionNode;
    SZrSemanticExpressionFact *receiverFact;
    SZrLspSemanticQuery query;
    const TZrChar *content =
        "var math = import(\"zr.math\");\n"
        "fn runImpl() {\n"
        "    return init math.Vector3(4.0, 5.0, 6.0).normalized.y;\n"
        "}\n";

    TEST_START("LSP Native Construct Member Chain Fails Closed Without Expression Fact");
    TEST_INFO("Native construct member-chain canonical fact",
              "A member-chain receiver derived from a native construct must not fall back to the preceding member text when its exact expression fact is unavailable.");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///native_construct_receiver_chain_fact.zr",
                               strlen("file:///native_construct_receiver_chain_fact.zr"));
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, ".y", 0, 0, &receiverEndPosition) ||
        !lsp_find_position_for_substring(content, ".y", 0, 1, &memberPosition) ||
        (analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri)) == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Member Chain Fails Closed Without Expression Fact",
                  "Failed to prepare the native construct member-chain fixture");
        return;
    }

    receiverEndFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, receiverEndPosition);
    receiverEndRange = ZrParser_FileRange_Create(receiverEndFilePosition,
                                                  receiverEndFilePosition,
                                                  uri);
    primaryNode = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
            analyzer->ast, receiverEndRange);
    if (primaryNode == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Member Chain Fails Closed Without Expression Fact",
                  "Expected an expression node for the receiver preceding the native x field");
        return;
    }

    receiverExpressionNode = primaryNode;
    if (primaryNode->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (primaryNode->data.primaryExpression.members == ZR_NULL ||
            primaryNode->data.primaryExpression.members->count < 2u) {
            ZrLanguageServer_LspSemanticQuery_Free(state, &query);
            ZrLanguageServer_LspContext_Free(state, context);
            TEST_FAIL(timer,
                      "LSP Native Construct Member Chain Fails Closed Without Expression Fact",
                      "Expected a primary member chain ending in the native x field");
            return;
        }
        receiverExpressionNode = primaryNode->data.primaryExpression.members->nodes[
                primaryNode->data.primaryExpression.members->count - 2u];
    }
    receiverFact = (SZrSemanticExpressionFact *)ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext, receiverExpressionNode);
    if (receiverFact != ZR_NULL ||
        ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Construct Member Chain Fails Closed Without Expression Fact",
                  "Native member-chain resolution accepted a receiver without an exact expression fact");
        return;
    }

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Construct Member Chain Fails Closed Without Expression Fact");
}
