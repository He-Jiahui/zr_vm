#include <stdio.h>
#include <string.h>

#include "zr_vm_cli/command.h"

#define CLI_ASSERT_TRUE(condition, message)                                                                            \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "assertion failed: %s\n", message);                                                        \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define CLI_ASSERT_INT_EQ(expected, actual, message)                                                                   \
    do {                                                                                                               \
        if ((expected) != (actual)) {                                                                                  \
            fprintf(stderr,                                                                                             \
                    "assertion failed: %s (expected=%d actual=%d)\n",                                                 \
                    message,                                                                                            \
                    (int)(expected),                                                                                    \
                    (int)(actual));                                                                                     \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

static int test_no_args_enters_repl(void) {
    char *argv[] = {"zr_vm_cli"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(1, argv, &command, error, sizeof(error)), "parse no-args");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_REPL, command.mode, "mode should be repl");
    CLI_ASSERT_TRUE(command.projectPath == ZR_NULL, "repl should not have project path");
    return 0;
}

static int test_positional_project_keeps_run_compatibility(void) {
    char *argv[] = {"zr_vm_cli", "demo.zrp"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv, &command, error, sizeof(error)), "parse positional run");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT, command.mode, "mode should be run project");
    CLI_ASSERT_TRUE(command.projectPath != ZR_NULL, "project path should be present");
    CLI_ASSERT_TRUE(strcmp(command.projectPath, "demo.zrp") == 0, "project path should match positional arg");
    return 0;
}

static int test_compile_flag_combinations_parse(void) {
    char *argv[] = {"zr_vm_cli", "--compile", "demo.zrp", "--intermediate", "--incremental", "--run"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(6, argv, &command, error, sizeof(error)), "parse compile flags");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_COMPILE_PROJECT, command.mode, "mode should be compile project");
    CLI_ASSERT_TRUE(strcmp(command.projectPath, "demo.zrp") == 0, "compile path should match");
    CLI_ASSERT_TRUE(command.emitIntermediate, "intermediate flag should be set");
    CLI_ASSERT_TRUE(command.incremental, "incremental flag should be set");
    CLI_ASSERT_TRUE(command.runAfterCompile, "run flag should be set");
    return 0;
}

static int test_aot_compile_flags_parse(void) {
    char *argv[] = {"zr_vm_cli",
                    "--compile",
                    "demo.zrp",
                    "--emit-aot-c",
                    "--emit-aot-llvm",
                    "--execution-mode",
                    "aot_c",
                    "--run",
                    "--require-aot-path",
                    "--emit-executed-via"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(10, argv, &command, error, sizeof(error)), "parse aot compile flags");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_COMPILE_PROJECT, command.mode, "mode should be compile project");
    CLI_ASSERT_TRUE(command.emitAotC, "emit aot c flag should be set");
    CLI_ASSERT_TRUE(command.emitAotLlvm, "emit aot llvm flag should be set");
    CLI_ASSERT_TRUE(command.requireAotPath, "require aot path flag should be set");
    CLI_ASSERT_TRUE(command.emitExecutedVia, "emit executed_via flag should be set");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_AOT_C, command.executionMode, "execution mode should be aot_c");
    return 0;
}

static int test_compile_only_modifiers_require_compile(void) {
    char *argv1[] = {"zr_vm_cli", "--run"};
    char *argv2[] = {"zr_vm_cli", "--intermediate"};
    char *argv3[] = {"zr_vm_cli", "--incremental"};
    char *argv4[] = {"zr_vm_cli", "--emit-aot-c"};
    char *argv5[] = {"zr_vm_cli", "--emit-aot-llvm"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv1, &command, error, sizeof(error)), "--run without compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "run error should mention compile");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv2, &command, error, sizeof(error)),
                    "--intermediate without compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "intermediate error should mention compile");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv3, &command, error, sizeof(error)),
                    "--incremental without compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "incremental error should mention compile");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv4, &command, error, sizeof(error)), "--emit-aot-c without compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "emit aot c error should mention compile");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv5, &command, error, sizeof(error)),
                    "--emit-aot-llvm without compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "emit aot llvm error should mention compile");
    return 0;
}

static int test_aot_runtime_options_require_aot_execution_mode(void) {
    char *argv1[] = {"zr_vm_cli", "demo.zrp", "--require-aot-path"};
    char *argv2[] = {"zr_vm_cli", "demo.zrp", "--execution-mode", "binary", "--require-aot-path"};
    char *argv3[] = {"zr_vm_cli", "demo.zrp", "--execution-mode", "aot_c", "--emit-executed-via"};
    char *argv4[] = {"zr_vm_cli", "--compile", "demo.zrp", "--emit-executed-via"};
    char *argv5[] = {"zr_vm_cli", "--compile", "demo.zrp", "--execution-mode", "aot_c"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv1, &command, error, sizeof(error)),
                    "--require-aot-path without aot execution mode should fail");
    CLI_ASSERT_TRUE(strstr(error, "aot") != ZR_NULL, "require aot path error should mention aot");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv2, &command, error, sizeof(error)),
                    "--require-aot-path with binary execution mode should fail");
    CLI_ASSERT_TRUE(strstr(error, "aot") != ZR_NULL, "binary require aot path error should mention aot");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(5, argv3, &command, error, sizeof(error)),
                    "emit executed_via should be accepted on aot run");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT, command.mode, "mode should stay run project");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_AOT_C, command.executionMode, "run execution mode should be aot_c");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(4, argv4, &command, error, sizeof(error)),
                    "--emit-executed-via without an active run path should fail");
    CLI_ASSERT_TRUE(strstr(error, "active run path") != ZR_NULL,
                    "emit executed_via compile-only error should mention active run path");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv5, &command, error, sizeof(error)),
                    "--execution-mode without compile --run should fail");
    CLI_ASSERT_TRUE(strstr(error, "active run path") != ZR_NULL,
                    "execution mode compile-only error should mention active run path");
    return 0;
}

