#ifndef ZR_VM_TEST_OWNERSHIP_OPTIONAL_CALLABLE_CASES_H
#define ZR_VM_TEST_OWNERSHIP_OPTIONAL_CALLABLE_CASES_H

static void test_nullable_callable_optional_and_direct_call_contracts(void) {
    const TZrChar *source =
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return 1; }\n"
            "fn run(): int {\n"
            "    var live: fn(int) -> int = "
            "fn(value: int): int => value + 10;\n"
            "    var absent = false ? live : null;\n"
            "    var liveResult = live?.(bump());\n"
            "    var absentResult = absent?.(bump());\n"
            "    var caught = 0;\n"
            "    try { absent(bump()); }\n"
            "    catch (error: NullReferenceError) { caught = 1; }\n"
            "    if (liveResult == 11 && absentResult == null && "
            "sideEffects == 1 && caught == 1) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "nullable_callable_optional_and_direct.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

#endif
