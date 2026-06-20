#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

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

static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    TZrChar sourceName[] = "semantic_query_test.zr";
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, sourceName, strlen(sourceName));
    return range;
}

static void init_node(SZrAstNode *node,
                      EZrAstNodeType type,
                      TZrSize startOffset,
                      TZrSize endOffset) {
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->location = test_range(startOffset, endOffset);
}

static void append_expression_fact(SZrSemanticContext *context,
                                   SZrAstNode *node,
                                   EZrValueType valueType) {
    SZrInferredType inferredType;
    SZrSemanticExpressionFact fact;

    ZrParser_InferredType_Init(g_state, &inferredType, valueType);
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    ZrParser_InferredType_Copy(g_state, &fact.inferredType, &inferredType);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &fact));
    ZrParser_InferredType_Free(g_state, &inferredType);
}

static void append_reference_fact(SZrSemanticContext *context,
                                  SZrAstNode *node,
                                  EZrSemanticReferenceKind kind,
                                  TZrSymbolId symbolId,
                                  SZrFileRange declarationRange) {
    SZrSemanticReferenceFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.declarationRange = declarationRange;
    fact.kind = kind;
    fact.symbolId = symbolId;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
}

static void test_semantic_query_type_at_copies_narrowest_expression_type(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode outerNode;
    SZrAstNode innerNode;
    SZrInferredType queriedType;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&outerNode, ZR_AST_BINARY_EXPRESSION, 0, 20);
    init_node(&innerNode, ZR_AST_INTEGER_LITERAL, 4, 6);
    append_expression_fact(context, &outerNode, ZR_VALUE_TYPE_BOOL);
    append_expression_fact(context, &innerNode, ZR_VALUE_TYPE_INT64);
    memset(&queriedType, 0, sizeof(queriedType));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_TypeAt(context,
                                                   test_range(5, 5),
                                                   ZR_NULL,
                                                   &queriedType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, queriedType.baseType);

    ZrParser_InferredType_Free(g_state, &queriedType);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_facts_at_collects_matching_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode node;
    SZrSemanticNumericFact numericFact;
    SZrSemanticReachabilityFact reachabilityFact;
    SZrSemanticLogicalFact logicalFact;
    SZrSemanticOwnershipFact ownershipFact;
    SZrParserSemanticQueryFacts facts;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&node, ZR_AST_INTEGER_LITERAL, 8, 12);
    append_expression_fact(context, &node, ZR_VALUE_TYPE_INT64);

    memset(&numericFact, 0, sizeof(numericFact));
    numericFact.node = &node;
    numericFact.range = node.location;
    numericFact.kind = ZR_SEMANTIC_NUMERIC_FACT_LITERAL;
    numericFact.exactness = ZR_SEMANTIC_FACT_EXACT;
    numericFact.sourceType = ZR_VALUE_TYPE_INT64;
    numericFact.targetType = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendNumeric(context, &numericFact));

    memset(&reachabilityFact, 0, sizeof(reachabilityFact));
    reachabilityFact.node = &node;
    reachabilityFact.range = node.location;
    reachabilityFact.state = ZR_SEMANTIC_REACHABILITY_REACHABLE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReachability(context, &reachabilityFact));

    memset(&logicalFact, 0, sizeof(logicalFact));
    logicalFact.node = &node;
    logicalFact.range = node.location;
    logicalFact.kind = ZR_SEMANTIC_LOGICAL_FACT_TRUTHY;
    logicalFact.exactness = ZR_SEMANTIC_FACT_APPROXIMATE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendLogical(context, &logicalFact));

    memset(&ownershipFact, 0, sizeof(ownershipFact));
    ownershipFact.node = &node;
    ownershipFact.range = node.location;
    ownershipFact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_COPY;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendOwnership(context, &ownershipFact));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FactsAt(context,
                                                    test_range(10, 10),
                                                    ZR_NULL,
                                                    &facts));
    TEST_ASSERT_NOT_NULL(facts.expression);
    TEST_ASSERT_NOT_NULL(facts.numeric);
    TEST_ASSERT_NOT_NULL(facts.reachability);
    TEST_ASSERT_NOT_NULL(facts.logical);
    TEST_ASSERT_NOT_NULL(facts.ownership);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_LITERAL, facts.numeric->kind);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_definition_of_returns_matching_declaration(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode readNode;
    const SZrSemanticReferenceFact *definition;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&declarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);
    init_node(&readNode, ZR_AST_IDENTIFIER_LITERAL, 10, 13);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          42,
                          declarationNode.location);
    append_reference_fact(context,
                          &readNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          42,
                          declarationNode.location);

    definition = ZrParser_SemanticQuery_DefinitionOf(context, test_range(11, 11), ZR_NULL);
    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_DECLARATION, definition->kind);
    TEST_ASSERT_EQUAL_UINT32(42, definition->symbolId);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)definition->range.start.offset);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_references_of_collects_symbol_references_in_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode declarationNode;
    SZrAstNode readNode;
    SZrAstNode writeNode;
    SZrAstNode otherNode;
    SZrParserSemanticQueryScope scope;
    SZrArray references;
    const SZrSemanticReferenceFact **referenceSlot;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 15);
    init_node(&declarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);
    init_node(&readNode, ZR_AST_IDENTIFIER_LITERAL, 10, 13);
    init_node(&writeNode, ZR_AST_IDENTIFIER_LITERAL, 20, 23);
    init_node(&otherNode, ZR_AST_IDENTIFIER_LITERAL, 4, 7);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          77,
                          declarationNode.location);
    append_reference_fact(context,
                          &readNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          77,
                          declarationNode.location);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          77,
                          declarationNode.location);
    append_reference_fact(context,
                          &otherNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          88,
                          otherNode.location);

    ZrCore_Array_Construct(&references);
    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ReferencesOf(context, 77, &scope, &references));
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)references.length);

    referenceSlot = (const SZrSemanticReferenceFact **)ZrCore_Array_Get(&references, 0);
    TEST_ASSERT_NOT_NULL(referenceSlot);
    TEST_ASSERT_NOT_NULL(*referenceSlot);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_DECLARATION, (*referenceSlot)->kind);

    referenceSlot = (const SZrSemanticReferenceFact **)ZrCore_Array_Get(&references, 1);
    TEST_ASSERT_NOT_NULL(referenceSlot);
    TEST_ASSERT_NOT_NULL(*referenceSlot);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_READ, (*referenceSlot)->kind);

    ZrCore_Array_Free(g_state, &references);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_references_of_clears_reused_output_when_missing(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode readNode;
    SZrArray references;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&declarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);
    init_node(&readNode, ZR_AST_IDENTIFIER_LITERAL, 10, 13);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          91,
                          declarationNode.location);
    append_reference_fact(context,
                          &readNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          91,
                          declarationNode.location);

    ZrCore_Array_Construct(&references);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ReferencesOf(context, 91, ZR_NULL, &references));
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)references.length);

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_ReferencesOf(context, 92, ZR_NULL, &references));
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)references.length);

    ZrCore_Array_Free(g_state, &references);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_node_scope_filters_outside_range(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode innerNode;
    SZrAstNode outerNode;
    SZrParserSemanticQueryScope scope;
    SZrInferredType queriedType;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 8);
    init_node(&innerNode, ZR_AST_INTEGER_LITERAL, 2, 4);
    init_node(&outerNode, ZR_AST_BOOLEAN_LITERAL, 12, 14);
    append_expression_fact(context, &innerNode, ZR_VALUE_TYPE_INT64);
    append_expression_fact(context, &outerNode, ZR_VALUE_TYPE_BOOL);
    memset(&queriedType, 0, sizeof(queriedType));

    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_TypeAt(context,
                                                   test_range(3, 3),
                                                   &scope,
                                                   &queriedType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, queriedType.baseType);
    ZrParser_InferredType_Free(g_state, &queriedType);
    memset(&queriedType, 0, sizeof(queriedType));

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_TypeAt(context,
                                                    test_range(13, 13),
                                                    &scope,
                                                    &queriedType));

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_diagnostics_returns_empty_foundation_list(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    memset(&diagnostics, 0xff, sizeof(diagnostics));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, ZR_NULL, &diagnostics));
    TEST_ASSERT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)diagnostics.count);

    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_query_type_at_copies_narrowest_expression_type);
    RUN_TEST(test_semantic_query_facts_at_collects_matching_facts);
    RUN_TEST(test_semantic_query_definition_of_returns_matching_declaration);
    RUN_TEST(test_semantic_query_references_of_collects_symbol_references_in_scope);
    RUN_TEST(test_semantic_query_references_of_clears_reused_output_when_missing);
    RUN_TEST(test_semantic_query_node_scope_filters_outside_range);
    RUN_TEST(test_semantic_query_diagnostics_returns_empty_foundation_list);
    return UNITY_END();
}
