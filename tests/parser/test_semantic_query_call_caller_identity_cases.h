#ifndef ZR_VM_TEST_SEMANTIC_QUERY_CALL_CALLER_IDENTITY_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_CALL_CALLER_IDENTITY_CASES_H

static void test_call_edge_fails_closed_for_ambiguous_equal_function_scopes(void) {
    const TZrChar *source = "target()";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticScopeFact scope;
    SZrSemanticReferenceFact callReference;
    TZrSymbolId firstCallerId;
    TZrSymbolId secondCallerId;
    TZrSymbolId targetId;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(context);
    firstCallerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "firstCaller"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            41U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("firstCaller", ZR_NULL, "firstCaller", 0U));
    secondCallerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "secondCaller"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            42U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("secondCaller", ZR_NULL, "secondCaller", 0U));
    targetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            43U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("target", ZR_NULL, "target", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, firstCallerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, secondCallerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, targetId);

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    scope.range = call_source_position(source, ZR_NULL, "target", 0U);
    scope.range.end.offset = strlen(source);
    scope.ownerSymbolId = firstCallerId;
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &scope));
    scope.ownerSymbolId = secondCallerId;
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &scope));

    memset(&callReference, 0, sizeof(callReference));
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.range = call_source_position(source, ZR_NULL, "target", 0U);
    callReference.symbolId = targetId;
    callReference.typeId = 43U;
    callReference.declarationRange = call_source_position(
            "target", ZR_NULL, "target", 0U);
    callReference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_IncomingCalls(
            context, targetId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, edge->callerSymbolId);
    TEST_ASSERT_EQUAL_UINT(targetId, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE,
                          edge->resolution);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, firstCallerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, secondCallerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(0U, edges.length);

    ZrCore_Array_Free(g_state, &edges);
    ZrParser_SemanticContext_Free(context);
}

#endif
