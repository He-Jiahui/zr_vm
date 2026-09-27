/* 仅把命令行筛选参数转交给 ctest，套件发现与执行以 CTest 注册表为准。 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#else
    #include <unistd.h>
#endif

static int set_process_env(const char* name, const char* value) {
#if defined(_WIN32)
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

static void print_usage(const char* executable) {
    printf("ZR-VM test runner is a thin wrapper around ctest.\n");
    printf("ctest is the only source of truth for discovery and execution.\n\n");
    printf("Primary suites:\n");
    printf("  core_runtime\n");
    printf("  language_pipeline\n");
    printf("  containers\n");
    printf("  language_server\n");
    printf("  language_server_stdio_smoke\n");
    printf("  cli_args\n");
    printf("  cli_integration\n");
    printf("  projects\n");
    printf("  performance_report\n\n");
    printf("Usage:\n");
    printf("  %s --help\n", executable);
    printf("  %s --tier <smoke|core|stress> --ctest [ctest arguments...]\n", executable);
    printf("  %s --ctest [ctest arguments...]\n\n", executable);
    printf("Environment:\n");
    printf("  ZR_VM_TEST_TIER=smoke|core|stress   Filter suite runners to a tier subset\n\n");
    printf("  ZR_VM_PERF_WARMUP=<n>               Override warmup iteration count for performance_report\n");
    printf("  ZR_VM_PERF_ITERATIONS=<n>           Override measured iteration count for performance_report\n\n");
    printf("Examples:\n");
    printf("  %s --ctest --output-on-failure\n", executable);
    printf("  %s --tier smoke --ctest --output-on-failure -R \"language_pipeline|projects\"\n", executable);
    printf("  %s --ctest --output-on-failure -R language_pipeline\n", executable);
    printf("  %s --ctest -N\n", executable);
}

int main(int argc, char* argv[]) {
    const char* tier = NULL;
    int arg_index = 1;

    if (argc == 1 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (arg_index + 1 < argc && strcmp(argv[arg_index], "--tier") == 0) {
        tier = argv[arg_index + 1];
        arg_index += 2;
    }

    if (arg_index >= argc || strcmp(argv[arg_index], "--ctest") != 0) {
        fprintf(stderr, "Unknown argument: %s\n\n", arg_index < argc ? argv[arg_index] : "(missing)");
        print_usage(argv[0]);
        return 1;
    }

    if (tier != NULL && set_process_env("ZR_VM_TEST_TIER", tier) != 0) {
        fprintf(stderr, "Failed to set ZR_VM_TEST_TIER.\n");
        return 1;
    }

    {
        char command[4096];
        int offset = snprintf(command, sizeof(command), "ctest");

        if (offset < 0 || (size_t)offset >= sizeof(command)) {
            return 1;
        }

        /* BUG: -R "a|b" 未转义即交给 system，| 成为管道；WSL --ctest -N -R "__zr_a__|__zr_b__" 输出 __zr_b__: not found。 */
        for (int i = arg_index + 1; i < argc; ++i) {
            int written = snprintf(command + offset, sizeof(command) - (size_t)offset, " %s", argv[i]);
            if (written < 0 || (size_t)written >= sizeof(command) - (size_t)offset) {
                fprintf(stderr, "ctest command line too long.\n");
                return 1;
            }
            offset += written;
        }

        /* BUG: POSIX system() 返回编码后的等待状态；ctest 失败码 1 经 main 截断为 0。
         * 复现：WSL 下运行包装器 --ctest --preset __zr_vm_no_such_preset__ 返回 0，直接运行 ctest 返回 1。 */
        return system(command);
    }
}
