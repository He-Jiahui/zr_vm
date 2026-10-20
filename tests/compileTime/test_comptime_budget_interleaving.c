#include "unity.h"

#include "zr_vm_parser/comptime_contract.h"

static void init_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits = {0};
    limits.fuel = 20U;
    limits.tokenCount = 20U;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static void test_commit_preserves_intervening_consumption(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetTransaction transaction;
    init_budget(&budget);
    ZrParser_ComptimeBudget_BeginTransaction(&budget, &transaction);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 5U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_FUEL, 2U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 3U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_CommitTransaction(&transaction));
    TEST_ASSERT_EQUAL_UINT64(7U, budget.usage.fuel);
    TEST_ASSERT_EQUAL_UINT64(3U, budget.usage.tokenCount);
}

static void test_two_pending_reservations_accumulate(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetTransaction first;
    SZrParserComptimeBudgetTransaction second;
    init_budget(&budget);
    ZrParser_ComptimeBudget_BeginTransaction(&budget, &first);
    ZrParser_ComptimeBudget_BeginTransaction(&budget, &second);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &first, ZR_PARSER_COMPTIME_BUDGET_FUEL, 5U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &second, ZR_PARSER_COMPTIME_BUDGET_FUEL, 4U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_CommitTransaction(&second));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_CommitTransaction(&first));
    TEST_ASSERT_EQUAL_UINT64(9U, budget.usage.fuel);
}

static void test_commit_rechecks_limit_after_intervening_consumption(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetTransaction transaction;
    init_budget(&budget);
    ZrParser_ComptimeBudget_BeginTransaction(&budget, &transaction);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 15U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 4U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_FUEL, 8U));
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_CommitTransaction(&transaction));
    ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    TEST_ASSERT_EQUAL_UINT64(8U, budget.usage.fuel);
    TEST_ASSERT_EQUAL_UINT64(0U, budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_FUEL, budget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(23U, budget.requestedUsage);
}

static void test_commit_rechecks_changed_limits(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetTransaction transaction;
    init_budget(&budget);
    ZrParser_ComptimeBudget_BeginTransaction(&budget, &transaction);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 8U));
    budget.limits.tokenCount = 4U;
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_CommitTransaction(&transaction));
    ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    TEST_ASSERT_EQUAL_UINT64(0U, budget.usage.tokenCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT,
                          budget.exceededResource);
}

static void test_commit_reports_resource_on_counter_overflow(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetTransaction transaction;
    init_budget(&budget);
    budget.limits.fuel = (TZrUInt64)-1;
    ZrParser_ComptimeBudget_BeginTransaction(&budget, &transaction);
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsumeTransaction(
            &transaction, ZR_PARSER_COMPTIME_BUDGET_FUEL, 1U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &budget, ZR_PARSER_COMPTIME_BUDGET_FUEL, (TZrUInt64)-1));
    TEST_ASSERT_FALSE(ZrParser_ComptimeBudget_CommitTransaction(&transaction));
    ZrParser_ComptimeBudget_AbortTransaction(&transaction);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)-1, budget.usage.fuel);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_FUEL, budget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)-1, budget.requestedUsage);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_commit_preserves_intervening_consumption);
    RUN_TEST(test_two_pending_reservations_accumulate);
    RUN_TEST(test_commit_rechecks_limit_after_intervening_consumption);
    RUN_TEST(test_commit_rechecks_changed_limits);
    RUN_TEST(test_commit_reports_resource_on_counter_overflow);
    return UNITY_END();
}
