#include <stdio.h>
#include <string.h>

#include "command/command.h"

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

#define CLI_ASSERT_SIZE_EQ(expected, actual, message)                                                                  \
    do {                                                                                                               \
        if ((expected) != (actual)) {                                                                                  \
            fprintf(stderr,                                                                                             \
                    "assertion failed: %s (expected=%llu actual=%llu)\n",                                             \
                    message,                                                                                            \
                    (unsigned long long)(expected),                                                                     \
                    (unsigned long long)(actual));                                                                      \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define CLI_ASSERT_STR_EQ(expected, actual, message)                                                                   \
    do {                                                                                                               \
        const char *expectedValue__ = (expected);                                                                      \
        const char *actualValue__ = (actual);                                                                          \
        if (expectedValue__ == ZR_NULL || actualValue__ == ZR_NULL || strcmp(expectedValue__, actualValue__) != 0) {  \
            fprintf(stderr,                                                                                             \
                    "assertion failed: %s (expected=%s actual=%s)\n",                                                 \
                    message,                                                                                            \
                    expectedValue__ != ZR_NULL ? expectedValue__ : "<null>",                                           \
                    actualValue__ != ZR_NULL ? actualValue__ : "<null>");                                              \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

static int cli_assert_parse_failure_contains(int argc,
                                             char **argv,
                                             const char *expectedFragment,
                                             const char *message) {
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(argc, argv, &command, error, sizeof(error)), message);
    CLI_ASSERT_TRUE(strstr(error, expectedFragment) != ZR_NULL, "parse error should contain expected fragment");
    return 0;
}

static int cli_assert_program_args(const SZrCliCommand *command,
                                   TZrSize expectedCount,
                                   const char *const *expectedArgs) {
    TZrSize index;

    CLI_ASSERT_TRUE(command != ZR_NULL, "command should not be null");
    CLI_ASSERT_SIZE_EQ(expectedCount, command->programArgCount, "program arg count should match");
    if (expectedCount == 0) {
        CLI_ASSERT_TRUE(command->programArgs == ZR_NULL, "program args should be null when empty");
        return 0;
    }

    CLI_ASSERT_TRUE(command->programArgs != ZR_NULL, "program args should be present");
    for (index = 0; index < expectedCount; index++) {
        CLI_ASSERT_STR_EQ(expectedArgs[index], command->programArgs[index], "program arg should match");
    }

    return 0;
}

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
    CLI_ASSERT_TRUE(command.inlineCode == ZR_NULL, "positional run should not have inline code");
    CLI_ASSERT_TRUE(command.moduleName == ZR_NULL, "positional run should not have module override");
    CLI_ASSERT_TRUE(!command.interactiveAfterRun, "positional run should not enable interactive follow-up by default");
    return 0;
}

static int test_help_aliases_and_exclusivity(void) {
    char *argv1[] = {"zr_vm_cli", "--help"};
    char *argv2[] = {"zr_vm_cli", "-h"};
    char *argv3[] = {"zr_vm_cli", "-?"};
    char *argv4[] = {"zr_vm_cli", "-h", "demo.zrp"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv1, &command, error, sizeof(error)), "parse --help");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_HELP, command.mode, "mode should be help");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv2, &command, error, sizeof(error)), "parse -h");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_HELP, command.mode, "mode should be help");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv3, &command, error, sizeof(error)), "parse -?");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_HELP, command.mode, "mode should be help");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv4, &command, error, sizeof(error)),
                    "help combined with run target should fail");
    CLI_ASSERT_TRUE(strstr(error, "cannot be combined") != ZR_NULL, "help exclusivity should be enforced");
    return 0;
}

static int test_version_aliases_and_exclusivity(void) {
    char *argv1[] = {"zr_vm_cli", "--version"};
    char *argv2[] = {"zr_vm_cli", "-V"};
    char *argv3[] = {"zr_vm_cli", "--version", "-i"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv1, &command, error, sizeof(error)), "parse --version");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_VERSION, command.mode, "mode should be version");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv2, &command, error, sizeof(error)), "parse -V");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_VERSION, command.mode, "mode should be version");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv3, &command, error, sizeof(error)),
                    "version combined with interactive should fail");
    CLI_ASSERT_TRUE(strstr(error, "cannot be combined") != ZR_NULL, "version exclusivity should be enforced");
    return 0;
}

