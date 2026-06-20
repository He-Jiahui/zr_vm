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
#include "zr_vm_library/file.h"
#include "zr_vm_library/zrm.h"

void setUp(void) {}

void tearDown(void) {}

static TZrBool write_text_file(const TZrChar *destinationPath, const TZrChar *contents) {
    FILE *destinationFile;
    size_t contentLength;

    if (destinationPath == ZR_NULL || contents == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(destinationPath)) {
        return ZR_FALSE;
    }

    destinationFile = fopen(destinationPath, "wb");
    if (destinationFile == ZR_NULL) {
        return ZR_FALSE;
    }

    contentLength = strlen(contents);
    if (contentLength > 0 && fwrite(contents, 1, contentLength, destinationFile) != contentLength) {
        fclose(destinationFile);
        return ZR_FALSE;
    }

    fclose(destinationFile);
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
                                                       "zrm_fixture",
                                                       "root",
                                                       "",
                                                       buffer,
                                                       bufferSize));
}

static void init_compile_command(SZrCliCommand *command, const TZrChar *projectPath, TZrBool emitZrm) {
    TEST_ASSERT_NOT_NULL(command);
    TEST_ASSERT_NOT_NULL(projectPath);

    memset(command, 0, sizeof(*command));
    command->mode = ZR_CLI_MODE_COMPILE_PROJECT;
    command->projectPath = projectPath;
    command->emitIntermediate = ZR_TRUE;
    command->emitZrm = emitZrm;
    command->incremental = ZR_TRUE;
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

static void init_binary_run_module_command(SZrCliCommand *command,
                                           const TZrChar *projectPath,
                                           const TZrChar *moduleName) {
    init_binary_run_command(command, projectPath);
    TEST_ASSERT_NOT_NULL(moduleName);
    command->mode = ZR_CLI_MODE_RUN_PROJECT_MODULE;
    command->moduleName = moduleName;
}

static void assert_capture_returns_expected_int(const SZrCliRunCapture *capture,
                                                TZrInt64 expectedValue,
                                                const TZrChar *expectedExecutedVia) {
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, capture->result.type);
    TEST_ASSERT_EQUAL_INT64(expectedValue, capture->result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_STRING(expectedExecutedVia, capture->executedVia);
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

static void prepare_zrm_reference_runtime_fixture(TZrChar *rootPath,
                                                 TZrSize rootPathSize,
                                                 TZrChar *providerProjectPath,
                                                 TZrSize providerProjectPathSize,
                                                 TZrChar *consumerProjectPath,
                                                 TZrSize consumerProjectPathSize) {
    TZrChar providerSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar consumerSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar consumerResourceProbePath[ZR_TESTS_PATH_MAX];
    TZrChar consumerResourcePath[ZR_TESTS_PATH_MAX];

    static const TZrChar *providerProjectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.fixture.math\", \"version\": \"1.0.0\", \"output\": \"dist/provider.zrm\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"ops\"\n"
            "}\n";
    static const TZrChar *providerModuleContent =
            "pub var answer: int = 42;\n";
    static const TZrChar *consumerProjectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.fixture.consumer\", \"version\": \"1.0.0\", \"output\": \"dist/consumer.zrm\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"math\": { \"assembly\": \"zr.fixture.math\", \"version\": \"1.0.0\", \"path\": \"../provider/dist/provider.zrm\" }\n"
            "  },\n"
            "  \"resources\": {\n"
            "    \"config/runtime.txt\": { \"path\": \"resources/runtime.txt\", \"compress\": true }\n"
            "  }\n"
            "}\n";
    static const TZrChar *consumerModuleContent =
            "var math = %import(\"&math.ops\");\n"
            "\n"
            "return math.answer;\n";
    static const TZrChar *consumerResourceProbeContent =
            "var system = %import(\"zr.system\");\n"
            "\n"
            "return system.assembly.readResourceText(\"config/runtime.txt\");\n";

    build_generated_fixture_root(rootPath, rootPathSize);
    clean_directory_tree(rootPath);

    snprintf(providerProjectPath, providerProjectPathSize, "%s/provider/provider.zrp", rootPath);
    snprintf(providerSourcePath, sizeof(providerSourcePath), "%s/provider/src/ops.zr", rootPath);
    snprintf(consumerProjectPath, consumerProjectPathSize, "%s/consumer/consumer.zrp", rootPath);
    snprintf(consumerSourcePath, sizeof(consumerSourcePath), "%s/consumer/src/main.zr", rootPath);
    snprintf(consumerResourceProbePath, sizeof(consumerResourceProbePath), "%s/consumer/src/resource_probe.zr", rootPath);
    snprintf(consumerResourcePath, sizeof(consumerResourcePath), "%s/consumer/resources/runtime.txt", rootPath);

    TEST_ASSERT_TRUE(write_text_file(providerProjectPath, providerProjectContent));
    TEST_ASSERT_TRUE(write_text_file(providerSourcePath, providerModuleContent));
    TEST_ASSERT_TRUE(write_text_file(consumerProjectPath, consumerProjectContent));
    TEST_ASSERT_TRUE(write_text_file(consumerSourcePath, consumerModuleContent));
    TEST_ASSERT_TRUE(write_text_file(consumerResourceProbePath, consumerResourceProbeContent));
    TEST_ASSERT_TRUE(write_text_file(consumerResourcePath, "consumer-runtime-resource"));
}

static void assert_zrm_has_resource(const TZrChar *zrmPath, const TZrChar *resourceName) {
    SZrLibrary_ZrmArchive archive;
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    const SZrLibrary_ZrmEntryInfo *entry;

    memset(&archive, 0, sizeof(archive));
    memset(error, 0, sizeof(error));

    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_Zrm_Open(zrmPath, &archive, error, sizeof(error)), error);
    entry = ZrLibrary_Zrm_FindResource(&archive, resourceName);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_STRING(resourceName, entry->logicalName);
    ZrLibrary_Zrm_Close(&archive);
}

static void test_cli_runtime_loads_referenced_zrm_module_and_current_assembly_resource(void) {
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar providerProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar consumerProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar providerZrmPath[ZR_TESTS_PATH_MAX];
    TZrChar consumerZrmPath[ZR_TESTS_PATH_MAX];
    SZrCliCommand providerCompileCommand;
    SZrCliCommand consumerCompileCommand;
    SZrCliCommand runCommand;
    SZrCliCommand runResourceCommand;
    SZrCliCompileSummary providerSummary;
    SZrCliCompileSummary consumerSummary;
    SZrCliRunCapture capture;

    memset(&providerSummary, 0, sizeof(providerSummary));
    memset(&consumerSummary, 0, sizeof(consumerSummary));
    memset(&capture, 0, sizeof(capture));

    prepare_zrm_reference_runtime_fixture(rootPath,
                                          sizeof(rootPath),
                                          providerProjectPath,
                                          sizeof(providerProjectPath),
                                          consumerProjectPath,
                                          sizeof(consumerProjectPath));

    init_compile_command(&providerCompileCommand, providerProjectPath, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&providerCompileCommand, &providerSummary));
    TEST_ASSERT_TRUE(providerSummary.packedAssembly);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(providerSummary.zrmPath));

    ZrLibrary_File_PathJoin(rootPath, "provider/dist/provider.zrm", providerZrmPath);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(providerZrmPath));

    init_compile_command(&consumerCompileCommand, consumerProjectPath, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCli_Compiler_CompileProjectWithSummary(&consumerCompileCommand, &consumerSummary));
    TEST_ASSERT_TRUE(consumerSummary.packedAssembly);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(consumerSummary.zrmPath));

    ZrLibrary_File_PathJoin(rootPath, "consumer/dist/consumer.zrm", consumerZrmPath);
    TEST_ASSERT_TRUE(ZrTests_File_Exists(consumerZrmPath));
    assert_zrm_has_resource(consumerZrmPath, "config/runtime.txt");

    init_binary_run_command(&runCommand, consumerProjectPath);
    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runCommand, &capture));
    assert_capture_returns_expected_int(&capture, 42, "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
    memset(&capture, 0, sizeof(capture));

    init_binary_run_module_command(&runResourceCommand, consumerProjectPath, "resource_probe");
    TEST_ASSERT_TRUE(ZrCli_Runtime_RunProjectCapture(&runResourceCommand, &capture));
    assert_capture_returns_expected_string(&capture, "consumer-runtime-resource", "binary");
    ZrCli_Runtime_RunCapture_Free(&capture);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cli_runtime_loads_referenced_zrm_module_and_current_assembly_resource);

    return UNITY_END();
}
