#ifndef ZR_VM_TESTS_AOT_C_TYPED_DIRECT_CALL_ARITHMETIC_SMOKE_SUPPORT_H
#define ZR_VM_TESTS_AOT_C_TYPED_DIRECT_CALL_ARITHMETIC_SMOKE_SUPPORT_H

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_hash_conf.h"
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

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

#ifndef ZR_TESTS_ARRAY_COUNT
#define ZR_TESTS_ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ZR_AOT_ARITHMETIC_SMOKE_MAYBE_UNUSED __attribute__((unused))
#else
#define ZR_AOT_ARITHMETIC_SMOKE_MAYBE_UNUSED
#endif

typedef struct SZrAotTypedDirectCallArithmeticSmokeCase {
    const char *source;
    const char *projectJson;
    const char *projectDirectory;
    const char *artifactName;
    const char *const *requiredGeneratedCNeedles;
    size_t requiredGeneratedCNeedleCount;
    TZrInt64 expectedResult;
} SZrAotTypedDirectCallArithmeticSmokeCase;

#if defined(ZR_PLATFORM_UNIX)
static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}
#endif

void setUp(void) {}

void tearDown(void) {}

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
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize byteLength = 0u;
    char *text;

    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &byteLength));
    text = (char *)malloc((size_t)byteLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    if (byteLength > 0u) {
        memcpy(text, bytes, (size_t)byteLength);
    }
    text[byteLength] = '\0';
    free(bytes);
    return text;
}

static void hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, bufferSize);

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1u, sizeof(chunk), file)) > 0u) {
        TZrSize index;

        for (index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static ZR_AOT_ARITHMETIC_SMOKE_MAYBE_UNUSED void run_i64_arithmetic_smoke_case(
        const SZrAotTypedDirectCallArithmeticSmokeCase *testCase) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)testCase;
    TEST_IGNORE_MESSAGE("AOT C typed direct-call arithmetic smoke currently validates the Unix shared-library path");
#else
    static const char *const forbiddenGeneratedCNeedles[] = {
            "/* zr_aot_static_i64_two_arg_direct_call_sync_stack_slot */",
            "/* zr_aot_static_i64_one_arg_direct_call_sync_stack_slot */",
            "SZrTypeValue *zr_aot_typed_destination",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "ZrLibrary_AotRuntime_CallStaticDirect(state,",
            "ZrLibrary_AotRuntime_CallStackValue(state,",
    };
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
    TZrChar sourceDirectory[ZR_TESTS_PATH_MAX];
    TZrChar binaryDirectory[ZR_TESTS_PATH_MAX];
    TZrChar generatedSourceDirectory[ZR_TESTS_PATH_MAX];
    TZrChar generatedLibraryDirectory[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];
    size_t index;

    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(testCase->source);
    TEST_ASSERT_NOT_NULL(testCase->projectJson);
    TEST_ASSERT_NOT_NULL(testCase->projectDirectory);
    TEST_ASSERT_NOT_NULL(testCase->artifactName);
    TEST_ASSERT_NOT_NULL(testCase->requiredGeneratedCNeedles);

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, testCase->source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_arithmetic",
                                                       testCase->projectDirectory,
                                                       testCase->artifactName,
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    snprintf(sourceDirectory,
             sizeof(sourceDirectory),
             "%s/src",
             testCase->projectDirectory);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_arithmetic",
                                                       sourceDirectory,
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    snprintf(binaryDirectory,
             sizeof(binaryDirectory),
             "%s/bin",
             testCase->projectDirectory);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_arithmetic",
                                                       binaryDirectory,
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    snprintf(generatedSourceDirectory,
             sizeof(generatedSourceDirectory),
             "%s/bin/aot_c/src",
             testCase->projectDirectory);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_arithmetic",
                                                       generatedSourceDirectory,
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    snprintf(generatedLibraryDirectory,
             sizeof(generatedLibraryDirectory),
             "%s/bin/aot_c/lib",
             testCase->projectDirectory);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_direct_call_arithmetic",
                                                       generatedLibraryDirectory,
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, testCase->projectJson);
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
    for (index = 0u; index < testCase->requiredGeneratedCNeedleCount; index++) {
        TEST_ASSERT_NOT_NULL(strstr(generatedCText, testCase->requiredGeneratedCNeedles[index]));
    }
    for (index = 0u; index < ZR_TESTS_ARRAY_COUNT(forbiddenGeneratedCNeedles); index++) {
        TEST_ASSERT_NULL(strstr(generatedCText, forbiddenGeneratedCNeedles[index]));
    }
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -g -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
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

    project = ZrLibrary_Project_New(state, (TZrNativeString)testCase->projectJson, (TZrNativeString)projectPath);
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
