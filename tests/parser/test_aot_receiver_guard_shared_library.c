#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include "harness/path_support.h"
#include "harness/aot_c_link_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"
#endif

#ifndef ZR_VM_TESTS_C_COMPILER
#define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_LLVM_COMPILER
#define ZR_VM_TESTS_LLVM_COMPILER "clang"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

void setUp(void) {}

void tearDown(void) {}

#if defined(ZR_PLATFORM_UNIX)
static const char *receiver_guard_source(void) {
    return "resource class Service {\n"
           "    pub const fn add(value: int): int { return value + 10; }\n"
           "    pub const @call(value: int): int { return value + 20; }\n"
           "    pub const fn explode(): int { throw \"receiver guard suffix failure\"; }\n"
           "}\n"
           "fn failIfEvaluated(): int { throw \"receiver guard evaluated arguments\"; }\n"
           "fn run(): int {\n"
           "    var seed = own Service();\n"
           "    var shared = share(seed);\n"
           "    var weak = degrade(shared);\n"
           "    var live = weak?.add(1);\n"
           "    var liveCallable = weak?.(2);\n"
           "    var observed = 0;\n"
           "    if (live != null) { observed = 10; }\n"
           "    if (liveCallable == 22) { observed = observed + 100; }\n"
           "    var suffixCaught = false;\n"
           "    try { weak.explode(); }\n"
           "    catch (error) { suffixCaught = true; }\n"
           "    drop(shared);\n"
           "    var afterSuffixThrow = wake(weak);\n"
           "    var suffixWakeReleased = afterSuffixThrow == null;\n"
           "    var expired = weak?.add(failIfEvaluated());\n"
           "    var expiredCallable = weak?.(failIfEvaluated());\n"
           "    var caught = 0;\n"
           "    try { weak.add(failIfEvaluated()); }\n"
           "    catch (error: NullReferenceError) { caught = 1; }\n"
           "    var callableCaught = 0;\n"
           "    try { weak(failIfEvaluated()); }\n"
           "    catch (error: NullReferenceError) { callableCaught = 1; }\n"
           "    if (suffixCaught && suffixWakeReleased &&\n"
           "        expired == null && expiredCallable == null) {\n"
           "        return observed + caught + callableCaught + 999;\n"
           "    }\n"
           "    return 0;\n"
           "}\n"
           "return run();\n";
}

static const char *ownership_intrinsics_source(void) {
    return "resource class Box {}\n"
           "fn run(): int {\n"
           "    var owner = own Box();\n"
           "    var shared = share(owner);\n"
           "    var weak = degrade(shared);\n"
           "    var awakened = wake(weak);\n"
           "    var gcOwner = own Box();\n"
           "    var gcBox = intoGc(gcOwner);\n"
           "    var allLive = awakened != null && gcBox != null;\n"
           "    drop(awakened);\n"
           "    drop(shared);\n"
           "    var expired = wake(weak);\n"
           "    if (allLive && expired == null) { return 1; }\n"
           "    return 0;\n"
           "}\n"
           "return run();\n";
}

static const char *intrinsic_named_members_source(void) {
    return "resource class Service {\n"
           "    pub const fn share(): int { return 1; }\n"
           "    pub const fn degrade(): int { return 2; }\n"
           "    pub const fn wake(): int { return 4; }\n"
           "    pub const fn intoGc(): int { return 8; }\n"
           "    pub const fn drop(): int { return 16; }\n"
           "}\n"
           "fn run(): int {\n"
           "    var seed = own Service();\n"
           "    var shared = share(seed);\n"
           "    var weak = degrade(shared);\n"
           "    var mask = 0;\n"
           "    if (weak?.share() == 1) { mask = mask + 1; }\n"
           "    if (weak?.degrade() == 2) { mask = mask + 2; }\n"
           "    if (weak?.wake() == 4) { mask = mask + 4; }\n"
           "    if (weak?.intoGc() == 8) { mask = mask + 8; }\n"
           "    if (weak?.drop() == 16) { mask = mask + 16; }\n"
           "    drop(shared);\n"
           "    if (weak?.share() == null) { mask = mask + 32; }\n"
           "    return mask;\n"
           "}\n"
           "return run();\n";
}

static SZrFunction *compile_source(SZrState *state, const char *source) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    sourceName = ZrCore_String_Create(state,
                                      (TZrNativeString)"main.zr",
                                      strlen("main.zr"));
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

static void hash_file_or_fail(const TZrChar *path,
                              TZrChar *buffer,
                              TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0u) {
        for (TZrSize index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer,
             bufferSize,
             ZR_STABLE_HASH_HEX_PRINTF_FORMAT,
             (unsigned long long)hash);
}

static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}