static int test_inline_code_aliases_parse(void) {
    char *argv1[] = {"zr_vm_cli", "-e", "return 1;"};
    char *argv2[] = {"zr_vm_cli", "-c", "return 2;"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(3, argv1, &command, error, sizeof(error)), "parse -e code");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_INLINE, command.mode, "mode should be inline");
    CLI_ASSERT_STR_EQ("return 1;", command.inlineCode, "inline code should match -e");
    CLI_ASSERT_TRUE(command.projectPath == ZR_NULL, "inline code should not set project path");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(3, argv2, &command, error, sizeof(error)), "parse -c code");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_INLINE, command.mode, "mode should be inline");
    CLI_ASSERT_STR_EQ("return 2;", command.inlineCode, "inline code should match -c");
    return 0;
}

static int test_project_module_mode_parse(void) {
    char *argv[] = {"zr_vm_cli", "--project", "demo.zrp", "-m", "tools.seed", "--execution-mode", "binary"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(7, argv, &command, error, sizeof(error)), "parse project module mode");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT_MODULE, command.mode, "mode should be run project module");
    CLI_ASSERT_STR_EQ("demo.zrp", command.projectPath, "project path should match");
    CLI_ASSERT_STR_EQ("tools.seed", command.moduleName, "module name should match");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_BINARY, command.executionMode, "execution mode should match");
    return 0;
}

static int test_module_mode_requires_project_and_rejects_unsupported_combinations(void) {
    char *argv1[] = {"zr_vm_cli", "-m", "tools.seed"};
    char *argv2[] = {"zr_vm_cli", "--compile", "demo.zrp", "-m", "tools.seed"};
    char *argv3[] = {"zr_vm_cli", "--project", "demo.zrp", "-m", "tools.seed", "--execution-mode", "aot_c"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv1, &command, error, sizeof(error)),
                    "-m without --project should fail");
    CLI_ASSERT_TRUE(strstr(error, "--project") != ZR_NULL, "module mode error should mention --project");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv2, &command, error, sizeof(error)),
                    "-m with --compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "compile+module error should mention compile");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(7, argv3, &command, error, sizeof(error)),
                    "module mode should reject aot execution");
    CLI_ASSERT_TRUE(strstr(error, "aot") != ZR_NULL, "module aot error should mention aot");
    return 0;
}

static int test_interactive_modifier_parse_and_validate(void) {
    char *argv1[] = {"zr_vm_cli", "-i"};
    char *argv2[] = {"zr_vm_cli", "demo.zrp", "-i"};
    char *argv3[] = {"zr_vm_cli", "--compile", "demo.zrp", "-i"};
    char *argv4[] = {"zr_vm_cli", "--help", "-i"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(2, argv1, &command, error, sizeof(error)), "parse repl interactive alias");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_REPL, command.mode, "interactive-only should map to repl");

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(3, argv2, &command, error, sizeof(error)), "parse run + interactive");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT, command.mode, "mode should be run project");
    CLI_ASSERT_TRUE(command.interactiveAfterRun, "interactive follow-up should be enabled");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(4, argv3, &command, error, sizeof(error)),
                    "compile-only interactive should fail");
    CLI_ASSERT_TRUE(strstr(error, "--interactive") != ZR_NULL, "compile-only interactive error should mention interactive");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv4, &command, error, sizeof(error)),
                    "help + interactive should fail");
    CLI_ASSERT_TRUE(strstr(error, "cannot be combined") != ZR_NULL, "interactive help error should mention combine");
    return 0;
}

