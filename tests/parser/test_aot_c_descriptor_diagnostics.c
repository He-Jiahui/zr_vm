#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#ifndef ZR_VM_TESTS_C_COMPILER
#define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

void setUp(void) {}

void tearDown(void) {}

static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));

    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void write_bad_abi_descriptor_library(const TZrChar *descriptorSourcePath,
                                             const TZrChar *sharedLibraryPath) {
    const char *descriptorSource =
            "#include \"zr_vm_common/zr_aot_abi.h\"\n"
            "static TZrInt64 bad_entry(struct SZrState *state) { (void)state; return 0; }\n"
            "static const FZrAotEntryThunk kThunks[] = { bad_entry };\n"
            "static const ZrAotCompiledModule kModule = {\n"
            "    1u,\n"
            "    ZR_AOT_BACKEND_KIND_C,\n"
            "    \"main\",\n"
            "    ZR_AOT_INPUT_KIND_BINARY,\n"
            "    \"\",\n"
            "    0,\n"
            "    0,\n"
            "    0,\n"
            "    kThunks,\n"
            "    1u,\n"
            "    bad_entry\n"
            "};\n"
            "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void) {\n"
            "    return &kModule;\n"
            "}\n";
    char command[4096];

    write_text_file_or_fail(descriptorSourcePath, descriptorSource);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(sharedLibraryPath));

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "\"%s\" "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             descriptorSourcePath,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));
}

static void test_aot_c_descriptor_diagnostic_names_abi_version_mismatch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C descriptor diagnostic test currently validates the Unix dlopen toolchain path");
#else
    const char *source = "return 1;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-descriptor-diagnostics\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrTypeValue result;
    const TZrChar *lastError;
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar descriptorSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project",
                                                       "aot_descriptor_diagnostics",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project/bin/aot_c/src",
                                                       "bad_abi_descriptor",
                                                       ".c",
                                                       descriptorSourcePath,
                                                       sizeof(descriptorSourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    write_bad_abi_descriptor_library(descriptorSourcePath, sharedLibraryPath);

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result));
    lastError = ZrLibrary_AotRuntime_GetLastError(state->global);
    TEST_ASSERT_NOT_NULL(lastError);
    TEST_ASSERT_NOT_NULL(strstr(lastError, "AOT descriptor validation failed for module 'main'"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "abiVersion"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "expected=2"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "actual=1"));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_descriptor_diagnostic_names_abi_version_mismatch);
    return UNITY_END();
}
