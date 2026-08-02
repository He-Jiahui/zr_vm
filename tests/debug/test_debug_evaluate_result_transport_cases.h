static void test_debug_evaluate_result_publishes_canonical_type_and_stop_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    cJSON *protocolResult;
    const cJSON *stateId;
    const cJSON *hasCanonicalType;
    const cJSON *canonicalTypeId;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    agent.threads = (ZrDebugThreadEntry *)calloc(1u, sizeof(*agent.threads));
    TEST_ASSERT_NOT_NULL(agent.threads);
    agent.threadCount = 1u;
    agent.threadCapacity = 1u;
    agent.threads[0].thread_id = 1u;
    agent.threads[0].state = state;
    agent.state = state;
    agent.currentThreadId = 1u;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    agent.stopStateId = 73u;
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_EvaluateWithCapabilities(
            &agent,
            1u,
            "1 + 2",
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            &result,
            error,
            sizeof(error)));
    TEST_ASSERT_EQUAL_UINT64(73u, result.state_id);
    TEST_ASSERT_TRUE(result.has_canonical_type);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, result.canonical_type_id);
    TEST_ASSERT_EQUAL_STRING("", error);

    protocolResult = zr_debug_protocol_make_evaluate_result(
            &agent,
            1u,
            1u,
            "1 + 2",
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            error,
            sizeof(error));
    TEST_ASSERT_NOT_NULL(protocolResult);
    stateId = cJSON_GetObjectItemCaseSensitive(protocolResult, "stateId");
    hasCanonicalType = cJSON_GetObjectItemCaseSensitive(protocolResult, "hasCanonicalType");
    canonicalTypeId = cJSON_GetObjectItemCaseSensitive(protocolResult, "canonicalTypeId");
    TEST_ASSERT_TRUE(cJSON_IsNumber(stateId));
    TEST_ASSERT_EQUAL_UINT64(73u, (TZrUInt64)stateId->valuedouble);
    TEST_ASSERT_TRUE(cJSON_IsTrue(hasCanonicalType));
    TEST_ASSERT_TRUE(cJSON_IsNumber(canonicalTypeId));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, (TZrUInt32)canonicalTypeId->valuedouble);
    cJSON_Delete(protocolResult);

    free(agent.threads);
    ZrTests_Runtime_State_Destroy(state);
}
