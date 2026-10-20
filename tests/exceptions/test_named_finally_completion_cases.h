#ifndef ZR_TEST_NAMED_FINALLY_COMPLETION_CASES_H
#define ZR_TEST_NAMED_FINALLY_COMPLETION_CASES_H

static void assert_named_finally_completion_source(
        const TZrChar *source, const TZrChar *sourceName) {
    SZrState *state = create_test_state();
    TZrBool created = state != ZR_NULL;
    TZrBool success = ZR_FALSE;
    TZrInt64 result = 0;

    if (created) {
        success = execute_source_expect_int64(state, source, sourceName, &result);
        destroy_test_state(state);
    }
    TEST_ASSERT_TRUE_MESSAGE(created, "Named finally fixture state creation failed");
    TEST_ASSERT_TRUE_MESSAGE(success, "Named finally source must compile and execute");
    TEST_ASSERT_EQUAL_INT64(1, result);
}

static void test_named_conditional_return_preserves_both_pending_values(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(choose: bool): int {\n"
            "    try {\n"
            "        if (choose) { return 7; }\n"
            "        return 8;\n"
            "    } finally {\n"
            "        marker = marker + 1;\n"
            "    }\n"
            "}\n"
            "var first = run(true);\n"
            "var firstMarker = marker;\n"
            "var second = run(false);\n"
            "if (first == 7 && second == 8 && firstMarker == 1 && marker == 2) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_conditional_finally_completion.zr");
}

static void test_named_both_arm_returns_preserve_values_and_run_finally_once(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(choose: bool): int {\n"
            "    try {\n"
            "        if (choose) { return 11; } else { return 22; }\n"
            "    } finally {\n"
            "        marker = marker + 1;\n"
            "    }\n"
            "}\n"
            "var first = run(true);\n"
            "var firstMarker = marker;\n"
            "var second = run(false);\n"
            "if (first == 11 && second == 22 && firstMarker == 1 && marker == 2) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_both_arm_finally_completion.zr");
}

static void test_named_throw_runs_finally_before_outer_catch(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(): void {\n"
            "    try {\n"
            "        throw \"named\";\n"
            "    } finally {\n"
            "        marker = marker + 1;\n"
            "    }\n"
            "}\n"
            "try {\n"
            "    run();\n"
            "} catch (e) {\n"
            "    if (marker == 1 && e.exception == \"named\") { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_throw_finally_completion.zr");
}

static void test_named_linear_finally_conditional_return_preserves_values(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(choose: bool): int {\n"
            "    try {\n"
            "        if (choose) { return 7; }\n"
            "        return 8;\n"
            "    } finally { marker = 9; }\n"
            "}\n"
            "var first = run(true);\n"
            "var firstMarker = marker;\n"
            "marker = 0;\n"
            "var second = run(false);\n"
            "if (first == 7 && second == 8 && firstMarker == 9 && marker == 9) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_linear_conditional_finally_completion.zr");
}

static void test_named_linear_finally_both_arm_returns_preserve_values(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(choose: bool): int {\n"
            "    try {\n"
            "        if (choose) { return 11; } else { return 22; }\n"
            "    } finally { marker = 9; }\n"
            "}\n"
            "var first = run(true);\n"
            "var firstMarker = marker;\n"
            "marker = 0;\n"
            "var second = run(false);\n"
            "if (first == 11 && second == 22 && firstMarker == 9 && marker == 9) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_linear_both_arm_finally_completion.zr");
}

static void test_named_linear_finally_throw_preserves_payload(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(): void {\n"
            "    try { throw \"named-linear\"; } finally { marker = 9; }\n"
            "}\n"
            "try { run(); } catch (e) {\n"
            "    if (marker == 9 && e.exception == \"named-linear\") { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_linear_throw_finally_completion.zr");
}

static void test_named_linear_finally_void_return_runs_cleanup(void) {
    static const TZrChar source[] =
            "var marker = 0;\n"
            "fn run(): void {\n"
            "    try { return; } finally { marker = 9; }\n"
            "}\n"
            "run();\n"
            "if (marker == 9) { return 1; }\n"
            "return 0;\n";

    assert_named_finally_completion_source(
            source, "named_linear_void_return_finally_completion.zr");
}

#endif
