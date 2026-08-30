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

static SZrFileRange relation_line_range(
        SZrString *source,
        TZrInt32 line,
        TZrInt32 startColumn,
        TZrInt32 endColumn) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.source = source;
    range.start.line = line;
    range.start.column = startColumn;
    range.end.line = line;
    range.end.column = endColumn;
    return range;
}

static void test_relation_queries_sort_line_only_ranges_independent_of_append_order(void) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "relation_line_order.zr");
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticRelationFact fact;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_ALIAS_TARGET;
    fact.sourceSymbolId = 51U;
    fact.targetSymbolId = 52U;
    fact.sourceRange = relation_line_range(sourceName, 3, 5, 11);
    fact.targetRange = relation_line_range(sourceName, 10, 1, 7);
    fact.hasSourceRange = ZR_TRUE;
    fact.hasTargetRange = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));

    fact.sourceRange = relation_line_range(sourceName, 2, 5, 11);
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, fact.sourceSymbolId, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(2, relation->sourceRange.start.line);
    relation = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(3, relation->sourceRange.start.line);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

#endif
