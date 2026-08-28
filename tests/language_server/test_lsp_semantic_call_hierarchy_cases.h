#ifndef ZR_TEST_LSP_SEMANTIC_CALL_HIERARCHY_CASES_H
#define ZR_TEST_LSP_SEMANTIC_CALL_HIERARCHY_CASES_H

static void test_local_method_call_hierarchy_uses_canonical_edges(
        SZrState *state) {
    static const TZrChar *content =
            "class Left {\n"
            "    pub fn read(value: int): int { return value; }\n"
            "}\n"
            "class Right {\n"
            "    pub fn read(value: int): int { return value; }\n"
            "}\n"
            "fn run(left: Left, value: int): int {\n"
            "    var first = left.read(value);\n"
            "    return first + left.read(value);\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrString *tamperedRunName = ZR_NULL;
    SZrString *tamperedMethodName = ZR_NULL;
    SZrLspPosition runPosition;
    SZrLspPosition leftReadPosition;
    SZrLspPosition rightReadPosition;
    SZrArray runItems = {0};
    SZrArray leftReadItems = {0};
    SZrArray rightReadItems = {0};
    SZrArray outgoing = {0};
    SZrArray incoming = {0};
    SZrArray unrelatedIncoming = {0};
    SZrLspHierarchyItem *runItem = ZR_NULL;
    SZrLspHierarchyItem *leftReadItem = ZR_NULL;
    SZrLspHierarchyItem *rightReadItem = ZR_NULL;
    SZrLspHierarchyCall *outgoingCall = ZR_NULL;
    SZrLspHierarchyCall *incomingCall = ZR_NULL;
    const TZrChar *failure = "method call hierarchy preparation";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP Local Method Call Hierarchy Uses Canonical Edges");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_local_method_call_hierarchy.zr",
            strlen("file:///semantic_query_local_method_call_hierarchy.zr"));
    tamperedRunName = ZrCore_String_Create(
            state, "read", strlen("read"));
    tamperedMethodName = ZrCore_String_Create(
            state, "run", strlen("run"));
    if (context == ZR_NULL || uri == ZR_NULL || tamperedRunName == ZR_NULL ||
        tamperedMethodName == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "fn run", 0U, 3, &runPosition) ||
        !find_position(content, "pub fn read", 0U, 7, &leftReadPosition) ||
        !find_position(content, "pub fn read", 1U, 7, &rightReadPosition) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(
                state, context, uri, runPosition, &runItems) ||
        runItems.length != 1U ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(
                state, context, uri, leftReadPosition, &leftReadItems) ||
        leftReadItems.length != 1U ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(
                state, context, uri, rightReadPosition, &rightReadItems) ||
        rightReadItems.length != 1U) {
        goto cleanup;
    }
    runItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(&runItems, 0U);
    leftReadItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(
            &leftReadItems, 0U);
    rightReadItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(
            &rightReadItems, 0U);
    if (runItem == ZR_NULL || leftReadItem == ZR_NULL ||
        rightReadItem == ZR_NULL ||
        leftReadItem->kind != ZR_LSP_SYMBOL_KIND_METHOD ||
        rightReadItem->kind != ZR_LSP_SYMBOL_KIND_METHOD ||
        runItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        leftReadItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        rightReadItem->semanticId == ZR_SEMANTIC_ID_INVALID ||
        leftReadItem->semanticId == rightReadItem->semanticId) {
        failure = "distinct canonical method identities";
        goto cleanup;
    }

    runItem->name = tamperedRunName;
    leftReadItem->name = tamperedMethodName;
    if (!ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
                state, context, runItem, &outgoing) ||
        outgoing.length != 1U ||
        !ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(
                state, context, leftReadItem, &incoming) ||
        incoming.length != 1U ||
        !ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(
                state, context, rightReadItem, &unrelatedIncoming) ||
        unrelatedIncoming.length != 0U) {
        failure = "canonical receiver method edge projection";
        goto cleanup;
    }
    outgoingCall = *(SZrLspHierarchyCall **)ZrCore_Array_Get(&outgoing, 0U);
    incomingCall = *(SZrLspHierarchyCall **)ZrCore_Array_Get(&incoming, 0U);
    if (outgoingCall == ZR_NULL || incomingCall == ZR_NULL ||
        outgoingCall->item == ZR_NULL || incomingCall->item == ZR_NULL ||
        outgoingCall->item->semanticId != leftReadItem->semanticId ||
        incomingCall->item->semanticId != runItem->semanticId ||
        outgoingCall->item->semanticId == rightReadItem->semanticId ||
        outgoingCall->fromRanges.length != 2U ||
        incomingCall->fromRanges.length != 2U) {
        failure = "exact grouped method targets and callsites";
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &runItems);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &leftReadItems);
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &rightReadItems);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &outgoing);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &incoming);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &unrelatedIncoming);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(
                timer,
                "LSP Local Method Call Hierarchy Uses Canonical Edges");
    } else {
        TEST_FAIL(
                timer,
                "LSP Local Method Call Hierarchy Uses Canonical Edges",
                failure);
    }
}

