static void expect_span_inline_receiver_result(
        const char *path,
        const char *source,
        TZrInt64 expected) {
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, path, source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_inferred_span_receiver_supports_index_write(void) {
    static const char source[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view = xs.span();\n"
            "view[1] = 40;\n"
            "return xs[1];\n";

    expect_span_inline_receiver_result(
            "span_inferred_index_write.zr", source, 40);
}

static void test_explicit_span_receiver_supports_index_write(void) {
    static const char source[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view: container.Span<int> = xs.span();\n"
            "view[1] = 40;\n"
            "return xs[1];\n";

    expect_span_inline_receiver_result(
            "span_explicit_index_write.zr", source, 40);
}

static void test_explicit_span_receiver_supports_index_read(void) {
    static const char source[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view: container.Span<int> = xs.span();\n"
            "return view[1];\n";

    expect_span_inline_receiver_result(
            "span_explicit_index_read.zr", source, 22);
}

static void test_inferred_span_receiver_supports_slice(void) {
    static const char source[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view = xs.span();\n"
            "var sliced = view.slice(0, 2);\n"
            "return sliced[1];\n";

    expect_span_inline_receiver_result(
            "span_inferred_slice.zr", source, 22);
}

static void test_explicit_span_receiver_supports_slice(void) {
    static const char source[] =
            "var container = import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(11);\n"
            "xs.add(22);\n"
            "var view: container.Span<int> = xs.span();\n"
            "var sliced = view.slice(0, 2);\n"
            "return sliced[1];\n";

    expect_span_inline_receiver_result(
            "span_explicit_slice.zr", source, 22);
}
