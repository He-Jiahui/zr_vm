#ifndef ZR_VM_TEST_SEMANTIC_QUERY_RELATION_CANONICAL_IDENTITY_MATRIX_CASES_H
#define ZR_VM_TEST_SEMANTIC_QUERY_RELATION_CANONICAL_IDENTITY_MATRIX_CASES_H

static TZrTypeId relation_identity_nominal(
        SZrSemanticContext *context,
        TZrNativeString moduleName,
        TZrNativeString typeName,
        TZrUInt32 definitionToken) {
    return ZrParser_CanonicalType_InternNominal(
            context,
            ZrCore_String_CreateFromNative(g_state, moduleName),
            ZrCore_String_CreateFromNative(g_state, typeName),
            definitionToken);
}

static void test_relation_queries_isolate_same_named_types_by_module_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId appNode;
    TZrTypeId libraryNode;
    TZrTypeId appBase;
    TZrTypeId libraryBase;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(context);
    appNode = relation_identity_nominal(context, "app.models", "Node", 1U);
    libraryNode = relation_identity_nominal(context, "lib.models", "Node", 1U);
    appBase = relation_identity_nominal(context, "app.contracts", "Base", 2U);
    libraryBase = relation_identity_nominal(context, "lib.contracts", "Base", 2U);
    TEST_ASSERT_NOT_EQUAL(appNode, libraryNode);
    TEST_ASSERT_NOT_EQUAL(appBase, libraryBase);

    relation_append(
            context,
            ZR_SEMANTIC_RELATION_BASE_TYPE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            appNode,
            appBase,
            10U,
            20U,
            ZR_NULL);
    relation_append(
            context,
            ZR_SEMANTIC_RELATION_BASE_TYPE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            libraryNode,
            libraryBase,
            30U,
            40U,
            ZR_NULL);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            context, appNode, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_UINT(appBase, relation->targetTypeId);
    TEST_ASSERT_EQUAL_STRING(
            "app.models",
            ZrCore_String_GetNativeString(relation->sourceModuleIdentity));
    TEST_ASSERT_EQUAL_STRING(
            "app.contracts",
            ZrCore_String_GetNativeString(relation->targetModuleIdentity));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DerivedTypesOf(
            context, libraryBase, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_UINT(libraryNode, relation->sourceTypeId);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_relation_queries_preserve_open_and_closed_generic_edges(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId boxType;
    TZrTypeId baseType;
    TZrTypeId genericParameter;
    TZrTypeId intType;
    TZrTypeId openBox;
    TZrTypeId closedBox;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *first;
    const SZrParserSemanticRelationQuery *second;

    TEST_ASSERT_NOT_NULL(context);
    boxType = relation_identity_nominal(context, "lib.generic", "Box", 7U);
    baseType = relation_identity_nominal(context, "lib.contracts", "Base", 8U);
    genericParameter = ZrParser_CanonicalType_InternGenericParameter(
            context, 17U, 0U);
    intType = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_INT64);
    openBox = ZrParser_CanonicalType_InternGenericInstance(
            context, boxType, &genericParameter, 1U);
    closedBox = ZrParser_CanonicalType_InternGenericInstance(
            context, boxType, &intType, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, openBox);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, closedBox);
    TEST_ASSERT_NOT_EQUAL(openBox, closedBox);

    relation_append(
            context,
            ZR_SEMANTIC_RELATION_BASE_TYPE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            openBox,
            baseType,
            10U,
            30U,
            ZR_NULL);
    relation_append(
            context,
            ZR_SEMANTIC_RELATION_BASE_TYPE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            closedBox,
            baseType,
            20U,
            30U,
            ZR_NULL);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DerivedTypesOf(
            context, baseType, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    first = relation_at(&relations, 0U);
    second = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_NOT_EQUAL(first->sourceTypeId, second->sourceTypeId);
    TEST_ASSERT_TRUE(
            (first->sourceTypeId == openBox && second->sourceTypeId == closedBox) ||
            (first->sourceTypeId == closedBox && second->sourceTypeId == openBox));
    TEST_ASSERT_EQUAL_STRING(
            "lib.generic",
            ZrCore_String_GetNativeString(first->sourceModuleIdentity));
    TEST_ASSERT_EQUAL_STRING(
            "lib.generic",
            ZrCore_String_GetNativeString(second->sourceModuleIdentity));

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_relation_queries_preserve_each_alias_chain_hop(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId targetType;
    TZrSymbolId outerAlias;
    TZrSymbolId innerAlias;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *first;
    const SZrParserSemanticRelationQuery *second;

    TEST_ASSERT_NOT_NULL(context);
    targetType = relation_identity_nominal(
            context, "lib.models", "Target", 21U);
    outerAlias = relation_register_symbol(
            context,
            "OuterAlias",
            ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            targetType,
            10U);
    innerAlias = relation_register_symbol(
            context,
            "InnerAlias",
            ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            targetType,
            20U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, outerAlias);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, innerAlias);

    relation_append(
            context,
            ZR_SEMANTIC_RELATION_ALIAS_TARGET,
            outerAlias,
            innerAlias,
            targetType,
            targetType,
            10U,
            20U,
            ZR_NULL);
    relation_append(
            context,
            ZR_SEMANTIC_RELATION_ALIAS_TARGET,
            innerAlias,
            ZR_SEMANTIC_ID_INVALID,
            targetType,
            targetType,
            20U,
            30U,
            ZR_NULL);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, innerAlias, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    first = relation_at(&relations, 0U);
    second = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET, first->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET, second->kind);
    TEST_ASSERT_TRUE(
            (first->sourceSymbolId == outerAlias &&
             first->targetSymbolId == innerAlias &&
             second->sourceSymbolId == innerAlias) ||
            (second->sourceSymbolId == outerAlias &&
             second->targetSymbolId == innerAlias &&
             first->sourceSymbolId == innerAlias));

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_relation_queries_isolate_overloads_by_symbol_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId firstCallable;
    TZrTypeId secondCallable;
    TZrSymbolId firstImplementation;
    TZrSymbolId secondImplementation;
    TZrSymbolId firstContract;
    TZrSymbolId secondContract;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(context);
    firstCallable = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_INT64);
    secondCallable = ZrParser_CanonicalType_InternPrimitive(
            context, ZR_VALUE_TYPE_STRING);
    firstImplementation = relation_register_symbol(
            context,
            "convert",
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            firstCallable,
            10U);
    secondImplementation = relation_register_symbol(
            context,
            "convert",
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            secondCallable,
            20U);
    firstContract = relation_register_symbol(
            context,
            "convert",
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            firstCallable,
            30U);
    secondContract = relation_register_symbol(
            context,
            "convert",
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            secondCallable,
            40U);
    TEST_ASSERT_NOT_EQUAL(firstImplementation, secondImplementation);
    TEST_ASSERT_NOT_EQUAL(firstContract, secondContract);

    relation_append(
            context,
            ZR_SEMANTIC_RELATION_IMPLEMENTATION,
            firstImplementation,
            firstContract,
            firstCallable,
            firstCallable,
            10U,
            30U,
            ZR_NULL);
    relation_append(
            context,
            ZR_SEMANTIC_RELATION_IMPLEMENTATION,
            secondImplementation,
            secondContract,
            secondCallable,
            secondCallable,
            20U,
            40U,
            ZR_NULL);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            context, firstContract, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_UINT(firstImplementation, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(firstContract, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(firstCallable, relation->sourceTypeId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            context, secondContract, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_UINT(secondImplementation, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(secondContract, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(secondCallable, relation->sourceTypeId);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

#endif
