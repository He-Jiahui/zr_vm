#include "aot_c_typed_direct_call_bool_smoke_support.h"

static void test_aot_c_generated_shared_library_executes_static_bool_no_arg_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func yes(): bool {\n"
            "    return true;\n"
            "}\n"
            "var flag: bool = yes();\n"
            "if (!flag) {\n"
            "    return 0;\n"
            "}\n"
            "return 42;",
            "aot-runtime-static-bool-no-arg-typed-call-smoke",
            "runtime_static_bool_no_arg_project",
            "static_bool_no_arg_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state) {",
            "return ZR_TRUE;",
            "/* zr_aot_static_bool_no_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state)",
            "/* zr_aot_static_bool_no_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_returns_static_bool_no_arg_result_through_bool_boundary(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func answer(): bool {\n"
            "    var result: bool = true;\n"
            "    return result;\n"
            "}\n"
            "var flag: bool = answer();\n"
            "if (!flag) {\n"
            "    return 0;\n"
            "}\n"
            "return 42;",
            "aot-runtime-static-bool-return-boundary-smoke",
            "runtime_static_bool_return_boundary_project",
            "static_bool_return_boundary_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state) {",
            "return ZR_TRUE;",
            "/* zr_aot_static_bool_no_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state)",
            "/* zr_aot_static_bool_no_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke_with_options(
            &testCase,
            "ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b",
            ZR_FALSE);
}

