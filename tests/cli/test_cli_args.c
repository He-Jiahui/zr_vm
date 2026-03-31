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

static int test_compile_only_modifiers_require_compile(void) {
    char *argv1[] = {"zr_vm_cli", "--run"};
    char *argv2[] = {"zr_vm_cli", "--intermediate"};
    char *argv3[] = {"zr_vm_cli", "--incremental"};
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
    if (test_compile_only_modifiers_require_compile() != 0) {
        return 1;
    }
    if (test_unknown_and_duplicate_modes_fail() != 0) {
        return 1;
    }

    printf("cli args tests passed\n");
    return 0;
}