static int test_debug_run_flags_parse(void) {
    char *argv1[] = {"zr_vm_cli",
                     "demo.zrp",
                     "--debug",
                     "--debug-address",
                     "127.0.0.1:4711",
                     "--debug-wait",
                     "--debug-print-endpoint"};
    char *argv2[] = {"zr_vm_cli", "--compile", "demo.zrp", "--run", "--debug"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(7, argv1, &command, error, sizeof(error)), "parse run debug flags");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT, command.mode, "mode should be run project");
    CLI_ASSERT_TRUE(command.debugEnabled, "debug flag should be set");
    CLI_ASSERT_TRUE(command.debugAddress != ZR_NULL, "debug address should be present");
    CLI_ASSERT_TRUE(strcmp(command.debugAddress, "127.0.0.1:4711") == 0, "debug address should match");
    CLI_ASSERT_TRUE(command.debugWait, "debug wait should be set");
    CLI_ASSERT_TRUE(command.debugPrintEndpoint, "debug print endpoint should be set");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(5, argv2, &command, error, sizeof(error)),
                    "parse compile run debug flags");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_COMPILE_PROJECT, command.mode, "mode should be compile project");
    CLI_ASSERT_TRUE(command.runAfterCompile, "run flag should be set");
    CLI_ASSERT_TRUE(command.debugEnabled, "compile run debug flag should be set");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_BINARY, command.executionMode, "compile run should default to binary");
    return 0;
}

static int test_debug_flags_require_debug_and_active_run_path(void) {
    char *argv1[] = {"zr_vm_cli", "demo.zrp", "--debug-address", "127.0.0.1:9000"};
    char *argv2[] = {"zr_vm_cli", "demo.zrp", "--debug-wait"};
    char *argv3[] = {"zr_vm_cli", "demo.zrp", "--debug-print-endpoint"};
    char *argv4[] = {"zr_vm_cli", "--debug"};
    char *argv5[] = {"zr_vm_cli", "--compile", "demo.zrp", "--debug"};
    char *argv6[] = {"zr_vm_cli", "demo.zrp", "--debug-address"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(4, argv1, &command, error, sizeof(error)),
                    "--debug-address without --debug should fail");
    CLI_ASSERT_TRUE(strstr(error, "--debug") != ZR_NULL, "debug address error should mention debug");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv2, &command, error, sizeof(error)),
                    "--debug-wait without --debug should fail");
    CLI_ASSERT_TRUE(strstr(error, "--debug") != ZR_NULL, "debug wait error should mention debug");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv3, &command, error, sizeof(error)),
                    "--debug-print-endpoint without --debug should fail");
    CLI_ASSERT_TRUE(strstr(error, "--debug") != ZR_NULL, "debug print endpoint error should mention debug");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv4, &command, error, sizeof(error)), "--debug without run path should fail");
    CLI_ASSERT_TRUE(strstr(error, "run path") != ZR_NULL, "debug without run path error should mention run path");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(4, argv5, &command, error, sizeof(error)),
                    "--debug on compile-only path should fail");
    CLI_ASSERT_TRUE(strstr(error, "active run path") != ZR_NULL,
                    "compile-only debug error should mention active run path");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv6, &command, error, sizeof(error)),
                    "--debug-address without value should fail");
    CLI_ASSERT_TRUE(strstr(error, "Missing") != ZR_NULL, "missing debug address should mention missing");
    return 0;
}

static int test_unknown_and_duplicate_modes_fail(void) {
    char *argv1[] = {"zr_vm_cli", "--wat"};
    char *argv2[] = {"zr_vm_cli", "--compile", "a.zrp", "--compile", "b.zrp"};
    char *argv3[] = {"zr_vm_cli", "a.zrp", "b.zrp"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(2, argv1, &command, error, sizeof(error)), "unknown flag should fail");
    CLI_ASSERT_TRUE(strstr(error, "Unknown option") != ZR_NULL, "unknown flag message should mention unknown option");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv2, &command, error, sizeof(error)), "duplicate compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "Duplicate") != ZR_NULL, "duplicate compile message should mention duplicate");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv3, &command, error, sizeof(error)),
                    "multiple positional args should fail");
    CLI_ASSERT_TRUE(strstr(error, "Unexpected positional") != ZR_NULL,
                    "multiple positional args message should mention unexpected positional");
    return 0;
}

int main(void) {
    if (test_no_args_enters_repl() != 0) {
        return 1;
    }
    if (test_positional_project_keeps_run_compatibility() != 0) {
        return 1;
    }
    if (test_compile_flag_combinations_parse() != 0) {
        return 1;
    }
    if (test_aot_compile_flags_parse() != 0) {
        return 1;
    }
    if (test_compile_only_modifiers_require_compile() != 0) {
        return 1;
    }
    if (test_aot_runtime_options_require_aot_execution_mode() != 0) {
        return 1;
    }
    if (test_debug_run_flags_parse() != 0) {
        return 1;
    }
    if (test_debug_flags_require_debug_and_active_run_path() != 0) {
        return 1;
    }
    if (test_unknown_and_duplicate_modes_fail() != 0) {
        return 1;
    }

    printf("cli args tests passed\n");
    return 0;
}
