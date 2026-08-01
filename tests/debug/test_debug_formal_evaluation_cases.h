static void test_debug_evaluate_formal_shift_expression_returns_int(void) {
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

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "1 << 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("4", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_formal_bitwise_expressions_return_int(void) {
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

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "8 >> 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("2", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "5 & 3", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("1", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_formal_nested_shift_arithmetic_returns_int(void) {
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

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1u, "1 << (1 + 1)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("4", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_formal_array_literal_requires_explicit_allocation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    ZrDebugEvaluationEffectPolicy policy;
    ZrDebugValuePreview *values = ZR_NULL;
    TZrSize valueCount = 0;
    TZrSize namedVariables = 0;
    TZrSize indexedVariables = 0;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    agent.nextVariableHandleId = ZR_DEBUG_VARIABLE_HANDLE_BASE;

    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(
            &agent, 1u, "[1 + 2, 4]", &policy, error, sizeof(error)));
    TEST_ASSERT_TRUE(policy.hasCanonicalFacts);
    TEST_ASSERT_TRUE((policy.effectFlags & ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION) != 0u);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    TEST_ASSERT_FALSE(ZrDebug_EvaluateWithCapabilities(
            &agent,
            1u,
            "[1 + 2, 4]",
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            &result,
            error,
            sizeof(error)));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(error, "explicit capability"), error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_EvaluateWithCapabilities(
            &agent,
            1u,
            "[1 + 2, 4]",
            ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION,
            &result,
            error,
            sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("array", result.type_name);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, result.variables_reference);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)result.indexed_variables);
    TEST_ASSERT_EQUAL_STRING("", error);

    TEST_ASSERT_TRUE(ZrDebug_ReadVariables(&agent,
                                           result.variables_reference,
                                           0u,
                                           0u,
                                           &values,
                                           &valueCount,
                                           &namedVariables,
                                           &indexedVariables));
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)namedVariables);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)indexedVariables);
    TEST_ASSERT_EQUAL_STRING("0", values[0].name);
    TEST_ASSERT_EQUAL_STRING("int", values[0].type_name);
    TEST_ASSERT_EQUAL_STRING("3", values[0].value_text);
    TEST_ASSERT_EQUAL_STRING("1", values[1].name);
    TEST_ASSERT_EQUAL_STRING("int", values[1].type_name);
    TEST_ASSERT_EQUAL_STRING("4", values[1].value_text);

    ZrDebug_Free(values);
    ZrDebug_Free(agent.variableHandles);
    ZrTests_Runtime_State_Destroy(state);
}
