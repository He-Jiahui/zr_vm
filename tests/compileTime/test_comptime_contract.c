#include "unity.h"

#include "zr_vm_parser/comptime_contract.h"

static void test_comptime_effect_policy_matches_typed_evaluation_contexts(void) {
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE,
            ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE));
    TEST_ASSERT_FALSE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC));
    TEST_ASSERT_FALSE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD));

    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_CHECK,
            ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE));
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_CHECK,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC));
    TEST_ASSERT_FALSE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_CHECK,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD));

    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM,
            ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE));
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC));
    TEST_ASSERT_TRUE(ZrParser_ComptimeEffect_IsAllowed(
            ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM,
            ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD));
}

static void test_comptime_budget_rejects_overrun_without_partial_consumption(void) {
    SZrParserComptimeBudgetLimits limits = {0};
    SZrParserComptimeBudget budget;

    limits.fuel = 3;
    limits.callDepth = 2;
    limits.heapBytes = 16;
    limits.aggregateCount = 4;
    limits.generatedDeclarationCount = 5;
    limits.diagnosticCount = 1;
    ZrParser_ComptimeBudget_Init(&budget, &limits);

    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_FUEL, 2));
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_FUEL, 2));
    TEST_ASSERT_EQUAL_UINT64(2, budget.usage.fuel);
    TEST_ASSERT_EQUAL(ZR_PARSER_COMPTIME_BUDGET_FUEL, budget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(3, budget.exceededLimit);
    TEST_ASSERT_EQUAL_UINT64(4, budget.requestedUsage);

    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT, 1));
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT, 1));
    TEST_ASSERT_EQUAL_UINT64(1, budget.usage.diagnosticCount);
    TEST_ASSERT_EQUAL(ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT,
                      budget.exceededResource);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_comptime_effect_policy_matches_typed_evaluation_contexts);
    RUN_TEST(test_comptime_budget_rejects_overrun_without_partial_consumption);
    return UNITY_END();
}
