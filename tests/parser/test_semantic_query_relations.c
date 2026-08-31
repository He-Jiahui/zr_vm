#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/semantic_relations.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

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

static TZrSymbolId relation_register_symbol(
        SZrSemanticContext *context,
        TZrNativeString name,
        EZrSemanticSymbolKind kind,
        TZrTypeId typeId,
        TZrSize offset) {
    return ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_Create(g_state, name, strlen(name)),
            kind,
            typeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            relation_range(offset, offset + 1U));
}

static void relation_release_compiler_function(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }
    if (cs->topLevelFunction != ZR_NULL &&
        cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }
    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static const SZrSemanticSymbolRecord *relation_find_symbol(
        const SZrSemanticContext *context,
        TZrNativeString name) {
    TZrSize index;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        const TZrChar *symbolName = symbol != ZR_NULL && symbol->name != ZR_NULL
                ? ZrCore_String_GetNativeString(symbol->name)
                : ZR_NULL;
        if (symbolName != ZR_NULL && strcmp(symbolName, name) == 0) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *relation_find_symbol_by_node(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static const SZrParserSemanticRelationQuery *relation_find_type_target(
        SZrArray *relations,
        EZrSemanticRelationKind kind,
        TZrTypeId targetTypeId) {
    TZrSize index;

    if (relations == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < relations->length; index++) {
        const SZrParserSemanticRelationQuery *relation = relation_at(relations, index);
        if (relation != ZR_NULL && relation->kind == kind &&
            relation->targetTypeId == targetTypeId) {
            return relation;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticVisibleSymbolFact *relation_find_visible_fact(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *fact =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->visibleSymbolFacts, index);
        if (fact != ZR_NULL && fact->symbolId == symbolId) {
            return fact;
        }
    }
    return ZR_NULL;
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

static void test_relation_append_deduplicates_exact_edges_and_preserves_multiple_definitions(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrArray relations;
    const SZrParserSemanticRelationQuery *first;
    const SZrParserSemanticRelationQuery *second;

    TEST_ASSERT_NOT_NULL(context);
    ZrCore_Array_Construct(&relations);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
                    7U,
                    7U,
                    17U,
                    17U,
                    10U,
                    20U,
                    ZR_NULL);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
                    7U,
                    7U,
                    17U,
                    17U,
                    10U,
                    20U,
                    ZR_NULL);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
                    7U,
                    7U,
                    17U,
                    17U,
                    10U,
                    30U,
                    ZR_NULL);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, 7U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    first = relation_at(&relations, 0U);
    second = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_UINT(20U, first->targetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(30U, second->targetRange.start.offset);

    relation_append(context,
                    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                    8U,
                    9U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    40U,
                    50U,
                    "zro://fixtures/one.zro");
    relation_append(context,
                    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                    8U,
                    9U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    40U,
                    50U,
                    "zro://fixtures/one.zro");
    relation_append(context,
                    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                    8U,
                    9U,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    40U,
                    50U,
                    "zro://fixtures/two.zro");
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, 8U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    first = relation_at(&relations, 0U);
    second = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_STRING(
            "zro://fixtures/one.zro",
            ZrCore_String_GetNativeString(first->externalOriginUri));
    TEST_ASSERT_EQUAL_STRING(
            "zro://fixtures/two.zro",
            ZrCore_String_GetNativeString(second->externalOriginUri));

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_external_relation_requires_and_projects_virtual_declaration_uri(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticRelationFact fact;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN;
    fact.sourceSymbolId = 17U;
    fact.targetTypeId = 29U;
    fact.isExternal = ZR_TRUE;
    fact.externalOriginUri = ZrCore_String_Create(
            g_state,
            "zro://fixtures/external-library.zro",
            strlen("zro://fixtures/external-library.zro"));
    TEST_ASSERT_NOT_NULL(fact.externalOriginUri);
    TEST_ASSERT_FALSE(ZrParser_SemanticRelations_Append(context, &fact));

    fact.virtualDeclarationUri = ZrCore_String_Create(
            g_state,
            "zr-decompiled:/external-library",
            strlen("zr-decompiled:/external-library"));
    TEST_ASSERT_NOT_NULL(fact.virtualDeclarationUri);
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_Append(context, &fact));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, fact.sourceSymbolId, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_TRUE(relation->isExternal);
    TEST_ASSERT_FALSE(relation->hasSourceRange);
    TEST_ASSERT_FALSE(relation->hasTargetRange);
    TEST_ASSERT_NOT_NULL(relation->externalOriginUri);
    TEST_ASSERT_NOT_NULL(relation->virtualDeclarationUri);
    TEST_ASSERT_EQUAL_STRING(
            "zro://fixtures/external-library.zro",
            ZrCore_String_GetNativeString(relation->externalOriginUri));
    TEST_ASSERT_EQUAL_STRING(
            "zr-decompiled:/external-library",
            ZrCore_String_GetNativeString(relation->virtualDeclarationUri));

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
    relation_append(context,
                    ZR_SEMANTIC_RELATION_OVERRIDE,
                    42U,
                    99U,
                    33U,
                    32U,
                    45U,
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
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_OVERRIDE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(42U, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(99U, relation->targetSymbolId);
    relation = relation_at(&relations, 1U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPLEMENTATION, relation->kind);
    TEST_ASSERT_EQUAL_UINT(41U, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(99U, relation->targetSymbolId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, 41U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, ZR_SEMANTIC_ID_INVALID, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            context, 99U, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_ImplementationsOf(
            context, ZR_SEMANTIC_ID_INVALID, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(context, 21U, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_BaseTypesOf(
            context, ZR_SEMANTIC_ID_INVALID, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DerivedTypesOf(context, 22U, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_DerivedTypesOf(
            context, ZR_SEMANTIC_ID_INVALID, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_relation_queries_project_canonical_module_identities(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *sourceModule;
    SZrString *targetModule;
    TZrTypeId sourceTypeId;
    TZrTypeId targetTypeId;
    TZrTypeId targetInstanceTypeId;
    TZrTypeId intTypeId;
    TZrTypeId localTypeId;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(context);
    sourceModule = ZrCore_String_CreateFromNative(g_state, "app.models");
    targetModule = ZrCore_String_CreateFromNative(g_state, "lib.contracts");
    TEST_ASSERT_NOT_NULL(sourceModule);
    TEST_ASSERT_NOT_NULL(targetModule);
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
    localTypeId = ZrParser_CanonicalType_InternNominal(
            context,
            ZR_NULL,
            ZrCore_String_CreateFromNative(g_state, "Local"),
            13U);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    targetInstanceTypeId = ZrParser_CanonicalType_InternGenericInstance(
            context, targetTypeId, &intTypeId, 1U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, sourceTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, targetTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, localTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, intTypeId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, targetInstanceTypeId);

    relation_append(context,
                    ZR_SEMANTIC_RELATION_BASE_TYPE,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    sourceTypeId,
                    targetTypeId,
                    30U,
                    2U,
                    ZR_NULL);
    relation_append(context,
                    ZR_SEMANTIC_RELATION_BASE_TYPE,
                    ZR_SEMANTIC_ID_INVALID,
                    ZR_SEMANTIC_ID_INVALID,
                    localTypeId,
                    targetInstanceTypeId,
                    40U,
                    2U,
                    ZR_NULL);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            context, sourceTypeId, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_NOT_NULL(relation->sourceModuleIdentity);
    TEST_ASSERT_NOT_NULL(relation->targetModuleIdentity);
    TEST_ASSERT_EQUAL_STRING(
            "app.models",
            ZrCore_String_GetNativeString(relation->sourceModuleIdentity));
    TEST_ASSERT_EQUAL_STRING(
            "lib.contracts",
            ZrCore_String_GetNativeString(relation->targetModuleIdentity));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            context, localTypeId, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_NULL(relation->sourceModuleIdentity);
    TEST_ASSERT_NOT_NULL(relation->targetModuleIdentity);
    TEST_ASSERT_EQUAL_STRING(
            "lib.contracts",
            ZrCore_String_GetNativeString(relation->targetModuleIdentity));

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_type_declaration_relation_publishes_identity_edge_once(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode sourceDeclaration;
    SZrAstNode targetDeclaration;
    SZrArray relations;
    TZrSymbolId sourceSymbolId;
    TZrSymbolId targetSymbolId;

    TEST_ASSERT_NOT_NULL(context);
    memset(&sourceDeclaration, 0, sizeof(sourceDeclaration));
    memset(&targetDeclaration, 0, sizeof(targetDeclaration));
    sourceDeclaration.location = relation_range(10U, 16U);
    targetDeclaration.location = relation_range(30U, 36U);
    sourceSymbolId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_Create(g_state, "Derived", strlen("Derived")),
            ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            21U,
            ZR_SEMANTIC_ID_INVALID,
            &sourceDeclaration,
            sourceDeclaration.location);
    targetSymbolId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_Create(g_state, "Base", strlen("Base")),
            ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            22U,
            ZR_SEMANTIC_ID_INVALID,
            &targetDeclaration,
            targetDeclaration.location);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, sourceSymbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, targetSymbolId);

    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
            context,
            ZR_SEMANTIC_RELATION_BASE_TYPE,
            &sourceDeclaration,
            &targetDeclaration));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
            context,
            ZR_SEMANTIC_RELATION_BASE_TYPE,
            &sourceDeclaration,
            &targetDeclaration));
    TEST_ASSERT_FALSE(ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
            context,
            ZR_SEMANTIC_RELATION_ALIAS_TARGET,
            &sourceDeclaration,
            &targetDeclaration));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(context, 21U, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_EQUAL_UINT(sourceSymbolId, relation_at(&relations, 0U)->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(targetSymbolId, relation_at(&relations, 0U)->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(10U, relation_at(&relations, 0U)->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(30U, relation_at(&relations, 0U)->targetRange.start.offset);

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

static void test_property_contracts_publish_accessor_relations_once(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticPropertyContract contract;
    SZrArray relations;
    TZrSymbolId propertySymbolId;
    TZrSymbolId getterSymbolId;
    TZrSymbolId setterSymbolId;
    TZrSymbolId setterValueSymbolId;

    TEST_ASSERT_NOT_NULL(context);
    propertySymbolId = relation_register_symbol(
            context, "value", ZR_SEMANTIC_SYMBOL_KIND_PROPERTY, 21U, 10U);
    getterSymbolId = relation_register_symbol(
            context, "getValue", ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, 31U, 20U);
    setterSymbolId = relation_register_symbol(
            context, "setValue", ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, 32U, 30U);
    setterValueSymbolId = relation_register_symbol(
            context, "next", ZR_SEMANTIC_SYMBOL_KIND_PARAMETER, 21U, 31U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, propertySymbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, getterSymbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, setterSymbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, setterValueSymbolId);

    memset(&contract, 0, sizeof(contract));
    contract.propertySymbolId = propertySymbolId;
    contract.propertyTypeId = 21U;
    contract.getterSymbolId = getterSymbolId;
    contract.setterSymbolId = setterSymbolId;
    contract.setterValueSymbolId = setterValueSymbolId;
    contract.getterCallableTypeId = 31U;
    contract.setterCallableTypeId = 32U;
    contract.declarationRange = relation_range(10U, 15U);
    TEST_ASSERT_TRUE(ZrParser_Semantic_PublishPropertyContract(context, &contract));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishPropertyContracts(context));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishPropertyContracts(context));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, propertySymbolId, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR,
                          relation_at(&relations, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT(propertySymbolId, relation_at(&relations, 0U)->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(getterSymbolId, relation_at(&relations, 0U)->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(setterSymbolId, relation_at(&relations, 1U)->targetSymbolId);
    TEST_ASSERT_TRUE(relation_at(&relations, 0U)->hasSourceRange);
    TEST_ASSERT_EQUAL_UINT(10U, relation_at(&relations, 0U)->sourceRange.start.offset);
    TEST_ASSERT_TRUE(relation_at(&relations, 0U)->hasTargetRange);
    TEST_ASSERT_EQUAL_UINT(20U, relation_at(&relations, 0U)->targetRange.start.offset);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_property_contract_relations_reject_mismatched_accessor_atomically(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticPropertyContract contract;
    SZrArray relations;
    TZrSymbolId propertySymbolId;
    TZrSymbolId getterSymbolId;
    TZrSymbolId setterSymbolId;
    TZrSymbolId setterValueSymbolId;

    TEST_ASSERT_NOT_NULL(context);
    propertySymbolId = relation_register_symbol(
            context, "value", ZR_SEMANTIC_SYMBOL_KIND_PROPERTY, 21U, 10U);
    getterSymbolId = relation_register_symbol(
            context, "getValue", ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, 31U, 20U);
    setterSymbolId = relation_register_symbol(
            context, "setValue", ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, 32U, 30U);
    setterValueSymbolId = relation_register_symbol(
            context, "next", ZR_SEMANTIC_SYMBOL_KIND_PARAMETER, 21U, 31U);

    memset(&contract, 0, sizeof(contract));
    contract.propertySymbolId = propertySymbolId;
    contract.propertyTypeId = 21U;
    contract.getterSymbolId = getterSymbolId;
    contract.setterSymbolId = setterSymbolId;
    contract.setterValueSymbolId = setterValueSymbolId;
    contract.getterCallableTypeId = 31U;
    contract.setterCallableTypeId = 99U;
    contract.declarationRange = relation_range(10U, 15U);
    TEST_ASSERT_TRUE(ZrParser_Semantic_PublishPropertyContract(context, &contract));
    TEST_ASSERT_FALSE(ZrParser_SemanticRelations_PublishPropertyContracts(context));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, propertySymbolId, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_reference_definitions_publish_declaration_edges_once(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticReferenceFact fact;
    SZrArray relations;
    TZrSymbolId symbolId;
    SZrFileRange declarationRange;

    TEST_ASSERT_NOT_NULL(context);
    declarationRange = relation_range(10U, 14U);
    symbolId = relation_register_symbol(
            context, "seed", ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, 21U, 10U);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, symbolId);

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    fact.symbolId = symbolId;
    fact.typeId = 21U;
    fact.range = declarationRange;
    fact.declarationRange = declarationRange;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_REFERENCE_WRITE;
    fact.symbolId = symbolId;
    fact.typeId = 21U;
    fact.range = relation_range(30U, 34U);
    fact.declarationRange = declarationRange;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_REFERENCE_READ;
    fact.symbolId = symbolId;
    fact.typeId = 21U;
    fact.range = relation_range(50U, 54U);
    fact.declarationRange = declarationRange;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(context));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishReferenceDefinitions(context));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishReferenceDefinitions(context));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            context, symbolId, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
                          relation_at(&relations, 0U)->kind);
    TEST_ASSERT_EQUAL_UINT(symbolId, relation_at(&relations, 0U)->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(symbolId, relation_at(&relations, 0U)->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(10U, relation_at(&relations, 0U)->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(30U, relation_at(&relations, 0U)->targetRange.start.offset);

    ZrCore_Array_Free(g_state, &relations);
    ZrParser_SemanticContext_Free(context);
}

static void test_compiled_source_publishes_reference_definition_relations(void) {
    const TZrChar *source =
            "fn read(): int {\n"
            "    var seed: int;\n"
            "    seed = 2;\n"
            "    return seed;\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_source.zr");
    SZrAstNode *ast;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *seed;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;
    const TZrChar *definitionText;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    seed = relation_find_symbol(cs.semanticContext, "seed");
    TEST_ASSERT_NOT_NULL(seed);
    definitionText = strstr(strstr(source, "seed") + 1U, "seed");
    TEST_ASSERT_NOT_NULL(definitionText);
    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, seed->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
                          relation->kind);
    TEST_ASSERT_EQUAL_UINT(seed->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(seed->id, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(
            (TZrUInt32)(definitionText - source),
            relation->targetRange.start.offset);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_source_publishes_type_hierarchy_relations(void) {
    const TZrChar *source =
            "class Base { }\n"
            "interface Readable { fn read(): int; }\n"
            "class Device : Base, Readable {\n"
            "    pub fn read(): int { return 1; }\n"
            "}\n"
            "interface StreamReadable : Readable { fn available(): int; }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_type_hierarchy.zr");
    SZrAstNode *ast;
    SZrAstNode *baseNode;
    SZrAstNode *readableNode;
    SZrAstNode *deviceNode;
    SZrAstNode *streamReadableNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *base;
    const SZrSemanticSymbolRecord *readable;
    const SZrSemanticSymbolRecord *device;
    const SZrSemanticSymbolRecord *streamReadable;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    baseNode = ast->data.script.statements->nodes[0];
    readableNode = ast->data.script.statements->nodes[1];
    deviceNode = ast->data.script.statements->nodes[2];
    streamReadableNode = ast->data.script.statements->nodes[3];
    TEST_ASSERT_NOT_NULL(baseNode);
    TEST_ASSERT_NOT_NULL(readableNode);
    TEST_ASSERT_NOT_NULL(deviceNode);
    TEST_ASSERT_NOT_NULL(streamReadableNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    base = relation_find_symbol_by_node(cs.semanticContext, baseNode);
    readable = relation_find_symbol_by_node(cs.semanticContext, readableNode);
    device = relation_find_symbol_by_node(cs.semanticContext, deviceNode);
    streamReadable = relation_find_symbol_by_node(
            cs.semanticContext, streamReadableNode);
    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(readable);
    TEST_ASSERT_NOT_NULL(device);
    TEST_ASSERT_NOT_NULL(streamReadable);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            cs.semanticContext, device->typeId, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    relation = relation_find_type_target(
            &relations, ZR_SEMANTIC_RELATION_BASE_TYPE, base->typeId);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_UINT(device->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(base->id, relation->targetSymbolId);
    relation = relation_find_type_target(
            &relations, ZR_SEMANTIC_RELATION_BASE_TYPE, readable->typeId);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_UINT(device->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(readable->id, relation->targetSymbolId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DerivedTypesOf(
            cs.semanticContext, base->typeId, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    TEST_ASSERT_EQUAL_UINT(device->typeId, relation_at(&relations, 0U)->sourceTypeId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
            cs.semanticContext, streamReadable->typeId, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_BASE_TYPE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(streamReadable->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(readable->id, relation->targetSymbolId);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            cs.semanticContext, readable->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPLEMENTATION, relation->kind);
    TEST_ASSERT_EQUAL_UINT(device->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(readable->id, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(device->typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(readable->typeId, relation->targetTypeId);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_source_publishes_interface_member_implementation(void) {
    const TZrChar *source =
            "interface Readable { fn read(): int; }\n"
            "class Device : Readable {\n"
            "    pub fn read(): int { return 1; }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_interface_members.zr");
    SZrAstNode *ast;
    SZrAstNode *interfaceMethodNode;
    SZrAstNode *classMethodNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *interfaceMethod;
    const SZrSemanticSymbolRecord *classMethod;
    const SZrParserSemanticRelationQuery *relation;
    SZrArray relations;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(2U, ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[1]);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[0]->data.interfaceDeclaration.members);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[1]->data.classDeclaration.members);
    interfaceMethodNode = ast->data.script.statements->nodes[0]
                                  ->data.interfaceDeclaration.members->nodes[0];
    classMethodNode = ast->data.script.statements->nodes[1]
                              ->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(interfaceMethodNode);
    TEST_ASSERT_NOT_NULL(classMethodNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    interfaceMethod = relation_find_symbol_by_node(
            cs.semanticContext, interfaceMethodNode);
    classMethod = relation_find_symbol_by_node(cs.semanticContext, classMethodNode);
    TEST_ASSERT_NOT_NULL(interfaceMethod);
    TEST_ASSERT_NOT_NULL(classMethod);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            cs.semanticContext, interfaceMethod->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPLEMENTATION, relation->kind);
    TEST_ASSERT_EQUAL_UINT(classMethod->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(interfaceMethod->id, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(classMethod->typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(interfaceMethod->typeId, relation->targetTypeId);
    TEST_ASSERT_TRUE(relation->hasSourceRange);
    TEST_ASSERT_TRUE(relation->hasTargetRange);
    TEST_ASSERT_EQUAL_UINT(
            classMethodNode->location.start.offset, relation->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            classMethodNode->location.end.offset, relation->sourceRange.end.offset);
    TEST_ASSERT_EQUAL_UINT(
            interfaceMethodNode->location.start.offset, relation->targetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            interfaceMethodNode->location.end.offset, relation->targetRange.end.offset);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_source_publishes_override_relation(void) {
    const TZrChar *source =
            "abstract class Base {\n"
            "    pub abstract fn ping(): int;\n"
            "}\n"
            "class Derived : Base {\n"
            "    pub override fn ping(): int { return 1; }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_override.zr");
    SZrAstNode *ast;
    SZrAstNode *baseMethodNode;
    SZrAstNode *derivedMethodNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *baseMethod;
    const SZrSemanticSymbolRecord *derivedMethod;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[1]);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.classDeclaration.members);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[1]->data.classDeclaration.members);
    baseMethodNode = ast->data.script.statements->nodes[0]->data.classDeclaration.members->nodes[0];
    derivedMethodNode = ast->data.script.statements->nodes[1]->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(baseMethodNode);
    TEST_ASSERT_NOT_NULL(derivedMethodNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    baseMethod = relation_find_symbol_by_node(cs.semanticContext, baseMethodNode);
    derivedMethod = relation_find_symbol_by_node(cs.semanticContext, derivedMethodNode);
    TEST_ASSERT_NOT_NULL(baseMethod);
    TEST_ASSERT_NOT_NULL(derivedMethod);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, derivedMethod->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_OVERRIDE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(derivedMethod->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(baseMethod->id, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(derivedMethod->typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(baseMethod->typeId, relation->targetTypeId);
    TEST_ASSERT_EQUAL_UINT(
            derivedMethodNode->location.start.offset, relation->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            baseMethodNode->location.start.offset, relation->targetRange.start.offset);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
            cs.semanticContext, baseMethod->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_OVERRIDE, relation->kind);
    TEST_ASSERT_EQUAL_UINT(derivedMethod->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(baseMethod->id, relation->targetSymbolId);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_source_publishes_explicit_constructor_relation(void) {
    const TZrChar *source =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub @constructor(x: int) { this.x = x; }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_constructor.zr");
    SZrAstNode *ast;
    SZrAstNode *typeNode;
    SZrAstNode *constructorNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *typeSymbol;
    const SZrSemanticSymbolRecord *constructorSymbol;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    typeNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(typeNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, typeNode->type);
    TEST_ASSERT_NOT_NULL(typeNode->data.structDeclaration.members);
    constructorNode = typeNode->data.structDeclaration.members->nodes[1];
    TEST_ASSERT_NOT_NULL(constructorNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    typeSymbol = relation_find_symbol_by_node(cs.semanticContext, typeNode);
    constructorSymbol = relation_find_symbol_by_node(cs.semanticContext, constructorNode);
    TEST_ASSERT_NOT_NULL(typeSymbol);
    TEST_ASSERT_NOT_NULL(constructorSymbol);

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, typeSymbol->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_CONSTRUCTOR, relation->kind);
    TEST_ASSERT_EQUAL_UINT(typeSymbol->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(constructorSymbol->id, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(typeSymbol->typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(constructorSymbol->typeId, relation->targetTypeId);
    TEST_ASSERT_EQUAL_UINT(typeNode->location.start.offset, relation->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT(
            constructorNode->location.start.offset, relation->targetRange.start.offset);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_source_omits_synthesized_constructor_relation(void) {
    const TZrChar *source =
            "struct Empty {\n"
            "    pub var value: int;\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_synthesized_constructor.zr");
    SZrAstNode *ast;
    SZrAstNode *typeNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *typeSymbol;
    SZrArray relations;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    typeNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(typeNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    typeSymbol = relation_find_symbol_by_node(cs.semanticContext, typeNode);
    TEST_ASSERT_NOT_NULL(typeSymbol);
    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, typeSymbol->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(0U, relations.length);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_type_value_alias_publishes_canonical_target_relation(void) {
    const TZrChar *source =
            "var MatrixType = int[][];\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_type_value_alias.zr");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *alias;
    const SZrSemanticVisibleSymbolFact *visible;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *relation;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    alias = relation_find_symbol_by_node(cs.semanticContext, declaration);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, alias->typeId);
    visible = relation_find_visible_fact(cs.semanticContext, alias->id);
    TEST_ASSERT_NOT_NULL(visible);
    TEST_ASSERT_FALSE(visible->isImport);
    TEST_ASSERT_TRUE(visible->isAlias);
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishAliasTargets(
            cs.semanticContext));

    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, alias->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(1U, relations.length);
    relation = relation_at(&relations, 0U);
    TEST_ASSERT_NOT_NULL(relation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET, relation->kind);
    TEST_ASSERT_EQUAL_UINT(alias->id, relation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID, relation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, relation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, relation->targetTypeId);
    TEST_ASSERT_TRUE(relation->hasSourceRange);
    TEST_ASSERT_FALSE(relation->hasTargetRange);

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_import_publishes_external_origin_relation(void) {
    const TZrChar *source =
            "var {Vec3: Vector3} = import(\"zr.math\");\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_import.zr");
    SZrAstNode *ast;
    SZrAstNode *modulePath;
    SZrAstNode *bindingNode;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *alias;
    const SZrSemanticVisibleSymbolFact *visible;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *aliasRelation;
    const SZrParserSemanticRelationQuery *originRelation;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_VARIABLE_DECLARATION, ast->data.script.statements->nodes[0]->type);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[0]->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_IMPORT_EXPRESSION,
            ast->data.script.statements->nodes[0]->data.variableDeclaration.value->type);
    modulePath = ast->data.script.statements->nodes[0]->data.variableDeclaration.value->data
            .importExpression.modulePath;
    TEST_ASSERT_NOT_NULL(modulePath);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, modulePath->type);
    TEST_ASSERT_NOT_NULL(modulePath->data.stringLiteral.value);
    TEST_ASSERT_EQUAL_STRING(
            "zr.math",
            ZrCore_String_GetNativeString(modulePath->data.stringLiteral.value));
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[0]->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_DESTRUCTURING_OBJECT,
            ast->data.script.statements->nodes[0]->data.variableDeclaration.pattern->type);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[0]->data.variableDeclaration.pattern->data
                    .destructuringObject.keys);
    TEST_ASSERT_NOT_NULL(
            ast->data.script.statements->nodes[0]->data.variableDeclaration.pattern->data
                    .destructuringObject.keys->nodes[0]);
    bindingNode = ast->data.script.statements->nodes[0]->data.variableDeclaration.pattern->data
            .destructuringObject.keys->nodes[0]->data.keyValuePair.key;
    TEST_ASSERT_NOT_NULL(bindingNode);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, modulePath->type);
    TEST_ASSERT_NOT_NULL(modulePath->data.stringLiteral.value);
    TEST_ASSERT_EQUAL_STRING(
            "zr.math",
            ZrCore_String_GetNativeString(modulePath->data.stringLiteral.value));
    alias = relation_find_symbol_by_node(cs.semanticContext, bindingNode);
    TEST_ASSERT_NOT_NULL(alias);
    visible = relation_find_visible_fact(cs.semanticContext, alias->id);
    TEST_ASSERT_NOT_NULL(visible);
    TEST_ASSERT_TRUE(visible->isImport);
    TEST_ASSERT_NOT_NULL(visible->externalOriginUri);
    TEST_ASSERT_EQUAL_STRING(
            "zr.math", ZrCore_String_GetNativeString(visible->externalOriginUri));
    TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishImportOrigins(
            cs.semanticContext));
    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, alias->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    aliasRelation = relation_at(&relations, 0U);
    originRelation = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(aliasRelation);
    TEST_ASSERT_NOT_NULL(originRelation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET,
                          aliasRelation->kind);
    TEST_ASSERT_EQUAL_UINT(alias->id, aliasRelation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID,
                           aliasRelation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, aliasRelation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, aliasRelation->targetTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                          originRelation->kind);
    TEST_ASSERT_EQUAL_UINT(alias->id, originRelation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID,
                           originRelation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, originRelation->sourceTypeId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, originRelation->targetTypeId);
    TEST_ASSERT_TRUE(originRelation->isExternal);
    TEST_ASSERT_NOT_NULL(originRelation->externalOriginUri);
    TEST_ASSERT_EQUAL_STRING(
            "zr.math",
            ZrCore_String_GetNativeString(originRelation->externalOriginUri));

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compiled_direct_import_publishes_external_origin_relation(void) {
    const TZrChar *source =
            "var math = import(\"zr.math\");\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_relation_direct_import.zr");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrCompilerState cs;
    const SZrSemanticSymbolRecord *alias;
    SZrArray relations;
    const SZrParserSemanticRelationQuery *aliasRelation;
    const SZrParserSemanticRelationQuery *originRelation;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);

    TEST_ASSERT_FALSE_MESSAGE(cs.hasError, cs.errorMessage);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    alias = relation_find_symbol_by_node(cs.semanticContext, declaration);
    TEST_ASSERT_NOT_NULL(alias);
    ZrCore_Array_Construct(&relations);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_RelationsOfSymbol(
            cs.semanticContext, alias->id, ZR_NULL, &relations));
    TEST_ASSERT_EQUAL_UINT(2U, relations.length);
    aliasRelation = relation_at(&relations, 0U);
    originRelation = relation_at(&relations, 1U);
    TEST_ASSERT_NOT_NULL(aliasRelation);
    TEST_ASSERT_NOT_NULL(originRelation);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_ALIAS_TARGET,
                          aliasRelation->kind);
    TEST_ASSERT_EQUAL_UINT(alias->id, aliasRelation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(ZR_SEMANTIC_ID_INVALID,
                           aliasRelation->targetSymbolId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, aliasRelation->targetTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
                          originRelation->kind);
    TEST_ASSERT_EQUAL_UINT(alias->id, originRelation->sourceSymbolId);
    TEST_ASSERT_EQUAL_UINT(alias->typeId, originRelation->targetTypeId);
    TEST_ASSERT_TRUE(originRelation->isExternal);
    TEST_ASSERT_NOT_NULL(originRelation->externalOriginUri);
    TEST_ASSERT_EQUAL_STRING(
            "zr.math",
            ZrCore_String_GetNativeString(originRelation->externalOriginUri));

    ZrCore_Array_Free(g_state, &relations);
    relation_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#include "test_semantic_query_meta_override_cases.h"
#include "test_semantic_query_relation_endpoint_identity_cases.h"
#include "test_semantic_query_relation_source_identity_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_relations_of_symbol_projects_sorted_snapshot_edges);
    RUN_TEST(test_relation_append_deduplicates_exact_edges_and_preserves_multiple_definitions);
    RUN_TEST(test_external_relation_requires_and_projects_virtual_declaration_uri);
    RUN_TEST(test_type_and_implementation_queries_preserve_edge_direction);
    RUN_TEST(test_relation_queries_project_canonical_module_identities);
    RUN_TEST(test_type_declaration_relation_publishes_identity_edge_once);
    RUN_TEST(test_relations_of_symbol_honors_node_scope);
    RUN_TEST(test_property_contracts_publish_accessor_relations_once);
    RUN_TEST(test_property_contract_relations_reject_mismatched_accessor_atomically);
    RUN_TEST(test_reference_definitions_publish_declaration_edges_once);
    RUN_TEST(test_compiled_source_publishes_reference_definition_relations);
    RUN_TEST(test_compiled_source_publishes_type_hierarchy_relations);
    RUN_TEST(test_compiled_source_publishes_interface_member_implementation);
    RUN_TEST(test_compiled_source_publishes_override_relation);
    RUN_TEST(test_compiled_source_publishes_meta_function_override_relation);
    RUN_TEST(test_compiled_source_publishes_explicit_constructor_relation);
    RUN_TEST(test_compiled_source_omits_synthesized_constructor_relation);
    RUN_TEST(test_compiled_type_value_alias_publishes_canonical_target_relation);
    RUN_TEST(test_compiled_import_publishes_external_origin_relation);
    RUN_TEST(test_compiled_direct_import_publishes_external_origin_relation);
    RUN_TEST(test_relation_node_scope_fails_closed_for_missing_fact_source);
    RUN_TEST(test_relation_queries_sort_line_only_ranges_independent_of_append_order);
    RUN_TEST(test_relation_append_requires_identity_for_both_endpoints);
    return UNITY_END();
}
