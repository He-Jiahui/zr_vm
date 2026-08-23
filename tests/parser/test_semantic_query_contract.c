#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
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

static SZrFileRange contract_range(TZrSize startOffset, TZrSize endOffset) {
    TZrChar sourceName[] = "semantic_query_contract.zr";
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

static void contract_init_node(SZrAstNode *node,
                               TZrSize startOffset,
                               TZrSize endOffset) {
    memset(node, 0, sizeof(*node));
    node->type = ZR_AST_INTEGER_LITERAL;
    node->location = contract_range(startOffset, endOffset);
}

static void contract_append_expression(SZrSemanticContext *context,
                                       SZrAstNode *node,
                                       EZrSemanticFactExactness exactness) {
    SZrInferredType inferredType;
    SZrSemanticExpressionFact fact;

    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
    fact.exactness = exactness;
    ZrParser_InferredType_Copy(g_state, &fact.inferredType, &inferredType);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &fact));
    ZrParser_InferredType_Free(g_state, &inferredType);
}

static void contract_append_persistent_diagnostic(SZrSemanticContext *context) {
    SZrSemanticDiagnosticFact fact;

    memset(&fact, 0, sizeof(fact));
    TEST_ASSERT_TRUE(ZrParser_DiagnosticBuilder_Build(
            g_state,
            &fact.diagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_ERROR,
            contract_range(12U, 14U),
            "semantic_query_contract",
            "Persistent diagnostic",
            "The semantic fact is present before a query runs.",
            "Resolve the semantic condition."));
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendDiagnostic(context, &fact));
    ZrParser_StructuredDiagnostic_Free(g_state, &fact.diagnostic);
}

static void test_type_at_fails_closed_for_approximate_expression_fact(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode node;
    SZrInferredType queriedType;

    TEST_ASSERT_NOT_NULL(context);
    contract_init_node(&node, 4U, 6U);
    contract_append_expression(context, &node, ZR_SEMANTIC_FACT_APPROXIMATE);
    memset(&queriedType, 0, sizeof(queriedType));

    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_TypeAt(
            context, contract_range(5U, 5U), ZR_NULL, &queriedType));

    ZrParser_InferredType_Free(g_state, &queriedType);
    ZrParser_SemanticContext_Free(context);
}

static void test_facts_at_returns_repeatable_borrowed_fact_view(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode node;
    SZrParserSemanticQueryFacts first;
    SZrParserSemanticQueryFacts second;

    TEST_ASSERT_NOT_NULL(context);
    contract_init_node(&node, 8U, 10U);
    contract_append_expression(context, &node, ZR_SEMANTIC_FACT_EXACT);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FactsAt(
            context, contract_range(9U, 9U), ZR_NULL, &first));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FactsAt(
            context, contract_range(9U, 9U), ZR_NULL, &second));
    TEST_ASSERT_NOT_NULL(first.expression);
    TEST_ASSERT_EQUAL_PTR(first.expression, second.expression);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, first.expression->exactness);

    ZrParser_SemanticContext_Free(context);
}

static void test_diagnostics_query_does_not_materialize_context_state(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    TEST_ASSERT_NOT_NULL(context);
    contract_append_persistent_diagnostic(context);
    ZrParser_SemanticQueryScope_Module(&scope);
    TEST_ASSERT_EQUAL_UINT(0U, context->queryDiagnostics.length);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT(0U, diagnostics.count);
    TEST_ASSERT_EQUAL_UINT(0U, context->queryDiagnostics.length);

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_MaterializeDiagnostics(context, &scope));
    TEST_ASSERT_EQUAL_UINT(1U, context->queryDiagnostics.length);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_Diagnostics(context, &scope, &diagnostics));
    TEST_ASSERT_NOT_NULL(diagnostics.items);
    TEST_ASSERT_EQUAL_UINT(1U, diagnostics.count);
    TEST_ASSERT_EQUAL_UINT(1U, context->queryDiagnostics.length);

    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_type_at_fails_closed_for_approximate_expression_fact);
    RUN_TEST(test_facts_at_returns_repeatable_borrowed_fact_view);
    RUN_TEST(test_diagnostics_query_does_not_materialize_context_state);
    return UNITY_END();
}
