static void test_pinned_span_scalar_last_use_allows_pin_close(void) {
    static const char source[] =
            "var {Span} = import(\"zr.container\");\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.write(0, [11, 12, 13, 14]);\n"
            "var pin = buffer.pin();\n"
            "var view: Span<u8> = pin.span();\n"
            "buffer.close();\n"
            "var value = view[1];\n"
            "pin.close();\n"
            "return value + 31;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_pinned_scalar_last_use.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(43, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_pinned_span_close_rejects_later_view_read(void) {
    static const char source[] =
            "var {Span} = import(\"zr.container\");\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(2);\n"
            "var pin = buffer.pin();\n"
            "var view: Span<u8> = pin.span();\n"
            "pin.close();\n"
            "return view[0];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "span_pinned_close_before_read.zr", source);
    TEST_ASSERT_NULL(function);
    ZrContainerTests_DestroyState(state);
}
