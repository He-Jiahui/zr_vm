#include "aot_c_typed_direct_call_bitwise_smoke_support.h"

#define ZR_AOT_BITWISE_TWO_ARG_DECL \
    "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);"
#define ZR_AOT_BITWISE_TWO_ARG_DEF \
    "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {"
#define ZR_AOT_BITWISE_ONE_ARG_DECL \
    "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0);"
#define ZR_AOT_BITWISE_ONE_ARG_DEF \
    "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0) {"

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_or_typed_thunk(void) {
    const SZrAotTypedDirectCallBitwiseSmokeCase testCase = {
            "func join(left: int, right: int): int {\n"
            "    return left | right;\n"
            "}\n"
            "var left: int = 34;\n"
            "var right: int = 10;\n"
            "var value: int = join(left, right);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-bitwise-or-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_bitwise_or_project",
            "static_i64_bitwise_or_typed_call_smoke",
            ZR_AOT_BITWISE_TWO_ARG_DECL,
            ZR_AOT_BITWISE_TWO_ARG_DEF,
            "return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1);",
            "/* zr_aot_static_i64_two_arg_direct_call */",
            "zr_aot_typed_i64_fn_1(zr_aot_s",
            "/* zr_aot_static_i64_two_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_bitwise_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_xor_typed_thunk(void) {
    const SZrAotTypedDirectCallBitwiseSmokeCase testCase = {
            "func toggle(left: int, right: int): int {\n"
            "    return left ^ right;\n"
            "}\n"
            "var left: int = 58;\n"
            "var right: int = 16;\n"
            "var value: int = toggle(left, right);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-bitwise-xor-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_bitwise_xor_project",
            "static_i64_bitwise_xor_typed_call_smoke",
            ZR_AOT_BITWISE_TWO_ARG_DECL,
            ZR_AOT_BITWISE_TWO_ARG_DEF,
            "return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1);",
            "/* zr_aot_static_i64_two_arg_direct_call */",
            "zr_aot_typed_i64_fn_1(zr_aot_s",
            "/* zr_aot_static_i64_two_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_bitwise_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_not_typed_thunk(void) {
    const SZrAotTypedDirectCallBitwiseSmokeCase testCase = {
            "func invert(value: int): int {\n"
            "    return ~value;\n"
            "}\n"
            "var seed: int = 0;\n"
            "var value: int = invert(seed);\n"
            "return value + 43;",
            "{"
            "\"name\":\"aot-runtime-static-i64-bitwise-not-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_bitwise_not_project",
            "static_i64_bitwise_not_typed_call_smoke",
            ZR_AOT_BITWISE_ONE_ARG_DECL,
            ZR_AOT_BITWISE_ONE_ARG_DEF,
            "return (TZrInt64)(~zr_aot_arg0);",
            "/* zr_aot_static_i64_one_arg_direct_call */",
            "zr_aot_typed_i64_fn_1(zr_aot_s",
            "/* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_bitwise_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_and_const_typed_thunk(void) {
    const SZrAotTypedDirectCallBitwiseSmokeCase testCase = {
            "func maskBy(value: int): int {\n"
            "    return value & 47;\n"
            "}\n"
            "var seed: int = 58;\n"
            "var value: int = maskBy(seed);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-bitwise-and-const-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_bitwise_and_const_project",
            "static_i64_bitwise_and_const_typed_call_smoke",
            ZR_AOT_BITWISE_ONE_ARG_DECL,
            ZR_AOT_BITWISE_ONE_ARG_DEF,
            "return (TZrInt64)(zr_aot_arg0 & (TZrInt64)47);",
            "/* zr_aot_static_i64_one_arg_direct_call */",
            "zr_aot_typed_i64_fn_1(zr_aot_s",
            "/* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_bitwise_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_or_const_typed_thunk(void) {
    const SZrAotTypedDirectCallBitwiseSmokeCase testCase = {
            "func flags(value: int): int {\n"
            "    return value | 10;\n"
            "}\n"
            "var seed: int = 32;\n"
            "var value: int = flags(seed);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-bitwise-or-const-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_bitwise_or_const_project",
            "static_i64_bitwise_or_const_typed_call_smoke",
            ZR_AOT_BITWISE_ONE_ARG_DECL,
            ZR_AOT_BITWISE_ONE_ARG_DEF,
            "return (TZrInt64)(zr_aot_arg0 | (TZrInt64)10);",
            "/* zr_aot_static_i64_one_arg_direct_call */",
            "zr_aot_typed_i64_fn_1(zr_aot_s",
            "/* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_bitwise_smoke_run_case(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_xor_const_typed_thunk(void) {
    const SZrAotTypedDirectCallBitwiseSmokeCase testCase = {
            "func flip(value: int): int {\n"
            "    return value ^ 6;\n"
            "}\n"
            "var seed: int = 44;\n"
            "var value: int = flip(seed);\n"
            "return value;",
            "{"
            "\"name\":\"aot-runtime-static-i64-bitwise-xor-const-typed-call-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}",
            "runtime_static_i64_bitwise_xor_const_project",
            "static_i64_bitwise_xor_const_typed_call_smoke",
            ZR_AOT_BITWISE_ONE_ARG_DECL,
            ZR_AOT_BITWISE_ONE_ARG_DEF,
            "return (TZrInt64)(zr_aot_arg0 ^ (TZrInt64)6);",
            "/* zr_aot_static_i64_one_arg_direct_call */",
            "zr_aot_typed_i64_fn_1(zr_aot_s",
            "/* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */",
            42
    };

    aot_c_bitwise_smoke_run_case(&testCase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_or_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_xor_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_not_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_and_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_or_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_bitwise_xor_const_typed_thunk);
    return UNITY_END();
}
