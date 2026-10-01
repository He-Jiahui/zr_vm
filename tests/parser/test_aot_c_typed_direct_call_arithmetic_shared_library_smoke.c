#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

/* i64 typed 调用算术矩阵：各静态用例提供源码、生成 C 针脚与预期结果，交给共享 fixture 完成编译、链接和执行。 */
#include "aot_c_typed_direct_call_arithmetic_smoke_support.h"

#include <stdint.h>

#if defined(ZR_PLATFORM_UNIX)
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static const char *const i64_two_arg_multiply_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1);",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(zr_aot_s",
};

/* 除法针脚同时要求 state 参数与除零诊断，区分有失败路径的 thunk 和纯算术 thunk。 */
static const char *const i64_two_arg_divide_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "ZrLibrary_AotRuntime_RecordError(state, \"generated AOT signed divide by zero\");",
        "generated AOT signed divide by zero",
        "return (TZrInt64)(zr_aot_arg0 / zr_aot_arg1);",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(state, zr_aot_s",
};

static const char *const i64_two_arg_modulo_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);",
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "ZrLibrary_AotRuntime_RecordError(state, \"generated AOT signed modulo by zero\");",
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
        "fn product(left: int, right: int): int {\n"
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
        ZR_NULL,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_divide_case = {
        "fn ratio(left: int, right: int): int {\n"
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
        ZR_NULL,
};

static const char *const i64_two_arg_divide_near_boundary_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(state, zr_aot_s",
};

static const SZrAotTypedDirectCallArithmeticSmokeCase
        i64_two_arg_divide_near_boundary_case = {
        "fn ratio(left: int, right: int): int {\n"
        "    return left / right;\n"
        "}\n"
        "var min: int = -9223372036854775807 - 1;\n"
        "var left: int = min + 1;\n"
        "var right: int = -1;\n"
        "var value: int = ratio(left, right);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-divide-near-boundary-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_divide_near_boundary_project",
        "static_i64_divide_near_boundary_typed_call_smoke",
        i64_two_arg_divide_near_boundary_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_divide_near_boundary_needles),
        INT64_MAX,
        ZR_NULL,
};

static const char *const i64_two_arg_divide_overflow_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "ZrLibrary_AotRuntime_RecordError(state, \"generated AOT signed division overflow\");",
        "generated AOT signed division overflow",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(state, zr_aot_s",
};

static const SZrAotTypedDirectCallArithmeticSmokeCase
        i64_two_arg_divide_overflow_case = {
        "fn ratio(left: int, right: int): int {\n"
        "    return left / right;\n"
        "}\n"
        "var left: int = -9223372036854775807 - 1;\n"
        "var right: int = -1;\n"
        "var value: int = ratio(left, right);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-divide-overflow-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_divide_overflow_project",
        "static_i64_divide_overflow_typed_call_smoke",
        i64_two_arg_divide_overflow_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_divide_overflow_needles),
        0,
        "generated AOT signed division overflow",
};

static const char *const i64_two_arg_modulo_overflow_needles[] = {
        "static TZrInt64 zr_aot_typed_i64_fn_1(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {",
        "ZrLibrary_AotRuntime_RecordError(state, \"generated AOT signed modulo overflow\");",
        "generated AOT signed modulo overflow",
        "/* zr_aot_static_i64_two_arg_direct_call */",
        "zr_aot_typed_i64_fn_1(state, zr_aot_s",
};

static const SZrAotTypedDirectCallArithmeticSmokeCase
        i64_two_arg_modulo_overflow_case = {
        "fn remainder(left: int, right: int): int {\n"
        "    return left % right;\n"
        "}\n"
        "var left: int = -9223372036854775807 - 1;\n"
        "var right: int = -1;\n"
        "var value: int = remainder(left, right);\n"
        "return value;",
        "{"
        "\"name\":\"aot-runtime-static-i64-modulo-overflow-typed-call-smoke\","
        "\"source\":\"src\","
        "\"binary\":\"bin\","
        "\"entry\":\"main\""
        "}",
        "runtime_static_i64_modulo_overflow_project",
        "static_i64_modulo_overflow_typed_call_smoke",
        i64_two_arg_modulo_overflow_needles,
        ZR_TESTS_ARRAY_COUNT(i64_two_arg_modulo_overflow_needles),
        0,
        "generated AOT signed modulo overflow",
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_modulo_case = {
        "fn remainder(left: int, right: int): int {\n"
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
        ZR_NULL,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_two_arg_bitwise_and_case = {
        "fn mask(left: int, right: int): int {\n"
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
        ZR_NULL,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_one_arg_multiply_const_case = {
        "fn scale(value: int): int {\n"
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
        ZR_NULL,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_one_arg_subtract_const_case = {
        "fn decBy(value: int): int {\n"
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
        ZR_NULL,
};

static const SZrAotTypedDirectCallArithmeticSmokeCase i64_one_arg_negate_case = {
        "fn negate(value: int): int {\n"
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
        ZR_NULL,
};

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_multiply_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_multiply_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_divide_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_divide_case);
}

static void test_aot_c_generated_shared_library_executes_static_i64_two_arg_divide_near_boundary_typed_thunk(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_divide_near_boundary_case);
}

static void run_i64_two_arg_divide_overflow_child_case(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_divide_overflow_case);
}

