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

static void test_debug_evaluation_effect_policy_classifies_canonical_expression_shapes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluationEffectPolicy policy;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(&agent, 1u, "1 + 2", &policy, error, sizeof(error)));
    TEST_ASSERT_TRUE(policy.isPure);
    TEST_ASSERT_TRUE(policy.hasCanonicalFacts);
    TEST_ASSERT_EQUAL_UINT32(ZR_DEBUG_EVALUATION_EFFECT_NONE, policy.effectFlags);

    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(&agent, 1u, "sideEffect()", &policy, error, sizeof(error)));
    TEST_ASSERT_FALSE_MESSAGE(policy.isPure, "call expression must not be pure");
    TEST_ASSERT_TRUE((policy.effectFlags & ZR_DEBUG_EVALUATION_EFFECT_CALL) != 0u);

    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(&agent, 1u, "local = 1", &policy, error, sizeof(error)));
    TEST_ASSERT_FALSE_MESSAGE(policy.isPure, "assignment expression must not be pure");
    TEST_ASSERT_TRUE((policy.effectFlags & ZR_DEBUG_EVALUATION_EFFECT_MUTATION) != 0u);

    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';
    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(&agent, 1u, "unknownLocal", &policy, error, sizeof(error)));
    TEST_ASSERT_FALSE_MESSAGE(policy.hasCanonicalFacts,
                              "unresolved identifier must not publish canonical facts");
    TEST_ASSERT_FALSE_MESSAGE(policy.isPure, "unresolved identifier must not be pure");

    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluation_effect_policy_requires_explicit_capabilities(void) {
    ZrDebugEvaluationEffectPolicy policy;

    memset(&policy, 0, sizeof(policy));
    policy.hasCanonicalFacts = ZR_TRUE;
    policy.isPure = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrDebug_EvaluationEffectPolicy_Allows(&policy, ZR_DEBUG_EVALUATION_EFFECT_NONE));

    policy.isPure = ZR_FALSE;
    policy.effectFlags = ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER;
    TEST_ASSERT_FALSE(ZrDebug_EvaluationEffectPolicy_Allows(&policy, ZR_DEBUG_EVALUATION_EFFECT_NONE));
    TEST_ASSERT_TRUE(ZrDebug_EvaluationEffectPolicy_Allows(
            &policy,
            ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER));

    policy.effectFlags = ZR_DEBUG_EVALUATION_EFFECT_CALL |
                         ZR_DEBUG_EVALUATION_EFFECT_NATIVE_CALL;
    TEST_ASSERT_FALSE(ZrDebug_EvaluationEffectPolicy_Allows(
            &policy,
            ZR_DEBUG_EVALUATION_EFFECT_CALL));
    TEST_ASSERT_TRUE(ZrDebug_EvaluationEffectPolicy_Allows(
            &policy,
            ZR_DEBUG_EVALUATION_EFFECT_CALL |
                    ZR_DEBUG_EVALUATION_EFFECT_NATIVE_CALL));

    policy.hasCanonicalFacts = ZR_FALSE;
    policy.effectFlags = ZR_DEBUG_EVALUATION_EFFECT_NONE;
    TEST_ASSERT_FALSE(ZrDebug_EvaluationEffectPolicy_Allows(
            &policy,
            ZR_DEBUG_EVALUATION_EFFECT_OWNER_MUTATION));
}

static void test_debug_evaluation_effect_policy_marks_resolved_property_getter(void) {
    const TZrChar *source =
            "class Meter {\n"
            "  pub static property shared: int { get { return 9; } }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    ZrDebugEvaluationEffectPolicy policy;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_effect_property_getter.zr", source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    memset(&policy, 0, sizeof(policy));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_ClassifyEvaluationEffect(
            &agent,
            1u,
            "Meter.shared",
            &policy,
            error,
            sizeof(error)));
    TEST_ASSERT_TRUE(policy.hasCanonicalFacts);
    TEST_ASSERT_FALSE(policy.isPure);
    TEST_ASSERT_TRUE((policy.effectFlags & ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER) != 0u);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}
