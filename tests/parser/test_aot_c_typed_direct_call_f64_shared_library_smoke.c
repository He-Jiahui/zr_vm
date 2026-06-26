#include "aot_c_typed_direct_call_f64_smoke_support.h"

static void test_aot_c_generated_shared_library_executes_static_f64_no_arg_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func answer(): float {\n"
            "    return 5.0;\n"
            "}\n"
            "var value: float = answer();\n"
            "return <int> value + 37;",
            "aot-runtime-static-f64-no-arg-typed-call-smoke",
            "runtime_static_f64_no_arg_project",
            "static_f64_no_arg_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(void);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(void) {",
            "return (TZrFloat64)5;",
            "/* zr_aot_static_f64_no_arg_direct_call */",
            "zr_aot_typed_f64_fn_1()",
            "/* zr_aot_static_f64_no_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_returns_static_f64_no_arg_result_through_f64_boundary(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func answer(): float {\n"
            "    var result: float = 42.0;\n"
            "    return result;\n"
            "}\n"
            "var value: float = answer();\n"
            "return <int> value;",
            "aot-runtime-static-f64-return-boundary-smoke",
            "runtime_static_f64_return_boundary_project",
            "static_f64_return_boundary_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(void);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(void) {",
            "return (TZrFloat64)42;",
            "/* zr_aot_static_f64_no_arg_direct_call */",
            "zr_aot_typed_f64_fn_1()",
            "/* zr_aot_static_f64_no_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke_with_options(
            &testCase,
            "ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f",
            ZR_FALSE);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func pass(value: float): float {\n"
            "    return value;\n"
            "}\n"
            "var seed: float = 5.0;\n"
            "var value: float = pass(seed);\n"
            "return <int> value + 37;",
            "aot-runtime-static-f64-one-arg-typed-call-smoke",
            "runtime_static_f64_one_arg_project",
            "static_f64_one_arg_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return zr_aot_arg0;",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_negate_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func negate(value: float): float {\n"
            "    return -value;\n"
            "}\n"
            "var seed: float = -42.0;\n"
            "var value: float = negate(seed);\n"
            "return <int> value;",
            "aot-runtime-static-f64-one-arg-negate-typed-call-smoke",
            "runtime_static_f64_one_arg_negate_project",
            "static_f64_one_arg_negate_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return (TZrFloat64)(-zr_aot_arg0);",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_add_const_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func inc(value: float): float {\n"
            "    return value + 37.0;\n"
            "}\n"
            "var seed: float = 5.0;\n"
            "var value: float = inc(seed);\n"
            "return <int> value;",
            "aot-runtime-static-f64-one-arg-add-const-typed-call-smoke",
            "runtime_static_f64_one_arg_add_const_project",
            "static_f64_one_arg_add_const_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return (TZrFloat64)(zr_aot_arg0 + (TZrFloat64)37);",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_subtract_const_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func dec(value: float): float {\n"
            "    return value - 8.0;\n"
            "}\n"
            "var seed: float = 50.0;\n"
            "var value: float = dec(seed);\n"
            "return <int> value;",
            "aot-runtime-static-f64-one-arg-subtract-const-typed-call-smoke",
            "runtime_static_f64_one_arg_subtract_const_project",
            "static_f64_one_arg_subtract_const_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return (TZrFloat64)(zr_aot_arg0 - (TZrFloat64)8);",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_multiply_const_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func scale(value: float): float {\n"
            "    return value * 21.0;\n"
            "}\n"
            "var seed: float = 2.0;\n"
            "var value: float = scale(seed);\n"
            "return <int> value;",
            "aot-runtime-static-f64-one-arg-multiply-const-typed-call-smoke",
            "runtime_static_f64_one_arg_multiply_const_project",
            "static_f64_one_arg_multiply_const_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return (TZrFloat64)(zr_aot_arg0 * (TZrFloat64)21);",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_divide_const_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func halve(value: float): float {\n"
            "    return value / 2.0;\n"
            "}\n"
            "var seed: float = 84.0;\n"
            "var value: float = halve(seed);\n"
            "return <int> value;",
            "aot-runtime-static-f64-one-arg-divide-const-typed-call-smoke",
            "runtime_static_f64_one_arg_divide_const_project",
            "static_f64_one_arg_divide_const_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return (TZrFloat64)(zr_aot_arg0 / (TZrFloat64)2);",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_one_arg_modulo_const_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func remainder(value: float): float {\n"
            "    return value % 50.0;\n"
            "}\n"
            "var seed: float = 92.0;\n"
            "var value: float = remainder(seed);\n"
            "return <int> value;",
            "aot-runtime-static-f64-one-arg-modulo-const-typed-call-smoke",
            "runtime_static_f64_one_arg_modulo_const_project",
            "static_f64_one_arg_modulo_const_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0) {",
            "return (TZrFloat64)fmod(zr_aot_arg0, (TZrFloat64)50);",
            "/* zr_aot_static_f64_one_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_one_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_add_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func sum(left: float, right: float): float {\n"
            "    return left + right;\n"
            "}\n"
            "var first: float = 19.0;\n"
            "var second: float = 18.0;\n"
            "var value: float = sum(first, second);\n"
            "return <int> value + 5;",
            "aot-runtime-static-f64-two-arg-typed-call-smoke",
            "runtime_static_f64_two_arg_project",
            "static_f64_two_arg_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1);",
            "/* zr_aot_static_f64_two_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_three_arg_add_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func sum3(left: float, middle: float, right: float): float {\n"
            "    return left + middle + right;\n"
            "}\n"
            "var first: float = 12.0;\n"
            "var second: float = 20.0;\n"
            "var third: float = 10.0;\n"
            "var value: float = sum3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-f64-three-arg-add-typed-call-smoke",
            "runtime_static_f64_three_arg_add_project",
            "static_f64_three_arg_add_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {",
            "return (TZrFloat64)(zr_aot_arg0 + zr_aot_arg1 + zr_aot_arg2);",
            "/* zr_aot_static_f64_three_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_three_arg_multiply_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func product3(left: float, middle: float, right: float): float {\n"
            "    return left * middle * right;\n"
            "}\n"
            "var first: float = 3.0;\n"
            "var second: float = 7.0;\n"
            "var third: float = 2.0;\n"
            "var value: float = product3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-f64-three-arg-multiply-typed-call-smoke",
            "runtime_static_f64_three_arg_multiply_project",
            "static_f64_three_arg_multiply_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {",
            "return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1 * zr_aot_arg2);",
            "/* zr_aot_static_f64_three_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_three_arg_subtract_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func diff3(left: float, middle: float, right: float): float {\n"
            "    return left - middle - right;\n"
            "}\n"
            "var first: float = 60.0;\n"
            "var second: float = 15.0;\n"
            "var third: float = 3.0;\n"
            "var value: float = diff3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-f64-three-arg-subtract-typed-call-smoke",
            "runtime_static_f64_three_arg_subtract_project",
            "static_f64_three_arg_subtract_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {",
            "return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1 - zr_aot_arg2);",
            "/* zr_aot_static_f64_three_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_three_arg_divide_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func ratio3(left: float, middle: float, right: float): float {\n"
            "    return left / middle / right;\n"
            "}\n"
            "var first: float = 168.0;\n"
            "var second: float = 2.0;\n"
            "var third: float = 2.0;\n"
            "var value: float = ratio3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-f64-three-arg-divide-typed-call-smoke",
            "runtime_static_f64_three_arg_divide_project",
            "static_f64_three_arg_divide_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {",
            "return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1 / zr_aot_arg2);",
            "/* zr_aot_static_f64_three_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_three_arg_modulo_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func remainder3(left: float, middle: float, right: float): float {\n"
            "    return left % middle % right;\n"
            "}\n"
            "var first: float = 92.0;\n"
            "var second: float = 50.0;\n"
            "var third: float = 43.0;\n"
            "var value: float = remainder3(first, second, third);\n"
            "return <int> value;",
            "aot-runtime-static-f64-three-arg-modulo-typed-call-smoke",
            "runtime_static_f64_three_arg_modulo_project",
            "static_f64_three_arg_modulo_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1, TZrFloat64 zr_aot_arg2) {",
            "return (TZrFloat64)fmod(fmod(zr_aot_arg0, zr_aot_arg1), zr_aot_arg2);",
            "/* zr_aot_static_f64_three_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_three_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_subtract_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func diff(left: float, right: float): float {\n"
            "    return left - right;\n"
            "}\n"
            "var first: float = 50.0;\n"
            "var second: float = 9.0;\n"
            "var value: float = diff(first, second);\n"
            "return <int> value + 1;",
            "aot-runtime-static-f64-two-arg-subtract-typed-call-smoke",
            "runtime_static_f64_two_arg_subtract_project",
            "static_f64_two_arg_subtract_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrFloat64)(zr_aot_arg0 - zr_aot_arg1);",
            "/* zr_aot_static_f64_two_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_multiply_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func product(left: float, right: float): float {\n"
            "    return left * right;\n"
            "}\n"
            "var first: float = 6.0;\n"
            "var second: float = 7.0;\n"
            "var value: float = product(first, second);\n"
            "return <int> value;",
            "aot-runtime-static-f64-two-arg-multiply-typed-call-smoke",
            "runtime_static_f64_two_arg_multiply_project",
            "static_f64_two_arg_multiply_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrFloat64)(zr_aot_arg0 * zr_aot_arg1);",
            "/* zr_aot_static_f64_two_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(zr_aot_f",
            "/* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_divide_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func ratio(left: float, right: float): float {\n"
            "    return left / right;\n"
            "}\n"
            "var first: float = 84.0;\n"
            "var second: float = 2.0;\n"
            "var value: float = ratio(first, second);\n"
            "return <int> value;",
            "aot-runtime-static-f64-two-arg-divide-typed-call-smoke",
            "runtime_static_f64_two_arg_divide_project",
            "static_f64_two_arg_divide_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrFloat64)(zr_aot_arg0 / zr_aot_arg1);",
            "/* zr_aot_static_f64_two_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

static void test_aot_c_generated_shared_library_executes_static_f64_two_arg_modulo_typed_thunk(void) {
    const SZrAotTypedDirectCallF64SmokeCase testCase = {
            "func remainder(left: float, right: float): float {\n"
            "    return left % right;\n"
            "}\n"
            "var first: float = 93.0;\n"
            "var second: float = 51.0;\n"
            "var value: float = remainder(first, second);\n"
            "return <int> value;",
            "aot-runtime-static-f64-two-arg-modulo-typed-call-smoke",
            "runtime_static_f64_two_arg_modulo_project",
            "static_f64_two_arg_modulo_typed_call_smoke",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);",
            "static TZrFloat64 zr_aot_typed_f64_fn_1(struct SZrState *state, TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {",
            "return (TZrFloat64)fmod(zr_aot_arg0, zr_aot_arg1);",
            "/* zr_aot_static_f64_two_arg_direct_call */",
            "zr_aot_typed_f64_fn_1(state, zr_aot_f",
            "/* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */",
            42,
    };

    run_aot_c_typed_direct_call_f64_smoke(&testCase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_no_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_returns_static_f64_no_arg_result_through_f64_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_negate_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_add_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_subtract_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_multiply_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_divide_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_one_arg_modulo_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_add_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_three_arg_add_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_three_arg_subtract_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_three_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_three_arg_modulo_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_three_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_subtract_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_f64_two_arg_modulo_typed_thunk);
    return UNITY_END();
}
