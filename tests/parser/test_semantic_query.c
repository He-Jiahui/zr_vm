#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/diagnostic_messages.h"
#include "zr_vm_parser/diagnostic_registry.h"
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

static void test_semantic_query_owner_hover_and_diagnostic_share_canonical_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *moduleIdentity = ZrCore_String_CreateFromNative(
            g_state, "app.resource");
    SZrString *resourceName = ZrCore_String_CreateFromNative(
            g_state, "Socket");
    SZrAstNode movedNode;
    SZrAstNode useNode;
    SZrSemanticExpressionFact expressionFact;
    SZrSemanticOwnershipFact ownershipFact;
    SZrParserSemanticTypeQuery typeQuery;
    SZrParserSemanticQueryFacts facts;
    SZrParserSemanticQueryDiagnostics diagnostics;
    TZrTypeId resourceType;
    TZrTypeId uniqueType;
    TZrChar display[128];

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(moduleIdentity);
    TEST_ASSERT_NOT_NULL(resourceName);
    resourceType = ZrParser_CanonicalType_InternNominal(
            context, moduleIdentity, resourceName, 0x02000001u);
    uniqueType = ZrParser_CanonicalType_InternOwner(
            context, resourceType, ZR_CANONICAL_OWNER_UNIQUE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, resourceType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, uniqueType);

    init_node(&movedNode, ZR_AST_IDENTIFIER_LITERAL, 2, 7);
    init_node(&useNode, ZR_AST_IDENTIFIER_LITERAL, 12, 17);
    memset(&expressionFact, 0, sizeof(expressionFact));
    expressionFact.node = &useNode;
    expressionFact.range = useNode.location;
    expressionFact.kind = ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER;
    expressionFact.exactness = ZR_SEMANTIC_FACT_EXACT;
    expressionFact.typeId = uniqueType;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(
            context, &expressionFact));

    memset(&ownershipFact, 0, sizeof(ownershipFact));
    ownershipFact.node = &useNode;
    ownershipFact.relatedNode = &movedNode;
    ownershipFact.range = useNode.location;
    ownershipFact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_ERROR;
    ownershipFact.qualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
    ownershipFact.symbolId = 701u;
    ownershipFact.isViolation = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendOwnership(
            context, &ownershipFact));

    memset(&typeQuery, 0, sizeof(typeQuery));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(
            context, test_range(14, 14), ZR_NULL, &typeQuery));
    TEST_ASSERT_EQUAL_UINT32(uniqueType, typeQuery.typeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            context, typeQuery.typeId, display, sizeof(display)));
    TEST_ASSERT_EQUAL_STRING("Unique<app.resource.Socket>", display);

    memset(&facts, 0, sizeof(facts));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FactsAt(
            context, test_range(14, 14), ZR_NULL, &facts));
    TEST_ASSERT_NOT_NULL(facts.ownership);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_OWNERSHIP_FACT_ERROR, facts.ownership->kind);

    memset(&diagnostics, 0, sizeof(diagnostics));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(
            context, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(
            context, ZR_NULL, &diagnostics));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)diagnostics.count);
    assert_zr_string_equals("use_after_move", diagnostics.items[0].code);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)diagnostics.items[0].relatedInformation.length);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_query_canonical_type_at_reads_type_reference_fact(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *moduleIdentity = ZrCore_String_CreateFromNative(g_state, "app.resource");
    SZrString *resourceName = ZrCore_String_CreateFromNative(g_state, "Socket");
    SZrAstNode typeNode;
    SZrSemanticReferenceFact fact;
    SZrParserSemanticTypeQuery query;
    TZrTypeId resourceType;
    TZrTypeId uniqueType;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(moduleIdentity);
    TEST_ASSERT_NOT_NULL(resourceName);
    resourceType = ZrParser_CanonicalType_InternNominal(
            context, moduleIdentity, resourceName, 0x02000001u);
    uniqueType = ZrParser_CanonicalType_InternOwner(
            context, resourceType, ZR_CANONICAL_OWNER_UNIQUE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, uniqueType);

    init_node(&typeNode, ZR_AST_GENERIC_TYPE, 4, 18);
    memset(&fact, 0, sizeof(fact));
    fact.node = &typeNode;
    fact.range = typeNode.location;
    fact.kind = ZR_SEMANTIC_REFERENCE_TYPE;
    fact.typeId = uniqueType;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    memset(&query, 0, sizeof(query));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(
            context, test_range(6, 6), ZR_NULL, &query));
    TEST_ASSERT_EQUAL_UINT32(uniqueType, query.typeId);
    TEST_ASSERT_NOT_NULL(query.reference);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_TYPE, query.reference->kind);

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