static void execute_source_backend(const char *source,
                                   TZrInt64 expectedResult,
                                   EZrAotBackendKind backendKind,
                                   EZrLibraryProjectExecutionMode executionMode,
                                   EZrLibraryExecutedVia expectedExecutedVia,
                                   const char *artifactName,
                                   const char *backendDirectory) {
    const char *projectJson =
            "{"
            "\"name\":\"aot-receiver-guard-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
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
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedDirectory[ZR_TESTS_PATH_MAX];
    TZrChar libraryDirectory[ZR_TESTS_PATH_MAX];
    char command[4096];

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    snprintf(generatedDirectory,
             sizeof(generatedDirectory),
             "runtime_project/bin/%s/src",
             backendDirectory);
    snprintf(libraryDirectory,
             sizeof(libraryDirectory),
             "runtime_project/bin/%s/lib",
             backendDirectory);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactName,
                                                       "runtime_project",
                                                       "receiver_guard",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactName,
                                                       "runtime_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactName,
                                                       "runtime_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactName,
                                                       generatedDirectory,
                                                       "main",
                                                       backendKind == ZR_AOT_BACKEND_KIND_C ? ".c" : ".ll",
                                                       generatedPath,
                                                       sizeof(generatedPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactName,
                                                       libraryDirectory,
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state,
                                                               function,
                                                               zroPath,
                                                               &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath,
                                          &embeddedBlob,
                                          &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    if (backendKind == ZR_AOT_BACKEND_KIND_C) {
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state,
                                                                 function,
                                                                 generatedPath,
                                                                 &aotOptions));
        snprintf(command,
                 sizeof(command),
                 "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
                 "-I\"%s/zr_vm_common/include\" "
                 "-I\"%s/zr_vm_core/include\" "
                 "-I\"%s/zr_vm_library/include\" "
                 "\"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
                 ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
                 "-lzr_xx_hash -lzr_utf8proc -lm -o \"%s\"",
                 ZR_VM_TESTS_C_COMPILER,
                 ZR_VM_TESTS_REPO_ROOT,
                 ZR_VM_TESTS_REPO_ROOT,
                 ZR_VM_TESTS_REPO_ROOT,
                 generatedPath,
                 ZR_VM_TESTS_BUILD_LIB_DIR,
                 ZR_VM_TESTS_BUILD_LIB_DIR,
                 sharedLibraryPath);
    } else {
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state,
                                                                    function,
                                                                    generatedPath,
                                                                    &aotOptions));
        snprintf(command,
                 sizeof(command),
                 "\"%s\" -mllvm -opaque-pointers -fPIC -shared \"%s\" "
                 "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
                 ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
                 "-lzr_xx_hash -lzr_utf8proc -lm -o \"%s\"",
                 ZR_VM_TESTS_LLVM_COMPILER,
                 generatedPath,
                 ZR_VM_TESTS_BUILD_LIB_DIR,
                 ZR_VM_TESTS_BUILD_LIB_DIR,
                 sharedLibraryPath);
    }
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    project = ZrLibrary_Project_New(state,
                                    (TZrNativeString)projectJson,
                                    (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          executionMode,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state,
                                                               backendKind,
                                                               &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(expectedResult, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(expectedExecutedVia,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
}
#endif

static void test_aot_c_receiver_guards_execute_optional_and_direct_contracts(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT receiver-guard shared-library smoke validates the Unix toolchain path");
#else
    execute_source_backend(receiver_guard_source(),
                           1111,
                           ZR_AOT_BACKEND_KIND_C,
                           ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                           ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                           "aot_c_receiver_guard_shared_library",
                           "aot_c");
#endif
}

static void test_aot_llvm_receiver_guards_execute_optional_and_direct_contracts(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT receiver-guard shared-library smoke validates the Unix toolchain path");
#else
    execute_source_backend(receiver_guard_source(),
                           1111,
                           ZR_AOT_BACKEND_KIND_LLVM,
                           ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
                           ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM,
                           "aot_llvm_receiver_guard_shared_library",
                           "aot_llvm");
#endif
}

static void test_aot_c_ownership_intrinsics_execute_all_operations(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT ownership-intrinsic shared-library smoke validates the Unix toolchain path");
#else
    execute_source_backend(ownership_intrinsics_source(),
                           1,
                           ZR_AOT_BACKEND_KIND_C,
                           ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                           ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                           "aot_c_ownership_intrinsic_shared_library",
                           "aot_c");
#endif
}

static void test_aot_llvm_ownership_intrinsics_execute_all_operations(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT ownership-intrinsic shared-library smoke validates the Unix toolchain path");
#else
    execute_source_backend(ownership_intrinsics_source(),
                           1,
                           ZR_AOT_BACKEND_KIND_LLVM,
                           ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
                           ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM,
                           "aot_llvm_ownership_intrinsic_shared_library",
                           "aot_llvm");
#endif
}

static void test_aot_c_optional_intrinsic_named_members_use_normal_dispatch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT intrinsic-named member smoke validates the Unix toolchain path");
#else
    execute_source_backend(intrinsic_named_members_source(),
                           63,
                           ZR_AOT_BACKEND_KIND_C,
                           ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                           ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                           "aot_c_intrinsic_named_member_shared_library",
                           "aot_c");
#endif
}

static void test_aot_llvm_optional_intrinsic_named_members_use_normal_dispatch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT intrinsic-named member smoke validates the Unix toolchain path");
#else
    execute_source_backend(intrinsic_named_members_source(),
                           63,
                           ZR_AOT_BACKEND_KIND_LLVM,
                           ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
                           ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM,
                           "aot_llvm_intrinsic_named_member_shared_library",
                           "aot_llvm");
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_receiver_guards_execute_optional_and_direct_contracts);
    RUN_TEST(test_aot_llvm_receiver_guards_execute_optional_and_direct_contracts);
    RUN_TEST(test_aot_c_ownership_intrinsics_execute_all_operations);
    RUN_TEST(test_aot_llvm_ownership_intrinsics_execute_all_operations);
    RUN_TEST(test_aot_c_optional_intrinsic_named_members_use_normal_dispatch);
    RUN_TEST(test_aot_llvm_optional_intrinsic_named_members_use_normal_dispatch);
    return UNITY_END();
}
