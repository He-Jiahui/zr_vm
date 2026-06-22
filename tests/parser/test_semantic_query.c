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

static void append_reference_fact_with_definite_assignment(
        SZrSemanticContext *context,
        SZrAstNode *node,
        EZrSemanticReferenceKind kind,
        TZrSymbolId symbolId,
        SZrFileRange declarationRange,
        EZrSemanticDefiniteAssignmentState state) {
    SZrSemanticReferenceFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.declarationRange = declarationRange;
    fact.kind = kind;
    fact.symbolId = symbolId;
    fact.isResolved = ZR_TRUE;
    fact.hasDefiniteAssignmentState = ZR_TRUE;
    fact.definiteAssignmentState = state;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));
}

static void append_reachability_fact(SZrSemanticContext *context,
                                     SZrAstNode *node,
                                     EZrSemanticReachabilityState state,
                                     EZrSemanticReachabilityCause cause,
                                     SZrAstNode *causeNode) {
    SZrSemanticReachabilityFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.state = state;
    fact.cause = cause;
    fact.causeNode = causeNode;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReachability(context, &fact));
}

static void append_overflow_numeric_fact(SZrSemanticContext *context,
                                         SZrAstNode *node) {
    SZrSemanticNumericFact fact;

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ZR_SEMANTIC_NUMERIC_FACT_PROMOTION;
    fact.exactness = ZR_SEMANTIC_FACT_APPROXIMATE;
    fact.sourceType = ZR_VALUE_TYPE_INT64;
    fact.targetType = ZR_VALUE_TYPE_INT64;
    fact.mayOverflow = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendNumeric(context, &fact));
}

static void append_array_bounds_expression_fact(SZrSemanticContext *context,
                                                SZrAstNode *node,
                                                const TZrChar *message) {
    SZrInferredType inferredType;
    SZrSemanticExpressionFact fact;

    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_MEMBER;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    ZrParser_InferredType_Copy(g_state, &fact.inferredType, &inferredType);
    fact.hasMemberInfo = ZR_TRUE;
    fact.memberRange = node->location;
    fact.memberIsComputed = ZR_TRUE;
    fact.diagnosticMessage = ZrCore_String_Create(g_state, (TZrNativeString)message, strlen(message));

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &fact));
    ZrParser_InferredType_Free(g_state, &inferredType);
}

static void assert_zr_string_equals(const TZrChar *expected, SZrString *actual) {
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, ZrCore_String_GetNativeString(actual));
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

static void test_semantic_query_definition_of_prefers_reaching_write_definition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode writeNode;
    SZrAstNode readNode;
    const SZrSemanticReferenceFact *definition;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&declarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);
    init_node(&writeNode, ZR_AST_IDENTIFIER_LITERAL, 10, 13);
    init_node(&readNode, ZR_AST_IDENTIFIER_LITERAL, 20, 23);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          43,
                          declarationNode.location);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          43,
                          declarationNode.location);
    append_reference_fact(context,
                          &readNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          43,
                          declarationNode.location);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(context));

    definition = ZrParser_SemanticQuery_DefinitionOf(context, test_range(21, 21), ZR_NULL);
    TEST_ASSERT_NOT_NULL(definition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_WRITE, definition->kind);
    TEST_ASSERT_EQUAL_UINT32(43, definition->symbolId);
    TEST_ASSERT_EQUAL_UINT32(10, (TZrUInt32)definition->range.start.offset);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)definition->declarationRange.start.offset);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_definitions_of_returns_multiple_reaching_writes(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode declarationNode;
    SZrAstNode writeNodeA;
    SZrAstNode writeNodeB;
    SZrAstNode readNode;
    SZrSemanticReferenceFact *readFact;
    SZrArray definitions;
    const SZrSemanticReferenceFact **definitionSlot;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&declarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);
    init_node(&writeNodeA, ZR_AST_IDENTIFIER_LITERAL, 10, 13);
    init_node(&writeNodeB, ZR_AST_IDENTIFIER_LITERAL, 20, 23);
    init_node(&readNode, ZR_AST_IDENTIFIER_LITERAL, 30, 33);

    append_reference_fact(context,
                          &declarationNode,
                          ZR_SEMANTIC_REFERENCE_DECLARATION,
                          44,
                          declarationNode.location);
    append_reference_fact(context,
                          &writeNodeA,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          44,
                          declarationNode.location);
    append_reference_fact(context,
                          &writeNodeB,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          44,
                          declarationNode.location);
    append_reference_fact(context,
                          &readNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          44,
                          declarationNode.location);

    readFact = (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, 3);
    TEST_ASSERT_NOT_NULL(readFact);
    readFact->hasDefinitionRange = ZR_FALSE;
    ZrCore_Array_Init(g_state, &readFact->definitionRanges, sizeof(SZrFileRange), 2);
    ZrCore_Array_Push(g_state, &readFact->definitionRanges, &writeNodeB.location);
    ZrCore_Array_Push(g_state, &readFact->definitionRanges, &writeNodeA.location);

    ZrCore_Array_Construct(&definitions);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_DefinitionsOf(context,
                                                          test_range(31, 31),
                                                          ZR_NULL,
                                                          &definitions));
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)definitions.length);

    definitionSlot = (const SZrSemanticReferenceFact **)ZrCore_Array_Get(&definitions, 0);
    TEST_ASSERT_NOT_NULL(definitionSlot);
    TEST_ASSERT_NOT_NULL(*definitionSlot);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_WRITE, (*definitionSlot)->kind);
    TEST_ASSERT_EQUAL_UINT32(10, (TZrUInt32)(*definitionSlot)->range.start.offset);

    definitionSlot = (const SZrSemanticReferenceFact **)ZrCore_Array_Get(&definitions, 1);
    TEST_ASSERT_NOT_NULL(definitionSlot);
    TEST_ASSERT_NOT_NULL(*definitionSlot);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_WRITE, (*definitionSlot)->kind);
    TEST_ASSERT_EQUAL_UINT32(20, (TZrUInt32)(*definitionSlot)->range.start.offset);

    ZrCore_Array_Free(g_state, &definitions);
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

