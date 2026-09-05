#ifndef ZR_TEST_OWNERSHIP_LOOP_FINALLY_CASES_H
#define ZR_TEST_OWNERSHIP_LOOP_FINALLY_CASES_H

static void test_loop_local_transfers_lower_without_pending_finally_exit(void) {
    SZrFunction *function = compile_source(
            "var i = 0;\n"
            "try { while (i < 3) {\n"
            " i = i + 1; if (i < 2) { continue; } break;\n"
            "} } finally { i = i + 10; }\nreturn i;\n");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->exceptionHandlerCount);
    for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
        EZrInstructionCode opcode =
                function->instructionsList[index].instruction.operationCode;
        TEST_ASSERT_NOT_EQUAL(ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK), opcode);
        TEST_ASSERT_NOT_EQUAL(ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE), opcode);
    }
    ZrCore_Function_Free(g_state, function);
}

static void test_pending_nested_finally_break_keeps_outer_try_active(void) {
    pending_assert_script(
            "fn run(): int {\n"
            " var i = 0;\n"
            " try { while (i < 2) { try {\n"
            "  var unique = own Tracker(); var shared = share(unique);\n"
            "  i = i + 1; break;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 1; }\n"
            " } Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 3; }\n"
            " return Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 1231);
}

static void test_pending_nested_finally_continue_keeps_outer_try_active(void) {
    pending_assert_script(
            "fn run(): int {\n"
            " var i = 0;\n"
            " try { while (i < 2) { try {\n"
            "  var unique = own Tracker(); var shared = share(unique);\n"
            "  i = i + 1; continue;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 1; }\n"
            " } Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 3; }\n"
            " return Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 11232);
}

static void test_pending_catch_local_transfers_do_not_exit_finally(void) {
    pending_assert_script(
            "fn run(): int {\n"
            " var i = 0;\n"
            " try { throw \"caught\"; } catch (error) { while (i < 3) {\n"
            "  var unique = own Tracker(); var shared = share(unique);\n"
            "  i = i + 1; if (i < 2) { continue; } break;\n"
            " } Tracker.trace = Tracker.trace * 10 + 1;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 2; }\n"
            " return Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 122);
}

static void test_pending_return_target_survives_instruction_compaction(void) {
    SZrFunction *function = compile_source(
            "var value = 7;\n"
            "try { return value * 10 + 2; } finally { value = value + 1; }\n");
    TZrUInt32 pendingCount = 0u;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
        TZrInstruction *instruction = &function->instructionsList[index];
        if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN)) {
            TZrInt32 target = instruction->instruction.operand.operand2[0];
            ++pendingCount;
            TEST_ASSERT_TRUE(target >= 0 && (TZrUInt32)target < function->instructionsLength);
            TEST_ASSERT_EQUAL_INT(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN),
                    function->instructionsList[target].instruction.operationCode);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, pendingCount);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(72, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_pending_for_continue_resolves_existing_loop_label(void) {
    pending_assert_script(
            "fn run(): int {\n"
            " for (var i = 0; i < 2;) { try {\n"
            "  i = i + 1; continue;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 1; } }\n"
            " return Tracker.trace;\n"
            "}\nreturn run();\n", 11);
}

static void test_pending_foreach_continue_resolves_existing_loop_label(void) {
    pending_assert_script(
            "fn run(): int {\n"
            " var values = [1, 2];\n"
            " for (var item in values) { try { continue; } finally {\n"
            "  Tracker.trace = Tracker.trace * 10 + item;\n"
            " } }\n"
            " return Tracker.trace;\n"
            "}\nreturn run();\n", 12);
}

#endif