static int test_missing_required_values_fail(void) {
    char *argv1[] = {"zr_vm_cli", "--compile"};
    char *argv2[] = {"zr_vm_cli", "--project"};
    char *argv3[] = {"zr_vm_cli", "--project", "demo.zrp", "-m"};
    char *argv4[] = {"zr_vm_cli", "-e"};
    char *argv5[] = {"zr_vm_cli", "--debug", "--debug-address"};
    char *argv6[] = {"zr_vm_cli", "demo.zrp", "--execution-mode"};
    char *argv7[] = {"zr_vm_cli", "demo.zrp", "--execution-mode", "wasm"};

    if (cli_assert_parse_failure_contains(2, argv1, "Missing <project.zrp> after --compile", "compile without path should fail") != 0) {
        return 1;
    }
    if (cli_assert_parse_failure_contains(2, argv2, "Missing <project.zrp> after --project", "project without path should fail") != 0) {
        return 1;
    }
    if (cli_assert_parse_failure_contains(4, argv3, "Missing <module> after -m", "module flag without module should fail") != 0) {
        return 1;
    }
    if (cli_assert_parse_failure_contains(2, argv4, "Missing inline code after -e", "inline -e without code should fail") != 0) {
        return 1;
    }
    if (cli_assert_parse_failure_contains(3, argv5, "Missing address after --debug-address", "debug-address without value should fail") != 0) {
        return 1;
    }
    if (cli_assert_parse_failure_contains(3, argv6, "Missing execution mode after --execution-mode", "execution-mode without value should fail") != 0) {
        return 1;
    }
    if (cli_assert_parse_failure_contains(4, argv7, "Unknown execution mode: wasm", "unknown execution mode should fail") != 0) {
        return 1;
    }
    return 0;
}

static int test_passthrough_arguments_stop_cli_parsing(void) {
    char *argv1[] = {"zr_vm_cli", "demo.zrp", "--execution-mode", "binary", "--", "arg1", "--debug", "-x"};
    char *argv2[] = {"zr_vm_cli", "--compile", "demo.zrp", "--run", "--", "seed", "42"};
    const char *expected1[] = {"arg1", "--debug", "-x"};
    const char *expected2[] = {"seed", "42"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(8, argv1, &command, error, sizeof(error)), "parse run with passthrough");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT, command.mode, "mode should be run project");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_BINARY, command.executionMode, "execution mode should stay binary");
    if (cli_assert_program_args(&command, 3, expected1) != 0) {
        return 1;
    }

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(7, argv2, &command, error, sizeof(error)), "parse compile run with passthrough");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_COMPILE_PROJECT, command.mode, "mode should be compile project");
    CLI_ASSERT_TRUE(command.runAfterCompile, "compile run should stay enabled");
    if (cli_assert_program_args(&command, 2, expected2) != 0) {
        return 1;
    }
    return 0;
}

static int test_inline_and_module_passthrough_parse(void) {
    char *argv1[] = {"zr_vm_cli", "-e", "return 1;", "-i", "--", "foo", "bar"};
    char *argv2[] = {"zr_vm_cli", "--project", "demo.zrp", "-m", "tools.seed", "--", "seed"};
    const char *expected1[] = {"foo", "bar"};
    const char *expected2[] = {"seed"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(7, argv1, &command, error, sizeof(error)),
                    "inline with passthrough and interactive should parse");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_INLINE, command.mode, "mode should be inline");
    CLI_ASSERT_STR_EQ("-e", command.inlineModeAlias, "inline alias should be preserved");
    CLI_ASSERT_TRUE(command.interactiveAfterRun, "inline interactive tail should be enabled");
    if (cli_assert_program_args(&command, 2, expected1) != 0) {
        return 1;
    }

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(7, argv2, &command, error, sizeof(error)),
                    "project module with passthrough should parse");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_RUN_PROJECT_MODULE, command.mode, "mode should be project module");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_INTERP, command.executionMode, "module mode should default to interp");
    if (cli_assert_program_args(&command, 1, expected2) != 0) {
        return 1;
    }
    return 0;
}

static int test_passthrough_arguments_require_active_run_path(void) {
    char *argv1[] = {"zr_vm_cli", "--compile", "demo.zrp", "--", "arg1"};
    char *argv2[] = {"zr_vm_cli", "--", "arg1"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv1, &command, error, sizeof(error)),
                    "compile-only passthrough should fail");
    CLI_ASSERT_TRUE(strstr(error, "active run path") != ZR_NULL, "compile-only passthrough error should mention run path");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(3, argv2, &command, error, sizeof(error)),
                    "repl passthrough should fail");
    CLI_ASSERT_TRUE(strstr(error, "run path") != ZR_NULL, "repl passthrough error should mention run path");
    return 0;
}

