static void test_using_existing_lease_closes_once_at_inner_scope_exit(void) {
    static const char kSource[] =
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var lease = pool.rent<int>(1);\n"
            "using (lease) { var marker = 1; }\n"
            "return pool.returnCount;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "pool_lease_existing_using.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_using_inner_lease_keeps_older_outer_lease_open(void) {
    static const char kSource[] =
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var older = pool.rent<int>(1);\n"
            "if (true) {\n"
            "    var lease = pool.rent<int>(1);\n"
            "    using (lease) { var marker = 1; }\n"
            "}\n"
            "return pool.returnCount;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "pool_lease_nested_using.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_bare_using_outer_lease_closes_at_inner_scope_exit(void) {
    static const char kSource[] =
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var lease = pool.rent<int>(1);\n"
            "if (true) { using lease; }\n"
            "return pool.returnCount;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "pool_lease_bare_outer_using.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_inferred_non_idempotent_close_meta_runs_once_without_using(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "if (true) { var resource = new CloseProbe(); }\n"
            "return CloseProbe.calls;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "non_idempotent_auto_close.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_using_existing_non_idempotent_close_meta_closes_once(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "if (true) {\n"
            "    var resource = new CloseProbe();\n"
            "    using (resource) { var marker = 1; }\n"
            "}\n"
            "return CloseProbe.calls;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "non_idempotent_using_normal.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_using_existing_non_idempotent_close_meta_closes_once_on_throw(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "try {\n"
            "    var resource = new CloseProbe();\n"
            "    using (resource) { throw \"boom\"; }\n"
            "} catch (error) { }\n"
            "return CloseProbe.calls;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "non_idempotent_using_throw.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_using_existing_close_meta_replaces_pending_throw(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) {\n"
            "        CloseProbe.calls = CloseProbe.calls + 1;\n"
            "        throw \"replacement\";\n"
            "    }\n"
            "}\n"
            "fn run(): int {\n"
            "var caught = 0;\n"
            "try {\n"
            "    var resource = new CloseProbe();\n"
            "    using (resource) { throw \"original\"; }\n"
            "} catch (error) {\n"
            "    if (error.exception == \"replacement\") { caught = 10; }\n"
            "    else { caught = 20; }\n"
            "}\n"
            "return caught + CloseProbe.calls;\n"
            "}\n"
            "return run();\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "non_idempotent_using_replacement.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(11, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_top_level_using_existing_close_meta_replaces_pending_throw(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) {\n"
            "        CloseProbe.calls = CloseProbe.calls + 1;\n"
            "        throw \"replacement\";\n"
            "    }\n"
            "}\n"
            "var caught = 0;\n"
            "try {\n"
            "    var resource = new CloseProbe();\n"
            "    using (resource) { throw \"original\"; }\n"
            "} catch (error) {\n"
            "    if (error.exception == \"replacement\") { caught = 10; }\n"
            "    else { caught = 20; }\n"
            "}\n"
            "return caught + CloseProbe.calls;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "top_level_existing_using_replacement.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(11, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_bare_using_same_scope_reuses_inferred_close_mark(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "if (true) {\n"
            "    var resource = new CloseProbe();\n"
            "    using resource;\n"
            "}\n"
            "return CloseProbe.calls;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;
    TZrUInt32 markCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "non_idempotent_bare_same_scope.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
        if (function->instructionsList[index].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)) {
            ++markCount;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, markCount);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_nested_using_same_non_idempotent_close_meta_closes_once(void) {
    static const char kSource[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "if (true) {\n"
            "    var resource = new CloseProbe();\n"
            "    using (resource) {\n"
            "        using (resource) { var marker = 1; }\n"
            "    }\n"
            "}\n"
            "return CloseProbe.calls;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "non_idempotent_nested_using.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_using_plain_existing_local_keeps_value_readable(void) {
    static const char kSource[] =
            "var resource = 7;\n"
            "using (resource) { var marker = 1; }\n"
            "return resource;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "plain_existing_using.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_bare_using_plain_existing_local_keeps_value_readable(void) {
    static const char kSource[] =
            "var resource = 7;\n"
            "if (true) { using resource; }\n"
            "return resource;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "plain_existing_bare_using.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}