static void test_semantic_query_diagnostics_returns_empty_when_no_diagnostic_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    memset(&diagnostics, 0xff, sizeof(diagnostics));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, ZR_NULL, &diagnostics));
    TEST_ASSERT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)diagnostics.count);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_diagnostics_maps_unreachable_reachability_facts_in_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode returnNode;
    SZrAstNode unreachableNode;
    SZrAstNode reachableNode;
    SZrAstNode outsideNode;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 30);
    init_node(&returnNode, ZR_AST_RETURN_STATEMENT, 4, 9);
    init_node(&unreachableNode, ZR_AST_IDENTIFIER_LITERAL, 12, 16);
    init_node(&reachableNode, ZR_AST_IDENTIFIER_LITERAL, 18, 22);
    init_node(&outsideNode, ZR_AST_IDENTIFIER_LITERAL, 40, 44);

    append_reachability_fact(context,
                             &unreachableNode,
                             ZR_SEMANTIC_REACHABILITY_UNREACHABLE,
                             ZR_SEMANTIC_REACHABILITY_AFTER_RETURN,
                             &returnNode);
    append_reachability_fact(context,
                             &reachableNode,
                             ZR_SEMANTIC_REACHABILITY_REACHABLE,
                             ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN,
                             ZR_NULL);
    append_reachability_fact(context,
                             &outsideNode,
                             ZR_SEMANTIC_REACHABILITY_UNREACHABLE,
                             ZR_SEMANTIC_REACHABILITY_AFTER_THROW,
                             ZR_NULL);

    memset(&diagnostics, 0, sizeof(diagnostics));
    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(12, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("unreachable_code", diagnostics.items[0].code);
    assert_zr_string_equals("Unreachable code", diagnostics.items[0].message);
    TEST_ASSERT_NOT_NULL(diagnostics.items[0].cause);
    TEST_ASSERT_NOT_NULL(diagnostics.items[0].suggestion);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_diagnostics_maps_definite_assignment_read_facts_in_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode maybeReadNode;
    SZrAstNode uninitReadNode;
    SZrAstNode initReadNode;
    SZrAstNode outsideReadNode;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    SZrFileRange declarationRange;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 40);
    init_node(&maybeReadNode, ZR_AST_IDENTIFIER_LITERAL, 8, 12);
    init_node(&uninitReadNode, ZR_AST_IDENTIFIER_LITERAL, 16, 20);
    init_node(&initReadNode, ZR_AST_IDENTIFIER_LITERAL, 24, 28);
    init_node(&outsideReadNode, ZR_AST_IDENTIFIER_LITERAL, 60, 64);
    declarationRange = test_range(2, 6);

    append_reference_fact_with_definite_assignment(context,
                                                   &maybeReadNode,
                                                   ZR_SEMANTIC_REFERENCE_READ,
                                                   77,
                                                   declarationRange,
                                                   ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT);
    append_reference_fact_with_definite_assignment(context,
                                                   &uninitReadNode,
                                                   ZR_SEMANTIC_REFERENCE_READ,
                                                   77,
                                                   declarationRange,
                                                   ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT);
    append_reference_fact_with_definite_assignment(context,
                                                   &initReadNode,
                                                   ZR_SEMANTIC_REFERENCE_READ,
                                                   77,
                                                   declarationRange,
                                                   ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT);
    append_reference_fact_with_definite_assignment(context,
                                                   &outsideReadNode,
                                                   ZR_SEMANTIC_REFERENCE_READ,
                                                   77,
                                                   declarationRange,
                                                   ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT);

    memset(&diagnostics, 0, sizeof(diagnostics));
    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("possibly_uninitialized_read", diagnostics.items[0].code);
    assert_zr_string_equals("Variable may be read before assignment", diagnostics.items[0].message);
    TEST_ASSERT_TRUE(diagnostics.items[0].relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.items[0].relatedInformation.length);
    {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&diagnostics.items[0].relatedInformation,
                        0);
        TEST_ASSERT_NOT_NULL(related);
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)related->location.start.offset);
        assert_zr_string_equals("Variable declaration is here", related->message);
    }
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostics.items[1].severity);
    TEST_ASSERT_EQUAL_UINT32(16, (TZrUInt32)diagnostics.items[1].location.start.offset);
    assert_zr_string_equals("uninitialized_read", diagnostics.items[1].code);
    assert_zr_string_equals("Variable is read before assignment", diagnostics.items[1].message);
    TEST_ASSERT_TRUE(diagnostics.items[1].relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.items[1].relatedInformation.length);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_diagnostics_maps_numeric_overflow_facts_in_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode overflowNode;
    SZrAstNode outsideOverflowNode;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 30);
    init_node(&overflowNode, ZR_AST_BINARY_EXPRESSION, 8, 20);
    init_node(&outsideOverflowNode, ZR_AST_BINARY_EXPRESSION, 40, 52);

    append_overflow_numeric_fact(context, &overflowNode);
    append_overflow_numeric_fact(context, &outsideOverflowNode);

    memset(&diagnostics, 0, sizeof(diagnostics));
    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("numeric_overflow", diagnostics.items[0].code);
    assert_zr_string_equals("Numeric expression may overflow", diagnostics.items[0].message);
    TEST_ASSERT_NOT_NULL(diagnostics.items[0].cause);
    TEST_ASSERT_NOT_NULL(diagnostics.items[0].suggestion);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_diagnostics_maps_array_bounds_facts_in_scope(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode boundsNode;
    SZrAstNode outsideBoundsNode;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 30);
    init_node(&boundsNode, ZR_AST_PRIMARY_EXPRESSION, 8, 20);
    init_node(&outsideBoundsNode, ZR_AST_PRIMARY_EXPRESSION, 40, 52);

    append_array_bounds_expression_fact(context,
                                        &boundsNode,
                                        "Array index 2 is out of bounds (array size: 2)");
    append_array_bounds_expression_fact(context,
                                        &outsideBoundsNode,
                                        "Array index 4 is out of bounds (array size: 2)");

    memset(&diagnostics, 0, sizeof(diagnostics));
    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("array_index_out_of_bounds", diagnostics.items[0].code);
    assert_zr_string_equals("Array index 2 is out of bounds (array size: 2)",
                            diagnostics.items[0].message);
    TEST_ASSERT_NOT_NULL(diagnostics.items[0].cause);
    TEST_ASSERT_NOT_NULL(diagnostics.items[0].suggestion);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_diagnostics_consumes_linear_definite_assignment_resolution(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode scopeNode;
    SZrAstNode declarationNode;
    SZrAstNode readBeforeWriteNode;
    SZrAstNode writeNode;
    SZrAstNode readAfterWriteNode;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    SZrFileRange declarationRange;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&scopeNode, ZR_AST_BLOCK, 0, 40);
    init_node(&declarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 4);
    init_node(&readBeforeWriteNode, ZR_AST_IDENTIFIER_LITERAL, 8, 12);
    init_node(&writeNode, ZR_AST_IDENTIFIER_LITERAL, 16, 20);
    init_node(&readAfterWriteNode, ZR_AST_IDENTIFIER_LITERAL, 24, 28);
    declarationRange = declarationNode.location;

    append_reference_fact_with_definite_assignment(context,
                                                   &declarationNode,
                                                   ZR_SEMANTIC_REFERENCE_DECLARATION,
                                                   81,
                                                   declarationRange,
                                                   ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT);
    append_reference_fact(context,
                          &readBeforeWriteNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          81,
                          declarationRange);
    append_reference_fact(context,
                          &writeNode,
                          ZR_SEMANTIC_REFERENCE_WRITE,
                          81,
                          declarationRange);
    append_reference_fact(context,
                          &readAfterWriteNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          81,
                          declarationRange);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(context));

    memset(&diagnostics, 0, sizeof(diagnostics));
    ZrParser_SemanticQueryScope_Node(&scope, &scopeNode);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("uninitialized_read", diagnostics.items[0].code);

    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_query_type_at_copies_narrowest_expression_type);
    RUN_TEST(test_semantic_query_facts_at_collects_matching_facts);
    RUN_TEST(test_semantic_query_definition_of_returns_matching_declaration);
    RUN_TEST(test_semantic_query_definition_of_prefers_reaching_write_definition);
    RUN_TEST(test_semantic_query_definitions_of_returns_multiple_reaching_writes);
    RUN_TEST(test_semantic_query_references_of_collects_symbol_references_in_scope);
    RUN_TEST(test_semantic_query_references_of_clears_reused_output_when_missing);
    RUN_TEST(test_semantic_query_node_scope_filters_outside_range);
    RUN_TEST(test_semantic_query_diagnostics_returns_empty_when_no_diagnostic_facts);
    RUN_TEST(test_semantic_query_diagnostics_maps_unreachable_reachability_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_maps_definite_assignment_read_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_maps_numeric_overflow_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_maps_array_bounds_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_consumes_linear_definite_assignment_resolution);
    return UNITY_END();
}
