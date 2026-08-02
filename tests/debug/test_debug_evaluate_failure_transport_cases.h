static void test_debug_evaluate_failure_preserves_structured_diagnostic(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    ZrDebugEvaluateFailure failure;
    cJSON *data;
    const cJSON *code;
    const cJSON *cause;
    const cJSON *suggestion;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    memset(&failure, 0, sizeof(failure));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    agent.stopStateId = 79u;
    error[0] = '\0';

    TEST_ASSERT_FALSE(ZrDebug_EvaluateWithCapabilitiesDetailed(
            &agent,
            1u,
            "1 +",
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            &result,
            &failure,
            error,
            sizeof(error)));
    TEST_ASSERT_EQUAL_UINT64(79u, failure.state_id);
    TEST_ASSERT_EQUAL_INT(ZR_DEBUG_EVALUATE_FAILURE_PARSER, failure.kind);
    TEST_ASSERT_EQUAL_STRING("missing_right_operand", failure.code);
    assert_text_contains(failure.cause, "right-hand expression");
    assert_text_contains(failure.suggestion, "Add the right-hand expression");

    data = zr_debug_protocol_make_evaluate_failure_data(&failure);
    TEST_ASSERT_NOT_NULL(data);
    code = cJSON_GetObjectItemCaseSensitive(data, "code");
    cause = cJSON_GetObjectItemCaseSensitive(data, "cause");
    suggestion = cJSON_GetObjectItemCaseSensitive(data, "suggestion");
    TEST_ASSERT_TRUE(cJSON_IsString(code));
    TEST_ASSERT_EQUAL_STRING("missing_right_operand", code->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsString(cause));
    TEST_ASSERT_TRUE(cJSON_IsString(suggestion));
    cJSON_Delete(data);

    memset(&failure, 0, sizeof(failure));
    error[0] = '\0';
    TEST_ASSERT_FALSE(ZrDebug_EvaluateWithCapabilitiesDetailed(
            &agent,
            1u,
            "[1, 2]",
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            &result,
            &failure,
            error,
            sizeof(error)));
    TEST_ASSERT_EQUAL_INT(ZR_DEBUG_EVALUATE_FAILURE_CAPABILITY, failure.kind);
    TEST_ASSERT_EQUAL_STRING("debug_evaluation_capability_denied", failure.code);

    ZrTests_Runtime_State_Destroy(state);
}
