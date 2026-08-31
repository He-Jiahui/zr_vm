#ifndef ZR_VM_TEST_SEMANTIC_QUERY_RELATION_ENDPOINT_IDENTITY_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_RELATION_ENDPOINT_IDENTITY_CASES_H

static void test_relation_append_requires_identity_for_both_endpoints(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticRelationFact fact;

    TEST_ASSERT_NOT_NULL(context);

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_OVERRIDE;
    fact.sourceSymbolId = 7U;
    TEST_ASSERT_FALSE(ZrParser_SemanticRelations_Append(context, &fact));
    TEST_ASSERT_EQUAL_UINT(0U, context->relationFacts.length);

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_OVERRIDE;
    fact.targetSymbolId = 11U;
    TEST_ASSERT_FALSE(ZrParser_SemanticRelations_Append(context, &fact));
    TEST_ASSERT_EQUAL_UINT(0U, context->relationFacts.length);

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_BASE_TYPE;
    fact.sourceTypeId = 17U;
    fact.targetTypeId = 19U;
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));
    TEST_ASSERT_EQUAL_UINT(1U, context->relationFacts.length);

    ZrParser_SemanticContext_Free(context);
}

#endif