static int test_eval_rejects_compile_debug_and_module_options(void) {
    char *argv1[] = {"zr_vm_cli", "-e", "return 1;", "--debug"};
    char *argv2[] = {"zr_vm_cli", "-c", "return 1;", "--compile", "demo.zrp"};
    char *argv3[] = {"zr_vm_cli", "-e", "return 1;", "-m", "tools.seed"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(4, argv1, &command, error, sizeof(error)),
                    "eval + debug should fail");
    CLI_ASSERT_TRUE(strstr(error, "-e") != ZR_NULL || strstr(error, "-c") != ZR_NULL,
                    "eval debug error should mention inline mode");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv2, &command, error, sizeof(error)),
                    "eval + compile should fail");
    CLI_ASSERT_TRUE(strstr(error, "--compile") != ZR_NULL, "eval compile error should mention compile");

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv3, &command, error, sizeof(error)),
                    "eval + module should fail");
    CLI_ASSERT_TRUE(strstr(error, "-m") != ZR_NULL, "eval module error should mention -m");
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
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_BINARY, command.executionMode, "compile run should default to binary");
    CLI_ASSERT_TRUE(!command.interactiveAfterRun, "interactive tail should stay disabled by default");
    return 0;
}

static int test_compile_run_interactive_parse(void) {
    char *argv[] = {"zr_vm_cli", "--compile", "demo.zrp", "--run", "-i"};
    char error[256];
    SZrCliCommand command;

    CLI_ASSERT_TRUE(ZrCli_Command_Parse(5, argv, &command, error, sizeof(error)),
                    "compile run interactive should parse");
    CLI_ASSERT_INT_EQ(ZR_CLI_MODE_COMPILE_PROJECT, command.mode, "mode should be compile project");
    CLI_ASSERT_TRUE(command.runAfterCompile, "run should stay enabled");
    CLI_ASSERT_TRUE(command.interactiveAfterRun, "interactive tail should be enabled");
    CLI_ASSERT_INT_EQ(ZR_CLI_EXECUTION_MODE_BINARY, command.executionMode, "compile run interactive should default to binary");
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
    char *argv4[] = {"zr_vm_cli", "-e", "return 1;", "-c", "return 2;"};
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

    CLI_ASSERT_TRUE(!ZrCli_Command_Parse(5, argv4, &command, error, sizeof(error)),
                    "multiple inline modes should fail");
    CLI_ASSERT_TRUE(strstr(error, "Duplicate") != ZR_NULL || strstr(error, "cannot be combined") != ZR_NULL,
                    "duplicate inline mode should mention conflict");
    return 0;
}

int main(void) {
    if (test_no_args_enters_repl() != 0) {
        return 1;
    }
    if (test_positional_project_keeps_run_compatibility() != 0) {
        return 1;
    }
    if (test_help_aliases_and_exclusivity() != 0) {
        return 1;
    }
    if (test_version_aliases_and_exclusivity() != 0) {
        return 1;
    }
    if (test_inline_code_aliases_parse() != 0) {
        return 1;
    }
    if (test_project_module_mode_parse() != 0) {
        return 1;
    }
    if (test_module_mode_requires_project_and_rejects_unsupported_combinations() != 0) {
        return 1;
    }
    if (test_interactive_modifier_parse_and_validate() != 0) {
        return 1;
    }
    if (test_missing_required_values_fail() != 0) {
        return 1;
    }
    if (test_passthrough_arguments_stop_cli_parsing() != 0) {
        return 1;
    }
    if (test_inline_and_module_passthrough_parse() != 0) {
        return 1;
    }
    if (test_passthrough_arguments_require_active_run_path() != 0) {
        return 1;
    }
    if (test_eval_rejects_compile_debug_and_module_options() != 0) {
        return 1;
    }
    if (test_compile_flag_combinations_parse() != 0) {
        return 1;
    }
    if (test_compile_run_interactive_parse() != 0) {
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
