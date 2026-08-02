static void test_debug_breakpoint_condition_requires_pure_formal_evaluation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrBool satisfied = ZR_FALSE;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    error[0] = '\0';
    TEST_ASSERT_TRUE(zr_debug_breakpoint_condition_evaluate(
            &agent, "1 < 2", &satisfied, error, sizeof(error)));
    TEST_ASSERT_TRUE(satisfied);
    TEST_ASSERT_EQUAL_STRING("", error);

    satisfied = ZR_FALSE;
    error[0] = '\0';
    TEST_ASSERT_FALSE(zr_debug_breakpoint_condition_evaluate(
            &agent, "[1 + 2, 4]", &satisfied, error, sizeof(error)));
    TEST_ASSERT_FALSE(satisfied);
    assert_text_contains(error, "explicit capability");

    ZrTests_Runtime_State_Destroy(state);
}
