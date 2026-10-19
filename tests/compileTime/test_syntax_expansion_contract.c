/* Verify the appended syntax phase and the all-or-nothing syntax budget. */
#include "unity.h"

#include "zr_vm_parser/comptime_contract.h"

static void test_compile_phase_order_is_explicit(void) {
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsValid(
            ZR_PARSER_COMPILE_PHASE_BUILD_FACTS));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsValid(
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsValid(
            ZR_PARSER_COMPILE_PHASE_SIGNATURE));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsValid(
            ZR_PARSER_COMPILE_PHASE_SEMANTIC_EXPANSION));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsValid(
            ZR_PARSER_COMPILE_PHASE_LAYOUT));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsValid(
            ZR_PARSER_COMPILE_PHASE_LATE_CHECK));
    TEST_ASSERT_FALSE(ZrParser_CompilePhase_IsValid(
            (EZrParserCompilePhase)255));

    TEST_ASSERT_FALSE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_BUILD_FACTS,
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_PHASE_BUILD_FACTS));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_SIGNATURE,
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_SEMANTIC_EXPANSION,
            ZR_PARSER_COMPILE_PHASE_SIGNATURE));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_LAYOUT,
            ZR_PARSER_COMPILE_PHASE_SEMANTIC_EXPANSION));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
            ZR_PARSER_COMPILE_PHASE_LAYOUT));
}

static void test_syntax_context_only_allows_syntax_effect_at_syntax_phase(void) {
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowedAtPhase(
            ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE));
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowedAtPhase(
            ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_TOOL_EFFECT_SYNTAX_BUILD));
    TEST_ASSERT_FALSE(ZrParser_ComptimeEffect_IsAllowedAtPhase(
            ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD));
    TEST_ASSERT_FALSE(ZrParser_ComptimeEffect_IsAllowedAtPhase(
            ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_PHASE_SIGNATURE,
            ZR_PARSER_COMPILE_TOOL_EFFECT_SYNTAX_BUILD));
    TEST_ASSERT_FALSE(ZrParser_ComptimeEffect_IsAllowedAtPhase(
            ZR_PARSER_COMPTIME_CONTEXT_CHECK,
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            ZR_PARSER_COMPILE_TOOL_EFFECT_SYNTAX_BUILD));
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowedAtPhase(
            ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM,
            ZR_PARSER_COMPILE_PHASE_SEMANTIC_EXPANSION,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD));
}

static void test_syntax_budget_defaults_match_existing_limits(void) {
    TEST_ASSERT_EQUAL_UINT64(1000000U, ZR_PARSER_COMPTIME_DEFAULT_FUEL);
    TEST_ASSERT_EQUAL_UINT64(128U, ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH);
    TEST_ASSERT_EQUAL_UINT64(
            64U * 1024U * 1024U, ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES);
    TEST_ASSERT_EQUAL_UINT64(100000U,
                             ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT);
    TEST_ASSERT_EQUAL_UINT64(
            10000U,
            ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT);
    TEST_ASSERT_EQUAL_UINT64(1024U,
                             ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT);
    TEST_ASSERT_EQUAL_UINT64(1000000U,
                             ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT);
    TEST_ASSERT_EQUAL_UINT64(1000000U,
                             ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT);
    TEST_ASSERT_EQUAL_UINT64(100000U,
                             ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT);
    TEST_ASSERT_EQUAL_UINT64(10000U,
                             ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT);
}

static void test_syntax_budget_transaction_has_no_partial_consumption(void) {
    SZrParserComptimeBudgetLimits limits = {0};
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetTransaction transaction;

    limits.tokenCount = 5U;
    limits.nodeCount = 4U;
    limits.matchingCount = 3U;
    limits.expansionCount = 2U;
    ZrParser_ComptimeBudget_Init(&budget, &limits);

    ZrParser_ComptimeBudget_BeginTransaction(&budget, &transaction);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 5U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 4U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT, 3U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT, 2U));
    TEST_ASSERT_EQUAL_UINT64(0U, budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_UINT64(0U, budget.usage.nodeCount);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_CommitTransaction(&transaction));
    TEST_ASSERT_EQUAL_UINT64(5U, budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_UINT64(4U, budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT64(3U, budget.usage.matchingCount);
    TEST_ASSERT_EQUAL_UINT64(2U, budget.usage.expansionCount);

    ZrParser_ComptimeBudget_BeginTransaction(&budget, &transaction);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 0U));
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 1U));
    TEST_ASSERT_EQUAL(ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT,
                      transaction.exceededResource);
    ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    TEST_ASSERT_EQUAL_UINT64(5U, budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_UINT64(4U, budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT64(3U, budget.usage.matchingCount);
    TEST_ASSERT_EQUAL_UINT64(2U, budget.usage.expansionCount);
    TEST_ASSERT_EQUAL(ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT,
                      budget.exceededResource);
}

static void test_syntax_budget_direct_consumption_covers_new_resources(void) {
    SZrParserComptimeBudgetLimits limits = {0};
    SZrParserComptimeBudget budget;

    limits.tokenCount = 1U;
    limits.nodeCount = 1U;
    limits.matchingCount = 1U;
    limits.expansionCount = 1U;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 1U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 1U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT, 1U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT, 1U));
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 1U));
    TEST_ASSERT_EQUAL_UINT64(1U, budget.usage.tokenCount);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compile_phase_order_is_explicit);
    RUN_TEST(test_syntax_context_only_allows_syntax_effect_at_syntax_phase);
    RUN_TEST(test_syntax_budget_defaults_match_existing_limits);
    RUN_TEST(test_syntax_budget_transaction_has_no_partial_consumption);
    RUN_TEST(test_syntax_budget_direct_consumption_covers_new_resources);
    return UNITY_END();
}