static void test_semantic_query_declaration_of_uses_exact_symbol_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode callableDeclarationNode;
    SZrAstNode returnDeclarationNode;
    SZrSemanticReferenceFact fact;
    SZrSemanticReferenceFact *returnDeclaration;
    const SZrSemanticReferenceFact *declaration;

    TEST_ASSERT_NOT_NULL(context);
    init_node(&callableDeclarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);
    init_node(&returnDeclarationNode, ZR_AST_IDENTIFIER_LITERAL, 0, 3);

    memset(&fact, 0, sizeof(fact));
    fact.node = &callableDeclarationNode;
    fact.range = callableDeclarationNode.location;
    fact.declarationRange = callableDeclarationNode.location;
    fact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    fact.symbolId = 11;
    fact.typeId = 8;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    memset(&fact, 0, sizeof(fact));
    fact.node = &returnDeclarationNode;
    fact.range = returnDeclarationNode.location;
    fact.declarationRange = returnDeclarationNode.location;
    fact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    fact.symbolId = 12;
    fact.typeId = 7;
    fact.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &fact));

    declaration = ZrParser_SemanticQuery_DeclarationOf(context, 12, ZR_NULL);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_UINT32(12, declaration->symbolId);
    TEST_ASSERT_EQUAL_UINT32(7, declaration->typeId);

    returnDeclaration = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
            &context->referenceFacts, 1);
    TEST_ASSERT_NOT_NULL(returnDeclaration);
    returnDeclaration->isResolved = ZR_FALSE;
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_DeclarationOf(context, 12, ZR_NULL));

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

static void test_semantic_query_references_of_clears_reused_output_when_symbol_is_invalid(void) {
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
                          93,
                          declarationNode.location);
    append_reference_fact(context,
                          &readNode,
                          ZR_SEMANTIC_REFERENCE_READ,
                          93,
                          declarationNode.location);

    ZrCore_Array_Construct(&references);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ReferencesOf(context, 93, ZR_NULL, &references));
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)references.length);

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_ReferencesOf(
            context, ZR_SEMANTIC_ID_INVALID, ZR_NULL, &references));
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
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
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
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("possibly_uninitialized_read", diagnostics.items[0].code);
    assert_zr_string_equals("Variable may be read before assignment", diagnostics.items[0].message);
    TEST_ASSERT_TRUE(diagnostics.items[0].relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.items[0].relatedInformation.length);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, diagnostics.items[0].descriptorId);
    TEST_ASSERT_TRUE(diagnostics.items[0].fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.items[0].fixes.length);
    {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&diagnostics.items[0].relatedInformation,
                        0);
        const SZrStructuredDiagnosticFix *fix =
                (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
                        (SZrArray *)&diagnostics.items[0].fixes,
                        0);
        TEST_ASSERT_NOT_NULL(related);
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)related->location.start.offset);
        assert_zr_string_equals("Variable declaration is here", related->message);
        TEST_ASSERT_NOT_NULL(fix);
        assert_zr_string_equals("Replace with an initialized value", fix->title);
        assert_zr_string_equals("<value>", fix->editText);
        TEST_ASSERT_EQUAL_INT(ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS, fix->applicability);
        TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)fix->editRange.start.offset);
        TEST_ASSERT_EQUAL_UINT32(12, (TZrUInt32)fix->editRange.end.offset);
    }
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostics.items[1].severity);
    TEST_ASSERT_EQUAL_UINT32(16, (TZrUInt32)diagnostics.items[1].location.start.offset);
    assert_zr_string_equals("uninitialized_read", diagnostics.items[1].code);
    assert_zr_string_equals("Variable is read before assignment", diagnostics.items[1].message);
    TEST_ASSERT_TRUE(diagnostics.items[1].relatedInformation.isValid);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.items[1].relatedInformation.length);
    TEST_ASSERT_TRUE(diagnostics.items[1].fixes.isValid);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.items[1].fixes.length);

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
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
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
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
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
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)diagnostics.count);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR, diagnostics.items[0].severity);
    TEST_ASSERT_EQUAL_UINT32(8, (TZrUInt32)diagnostics.items[0].location.start.offset);
    assert_zr_string_equals("uninitialized_read", diagnostics.items[0].code);

    ZrParser_SemanticContext_Free(context);
}

