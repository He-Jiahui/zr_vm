#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

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
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "facts_test.zr", 13);
    return range;
}

static void test_semantic_context_initializes_fact_arrays(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_TRUE(context->expressionFacts.isValid);
    TEST_ASSERT_TRUE(context->referenceFacts.isValid);
    TEST_ASSERT_TRUE(context->numericFacts.isValid);
    TEST_ASSERT_TRUE(context->reachabilityFacts.isValid);
    TEST_ASSERT_TRUE(context->logicalFacts.isValid);
    TEST_ASSERT_TRUE(context->ownershipFacts.isValid);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->expressionFacts.length);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_expression_fact_roundtrip_by_node_and_position(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode fakeNode;
    SZrInferredType inferredType;
    SZrSemanticExpressionFact fact;
    const SZrSemanticExpressionFact *foundByNode;
    const SZrSemanticExpressionFact *foundByPosition;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fakeNode, 0, sizeof(fakeNode));
    fakeNode.type = ZR_AST_INTEGER_LITERAL;
    fakeNode.location = test_range(4, 6);
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);

    memset(&fact, 0, sizeof(fact));
    fact.node = &fakeNode;
    fact.range = fakeNode.location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.valueKind = ZR_SEMANTIC_VALUE_KIND_INT64;
    fact.hasConstant = ZR_TRUE;
    fact.constantValue.int64Value = 42;
    ZrParser_InferredType_Copy(g_state, &fact.inferredType, &inferredType);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &fact));
    foundByNode = ZrParser_SemanticFacts_FindExpressionByNode(context, &fakeNode);
    foundByPosition = ZrParser_SemanticFacts_FindExpressionAtPosition(context, test_range(5, 5));
    TEST_ASSERT_NOT_NULL(foundByNode);
    TEST_ASSERT_NOT_NULL(foundByPosition);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, foundByNode->inferredType.baseType);
    TEST_ASSERT_TRUE(foundByPosition->hasConstant);
    TEST_ASSERT_EQUAL_INT64(42, foundByPosition->constantValue.int64Value);

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_logical_fact_roundtrip_by_node_and_position(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode fakeNode;
    SZrSemanticLogicalFact fact;
    const SZrSemanticLogicalFact *foundByNode;
    const SZrSemanticLogicalFact *foundByPosition;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fakeNode, 0, sizeof(fakeNode));
    fakeNode.type = ZR_AST_LOGICAL_EXPRESSION;
    fakeNode.location = test_range(10, 24);

    memset(&fact, 0, sizeof(fact));
    fact.node = &fakeNode;
    fact.range = fakeNode.location;
    fact.kind = ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = ZR_TRUE;
    fact.knownValue = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendLogical(context, &fact));
    foundByNode = ZrParser_SemanticFacts_FindLogicalByNode(context, &fakeNode);
    foundByPosition = ZrParser_SemanticFacts_FindLogicalAtPosition(context, test_range(16, 16));
    TEST_ASSERT_NOT_NULL(foundByNode);
    TEST_ASSERT_NOT_NULL(foundByPosition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT, foundByPosition->kind);
    TEST_ASSERT_TRUE(foundByPosition->hasKnownValue);
    TEST_ASSERT_TRUE(foundByPosition->knownValue);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_context_reset_clears_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticReachabilityFact fact;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    fact.range = test_range(10, 20);
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = ZR_SEMANTIC_REACHABILITY_AFTER_RETURN;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReachability(context, &fact));
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)context->reachabilityFacts.length);

    ZrParser_SemanticContext_Reset(context);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->reachabilityFacts.length);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->expressionFacts.length);

    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_context_initializes_fact_arrays);
    RUN_TEST(test_semantic_expression_fact_roundtrip_by_node_and_position);
    RUN_TEST(test_semantic_logical_fact_roundtrip_by_node_and_position);
    RUN_TEST(test_semantic_context_reset_clears_facts);
    return UNITY_END();
}
