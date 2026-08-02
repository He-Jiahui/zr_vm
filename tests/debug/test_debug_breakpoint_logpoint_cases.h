static void test_debug_breakpoint_logpoint_requires_pure_formal_evaluation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar output[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(zr_debug_breakpoint_logpoint_format(
            &agent, "sum = {1 + 2}", output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("sum = 3\n", output);

    TEST_ASSERT_TRUE(zr_debug_breakpoint_logpoint_format(
            &agent, "array = {[1 + 2, 4]}", output, sizeof(output)));
    assert_text_contains(output, "<error:");
    assert_text_contains(output, "explicit capability");

    ZrTests_Runtime_State_Destroy(state);
}
