#include "aot_c_typed_direct_call_u64_smoke_support.h"

static void test_aot_c_generated_shared_library_executes_static_u64_no_arg_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func answer(): uint {\n"
            "    return 37;\n"
            "}\n"
            "var value: uint = answer();\n"
            "return <int> value + 5;",
            "aot-runtime-static-u64-no-arg-typed-call-smoke",
            "runtime_static_u64_no_arg_project",
            "static_u64_no_arg_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state) {",
            "return (TZrUInt64)37;",
            "/* zr_aot_static_u64_no_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state)",
            "/* zr_aot_static_u64_no_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func pass(value: uint): uint {\n"
            "    return value;\n"
            "}\n"
            "var seed: uint = 37;\n"
            "var value: uint = pass(seed);\n"
            "return <int> value + 5;",
            "aot-runtime-static-u64-one-arg-typed-call-smoke",
            "runtime_static_u64_one_arg_project",
            "static_u64_one_arg_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_add_const_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func inc(value: uint): uint {\n"
            "    return value + 1;\n"
            "}\n"
            "var seed: uint = 36;\n"
            "var value: uint = inc(seed);\n"
            "return <int> value + 5;",
            "aot-runtime-static-u64-one-arg-add-const-typed-call-smoke",
            "runtime_static_u64_one_arg_add_const_project",
            "static_u64_one_arg_add_const_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return (TZrUInt64)(zr_aot_arg0 + (TZrUInt64)1);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_subtract_const_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func dec(value: uint): uint {\n"
            "    return value - 8;\n"
            "}\n"
            "var seed: uint = 50;\n"
            "var value: uint = dec(seed);\n"
            "return <int> value;",
            "aot-runtime-static-u64-one-arg-subtract-const-typed-call-smoke",
            "runtime_static_u64_one_arg_subtract_const_project",
            "static_u64_one_arg_subtract_const_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return (TZrUInt64)(zr_aot_arg0 - (TZrUInt64)8);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_multiply_const_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func scale(value: uint): uint {\n"
            "    return value * 21;\n"
            "}\n"
            "var seed: uint = 2;\n"
            "var value: uint = scale(seed);\n"
            "return <int> value;",
            "aot-runtime-static-u64-one-arg-multiply-const-typed-call-smoke",
            "runtime_static_u64_one_arg_multiply_const_project",
            "static_u64_one_arg_multiply_const_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return (TZrUInt64)(zr_aot_arg0 * (TZrUInt64)21);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_and_const_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func mask(value: uint): uint {\n"
            "    return value & 58;\n"
            "}\n"
            "var seed: uint = 47;\n"
            "var value: uint = mask(seed);\n"
            "return <int> value;",
            "aot-runtime-static-u64-one-arg-bitwise-and-const-typed-call-smoke",
            "runtime_static_u64_one_arg_bitwise_and_const_project",
            "static_u64_one_arg_bitwise_and_const_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return (TZrUInt64)(zr_aot_arg0 & (TZrUInt64)58);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_or_const_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func flags(value: uint): uint {\n"
            "    return value | 10;\n"
            "}\n"
            "var seed: uint = 32;\n"
            "var value: uint = flags(seed);\n"
            "return <int> value;",
            "aot-runtime-static-u64-one-arg-bitwise-or-const-typed-call-smoke",
            "runtime_static_u64_one_arg_bitwise_or_const_project",
            "static_u64_one_arg_bitwise_or_const_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return (TZrUInt64)(zr_aot_arg0 | (TZrUInt64)10);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_xor_const_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func toggle(value: uint): uint {\n"
            "    return value ^ 21;\n"
            "}\n"
            "var seed: uint = 63;\n"
            "var value: uint = toggle(seed);\n"
            "return <int> value;",
            "aot-runtime-static-u64-one-arg-bitwise-xor-const-typed-call-smoke",
            "runtime_static_u64_one_arg_bitwise_xor_const_project",
            "static_u64_one_arg_bitwise_xor_const_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0) {",
            "return (TZrUInt64)(zr_aot_arg0 ^ (TZrUInt64)21);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func sum(left: uint, right: uint): uint {\n"
            "    return left + right;\n"
            "}\n"
            "var first: uint = 19;\n"
            "var second: uint = 18;\n"
            "var value: uint = sum(first, second);\n"
            "return <int> value + 5;",
            "aot-runtime-static-u64-two-arg-typed-call-smoke",
            "runtime_static_u64_two_arg_project",
            "static_u64_two_arg_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_multiply_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func product(left: uint, right: uint): uint {\n"
            "    return left * right;\n"
            "}\n"
            "var left: uint = 6;\n"
            "var right: uint = 7;\n"
            "var value: uint = product(left, right);\n"
            "return <int> value;",
            "aot-runtime-static-u64-two-arg-multiply-typed-call-smoke",
            "runtime_static_u64_two_arg_multiply_project",
            "static_u64_two_arg_multiply_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_subtract_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func diff(left: uint, right: uint): uint {\n"
            "    return left - right;\n"
            "}\n"
            "var left: uint = 50;\n"
            "var right: uint = 9;\n"
            "var value: uint = diff(left, right);\n"
            "return <int> value + 1;",
            "aot-runtime-static-u64-two-arg-subtract-typed-call-smoke",
            "runtime_static_u64_two_arg_subtract_project",
            "static_u64_two_arg_subtract_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_and_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func mask(left: uint, right: uint): uint {\n"
            "    return left & right;\n"
            "}\n"
            "var left: uint = 47;\n"
            "var right: uint = 58;\n"
            "var value: uint = mask(left, right);\n"
            "return <int> value;",
            "aot-runtime-static-u64-two-arg-bitwise-and-typed-call-smoke",
            "runtime_static_u64_two_arg_bitwise_and_project",
            "static_u64_two_arg_bitwise_and_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_or_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func combine(left: uint, right: uint): uint {\n"
            "    return left | right;\n"
            "}\n"
            "var left: uint = 40;\n"
            "var right: uint = 2;\n"
            "var value: uint = combine(left, right);\n"
            "return <int> value;",
            "aot-runtime-static-u64-two-arg-bitwise-or-typed-call-smoke",
            "runtime_static_u64_two_arg_bitwise_or_project",
            "static_u64_two_arg_bitwise_or_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_xor_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func toggle(left: uint, right: uint): uint {\n"
            "    return left ^ right;\n"
            "}\n"
            "var left: uint = 63;\n"
            "var right: uint = 21;\n"
            "var value: uint = toggle(left, right);\n"
            "return <int> value;",
            "aot-runtime-static-u64-two-arg-bitwise-xor-typed-call-smoke",
            "runtime_static_u64_two_arg_bitwise_xor_project",
            "static_u64_two_arg_bitwise_xor_typed_call_smoke",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);",
            "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {",
            "return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "zr_aot_typed_u64_fn_1(state, zr_aot_u",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_no_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_add_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_subtract_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_multiply_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_and_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_or_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_xor_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_subtract_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_xor_typed_thunk);
    return UNITY_END();
}