static void test_aot_c_generated_shared_library_executes_static_bool_one_arg_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func pass(flag: bool): bool {\n"
            "    return flag;\n"
            "}\n"
            "var seed: bool = true;\n"
            "var flag: bool = pass(seed);\n"
            "if (!flag) {\n"
            "    return 0;\n"
            "}\n"
            "return 42;",
            "aot-runtime-static-bool-one-arg-typed-call-smoke",
            "runtime_static_bool_one_arg_project",
            "static_bool_one_arg_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "/* zr_aot_static_bool_one_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_one_arg_logical_not_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func invert(flag: bool): bool {\n"
            "    return !flag;\n"
            "}\n"
            "var seed: bool = false;\n"
            "var flag: bool = invert(seed);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-bool-one-arg-not-typed-call-smoke",
            "runtime_static_bool_one_arg_not_project",
            "static_bool_one_arg_not_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0) {",
            "return (TZrBool)!zr_aot_arg0;",
            "/* zr_aot_static_bool_one_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_two_arg_logical_and_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func both(left: bool, right: bool): bool {\n"
            "    return left && right;\n"
            "}\n"
            "var left: bool = true;\n"
            "var right: bool = false;\n"
            "var flag: bool = both(left, right);\n"
            "if (flag) {\n"
            "    return 0;\n"
            "}\n"
            "return 42;",
            "aot-runtime-static-bool-two-arg-typed-call-smoke",
            "runtime_static_bool_two_arg_project",
            "static_bool_two_arg_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);",
            "/* zr_aot_static_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_three_arg_logical_and_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func all(left: bool, middle: bool, right: bool): bool {\n"
            "    return left && middle && right;\n"
            "}\n"
            "var left: bool = true;\n"
            "var middle: bool = true;\n"
            "var right: bool = true;\n"
            "var flag: bool = all(left, middle, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-bool-three-arg-typed-call-smoke",
            "runtime_static_bool_three_arg_project",
            "static_bool_three_arg_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {",
            "return (TZrBool)(zr_aot_arg0 && zr_aot_arg1 && zr_aot_arg2);",
            "/* zr_aot_static_bool_three_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_three_arg_logical_or_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func either3(left: bool, middle: bool, right: bool): bool {\n"
            "    return left || middle || right;\n"
            "}\n"
            "var left: bool = false;\n"
            "var middle: bool = false;\n"
            "var right: bool = true;\n"
            "var flag: bool = either3(left, middle, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-bool-three-arg-or-typed-call-smoke",
            "runtime_static_bool_three_arg_or_project",
            "static_bool_three_arg_or_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {",
            "return (TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2);",
            "/* zr_aot_static_bool_three_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_two_arg_logical_or_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func either(left: bool, right: bool): bool {\n"
            "    return left || right;\n"
            "}\n"
            "var left: bool = false;\n"
            "var right: bool = true;\n"
            "var flag: bool = either(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-bool-two-arg-or-typed-call-smoke",
            "runtime_static_bool_two_arg_or_project",
            "static_bool_two_arg_or_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);",
            "/* zr_aot_static_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_two_arg_equal_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func same(left: bool, right: bool): bool {\n"
            "    return left == right;\n"
            "}\n"
            "var left: bool = true;\n"
            "var right: bool = true;\n"
            "var flag: bool = same(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-bool-two-arg-equal-typed-call-smoke",
            "runtime_static_bool_two_arg_equal_project",
            "static_bool_two_arg_equal_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "/* zr_aot_static_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_bool_two_arg_not_equal_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func different(left: bool, right: bool): bool {\n"
            "    return left != right;\n"
            "}\n"
            "var left: bool = true;\n"
            "var right: bool = false;\n"
            "var flag: bool = different(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-bool-two-arg-not-equal-typed-call-smoke",
            "runtime_static_bool_two_arg_not_equal_project",
            "static_bool_two_arg_not_equal_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);",
            "/* zr_aot_static_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_b",
            "/* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_less_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func smaller(left: int, right: int): bool {\n"
            "    return left < right;\n"
            "}\n"
            "var left: int = 7;\n"
            "var right: int = 9;\n"
            "var flag: bool = smaller(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-i64-two-arg-less-bool-typed-call-smoke",
            "runtime_static_i64_two_arg_less_bool_project",
            "static_i64_two_arg_less_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);",
            "/* zr_aot_static_i64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_s",
            "/* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func same(left: int, right: int): bool {\n"
            "    return left == right;\n"
            "}\n"
            "var left: int = 21;\n"
            "var right: int = 21;\n"
            "var flag: bool = same(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-i64-two-arg-equal-bool-typed-call-smoke",
            "runtime_static_i64_two_arg_equal_bool_project",
            "static_i64_two_arg_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "/* zr_aot_static_i64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_s",
            "/* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_not_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func different(left: int, right: int): bool {\n"
            "    return left != right;\n"
            "}\n"
            "var left: int = 21;\n"
            "var right: int = 22;\n"
            "var flag: bool = different(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-i64-two-arg-not-equal-bool-typed-call-smoke",
            "runtime_static_i64_two_arg_not_equal_bool_project",
            "static_i64_two_arg_not_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);",
            "/* zr_aot_static_i64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_s",
            "/* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_greater_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func greater(left: int, right: int): bool {\n"
            "    return left > right;\n"
            "}\n"
            "var left: int = 50;\n"
            "var right: int = 8;\n"
            "var flag: bool = greater(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-i64-two-arg-greater-bool-typed-call-smoke",
            "runtime_static_i64_two_arg_greater_bool_project",
            "static_i64_two_arg_greater_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);",
            "/* zr_aot_static_i64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_s",
            "/* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_less_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func at_most(left: int, right: int): bool {\n"
            "    return left <= right;\n"
            "}\n"
            "var left: int = 8;\n"
            "var right: int = 50;\n"
            "var flag: bool = at_most(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-i64-two-arg-less-equal-bool-typed-call-smoke",
            "runtime_static_i64_two_arg_less_equal_bool_project",
            "static_i64_two_arg_less_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);",
            "/* zr_aot_static_i64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_s",
            "/* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_greater_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func at_least(left: int, right: int): bool {\n"
            "    return left >= right;\n"
            "}\n"
            "var left: int = 50;\n"
            "var right: int = 8;\n"
            "var flag: bool = at_least(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-i64-two-arg-greater-equal-bool-typed-call-smoke",
            "runtime_static_i64_two_arg_greater_equal_bool_project",
            "static_i64_two_arg_greater_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);",
            "/* zr_aot_static_i64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_s",
            "/* zr_aot_static_i64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_less_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func smaller(left: uint, right: uint): bool {\n"
            "    return left < right;\n"
            "}\n"
            "var left: uint = 7;\n"
            "var right: uint = 9;\n"
            "var flag: bool = smaller(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-u64-two-arg-less-bool-typed-call-smoke",
            "runtime_static_u64_two_arg_less_bool_project",
            "static_u64_two_arg_less_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);",
            "/* zr_aot_static_u64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func same(left: uint, right: uint): bool {\n"
            "    return left == right;\n"
            "}\n"
            "var left: uint = 21;\n"
            "var right: uint = 21;\n"
            "var flag: bool = same(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-u64-two-arg-equal-bool-typed-call-smoke",
            "runtime_static_u64_two_arg_equal_bool_project",
            "static_u64_two_arg_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "/* zr_aot_static_u64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_not_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func different(left: uint, right: uint): bool {\n"
            "    return left != right;\n"
            "}\n"
            "var left: uint = 21;\n"
            "var right: uint = 22;\n"
            "var flag: bool = different(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-u64-two-arg-not-equal-bool-typed-call-smoke",
            "runtime_static_u64_two_arg_not_equal_bool_project",
            "static_u64_two_arg_not_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);",
            "/* zr_aot_static_u64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_greater_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func greater(left: uint, right: uint): bool {\n"
            "    return left > right;\n"
            "}\n"
            "var left: uint = 50;\n"
            "var right: uint = 8;\n"
            "var flag: bool = greater(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-u64-two-arg-greater-bool-typed-call-smoke",
            "runtime_static_u64_two_arg_greater_bool_project",
            "static_u64_two_arg_greater_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);",
            "/* zr_aot_static_u64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_less_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func at_most(left: uint, right: uint): bool {\n"
            "    return left <= right;\n"
            "}\n"
            "var left: uint = 8;\n"
            "var right: uint = 50;\n"
            "var flag: bool = at_most(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-u64-two-arg-less-equal-bool-typed-call-smoke",
            "runtime_static_u64_two_arg_less_equal_bool_project",
            "static_u64_two_arg_less_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);",
            "/* zr_aot_static_u64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_greater_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func at_least(left: uint, right: uint): bool {\n"
            "    return left >= right;\n"
            "}\n"
            "var left: uint = 50;\n"
            "var right: uint = 8;\n"
            "var flag: bool = at_least(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-u64-two-arg-greater-equal-bool-typed-call-smoke",
            "runtime_static_u64_two_arg_greater_equal_bool_project",
            "static_u64_two_arg_greater_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);",
            "/* zr_aot_static_u64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_less_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func smaller(left: float, right: float): bool {\n"
            "    return left < right;\n"
            "}\n"
            "var left: float = 20.5;\n"
            "var right: float = 21.5;\n"
            "var flag: bool = smaller(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-f64-two-arg-less-bool-typed-call-smoke",
            "runtime_static_f64_two_arg_less_bool_project",
            "static_f64_two_arg_less_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);",
            "/* zr_aot_static_f64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_less_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func at_most(left: float, right: float): bool {\n"
            "    return left <= right;\n"
            "}\n"
            "var left: float = 21.5;\n"
            "var right: float = 21.5;\n"
            "var flag: bool = at_most(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-f64-two-arg-less-equal-bool-typed-call-smoke",
            "runtime_static_f64_two_arg_less_equal_bool_project",
            "static_f64_two_arg_less_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);",
            "/* zr_aot_static_f64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func same(left: float, right: float): bool {\n"
            "    return left == right;\n"
            "}\n"
            "var left: float = 21.5;\n"
            "var right: float = 21.5;\n"
            "var flag: bool = same(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-f64-two-arg-equal-bool-typed-call-smoke",
            "runtime_static_f64_two_arg_equal_bool_project",
            "static_f64_two_arg_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);",
            "/* zr_aot_static_f64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_not_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func different(left: float, right: float): bool {\n"
            "    return left != right;\n"
            "}\n"
            "var left: float = 21.5;\n"
            "var right: float = 22.5;\n"
            "var flag: bool = different(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-f64-two-arg-not-equal-bool-typed-call-smoke",
            "runtime_static_f64_two_arg_not_equal_bool_project",
            "static_f64_two_arg_not_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);",
            "/* zr_aot_static_f64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_greater_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func greater(left: float, right: float): bool {\n"
            "    return left > right;\n"
            "}\n"
            "var left: float = 50.5;\n"
            "var right: float = 8.5;\n"
            "var flag: bool = greater(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-f64-two-arg-greater-bool-typed-call-smoke",
            "runtime_static_f64_two_arg_greater_bool_project",
            "static_f64_two_arg_greater_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);",
            "/* zr_aot_static_f64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_greater_equal_bool_typed_thunk(void) {
    static const SZrAotTypedDirectCallBoolSmokeCase testCase = {
            "func at_least(left: float, right: float): bool {\n"
            "    return left >= right;\n"
            "}\n"
            "var left: float = 21.5;\n"
            "var right: float = 21.5;\n"
            "var flag: bool = at_least(left, right);\n"
            "if (flag) {\n"
            "    return 42;\n"
            "}\n"
            "return 0;",
            "aot-runtime-static-f64-two-arg-greater-equal-bool-typed-call-smoke",
            "runtime_static_f64_two_arg_greater_equal_bool_project",
            "static_f64_two_arg_greater_equal_bool_typed_call_smoke",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrBool zr_aot_typed_bool_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);",
            "/* zr_aot_static_f64_bool_two_arg_direct_call */",
            "zr_aot_typed_bool_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_bool_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_bool_smoke(&testCase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_no_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_returns_static_bool_no_arg_result_through_bool_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_one_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_one_arg_logical_not_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_two_arg_logical_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_three_arg_logical_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_three_arg_logical_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_two_arg_logical_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_two_arg_equal_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_bool_two_arg_not_equal_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_less_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_not_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_greater_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_less_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_greater_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_less_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_not_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_greater_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_less_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_greater_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_less_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_less_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_not_equal_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_greater_bool_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_greater_equal_bool_typed_thunk);
    return UNITY_END();
}
