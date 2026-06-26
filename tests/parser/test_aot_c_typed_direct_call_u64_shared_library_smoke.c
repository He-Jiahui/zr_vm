#include "aot_c_typed_direct_call_u64_smoke_support.h"

#define ZR_AOT_U64_NO_ARG_STATE_FREE_DECL "static TZrUInt64 zr_aot_typed_u64_fn_1(void);"
#define ZR_AOT_U64_NO_ARG_STATE_FREE_DEF "static TZrUInt64 zr_aot_typed_u64_fn_1(void) {"
#define ZR_AOT_U64_NO_ARG_STATE_FREE_CALL "zr_aot_typed_u64_fn_1()"
#define ZR_AOT_U64_NO_ARG_STATEFUL_CALL "zr_aot_typed_u64_fn_1(state)"
#define ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0);"
#define ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0) {"
#define ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL "zr_aot_typed_u64_fn_1(zr_aot_u"
#define ZR_AOT_U64_ONE_ARG_STATEFUL_CALL "zr_aot_typed_u64_fn_1(state, zr_aot_u"
#define ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);"
#define ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {"
#define ZR_AOT_U64_TWO_ARG_STATEFUL_DECL \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);"
#define ZR_AOT_U64_TWO_ARG_STATEFUL_DEF \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {"
#define ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL "zr_aot_typed_u64_fn_1(zr_aot_u"
#define ZR_AOT_U64_TWO_ARG_STATEFUL_CALL "zr_aot_typed_u64_fn_1(state, zr_aot_u"
#define ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);"
#define ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {"
#define ZR_AOT_U64_THREE_ARG_STATEFUL_DECL \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2);"
#define ZR_AOT_U64_THREE_ARG_STATEFUL_DEF \
    "static TZrUInt64 zr_aot_typed_u64_fn_1(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1, TZrUInt64 zr_aot_arg2) {"