static void test_diagnostic_registry_assigns_stable_descriptors(void) {
    const SZrDiagnosticDescriptor *possibleUninitialized;
    const SZrDiagnosticDescriptor *typeMismatch;
    const SZrDiagnosticDescriptor *constAssignment;
    const SZrDiagnosticDescriptor *invalidVariance;
    const SZrDiagnosticDescriptor *constInterfaceMismatch;
    const SZrDiagnosticDescriptor *unresolvedReference;
    const SZrDiagnosticDescriptor *memberNotFound;
    const SZrDiagnosticDescriptor *initializerRequiresAnnotation;
    const SZrDiagnosticDescriptor *returnTypeNotProvable;
    const SZrDiagnosticDescriptor *invalidDecorator;
    const SZrDiagnosticDescriptor *useAfterMove;
    const SZrDiagnosticDescriptor *reservedOwnershipIntrinsicName;
    const SZrDiagnosticDescriptor *ownershipIntrinsicCallRequired;
    const SZrDiagnosticDescriptor *ownershipIntrinsicArityMismatch;
    SZrStructuredDiagnostic diagnostic;
    TZrSize descriptorCount;
    TZrSize index;

    descriptorCount = ZrParser_DiagnosticRegistry_Count();
    TEST_ASSERT_EQUAL_UINT32(69, (TZrUInt32)descriptorCount);

    possibleUninitialized =
            ZrParser_DiagnosticRegistry_FindByCode("possibly_uninitialized_read");
    TEST_ASSERT_NOT_NULL(possibleUninitialized);
    TEST_ASSERT_EQUAL_UINT32(3003, possibleUninitialized->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_WARNING,
                          possibleUninitialized->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_FLOW,
                          possibleUninitialized->category);
    TEST_ASSERT_NOT_NULL(possibleUninitialized->titleKey);
    TEST_ASSERT_NOT_NULL(possibleUninitialized->messageFormatKey);
    TEST_ASSERT_NOT_NULL(possibleUninitialized->helpUri);
    TEST_ASSERT_EQUAL_PTR(
            possibleUninitialized,
            ZrParser_DiagnosticRegistry_FindById(possibleUninitialized->id));

    useAfterMove = ZrParser_DiagnosticRegistry_FindByCode("use_after_move");
    TEST_ASSERT_NOT_NULL(useAfterMove);
    TEST_ASSERT_EQUAL_UINT32(4001, useAfterMove->id);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_OWNERSHIP,
                          useAfterMove->category);

    reservedOwnershipIntrinsicName =
            ZrParser_DiagnosticRegistry_FindByCode(
                    "reserved_ownership_intrinsic_name");
    TEST_ASSERT_NOT_NULL(reservedOwnershipIntrinsicName);
    TEST_ASSERT_EQUAL_UINT32(4008, reservedOwnershipIntrinsicName->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            reservedOwnershipIntrinsicName->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(
            ZR_LINT_CATEGORY_OWNERSHIP,
            reservedOwnershipIntrinsicName->category);

    ownershipIntrinsicCallRequired =
            ZrParser_DiagnosticRegistry_FindByCode(
                    "ownership_intrinsic_call_required");
    TEST_ASSERT_NOT_NULL(ownershipIntrinsicCallRequired);
    TEST_ASSERT_EQUAL_UINT32(4009, ownershipIntrinsicCallRequired->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            ownershipIntrinsicCallRequired->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(
            ZR_LINT_CATEGORY_OWNERSHIP,
            ownershipIntrinsicCallRequired->category);

    ownershipIntrinsicArityMismatch =
            ZrParser_DiagnosticRegistry_FindByCode(
                    "ownership_intrinsic_arity_mismatch");
    TEST_ASSERT_NOT_NULL(ownershipIntrinsicArityMismatch);
    TEST_ASSERT_EQUAL_UINT32(4010, ownershipIntrinsicArityMismatch->id);
    TEST_ASSERT_EQUAL_INT(
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            ownershipIntrinsicArityMismatch->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(
            ZR_LINT_CATEGORY_OWNERSHIP,
            ownershipIntrinsicArityMismatch->category);

    typeMismatch = ZrParser_DiagnosticRegistry_FindByCode("type_mismatch");
    TEST_ASSERT_NOT_NULL(typeMismatch);
    TEST_ASSERT_EQUAL_UINT32(2011, typeMismatch->id);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_TYPE, typeMismatch->category);

    constAssignment =
            ZrParser_DiagnosticRegistry_FindByCode("const_assignment");
    TEST_ASSERT_NOT_NULL(constAssignment);
    TEST_ASSERT_EQUAL_UINT32(2012, constAssignment->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          constAssignment->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_SEMANTIC,
                          constAssignment->category);

    invalidVariance =
            ZrParser_DiagnosticRegistry_FindByCode("invalid_variance");
    TEST_ASSERT_NOT_NULL(invalidVariance);
    TEST_ASSERT_EQUAL_UINT32(2013, invalidVariance->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          invalidVariance->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_TYPE,
                          invalidVariance->category);

    constInterfaceMismatch =
            ZrParser_DiagnosticRegistry_FindByCode("const_interface_mismatch");
    TEST_ASSERT_NOT_NULL(constInterfaceMismatch);
    TEST_ASSERT_EQUAL_UINT32(2014, constInterfaceMismatch->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          constInterfaceMismatch->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_TYPE,
                          constInterfaceMismatch->category);

    unresolvedReference =
            ZrParser_DiagnosticRegistry_FindByCode("unresolved_reference");
    TEST_ASSERT_NOT_NULL(unresolvedReference);
    TEST_ASSERT_EQUAL_UINT32(2015, unresolvedReference->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          unresolvedReference->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_SEMANTIC,
                          unresolvedReference->category);

    memberNotFound =
            ZrParser_DiagnosticRegistry_FindByCode("member_not_found");
    TEST_ASSERT_NOT_NULL(memberNotFound);
    TEST_ASSERT_EQUAL_UINT32(2016, memberNotFound->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          memberNotFound->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_TYPE,
                          memberNotFound->category);

    initializerRequiresAnnotation =
            ZrParser_DiagnosticRegistry_FindByCode(
                    "initializer_requires_annotation");
    TEST_ASSERT_NOT_NULL(initializerRequiresAnnotation);
    TEST_ASSERT_EQUAL_UINT32(2017, initializerRequiresAnnotation->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          initializerRequiresAnnotation->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_TYPE,
                          initializerRequiresAnnotation->category);

    returnTypeNotProvable =
            ZrParser_DiagnosticRegistry_FindByCode(
                    "return_type_not_provable");
    TEST_ASSERT_NOT_NULL(returnTypeNotProvable);
    TEST_ASSERT_EQUAL_UINT32(2018, returnTypeNotProvable->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          returnTypeNotProvable->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_TYPE,
                          returnTypeNotProvable->category);

    invalidDecorator =
            ZrParser_DiagnosticRegistry_FindByCode("invalid_decorator");
    TEST_ASSERT_NOT_NULL(invalidDecorator);
    TEST_ASSERT_EQUAL_UINT32(2019, invalidDecorator->id);
    TEST_ASSERT_EQUAL_INT(ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                          invalidDecorator->defaultSeverity);
    TEST_ASSERT_EQUAL_INT(ZR_LINT_CATEGORY_SEMANTIC,
                          invalidDecorator->category);

    for (index = 0; index < descriptorCount; index++) {
        const SZrDiagnosticDescriptor *descriptor =
                ZrParser_DiagnosticRegistry_DescriptorAt(index);
        TZrSize previousIndex;

        TEST_ASSERT_NOT_NULL(descriptor);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, descriptor->id);
        TEST_ASSERT_NOT_NULL(descriptor->code);
        TEST_ASSERT_EQUAL_PTR(
                descriptor,
                ZrParser_DiagnosticRegistry_FindByCode(descriptor->code));
        TEST_ASSERT_EQUAL_PTR(
                descriptor,
                ZrParser_DiagnosticRegistry_FindById(descriptor->id));
        for (previousIndex = 0; previousIndex < index; previousIndex++) {
            const SZrDiagnosticDescriptor *previous =
                    ZrParser_DiagnosticRegistry_DescriptorAt(previousIndex);
            TEST_ASSERT_NOT_NULL(previous);
            TEST_ASSERT_NOT_EQUAL_UINT32(previous->id, descriptor->id);
            TEST_ASSERT_NOT_EQUAL(0, strcmp(previous->code, descriptor->code));
        }
    }

    TEST_ASSERT_NULL(ZrParser_DiagnosticRegistry_FindByCode("not_registered"));
    TEST_ASSERT_NULL(ZrParser_DiagnosticRegistry_FindById(0));
    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_Build(
            g_state,
            &diagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_INFO,
            test_range(0, 1),
            "not_registered",
            "Unknown diagnostic",
            ZR_NULL,
            ZR_NULL));
    TEST_ASSERT_EQUAL_UINT32(0, diagnostic.descriptorId);
    ZrParser_StructuredDiagnostic_Free(g_state, &diagnostic);
}

