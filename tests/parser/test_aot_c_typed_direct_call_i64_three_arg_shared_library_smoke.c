#include "aot_c_typed_direct_call_i64_smoke_support.h"

#define ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL \
    "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);"
#define ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF \
    "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {"
#define ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL \
    "zr_aot_typed_i64_fn_1(zr_aot_s"
#define ZR_AOT_I64_THREE_ARG_STATEFUL_DECL \
    "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2);"
#define ZR_AOT_I64_THREE_ARG_STATEFUL_DEF \
    "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2) {"
#define ZR_AOT_I64_THREE_ARG_STATEFUL_CALL \
    "zr_aot_typed_i64_fn_1(state, zr_aot_s"

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_add_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func sum3(left: int, middle: int, right: int): int {\n"
            "    return left + middle + right;\n"
            "}\n"
            "var first: int = 12;\n"
            "var second: int = 20;\n"
            "var third: int = 10;\n"
            "var value: int = sum3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_project",
            "static_i64_three_arg_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_multiply_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func product3(left: int, middle: int, right: int): int {\n"
            "    return left * middle * right;\n"
            "}\n"
            "var first: int = 2;\n"
            "var second: int = 3;\n"
            "var third: int = 7;\n"
            "var value: int = product3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_multiply_project",
            "static_i64_three_arg_multiply_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_subtract_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func diff3(left: int, middle: int, right: int): int {\n"
            "    return left - middle - right;\n"
            "}\n"
            "var first: int = 50;\n"
            "var second: int = 7;\n"
            "var third: int = 1;\n"
            "var value: int = diff3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_subtract_project",
            "static_i64_three_arg_subtract_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_divide_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func quotient3(left: int, middle: int, right: int): int {\n"
            "    return left / middle / right;\n"
            "}\n"
            "var first: int = 64;\n"
            "var second: int = 4;\n"
            "var third: int = 2;\n"
            "var value: int = quotient3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_divide_project",
            "static_i64_three_arg_divide_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATEFUL_DECL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_DEF,
            "return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            "/* zr_aot_static_i64_three_arg_direct_call_sync_stack_slot */",
            8
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_modulo_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func remainder3(left: int, middle: int, right: int): int {\n"
            "    return left % middle % right;\n"
            "}\n"
            "var first: int = 92;\n"
            "var second: int = 50;\n"
            "var third: int = 43;\n"
            "var value: int = remainder3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_modulo_project",
            "static_i64_three_arg_modulo_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATEFUL_DECL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_DEF,
            "return (TZrInt64)(zr_aot_arg0 % zr_aot_arg1 % zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            "/* zr_aot_static_i64_three_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_bitwise_and_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func mask3(left: int, middle: int, right: int): int {\n"
            "    return left & middle & right;\n"
            "}\n"
            "var first: int = 58;\n"
            "var second: int = 47;\n"
            "var third: int = 62;\n"
            "var value: int = mask3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_bitwise_and_project",
            "static_i64_three_arg_bitwise_and_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1 & zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_bitwise_or_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func flags3(left: int, middle: int, right: int): int {\n"
            "    return left | middle | right;\n"
            "}\n"
            "var first: int = 40;\n"
            "var second: int = 2;\n"
            "var third: int = 0;\n"
            "var value: int = flags3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_bitwise_or_project",
            "static_i64_three_arg_bitwise_or_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1 | zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_three_arg_bitwise_xor_typed_thunk(void) {
    const SZrAotTypedDirectCallI64SmokeCase testCase = {
            "func toggle3(left: int, middle: int, right: int): int {\n"
            "    return left ^ middle ^ right;\n"
            "}\n"
            "var first: int = 63;\n"
            "var second: int = 21;\n"
            "var third: int = 0;\n"
            "var value: int = toggle3(first, second, third);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-three-arg-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_three_arg_bitwise_xor_project",
            "static_i64_three_arg_bitwise_xor_typed_call_smoke",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DECL,
            ZR_AOT_I64_THREE_ARG_STATE_FREE_DEF,
            "return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1 ^ zr_aot_arg2);",
            "/* zr_aot_static_i64_three_arg_direct_call */",
            ZR_AOT_I64_THREE_ARG_STATE_FREE_CALL,
            ZR_AOT_I64_THREE_ARG_STATEFUL_CALL,
            42
    };

    aot_c_i64_smoke_run_case(&testCase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_add_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_subtract_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_modulo_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_bitwise_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_bitwise_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_three_arg_bitwise_xor_typed_thunk);
    return UNITY_END();
}
