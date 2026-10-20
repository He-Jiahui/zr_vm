#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "command/command.h"

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static int parse_option(const char *option, const char *text, SZrCliCommand *command, char *error) {
    char *argv[] = {"zr_vm_cli", "test", "demo.zrp", (char *)option, (char *)text};
    return ZrCli_Command_Parse(5, argv, command, error, 256);
}
static void reject_number(const char *option, const char *text) {
    SZrCliCommand command;
    char error[256];
    CHECK(!parse_option(option, text, &command, error));
    CHECK(error[0] != '\0' && strstr(error, option) != NULL);
}
static void timeout_control(void) {
    static const char *texts[] = {"1500ms", "2s", "3m", "18446744073709551615ms"};
    static const TZrUInt64 expected[] = {1500u, 2000u, 180000u, UINT64_MAX};
    SZrCliCommand command;
    char error[256];
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i) {
        CHECK(parse_option("--timeout", texts[i], &command, error));
        CHECK(command.testTimeoutMilliseconds == expected[i]);
        CHECK(command.mode == ZR_CLI_MODE_TEST);
    }
    reject_number("--timeout", "18446744073709552s");
}
static void jobs_control(void) {
    SZrCliCommand command;
    char error[256];
    CHECK(parse_option("--jobs", "1", &command, error));
    CHECK(command.testJobs == 1u);
    CHECK(parse_option("--jobs", "4294967295", &command, error));
    CHECK(command.testJobs == UINT32_MAX);
    reject_number("--jobs", "0");
    reject_number("--jobs", "-1");
}
static void compile_mode(int explicitInterp) {
    char *defaults[] = {"zr_vm_cli", "--compile", "demo.zrp", "--run"};
    char *binary[] = {"zr_vm_cli", "--compile", "demo.zrp", "--run", "--execution-mode", "binary"};
    char *interp[] = {"zr_vm_cli", "--compile", "demo.zrp", "--run", "--execution-mode", "interp"};
    char *interpFirst[] = {"zr_vm_cli", "--execution-mode", "interp", "--compile", "demo.zrp", "--run"};
    char **variants[] = {explicitInterp ? interp : defaults, explicitInterp ? interpFirst : binary};
    SZrCliCommand command;
    char error[256];
    for (size_t i = 0; i < 2u; ++i) {
        CHECK(ZrCli_Command_Parse(explicitInterp || i != 0 ? 6 : 4,
                                  variants[i], &command, error, sizeof(error)));
        CHECK(command.mode == ZR_CLI_MODE_COMPILE_PROJECT && command.runAfterCompile);
        CHECK(command.executionMode == (explicitInterp ? ZR_CLI_EXECUTION_MODE_INTERP
                                                       : ZR_CLI_EXECUTION_MODE_BINARY));
        CHECK(command.projectPath != NULL && strcmp(command.projectPath, "demo.zrp") == 0);
    }
}
static void stale_errno_control(void) {
    SZrCliCommand command;
    char error[256];
    errno = ERANGE;
    CHECK(parse_option("--timeout", "1ms", &command, error));
    CHECK(command.testTimeoutMilliseconds == 1u);
    errno = ERANGE;
    CHECK(parse_option("--jobs", "2", &command, error));
    CHECK(command.testJobs == 2u);
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (strcmp(argv[1], "timeout_overflow") == 0)
        reject_number("--timeout", "18446744073709551616ms");
    else if (strcmp(argv[1], "timeout_huge") == 0)
        reject_number("--timeout", "999999999999999999999999999999999999999999ms");
    else if (strcmp(argv[1], "timeout_control") == 0) timeout_control();
    else if (strcmp(argv[1], "jobs_overflow") == 0) {
        reject_number("--jobs", "4294967296");
        reject_number("--jobs", "999999999999999999999999999999999999999999");
    }
    else if (strcmp(argv[1], "jobs_control") == 0) jobs_control();
    else if (strcmp(argv[1], "compile_interp") == 0) compile_mode(1);
    else if (strcmp(argv[1], "compile_defaults") == 0) compile_mode(0);
    else if (strcmp(argv[1], "stale_errno") == 0) stale_errno_control();
    else return 2;
    return failures == 0 ? 0 : 1;
}