static int run_i64_two_arg_divide_overflow_child_suite(void) {
    UNITY_BEGIN();
    RUN_TEST(run_i64_two_arg_divide_overflow_child_case);
    return UNITY_END();
}

static void run_i64_two_arg_modulo_overflow_child_case(void) {
    run_i64_arithmetic_smoke_case(&i64_two_arg_modulo_overflow_case);
}

static int run_i64_two_arg_modulo_overflow_child_suite(void) {
    UNITY_BEGIN();
    RUN_TEST(run_i64_two_arg_modulo_overflow_child_case);
    return UNITY_END();
}

static void assert_i64_overflow_child_exits_cleanly(const char *childArgument,
                                                    const char *operationName) {
#if !defined(ZR_PLATFORM_UNIX) || !defined(__linux__)
    TEST_IGNORE_MESSAGE("AOT C typed direct-call overflow crash isolation requires Linux child processes");
#else
    pid_t childPid;
    pid_t waitedPid;
    int childStatus = 0;
    char failureMessage[128];

    (void)fflush(NULL);
    childPid = fork();
    TEST_ASSERT_TRUE_MESSAGE(childPid >= 0,
                             "fork failed before generated overflow execution");
    if (childPid == 0) {
        char *const childArguments[] = {
                (char *)"zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test",
                (char *)childArgument,
                ZR_NULL,
        };

        if (setpgid(0, 0) != 0) {
            static const char message[] = "[i64-overflow] child process group creation failed\n";
            (void)write(STDERR_FILENO, message, sizeof(message) - 1u);
            _exit(126);
        }
        execv("/proc/self/exe", childArguments);
        {
            static const char message[] = "[i64-overflow] exec /proc/self/exe failed\n";
            (void)write(STDERR_FILENO, message, sizeof(message) - 1u);
        }
        _exit(127);
    }

    do {
        waitedPid = waitpid(childPid, &childStatus, 0);
    } while (waitedPid < 0 && errno == EINTR);
    TEST_ASSERT_EQUAL_INT_MESSAGE(childPid, waitedPid,
                                 "waitpid failed for generated overflow child");
    if (WIFSIGNALED(childStatus)) {
        /* Compiler descendants share the child's group, including on alarm expiry. */
        (void)kill(-childPid, SIGKILL);
        (void)snprintf(failureMessage, sizeof(failureMessage),
                       "generated INT64_MIN / -1 %s child died from signal %d",
                       operationName,
                       WTERMSIG(childStatus));
        TEST_FAIL_MESSAGE(failureMessage);
    }
    TEST_ASSERT_TRUE_MESSAGE(WIFEXITED(childStatus),
                             "generated overflow child did not exit normally");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, WEXITSTATUS(childStatus),
                                  "generated overflow fixture did not observe checked runtime failure");
#endif
}

static void test_aot_c_generated_shared_library_rejects_static_i64_two_arg_divide_overflow(void) {
    assert_i64_overflow_child_exits_cleanly("--overflow-child", "division");
}

static void test_aot_c_generated_shared_library_rejects_static_i64_two_arg_modulo_overflow(void) {
    assert_i64_overflow_child_exits_cleanly("--modulo-overflow-child", "modulo");
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

/* TODO: 此目标在 tests/CMakeLists.txt 创建，但未列入 language_pipeline；
 * 验收文档按可执行文件手工运行。核对是否应加入常规 CTest 回归入口。 */
int main(int argc, char **argv) {
    if (argc == 2 && (strcmp(argv[1], "--overflow-child") == 0 ||
                      strcmp(argv[1], "--modulo-overflow-child") == 0)) {
#if defined(ZR_PLATFORM_UNIX) && defined(__linux__)
        struct rlimit coreLimit;

        if (getrlimit(RLIMIT_CORE, &coreLimit) != 0) {
            (void)fprintf(stderr, "[i64-overflow] getrlimit(RLIMIT_CORE) failed\n");
            return 125;
        }
        coreLimit.rlim_cur = 0;
        if (setrlimit(RLIMIT_CORE, &coreLimit) != 0) {
            (void)fprintf(stderr, "[i64-overflow] setrlimit(RLIMIT_CORE) failed\n");
            return 125;
        }
        (void)alarm(300u);
        if (strcmp(argv[1], "--modulo-overflow-child") == 0) {
            return run_i64_two_arg_modulo_overflow_child_suite();
        }
        return run_i64_two_arg_divide_overflow_child_suite();
#else
        (void)fprintf(stderr, "%s requires Unix\n", argv[1]);
        return 2;
#endif
    }

    if (argc == 2 && strcmp(argv[1], "--overflow-only") == 0) {
        UNITY_BEGIN();
        RUN_TEST(test_aot_c_generated_shared_library_rejects_static_i64_two_arg_divide_overflow);
        RUN_TEST(test_aot_c_generated_shared_library_rejects_static_i64_two_arg_modulo_overflow);
        return UNITY_END();
    }

    if (argc != 1) {
        (void)fprintf(stderr, "usage: %s [--overflow-only]\n", argv[0]);
        return 2;
    }

    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_multiply_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_divide_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_divide_near_boundary_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_rejects_static_i64_two_arg_divide_overflow);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_modulo_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_rejects_static_i64_two_arg_modulo_overflow);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_two_arg_bitwise_and_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_multiply_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_subtract_const_typed_thunk);
    RUN_TEST(test_aot_c_generated_shared_library_executes_static_i64_one_arg_negate_typed_thunk);
    return UNITY_END();
}
