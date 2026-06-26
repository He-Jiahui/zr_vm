#include "aot_c_typed_direct_call_arithmetic_smoke_support.h"

static const char *const i64_two_arg_multiply_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1);",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(zr_aot_s",
};

static const char *const i64_two_arg_divide_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "generated AOT signed divide by zero",
        "return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1);",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(state, zr_aot_s",
};

static const char *const i64_two_arg_modulo_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "generated AOT signed modulo by zero",
        "return (TZrInt64)(zr_aot_arg0 % zr_aot_arg1);",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(state, zr_aot_s",
};

static const char *const i64_two_arg_bitwise_and_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1);",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(zr_aot_s",
};

static const char *const i64_one_arg_multiply_const_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0) {",
        "return (TZrInt64)(zr_aot_arg0 * (TZrInt64)21);",
        "/* zr_aot_static_i64_one_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(zr_aot_s",
};

static const char *const i64_one_arg_subtract_const_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0) {",
        "return (TZrInt64)(zr_aot_arg0 - (TZrInt64)8);",
        "/* zr_aot_static_i64_one_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(zr_aot_s",
};

static const char *const i64_one_arg_negate_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0) {",
        "return (TZrInt64)(-zr_aot_arg0);",
        "/* zr_aot_static_i64_one_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(zr_aot_s",
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_multiply_case = {
        "func product(left: int, right: int): int {\n"
        "    return left * right;\n"
        "}\n"
        "var left: int = 6;\n"
        "var right: int = 7;\n"
        "var value: int = product(left, right);\n"
        "return value + 0;",
        "{"
        "\"name\":\"aot-runtime-static-i64-typed-arithmetic-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_project",
        "static_i64_multiply_typed_call_smoke",
        i64_two_arg_multiply_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_multiply_needles),
        42,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_divide_case = {
        "func ratio(left: int, right: int): int {\n"
        "    return left / right;\n"
        "}\n"
        "var left: int = 84;\n"
        "var right: int = 2;\n"
        "var value: int = ratio(left, right);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-divide-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_divide_project",
        "static_i64_divide_typed_call_smoke",
        i64_two_arg_divide_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_divide_needles),
        42,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_modulo_case = {
        "func remainder(left: int, right: int): int {\n"
        "    return left % right;\n"
        "}\n"
        "var left: int = 92;\n"
        "var right: int = 50;\n"
        "var value: int = remainder(left, right);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-modulo-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_modulo_project",
        "static_i64_modulo_typed_call_smoke",
        i64_two_arg_modulo_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_modulo_needles),
        42,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_bitwise_and_case = {
        "func mask(left: int, right: int): int {\n"
        "    return left & right;\n"
        "}\n"
        "var left: int = 58;\n"
        "var right: int = 47;\n"
        "var value: int = mask(left, right);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-bitwise-and-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_bitwise_and_project",
        "static_i64_bitwise_and_typed_call_smoke",
        i64_two_arg_bitwise_and_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_bitwise_and_needles),
        42,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_one_arg_multiply_const_case = {
        "func scale(value: int): int {\n"
        "    return value * 21;\n"
        "}\n"
        "var seed: int = 2;\n"
        "var value: int = scale(seed);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-one-arg-multiply-const-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_one_arg_multiply_const_project",
        "static_i64_one_arg_multiply_const_typed_call_smoke",
        i64_one_arg_multiply_const_needles,
        ZR_TESTS_ARRAY_COUNT(i64_one_arg_multiply_const_needles),
        42,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_one_arg_subtract_const_case = {
        "func decBy(value: int): int {\n"
        "    return value - 8;\n"
        "}\n"
        "var seed: int = 50;\n"
        "var value: int = decBy(seed);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-one-arg-subtract-const-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_one_arg_subtract_const_project",
        "static_i64_one_arg_subtract_const_typed_call_smoke",
        i64_one_arg_subtract_const_needles,
        ZR_TESTS_ARRAY_COUNT(i64_one_arg_subtract_const_needles),
        42,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_one_arg_negate_case = {
        "func negate(value: int): int {\n"
        "    return -value;\n"
        "}\n"
        "var seed: int = -42;\n"
        "var value: int = negate(seed);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-one-arg-negate-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_one_arg_negate_project",
        "static_i64_one_arg_negate_typed_call_smoke",
        i64_one_arg_negate_needles,
        ZR_TESTS_ARRAY_COUNT(i64_one_arg_negate_needles),
        42,
};

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_multiply_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_multiply_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_divide_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_divide_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_modulo_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_modulo_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_and_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_bitwise_and_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_multiply_const_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_one_arg_multiply_const_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_subtract_const_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_one_arg_subtract_const_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_negate_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_one_arg_negate_case);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_modulo_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_multiply_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_subtract_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_negate_typed_thunk);
    return UNITY_END();
}
