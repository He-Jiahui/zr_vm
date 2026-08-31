#ifndef ZR_VM_TEST_SEMANTIC_QUERY_RELATION_PROVIDER_GENERATION_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_RELATION_PROVIDER_GENERATION_CASES_H

static void test_external_relations_preserve_provider_generation_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticRelationFact fact;
    SZrString *sourceModule;
    SZrString *targetModule;
    TZrTypeId sourceTypeId;
    TZrTypeId targetTypeId;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *first;
    const SZrParserSemanticRelationQuery *second;

    TEST_ASSERT_NOT_NULL(context);
    sourceModule = ZrCore_String_CreateFromNative(g_state, "app.models");
    targetModule = ZrCore_String_CreateFromNative(g_state, "lib.contracts");
    sourceTypeId = ZrParser_CanonicalType_InternNominal(
            context,
            sourceModule,
            ZrCore_String_CreateFromNative(g_state, "Derived"),
            7U);
    targetTypeId = ZrParser_CanonicalType_InternNominal(
            context,
            targetModule,
            ZrCore_String_CreateFromNative(g_state, "Base"),
            11U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, sourceTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, targetTypeId);

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_BASE_TYPE;
    fact.sourceTypeId = sourceTypeId;
    fact.targetTypeId = targetTypeId;
    fact.sourceProviderGeneration = 3U;
    fact.targetProviderGeneration = 10U;
    fact.isExternal = ZR_TRUE;
    fact.externalOriginUri = ZrCore_String_CreateFromNative(
            g_state, "zro://fixtures/contracts.zro");
    fact.virtualDeclarationUri = ZrCore_String_CreateFromNative(
            g_state, "zr-decompiled:/contracts");
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));
    TEST_ASSERT_EQUAL_UINT(1U, context->relationFacts.length);

    fact.targetProviderGeneration = 11U;
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));
    TEST_ASSERT_EQUAL_UINT(2U, context->relationFacts.length);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            context, sourceTypeId, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    first = relation_at(&relations, 0U);
    second = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_STRING(
            "app.models",
            ZrCore_String_GetNativeString(first->sourceModuleIdentity));
    TEST_ASSERT_EQUAL_STRING(
            "lib.contracts",
            ZrCore_String_GetNativeString(first->targetModuleIdentity));
    TEST_ASSERT_EQUAL_UINT64(3U, first->sourceProviderGeneration);
    TEST_ASSERT_EQUAL_UINT64(10U, first->targetProviderGeneration);
    TEST_ASSERT_EQUAL_UINT64(3U, second->sourceProviderGeneration);
    TEST_ASSERT_EQUAL_UINT64(11U, second->targetProviderGeneration);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

#endif
