#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "debug_internal.h"

static void assert_text_contains(const TZrChar *text, const TZrChar *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), text);
}

static void test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1 +", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing expression after '+'");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "right-hand expression");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_expression_reports_missing_logical_operand_with_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true &&", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing expression after '&&'");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_short_circuits_or_without_resolving_missing_rhs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "true || missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_short_circuits_and_without_resolving_missing_rhs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "false && missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_composed_comparison_logical_expression_returns_bool(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(1 < 2) && (3 < 4)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "!(1 < 2)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_comparison_short_circuits_without_resolving_missing_rhs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(1 < 2) || missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(2 < 1) && missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_evaluates_selected_ternary_branch_without_resolving_skipped_branch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "true ? 1 : missingLocal", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("1", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "false ? missingLocal : 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("2", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"text\" + 1", &result, error, sizeof(error)));
    assert_text_contains(error, "numeric operands");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "Use numeric operands");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1 / 0", &result, error, sizeof(error)));
    assert_text_contains(error, "division by zero");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "guard the divisor");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true.", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing member name after '.'");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "member access");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reports_missing_index_close_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "false || true[0", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing closing ']' in index access");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_condition_reports_missing_group_close_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "(true || false", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing closing ')' in grouped expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"open", &result, error, sizeof(error)));
    assert_text_contains(error, "Unterminated string literal");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "debug evaluate");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "closing quote");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"bad\\q\"", &result, error, sizeof(error)));
    assert_text_contains(error, "Unsupported string escape");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "\\q");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "supported escape");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_rejects_function_call_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "sideEffect()", &result, error, sizeof(error)));
    assert_text_contains(error, "Function calls are not allowed");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "safe debug evaluate");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_rejects_assignment_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true = false", &result, error, sizeof(error)));
    assert_text_contains(error, "Assignment is not allowed");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "read-only");
    assert_text_contains(error, "Suggestion:");

    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "local = 1", &result, error, sizeof(error)));
    assert_text_contains(error, "Assignment is not allowed");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "read-only");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_expression_reports_missing_logical_operand_with_suggestion);
    RUN_TEST(test_debug_condition_short_circuits_or_without_resolving_missing_rhs);
    RUN_TEST(test_debug_condition_short_circuits_and_without_resolving_missing_rhs);
    RUN_TEST(test_debug_evaluate_composed_comparison_logical_expression_returns_bool);
    RUN_TEST(test_debug_condition_comparison_short_circuits_without_resolving_missing_rhs);
    RUN_TEST(test_debug_condition_evaluates_selected_ternary_branch_without_resolving_skipped_branch);
    RUN_TEST(test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_index_close_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_group_close_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_rejects_function_call_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_rejects_assignment_with_cause_and_suggestion);
    return UNITY_END();
}
