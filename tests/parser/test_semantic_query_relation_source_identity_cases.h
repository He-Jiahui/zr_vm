#ifndef ZR_VM_TEST_SEMANTIC_QUERY_RELATION_SOURCE_IDENTITY_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_RELATION_SOURCE_IDENTITY_CASES_H

static void test_relation_node_scope_fails_closed_for_missing_fact_source(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticRelationFact fact;
    SZrAstNode root;
    SZrParserSemanticQueryScope scope;
    SZrArray relations;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_ALIAS_TARGET;
    fact.sourceSymbolId = 41U;
    fact.targetSymbolId = 42U;
    fact.sourceRange = relation_range(10U, 11U);
    fact.sourceRange.source = ZR_NULL;
    fact.targetRange = relation_range(20U, 21U);
    fact.targetRange.source = ZR_NULL;
    fact.hasSourceRange = ZR_TRUE;
    fact.hasTargetRange = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));

    memset(&root, 0, sizeof(root));
    root.location = relation_range(0U, 30U);
    ZrParser_SemanticQueryScope_Node(&scope, &root);
    ZrCore_Array_Construct(&relations);

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, fact.sourceSymbolId, &scope, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    root.location.source = ZR_NULL;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, fact.sourceSymbolId, &scope, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

#endif
