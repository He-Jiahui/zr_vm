#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/semantic_relations.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrFileRange relation_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state,
                                        "semantic_relations.zr",
                                        strlen("semantic_relations.zr"));
    return range;
}

static void relation_append(SZrSemanticContext *context,
                            EZrSemanticRelationKind kind,
                            TZrSymbolId sourceSymbolId,
                            TZrSymbolId targetSymbolId,
                            TZrTypeId sourceTypeId,
                            TZrTypeId targetTypeId,
                            TZrSize sourceOffset,
                            TZrSize targetOffset,
                            TZrNativeString originUri) {
    SZrSemanticRelationFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.kind = kind;
    fact.sourceSymbolId = sourceSymbolId;
    fact.targetSymbolId = targetSymbolId;
    fact.sourceTypeId = sourceTypeId;
    fact.targetTypeId = targetTypeId;
    fact.sourceRange = relation_range(sourceOffset, sourceOffset + 1U);
    fact.targetRange = relation_range(targetOffset, targetOffset + 1U);
    fact.hasSourceRange = ZR_TRUE;
    fact.hasTargetRange = ZR_TRUE;
    if (originUri != ZR_NULL) {
        fact.isExternal = ZR_TRUE;
        fact.externalOriginUri = ZrCore_String_Create(
                g_state, originUri, strlen(originUri));
    }
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));
}

static const SZrParserSemanticRelationQuery *relation_at(SZrArray *relations,
                                                         TZrSize index) {
    return (const SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
            relations, index);
}

static void test_relations_of_symbol_projects_sorted_snapshot_edges(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrArray relations;
    const SZrParserSemanticRelationQuery *first;
    const SZrParserSemanticRelationQuery *second;

    TEST_ASSERT_NOT_NULL(context);
    ZrCore_Array_Construct(&relations);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                    7U,
                    12U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    22U,
                    2U,
                    "zro://fixtures/library.zro");
    relation_append(context,
                    ZR_SEMANTIC_RELATION_ALIAS_TARGET,
                    7U,
                    11U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    12U,
                    4U,
                    ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, 7U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    first = relation_at(&relations, 0U);
    second = relation_at(&relations, 1U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET, first->kind);
    TEST_ASSERT_EQUAL_UINT(7U, first->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(11U, first->targetSymbolId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN, second->kind);
    TEST_ASSERT_TRUE(second->isExternal);
    TEST_ASSERT_EQUAL_STRING("zro://fixtures/library.zro",
                             ZrCore_String_GetNativeString(second->externalOriginUri));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, 7U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_type_and_implementation_queries_preserve_edge_direction(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(context);
    ZrCore_Array_Construct(&relations);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_BASE_TYPE,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    21U,
                    22U,
                    30U,
                    2U,
                    ZR_NULL);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_IMPLEMENTATION,
                    41U,
                    99U,
                    31U,
                    32U,
                    44U,
                    4U,
                    ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(context, 21U, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_BASE_TYPE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(21U, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(22U, relation->targetTypeId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DerivedTypesOf(context, 22U, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_UINT(21U, relation->sourceTypeId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            context, 99U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPLEMENTATION, relation->kind);
    TEST_ASSERT_EQUAL_UINT(41U, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(99U, relation->targetSymbolId);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_relations_of_symbol_honors_node_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode root;
    SZrParserSemanticQueryScope scope;
    SZrArray relations;

    TEST_ASSERT_NOT_NULL(context);
    memset(&root, 0, sizeof(root));
    root.location = relation_range(10U, 15U);
    ZrCore_Array_Construct(&relations);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_ALIAS_TARGET,
                    7U,
                    11U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    12U,
                    4U,
                    ZR_NULL);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                    7U,
                    12U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    22U,
                    2U,
                    "zro://fixtures/library.zro");
    ZrParser_SemanticQueryScope_Node(&scope, &root);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, 7U, &scope, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET,
                          relation_at(&relations, 0U)->kind);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_relations_of_symbol_projects_sorted_snapshot_edges);
    RUN_TEST(test_type_and_implementation_queries_preserve_edge_direction);
    RUN_TEST(test_relations_of_symbol_honors_node_scope);
    return UNITY_END();
}
