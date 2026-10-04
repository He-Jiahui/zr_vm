/* Source comparison REDs. Use the real parser/compiler and stage harness;
 * no fixture-built SemanticIR, ExecIR, projection or legacy VM substitute.
 * C/LLVM source projection coverage is not established by these VM cases. */
static void assert_source_signed_comparison(
        const TZrChar *source, TZrInt64 expectedReturn) {
    SZrSourceExecBcReport report = run_source_branch(source);
    TEST_ASSERT_TRUE_MESSAGE(report.parsedAndCompiled,
            "comparison source must parse and compile before the source CFG gate");
    TEST_ASSERT_TRUE_MESSAGE(report.preSemanticIrValidated,
            "ordinary preSemanticIr validation must pass before source CFG admission");
    TEST_ASSERT_TRUE_MESSAGE(report.sourceCfgValidated,
            "source comparison producer did not establish an executable source CFG");
    assert_source_branch_report(report, expectedReturn, 0u);
}

static void test_source_signed_less_true_reaches_core_dispatcher(void) {
    assert_source_signed_comparison(
            "if (1 < 2) { return 9; } else { return 8; }\n", 9);
}

static void test_source_signed_less_false_reaches_core_dispatcher(void) {
    assert_source_signed_comparison(
            "if (2 < 1) { return 9; } else { return 8; }\n", 8);
}

static void test_source_signed_greater_true_reaches_core_dispatcher(void) {
    assert_source_signed_comparison(
            "if (2 > 1) { return 9; } else { return 8; }\n", 9);
}

static void test_source_signed_greater_false_reaches_core_dispatcher(void) {
    assert_source_signed_comparison(
            "if (1 > 2) { return 9; } else { return 8; }\n", 8);
}
