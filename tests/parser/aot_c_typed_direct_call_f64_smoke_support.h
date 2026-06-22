#ifndef ZR_VM_TESTS_AOT_C_TYPED_DIRECT_CALL_F64_SMOKE_SUPPORT_H
#define ZR_VM_TESTS_AOT_C_TYPED_DIRECT_CALL_F64_SMOKE_SUPPORT_H

#include "aot_c_typed_direct_call_arithmetic_smoke_support.h"

typedef struct SZrAotTypedDirectCallF64SmokeCase {
    const char *source;
    const char *projectName;
    const char *projectDirectory;
    const char *artifactName;
    const char *forwardDeclNeedle;
    const char *definitionNeedle;
    const char *returnNeedle;
    const char *directCallMarkerNeedle;
    const char *directCallNeedle;
    const char *syncMarkerNeedle;
    TZrInt64 expectedResult;
} SZrAotTypedDirectCallF64SmokeCase;

#if defined(ZR_PLATFORM_UNIX)
static void format_aot_c_typed_direct_call_f64_path(char *buffer,
                                                    size_t bufferSize,
                                                    const char *projectDirectory,
                                                    const char *suffix) {
    int written;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_size_t(0u, bufferSize);
    TEST_ASSERT_NOT_NULL(projectDirectory);
    TEST_ASSERT_NOT_NULL(suffix);

    written = snprintf(buffer, bufferSize, "%s/%s", projectDirectory, suffix);
    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_TRUE((size_t)written < bufferSize);
}
#endif

static void run_aot_c_typed_direct_call_f64_smoke(const SZrAotTypedDirectCallF64SmokeCase *testCase) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)testCase;
    TEST_IGNORE_MESSAGE("AOT C typed direct-call f64 smoke currently validates the Unix shared-library path");
#else
    SZrState *state;
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char sourceDirectory[ZR_TESTS_PATH_MAX];
    char binaryDirectory[ZR_TESTS_PATH_MAX];
    char generatedCDirectory[ZR_TESTS_PATH_MAX];
    char sharedLibraryDirectory[ZR_TESTS_PATH_MAX];
    char projectJson[512];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(testCase->source);
    TEST_ASSERT_NOT_NULL(testCase->projectName);
    TEST_ASSERT_NOT_NULL(testCase->projectDirectory);
    TEST_ASSERT_NOT_NULL(testCase->artifactName);
    TEST_ASSERT_NOT_NULL(testCase->forwardDeclNeedle);
    TEST_ASSERT_NOT_NULL(testCase->definitionNeedle);
    TEST_ASSERT_NOT_NULL(testCase->returnNeedle);
    TEST_ASSERT_NOT_NULL(testCase->directCallMarkerNeedle);
    TEST_ASSERT_NOT_NULL(testCase->directCallNeedle);
    TEST_ASSERT_NOT_NULL(testCase->syncMarkerNeedle);

    format_aot_c_typed_direct_call_f64_path(sourceDirectory,
                                            sizeof(sourceDirectory),
                                            testCase->projectDirectory,
                                            "src");
    format_aot_c_typed_direct_call_f64_path(binaryDirectory,
                                            sizeof(binaryDirectory),
                                            testCase->projectDirectory,
                                            "bin");
    format_aot_c_typed_direct_call_f64_path(generatedCDirectory,
                                            sizeof(generatedCDirectory),
                                            testCase->projectDirectory,
                                            "bin/aot_c/src");
    format_aot_c_typed_direct_call_f64_path(sharedLibraryDirectory,
                                            sizeof(sharedLibraryDirectory),
                                            testCase->projectDirectory,
                                            "bin/aot_c/lib");

    TEST_ASSERT_TRUE(snprintf(projectJson,
                              sizeof(projectJson),
                              "{\"name\":\"%s\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}",
                              testCase->projectName) > 0);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, testCase->source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_f64",
                                                       testCase->projectDirectory,
                                                       testCase->artifactName,
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_f64",
                                                       sourceDirectory,
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_f64",
                                                       binaryDirectory,
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_f64",
                                                       generatedCDirectory,
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_f64",
                                                       sharedLibraryDirectory,
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, testCase->source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, testCase->forwardDeclNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, testCase->definitionNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, testCase->returnNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, testCase->directCallMarkerNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, testCase->directCallNeedle));
    TEST_ASSERT_NULL(strstr(generatedCText, testCase->syncMarkerNeedle));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_typed_destination"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_FAST_SET(zr_aot_typed_destination,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallStaticDirect(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallStackValue(state,"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -g -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(testCase->expectedResult, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

#endif