#define ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL "zr_aot_typed_u64_fn_1(zr_aot_u"
#define ZR_AOT_U64_THREE_ARG_STATEFUL_CALL "zr_aot_typed_u64_fn_1(state, zr_aot_u"

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
            ZR_AOT_U64_NO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_NO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)37;",
            "/* zr_aot_static_u64_no_arg_direct_call */",
            ZR_AOT_U64_NO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_NO_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_returns_static_u64_no_arg_result_through_u64_boundary(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func answer(): uint {\n"
            "    var result: uint = 42;\n"
            "    return result;\n"
            "}\n"
            "var value: uint = answer();\n"
            "return <int> value;",
            "aot-runtime-static-u64-return-boundary-smoke",
            "runtime_static_u64_return_boundary_project",
            "static_u64_return_boundary_smoke",
            ZR_AOT_U64_NO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_NO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)42;",
            "/* zr_aot_static_u64_no_arg_direct_call */",
            ZR_AOT_U64_NO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_NO_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke_with_options(
            &testCase,
            "ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u",
            "ZrLibrary_AotRuntime_Return(state, &frame, 0, ZR_FALSE)",
            ZR_VALUE_TYPE_NULL,
            ZR_FALSE);
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return zr_aot_arg0;",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 + (TZrUInt64)1);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 - (TZrUInt64)8);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 * (TZrUInt64)21);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 & (TZrUInt64)58);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 | (TZrUInt64)10);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_ONE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 ^ (TZrUInt64)21);",
            "/* zr_aot_static_u64_one_arg_direct_call */",
            ZR_AOT_U64_ONE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_ONE_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_divide_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func quotient(left: uint, right: uint): uint {\n"
            "    return left / right;\n"
            "}\n"
            "var left: uint = 84;\n"
            "var right: uint = 2;\n"
            "var value: uint = quotient(left, right);\n"
            "return <int> value;",
            "aot-runtime-static-u64-two-arg-divide-typed-call-smoke",
            "runtime_static_u64_two_arg_divide_project",
            "static_u64_two_arg_divide_typed_call_smoke",
            ZR_AOT_U64_TWO_ARG_STATEFUL_DECL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_DEF,
            "return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_two_arg_modulo_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func remainder(left: uint, right: uint): uint {\n"
            "    return left % right;\n"
            "}\n"
            "var left: uint = 87;\n"
            "var right: uint = 45;\n"
            "var value: uint = remainder(left, right);\n"
            "return <int> value;",
            "aot-runtime-static-u64-two-arg-modulo-typed-call-smoke",
            "runtime_static_u64_two_arg_modulo_project",
            "static_u64_two_arg_modulo_typed_call_smoke",
            ZR_AOT_U64_TWO_ARG_STATEFUL_DECL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_DEF,
            "return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
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
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_TWO_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            ZR_AOT_U64_TWO_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_TWO_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_add_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func sum3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left + middle + right;\n"
            "}\n"
            "var first: uint = 12;\n"
            "var second: uint = 20;\n"
            "var third: uint = 10;\n"
            "var value: uint = sum3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-add-typed-call-smoke",
            "runtime_static_u64_three_arg_add_project",
            "static_u64_three_arg_add_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_multiply_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func product3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left * middle * right;\n"
            "}\n"
            "var first: uint = 2;\n"
            "var second: uint = 3;\n"
            "var third: uint = 7;\n"
            "var value: uint = product3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-multiply-typed-call-smoke",
            "runtime_static_u64_three_arg_multiply_project",
            "static_u64_three_arg_multiply_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_subtract_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func diff3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left - middle - right;\n"
            "}\n"
            "var first: uint = 50;\n"
            "var second: uint = 7;\n"
            "var third: uint = 1;\n"
            "var value: uint = diff3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-subtract-typed-call-smoke",
            "runtime_static_u64_three_arg_subtract_project",
            "static_u64_three_arg_subtract_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_divide_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func quotient3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left / middle / right;\n"
            "}\n"
            "var first: uint = 168;\n"
            "var second: uint = 2;\n"
            "var third: uint = 2;\n"
            "var value: uint = quotient3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-divide-typed-call-smoke",
            "runtime_static_u64_three_arg_divide_project",
            "static_u64_three_arg_divide_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATEFUL_DECL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_DEF,
            "return (TZrUInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            "/* zr_aot_static_u64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_modulo_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func remainder3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left % middle % right;\n"
            "}\n"
            "var first: uint = 92;\n"
            "var second: uint = 50;\n"
            "var third: uint = 43;\n"
            "var value: uint = remainder3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-modulo-typed-call-smoke",
            "runtime_static_u64_three_arg_modulo_project",
            "static_u64_three_arg_modulo_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATEFUL_DECL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_DEF,
            "return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1 % zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            "/* zr_aot_static_u64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_bitwise_and_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func mask3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left & middle & right;\n"
            "}\n"
            "var first: uint = 58;\n"
            "var second: uint = 47;\n"
            "var third: uint = 62;\n"
            "var value: uint = mask3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-bitwise-and-typed-call-smoke",
            "runtime_static_u64_three_arg_bitwise_and_project",
            "static_u64_three_arg_bitwise_and_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_bitwise_or_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func flags3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left | middle | right;\n"
            "}\n"
            "var first: uint = 40;\n"
            "var second: uint = 2;\n"
            "var third: uint = 0;\n"
            "var value: uint = flags3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-bitwise-or-typed-call-smoke",
            "runtime_static_u64_three_arg_bitwise_or_project",
            "static_u64_three_arg_bitwise_or_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_u64_three_arg_bitwise_xor_typed_thunk(void) {
    const SZrAotTypedDirectCallU64SmokeCase testCase = {
            "func toggle3(left: uint, middle: uint, right: uint): uint {\n"
            "    return left ^ middle ^ right;\n"
            "}\n"
            "var first: uint = 63;\n"
            "var second: uint = 21;\n"
            "var third: uint = 0;\n"
            "var value: uint = toggle3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-u64-three-arg-bitwise-xor-typed-call-smoke",
            "runtime_static_u64_three_arg_bitwise_xor_project",
            "static_u64_three_arg_bitwise_xor_typed_call_smoke",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_U64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);",
            "/* zr_aot_static_u64_three_arg_direct_call */",
            ZR_AOT_U64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_U64_THREE_ARG_STATEFUL_CALL,
            42,
    };

    run_aot_c_typed_direct_call_u64_smoke(&testCase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_no_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_returns_static_u64_no_arg_result_through_u64_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_add_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_subtract_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_multiply_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_and_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_or_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_one_arg_bitwise_xor_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_modulo_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_subtract_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_two_arg_bitwise_xor_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_add_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_subtract_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_modulo_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_bitwise_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_bitwise_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_u64_three_arg_bitwise_xor_typed_thunk);
    return UNITY_END();
}