static void test_local_lambda_call_hierarchy_uses_canonical_edges(
        SZrState *state) {
    static const TZrChar *content =
            "fn callee(): int { return 1; }\n"
            "fn outer(): int {\n"
            "    var callback = fn(): int => { return callee(); };\n"
            "    return callback();\n"
            "}\n";
    SZrParityTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrString *tamperedName = ZR_NULL;
    SZrLspPosition calleePosition;
    SZrArray calleeItems = {0};
    SZrArray incoming = {0};
    SZrArray lambdaOutgoing = {0};
    SZrArray invalidRangeOutgoing = {0};
    SZrLspHierarchyItem *calleeItem = ZR_NULL;
    SZrLspHierarchyCall *incomingCall = ZR_NULL;
    SZrLspHierarchyCall *outgoingCall = ZR_NULL;
    const TZrChar *failure = "lambda call hierarchy preparation";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP Local Lambda Call Hierarchy Uses Canonical Edges");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///semantic_query_local_lambda_call_hierarchy.zr",
            strlen("file:///semantic_query_local_lambda_call_hierarchy.zr"));
    tamperedName = ZrCore_String_Create(
            state, "not_callee", strlen("not_callee"));
    if (context == ZR_NULL || uri == ZR_NULL || tamperedName == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "fn callee", 0U, 3, &calleePosition) ||
        !ZrLanguageServer_Lsp_PrepareCallHierarchy(
                state, context, uri, calleePosition, &calleeItems) ||
        calleeItems.length != 1U) {
        goto cleanup;
    }
    calleeItem = *(SZrLspHierarchyItem **)ZrCore_Array_Get(
            &calleeItems, 0U);
    if (calleeItem == ZR_NULL ||
        calleeItem->semanticId == ZR_SEMANTIC_ID_INVALID) {
        failure = "canonical callee identity";
        goto cleanup;
    }

    calleeItem->name = tamperedName;
    if (!ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(
                state, context, calleeItem, &incoming) ||
        incoming.length != 1U) {
        failure = "lambda caller edge projection";
        goto cleanup;
    }
    incomingCall = *(SZrLspHierarchyCall **)ZrCore_Array_Get(
            &incoming, 0U);
    if (incomingCall == ZR_NULL || incomingCall->item == ZR_NULL ||
        !incomingCall->item->hasSemanticIdentity ||
        incomingCall->item->semanticId == ZR_SEMANTIC_ID_INVALID ||
        incomingCall->item->semanticId == calleeItem->semanticId ||
        incomingCall->fromRanges.length != 1U) {
        failure = "canonical lambda caller identity and callsite";
        goto cleanup;
    }
    if (!ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
                state, context, incomingCall->item, &lambdaOutgoing) ||
        lambdaOutgoing.length != 1U) {
        failure = "returned lambda item re-resolution";
        goto cleanup;
    }
    outgoingCall = *(SZrLspHierarchyCall **)ZrCore_Array_Get(
            &lambdaOutgoing, 0U);
    if (outgoingCall == ZR_NULL || outgoingCall->item == ZR_NULL ||
        outgoingCall->item->semanticId != calleeItem->semanticId ||
        outgoingCall->fromRanges.length != 1U) {
        failure = "lambda outgoing canonical target";
        goto cleanup;
    }
    incomingCall->item->selectionRange.start.character++;
    (void)ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(
            state, context, incomingCall->item, &invalidRangeOutgoing);
    if (invalidRangeOutgoing.length != 0U) {
        failure = "tampered lambda declaration range did not fail closed";
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    ZrLanguageServer_Lsp_FreeHierarchyItems(state, &calleeItems);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &incoming);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &lambdaOutgoing);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(state, &invalidRangeOutgoing);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
    if (valid) {
        TEST_PASS(
                timer,
                "LSP Local Lambda Call Hierarchy Uses Canonical Edges");
    } else {
        TEST_FAIL(
                timer,
                "LSP Local Lambda Call Hierarchy Uses Canonical Edges",
                failure);
    }
}

#endif
