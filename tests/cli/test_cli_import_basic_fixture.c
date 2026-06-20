#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command/command.h"
#include "compiler/compiler.h"
#include "harness/path_support.h"
#include "runtime/runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static TZrBool copy_file_bytes(const TZrChar *sourcePath, const TZrChar *destinationPath) {
    FILE *sourceFile;
    FILE *destinationFile;
    TZrByte buffer[4096];
    size_t readCount;

    if (sourcePath == ZR_NULL || destinationPath == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(destinationPath)) {
        return ZR_FALSE;
    }

    sourceFile = fopen(sourcePath, "rb");
    if (sourceFile == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationFile = fopen(destinationPath, "wb");
    if (destinationFile == ZR_NULL) {
        fclose(sourceFile);
        return ZR_FALSE;
    }

    while ((readCount = fread(buffer, 1, sizeof(buffer), sourceFile)) > 0) {
        if (fwrite(buffer, 1, readCount, destinationFile) != readCount) {
            fclose(destinationFile);
            fclose(sourceFile);
            return ZR_FALSE;
        }
    }

    fclose(destinationFile);
    fclose(sourceFile);
    return ZR_TRUE;
}

static void clean_directory_tree(const TZrChar *path) {
    TZrChar command[ZR_TESTS_PATH_MAX * 2];
    TZrChar shellPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(path);
    snprintf(shellPath, sizeof(shellPath), "%s", path);
#ifdef _MSC_VER
    for (TZrChar *cursor = shellPath; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    snprintf(command,
             sizeof(command),
             "cmd /c if exist \"%s\" rd /s /q \"%s\"",
             shellPath,
             shellPath);
#else
    snprintf(command, sizeof(command), "rm -rf \"%s\"", shellPath);
#endif
    system(command);
}

static void build_generated_fixture_root(TZrChar *buffer, TZrSize bufferSize) {
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("cli",
                                                       "import_basic_fixture",
                                                       "root",
                                                       "",
                                                       buffer,
                                                       bufferSize));
}

static void prepare_import_basic_fixture(TZrChar *rootPath,
                                         TZrSize rootPathSize,
                                         TZrChar *projectPath,
                                         TZrSize projectPathSize) {
    TZrChar fixtureProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureMainPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureGreetPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationMainPath[ZR_TESTS_PATH_MAX];
    TZrChar destinationGreetPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(rootPath);
    TEST_ASSERT_NOT_NULL(projectPath);

    build_generated_fixture_root(rootPath, rootPathSize);
    clean_directory_tree(rootPath);

    TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile("import_basic",
                                                 "import_basic.zrp",
                                                 fixtureProjectPath,
                                                 sizeof(fixtureProjectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile("import_basic",
                                                 "src/main.zr",
                                                 fixtureMainPath,
                                                 sizeof(fixtureMainPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile("import_basic",
                                                 "src/greet.zr",
                                                 fixtureGreetPath,
                                                 sizeof(fixtureGreetPath)));

    snprintf(destinationProjectPath, sizeof(destinationProjectPath), "%s/import_basic.zrp", rootPath);
    snprintf(destinationMainPath, sizeof(destinationMainPath), "%s/src/main.zr", rootPath);
    snprintf(destinationGreetPath, sizeof(destinationGreetPath), "%s/src/greet.zr", rootPath);

    TEST_ASSERT_TRUE(copy_file_bytes(fixtureProjectPath, destinationProjectPath));
    TEST_ASSERT_TRUE(copy_file_bytes(fixtureMainPath, destinationMainPath));
    TEST_ASSERT_TRUE(copy_file_bytes(fixtureGreetPath, destinationGreetPath));

    snprintf(projectPath, projectPathSize, "%s", destinationProjectPath);
}

static void init_compile_command(SZrCliCommand *command, const TZrChar *projectPath) {
    TEST_ASSERT_NOT_NULL(command);
    TEST_ASSERT_NOT_NULL(projectPath);

    memset(command, 0, sizeof(*command));
    command->mode = ZR_CLI_MODE_COMPILE_PROJECT;
    command->projectPath = projectPath;
    command->emitIntermediate = ZR_TRUE;
    command->executionMode = ZR_CLI_EXECUTION_MODE_BINARY;
}

static void init_binary_run_command(SZrCliCommand *command, const TZrChar *projectPath) {
    TEST_ASSERT_NOT_NULL(command);
    TEST_ASSERT_NOT_NULL(projectPath);

    memset(command, 0, sizeof(*command));
    command->mode = ZR_CLI_MODE_RUN_PROJECT;
    command->projectPath = projectPath;
    command->executionMode = ZR_CLI_EXECUTION_MODE_BINARY;
}

static void assert_capture_returns_expected_string(const SZrCliRunCapture *capture,
                                                   const TZrChar *expectedValue,
                                                   const TZrChar *expectedExecutedVia) {
    SZrString *resultString;

    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_NOT_NULL(capture->global);
    TEST_ASSERT_NOT_NULL(capture->global->mainThreadState);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, capture->result.type);
    resultString = ZR_CAST_STRING(capture->global->mainThreadState, capture->result.value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING(expectedValue, ZrCore_String_GetNativeString(resultString));
    TEST_ASSERT_EQUAL_STRING(expectedExecutedVia, capture->executedVia);
}

static void test_cli_import_basic_callable_export_runs_after_recompile(void) {
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    SZrCliCommand compileCommand;
    SZrCliCommand runCommand;
    SZrCliCompileSummary summary;
    SZrCliRunCapture capture;

    memset(&summary, 0, sizeof(summary));
    memset(&capture, 0, sizeof(capture));

    prepare_import_basic_fixture(rootPath, sizeof(rootPath), projectPath, sizeof(projectPath));

    init_compile_command(&compileCommand, projectPath);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&compileCommand, &summary));
    TEST_ASSERT_EQUAL_UINT32(2u, (unsigned int)summary.compiledCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)summary.skippedCount);
    TEST_ASSERT_EQUAL_UINT32(0u, (unsigned int)summary.removedCount);

    init_binary_run_command(&runCommand, projectPath);
    TEST_ASSERT_TRUE_MESSAGE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture),
                             "compiled import_basic callable export fixture should run");
    assert_capture_returns_expected_string(&capture, "hello from import", "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cli_import_basic_callable_export_runs_after_recompile);

    return UNITY_END();
}
