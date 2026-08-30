#ifndef ZR_VM_TEST_SEMANTIC_QUERY_CALL_EDGE_REFINEMENT_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_CALL_EDGE_REFINEMENT_CASES_H

static void test_resolved_call_edge_supersedes_unresolved_same_site(void) {
    const TZrChar *source = "target()";
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticScopeFact scope;
    SZrSemanticReferenceFact callReference;
    TZrSymbolId callerId;
    TZrSymbolId targetId;
    SZrArray edges;
    const SZrParserSemanticCallEdgeQuery *edge;

    TEST_ASSERT_NOT_NULL(context);
    callerId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "caller"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            11U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("caller", ZR_NULL, "caller", 0U));
    targetId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "target"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            12U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            call_source_position("target", ZR_NULL, "target", 0U));
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, callerId);
    TEST_ASSERT_NOT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, targetId);

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_SEMANTIC_SCOPE_KIND_FUNCTION;
    scope.ownerSymbolId = callerId;
    scope.range = call_source_position(source, ZR_NULL, "target", 0U);
    scope.range.end.offset = strlen(source);
    TEST_ASSERT_NOT_EQUAL_UINT(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_PublishScopeFact(context, &scope));

    memset(&callReference, 0, sizeof(callReference));
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.range = call_source_position(source, ZR_NULL, "target", 0U);
    callReference.typeId = 12U;
    callReference.isResolved = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &callReference));

    callReference.symbolId = targetId;
    callReference.declarationRange = call_source_position(
            "target", ZR_NULL, "target", 0U);
    callReference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));

    ZrCore_Array_Construct(&edges);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, callerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(targetId, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(12U, edge->callableTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED,
                          edge->resolution);
    TEST_ASSERT_TRUE(edge->hasTargetDeclarationRange);

    callReference.typeId = 13U;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &callReference));
    callReference.symbolId = ZR_SEMANTIC_ID_INVALID;
    callReference.isResolved = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(
            context, &callReference));
    TEST_ASSERT_TRUE(ZrParser_SemanticCalls_Publish(context));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_OutgoingCalls(
            context, callerId, ZR_NULL, &edges));
    TEST_ASSERT_EQUAL_UINT(1U, edges.length);
    edge = (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(&edges, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_UINT(targetId, edge->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(13U, edge->callableTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED,
                          edge->resolution);

    ZrCore_Array_Free(g_state, &edges);
    ZrParser_SemanticContext_Free(context);
}

#endif