static void test_diagnostic_message_table_covers_registry_and_falls_back_to_english(void) {
    TZrSize descriptorCount = ZrParser_DiagnosticRegistry_Count();
    TZrSize messageCount = ZrParser_DiagnosticMessages_Count();
    TZrSize index;

    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(descriptorCount * 2),
            (TZrUInt32)messageCount);

    for (index = 0; index < messageCount; index++) {
        const SZrDiagnosticMessage *message =
                ZrParser_DiagnosticMessages_MessageAt(index);
        TZrSize previousIndex;

        TEST_ASSERT_NOT_NULL(message);
        TEST_ASSERT_NOT_NULL(message->key);
        TEST_ASSERT_EQUAL_PTR(message, ZrParser_DiagnosticMessages_Find(message->key));
        for (previousIndex = 0; previousIndex < index; previousIndex++) {
            const SZrDiagnosticMessage *previous =
                    ZrParser_DiagnosticMessages_MessageAt(previousIndex);
            TEST_ASSERT_NOT_NULL(previous);
            TEST_ASSERT_NOT_EQUAL(0, strcmp(previous->key, message->key));
        }
    }
    TEST_ASSERT_NULL(ZrParser_DiagnosticMessages_MessageAt(messageCount));

    for (index = 0; index < descriptorCount; index++) {
        const SZrDiagnosticDescriptor *descriptor =
                ZrParser_DiagnosticRegistry_DescriptorAt(index);
        const SZrDiagnosticMessage *title;
        const SZrDiagnosticMessage *messageFormat;
        const TZrChar *englishTitle;
        const TZrChar *englishMessageFormat;

        TEST_ASSERT_NOT_NULL(descriptor);
        title = ZrParser_DiagnosticMessages_Find(descriptor->titleKey);
        messageFormat = ZrParser_DiagnosticMessages_Find(descriptor->messageFormatKey);
        TEST_ASSERT_NOT_NULL(title);
        TEST_ASSERT_NOT_NULL(messageFormat);
        TEST_ASSERT_NOT_NULL(title->english);
        TEST_ASSERT_NOT_NULL(messageFormat->english);
        TEST_ASSERT_TRUE(title->english[0] != '\0');
        TEST_ASSERT_TRUE(messageFormat->english[0] != '\0');
        TEST_ASSERT_NULL(title->chineseSimplified);
        TEST_ASSERT_NULL(messageFormat->chineseSimplified);

        englishTitle = ZrParser_DiagnosticMessages_Resolve(
                ZR_DIAGNOSTIC_LOCALE_ENGLISH,
                descriptor->titleKey);
        englishMessageFormat = ZrParser_DiagnosticMessages_Resolve(
                ZR_DIAGNOSTIC_LOCALE_ENGLISH,
                descriptor->messageFormatKey);
        TEST_ASSERT_EQUAL_STRING(title->english, englishTitle);
        TEST_ASSERT_EQUAL_STRING(messageFormat->english, englishMessageFormat);
        TEST_ASSERT_EQUAL_STRING(
                englishTitle,
                ZrParser_DiagnosticMessages_Resolve(
                        ZR_DIAGNOSTIC_LOCALE_CHINESE_SIMPLIFIED,
                        descriptor->titleKey));
        TEST_ASSERT_EQUAL_STRING(
                englishMessageFormat,
                ZrParser_DiagnosticMessages_Resolve(
                        ZR_DIAGNOSTIC_LOCALE_CHINESE_SIMPLIFIED,
                        descriptor->messageFormatKey));
    }

    TEST_ASSERT_EQUAL_STRING(
            "Type mismatch",
            ZrParser_DiagnosticMessages_Resolve(
                    ZR_DIAGNOSTIC_LOCALE_ENGLISH,
                    "diagnostic.type_mismatch.title"));
    TEST_ASSERT_EQUAL_STRING(
            "Expected '%s' but found '%s'",
            ZrParser_DiagnosticMessages_Resolve(
                    ZR_DIAGNOSTIC_LOCALE_ENGLISH,
                    "diagnostic.type_mismatch.message"));
    TEST_ASSERT_NULL(ZrParser_DiagnosticMessages_Find("diagnostic.not_registered.title"));
    TEST_ASSERT_NULL(ZrParser_DiagnosticMessages_Resolve(
            ZR_DIAGNOSTIC_LOCALE_ENGLISH,
            "diagnostic.not_registered.title"));
}

