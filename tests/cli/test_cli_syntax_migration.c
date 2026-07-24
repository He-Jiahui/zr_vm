#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "path_support.h"

#ifndef ZR_VM_CLI_EXECUTABLE_PATH
#error "ZR_VM_CLI_EXECUTABLE_PATH must identify the CLI executable"
#endif

#define CLI_MIGRATION_PATH_CAPACITY 2048U
#define CLI_MIGRATION_COMMAND_CAPACITY 6144U

static int cli_migration_fail(const char *message) {
    fprintf(stderr, "cli migration test failure: %s\n", message);
    return 1;
}

int main(void) {
    TZrChar fixturePath[ZR_TESTS_PATH_MAX];
    TZrChar reportPath[CLI_MIGRATION_PATH_CAPACITY];
    TZrChar command[CLI_MIGRATION_COMMAND_CAPACITY];
    TZrChar *before;
    TZrChar *after;
    TZrChar *report;
    TZrSize beforeLength = 0U;
    TZrSize afterLength = 0U;
    TZrSize reportLength = 0U;
    int commandLength;
    int systemResult;

    if (!ZrTests_Path_GetFixture(
                "syntax_migration_frontend/input",
                "machine_forms.zr",
                fixturePath,
                sizeof(fixturePath))) {
        return cli_migration_fail("fixture path should resolve");
    }
    commandLength = snprintf(
            reportPath,
            sizeof(reportPath),
            "%s/cli_syntax_migration_report.json",
            ZR_VM_TESTS_BINARY_DIR);
    if (commandLength < 0 || (TZrSize)commandLength >= sizeof(reportPath)) {
        return cli_migration_fail("report path should fit");
    }
    before = ZrTests_ReadTextFile(fixturePath, &beforeLength);
    if (before == ZR_NULL) {
        return cli_migration_fail("fixture should be readable before check");
    }
#ifdef _MSC_VER
    commandLength = snprintf(
            command,
            sizeof(command),
            "cmd.exe /d /s /c \"\"%s\" migrate syntax \"%s\" --check --format json > \"%s\"\"",
            ZR_VM_CLI_EXECUTABLE_PATH,
            fixturePath,
            reportPath);
#else
    commandLength = snprintf(
            command,
            sizeof(command),
            "\"%s\" migrate syntax \"%s\" --check --format json > \"%s\"",
            ZR_VM_CLI_EXECUTABLE_PATH,
            fixturePath,
            reportPath);
#endif
    if (commandLength < 0 || (TZrSize)commandLength >= sizeof(command)) {
        free(before);
        return cli_migration_fail("migration command should fit");
    }
    systemResult = system(command);
    if (systemResult != 0) {
        free(before);
        remove(reportPath);
        return cli_migration_fail("migration check command should succeed");
    }
    report = ZrTests_ReadTextFile(reportPath, &reportLength);
    after = ZrTests_ReadTextFile(fixturePath, &afterLength);
    remove(reportPath);
    if (report == ZR_NULL || after == ZR_NULL) {
        free(before);
        free(report);
        free(after);
        return cli_migration_fail("report and fixture should be readable after check");
    }
    if (strstr(report, "\"schemaVersion\":1") == ZR_NULL ||
        strstr(report, "\"oldConstructKind\":\"percentModule\"") == ZR_NULL ||
        strstr(report, "\"applicability\":\"targetNotPromoted\"") == ZR_NULL ||
        strstr(report, "\"write\":false") == ZR_NULL ||
        beforeLength != afterLength || memcmp(before, after, beforeLength) != 0) {
        free(before);
        free(report);
        free(after);
        return cli_migration_fail("check report should be structured and leave source unchanged");
    }
    free(before);
    free(report);
    free(after);
    printf("cli syntax migration test passed\n");
    return 0;
}
