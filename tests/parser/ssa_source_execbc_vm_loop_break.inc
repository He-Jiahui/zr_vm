/* Actual source-to-Core loop exit coverage shares the existing pipeline harness. */
static void assert_source_loop_break(const TZrChar *source,
                                    TZrBool takeConditionalBreak) {
    SZrSourceExecBcReport report = run_source_branch(source);
    const TZrChar *firstBreak = strstr(source, "break");
    const TZrChar *lastBreak = strstr(firstBreak + 5u, "break");
    TEST_ASSERT_TRUE_MESSAGE(report.parsedAndCompiled, "source compilation failed");
    TEST_ASSERT_TRUE_MESSAGE(report.sourceCfgValidated,
                             "conditional loop break did not publish source CFG");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, report.diagnosticCode);
    TEST_ASSERT_TRUE(report.execIrBuilt);
    TEST_ASSERT_TRUE(report.identityPublished);
    TEST_ASSERT_EQUAL_UINT32(2u, report.conditionalBranchCount);
    TEST_ASSERT_EQUAL_UINT32(0u, report.execIrPhiCount);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(9, report.oracleInteger);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, report.oracleEventCount,
            "pure loop exit fixture emitted a semantic effect event");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, report.loopBreakCount,
            "both source break sites must remain in unoptimized ExecIR");
    TEST_ASSERT_TRUE_MESSAGE(report.loopBreakTargetsValid,
            "a source break branch does not target the loop join return block");
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.materialized);
    TEST_ASSERT_TRUE(report.vmReturned);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_TRUE(report.pcSourceTraceValid);
    TEST_ASSERT_TRUE(report.vmEndsAtOracleBlock);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, report.vmBreakCount,
            "Core VM must execute exactly one source break branch");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            (TZrUInt32)((takeConditionalBreak ? firstBreak : lastBreak) - source),
            report.vmBreakOffset, "Core VM took the wrong source break site");
}

static void test_source_while_takes_conditional_break(void) {
    assert_source_loop_break(
            "while (true) { if (true) { break; } break; }\nreturn 9;\n",
            ZR_TRUE);
}

static void test_source_while_takes_fallthrough_break(void) {
    assert_source_loop_break(
            "while (true) { if (false) { break; } break; }\nreturn 9;\n",
            ZR_FALSE);
}

static void assert_source_loop_exit_is_unsupported(const TZrChar *source) {
    SZrSourceExecBcReport report = run_source_branch(source);
    TEST_ASSERT_TRUE(report.parsedAndCompiled);
    TEST_ASSERT_FALSE(report.sourceCfgValidated);
    TEST_ASSERT_FALSE(report.execIrBuilt);
    TEST_ASSERT_FALSE(report.oracleReturned);
    TEST_ASSERT_FALSE(report.projectionBuilt);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_FALSE(report.vmReturned);
}

static void test_source_conditional_continue_remains_unsupported(void) {
    assert_source_loop_exit_is_unsupported(
            "while (true) { if (true) { continue; } break; }\nreturn 9;\n");
}

static void test_source_conditional_break_cleanup_remains_unsupported(void) {
    assert_source_loop_exit_is_unsupported(
            "while (true) { if (true) { try { break; } finally { } } break; }\n"
            "return 9;\n");
}

static void test_source_for_conditional_break_remains_unsupported(void) {
    assert_source_loop_exit_is_unsupported(
            "for (; true; ) { if (true) { break; } break; }\nreturn 9;\n");
}

static void test_source_foreach_conditional_break_remains_unsupported(void) {
    assert_source_loop_exit_is_unsupported(
            "var values = [true];\n"
            "for (var item in values) { if (true) { break; } break; }\n"
            "return 9;\n");
}