#include "test_semantic_query_public_contract_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_query_type_at_copies_narrowest_expression_type);
    RUN_TEST(test_semantic_query_facts_at_collects_matching_facts);
    RUN_TEST(test_semantic_query_owner_hover_and_diagnostic_share_canonical_facts);
    RUN_TEST(test_semantic_query_canonical_type_at_reads_type_reference_fact);
    RUN_TEST(test_semantic_query_definition_of_returns_matching_declaration);
    RUN_TEST(test_semantic_query_declaration_of_uses_exact_symbol_identity);
    RUN_TEST(test_semantic_query_definition_of_prefers_reaching_write_definition);
    RUN_TEST(test_semantic_query_definitions_of_returns_multiple_reaching_writes);
    RUN_TEST(test_semantic_query_references_of_collects_symbol_references_in_scope);
    RUN_TEST(test_semantic_query_references_of_clears_reused_output_when_missing);
    RUN_TEST(test_semantic_query_references_of_clears_reused_output_when_symbol_is_invalid);
    RUN_TEST(test_semantic_query_node_scope_filters_outside_range);
    RUN_TEST(test_semantic_query_diagnostics_returns_empty_when_no_diagnostic_facts);
    RUN_TEST(test_semantic_query_diagnostics_maps_unreachable_reachability_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_maps_definite_assignment_read_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_maps_numeric_overflow_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_maps_array_bounds_facts_in_scope);
    RUN_TEST(test_semantic_query_diagnostics_consumes_linear_definite_assignment_resolution);
    RUN_TEST(test_diagnostic_registry_assigns_stable_descriptors);
    RUN_TEST(test_diagnostic_message_table_covers_registry_and_falls_back_to_english);
    RUN_TEST(test_semantic_query_public_contract_ignores_private_variable_type_changes);
    RUN_TEST(test_semantic_query_public_contract_changes_for_public_signatures);
    RUN_TEST(test_semantic_query_public_contract_is_stable_across_declaration_order);
    RUN_TEST(test_semantic_query_public_contract_normalizes_generic_owner_ids);
    RUN_TEST(test_semantic_query_public_contract_hashes_parameter_names);
    RUN_TEST(test_semantic_query_public_contract_rejects_unnormalized_public_surfaces);
    RUN_TEST(test_semantic_query_public_contract_rejects_poisoned_or_unsupported_modules);
    RUN_TEST(test_semantic_query_public_contract_rejects_mismatched_semantic_owners);
    RUN_TEST(test_semantic_query_public_contract_rejects_noncanonical_generic_constraints);
    RUN_TEST(test_semantic_query_public_contract_rejects_removed_intermediate_wire_value);
    return UNITY_END();
}
