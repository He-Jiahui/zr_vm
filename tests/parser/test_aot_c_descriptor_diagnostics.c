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

static void write_c_byte_array_from_file_or_fail(FILE *sourceFile, const TZrChar *blobPath) {
    FILE *blobFile;
    int byteValue;
    TZrSize index = 0;

    TEST_ASSERT_NOT_NULL(sourceFile);
    TEST_ASSERT_NOT_NULL(blobPath);

    blobFile = fopen(blobPath, "rb");
    TEST_ASSERT_NOT_NULL(blobFile);

    while ((byteValue = fgetc(blobFile)) != EOF) {
        if (index > 0) {
            TEST_ASSERT_GREATER_THAN_INT(0, fprintf(sourceFile, ","));
        }
        if ((index % 12u) == 0u) {
            TEST_ASSERT_GREATER_THAN_INT(0, fprintf(sourceFile, "\n    "));
        } else {
            TEST_ASSERT_GREATER_THAN_INT(0, fprintf(sourceFile, " "));
        }
        TEST_ASSERT_GREATER_THAN_INT(0, fprintf(sourceFile, "0x%02x", (unsigned)(byteValue & 0xff)));
        index++;
    }

    TEST_ASSERT_TRUE(feof(blobFile));
    TEST_ASSERT_EQUAL_INT(0, fclose(blobFile));
    TEST_ASSERT_GREATER_THAN_UINT64(0u, index);
    TEST_ASSERT_GREATER_THAN_INT(0, fprintf(sourceFile, "\n"));
}

static void write_c_text_or_fail(FILE *sourceFile, const char *text) {
    TEST_ASSERT_NOT_NULL(sourceFile);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), sourceFile));
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

static void write_missing_code_registration_descriptor_library(const TZrChar *descriptorSourcePath,
                                                               const TZrChar *sharedLibraryPath) {
    const char *descriptorSource =
            "#include \"zr_vm_common/zr_aot_abi.h\"\n"
            "static TZrInt64 bad_entry(struct SZrState *state) { (void)state; return 0; }\n"
            "static const TZrByte kBlob[] = { 0x7a, 0x72, 0x6f };\n"
            "static const FZrAotEntryThunk kThunks[] = { bad_entry };\n"
            "static const ZrAotCompiledModule kModule = {\n"
            "    .abiVersion = ZR_VM_AOT_ABI_VERSION,\n"
            "    .backendKind = ZR_AOT_BACKEND_KIND_C,\n"
            "    .moduleName = \"main\",\n"
            "    .inputKind = ZR_AOT_INPUT_KIND_BINARY,\n"
            "    .inputHash = \"\",\n"
            "    .runtimeContracts = ZR_NULL,\n"
            "    .embeddedModuleBlob = kBlob,\n"
            "    .embeddedModuleBlobLength = sizeof(kBlob),\n"
            "    .functionThunks = kThunks,\n"
            "    .functionThunkCount = 1u,\n"
            "    .entryThunk = bad_entry,\n"
            "    .methodInfos = ZR_NULL,\n"
            "    .methodInfoCount = 0u,\n"
            "    .gcDescriptors = ZR_NULL,\n"
            "    .gcDescriptorCount = 0u,\n"
            "    .codeRegistration = ZR_NULL,\n"
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

static void write_member_token_remap_descriptor_library(const TZrChar *descriptorSourcePath,
                                                        const TZrChar *sharedLibraryPath,
                                                        const TZrChar *embeddedBlobPath,
                                                        const char *remapEntriesSource,
                                                        const char *remapCountText) {
    const char *descriptorSourcePrefix =
            "#include \"zr_vm_common/zr_aot_abi.h\"\n"
            "static TZrInt64 bad_entry(struct SZrState *state) { (void)state; return 0; }\n"
            "static void bad_invoker(struct SZrState *state,\n"
            "                        FZrAotEntryThunk target,\n"
            "                        const struct SZrAotMethodInfo *method,\n"
            "                        struct SZrTypeValue *self,\n"
            "                        struct SZrTypeValue *args,\n"
            "                        struct SZrTypeValue *outReturn) {\n"
            "    (void)state; (void)target; (void)method; (void)self; (void)args; (void)outReturn;\n"
            "}\n"
            "static const TZrByte kBlob[] = {";
    const char *descriptorSourceMiddle =
            "};\n"
            "static const FZrAotEntryThunk kThunks[] = { bad_entry };\n"
            "static const FZrAotReflectionInvoker kInvokers[] = { bad_invoker };\n"
            "static const SZrAotMemberTokenRemap kRemaps[] = {\n";
    const char *descriptorSourceRegistrationSuffix =
            "};\n"
            "static const SZrAotCodeRegistration kCodeRegistration = {\n"
            "    .functionCount = 1u,\n"
            "    .functionPointers = kThunks,\n"
            "    .methodInfos = ZR_NULL,\n"
            "    .methodInfoCount = 0u,\n"
            "    .methodTokens = ZR_NULL,\n"
            "    .methodTokenCount = 0u,\n"
            "    .memberTokenRemaps = kRemaps,\n"
            "    .memberTokenRemapCount = ";
    const char *descriptorSourceModulePrefix =
            ",\n"
            "    .invokers = kInvokers,\n"
            "    .invokerCount = 1u,\n"
            "    .typeLayouts = ZR_NULL,\n"
            "    .typeLayoutCount = 0u,\n"
            "    .typeLayoutTokens = ZR_NULL,\n"
            "    .typeLayoutTokenCount = 0u,\n"
            "    .gcDescriptors = ZR_NULL,\n"
            "    .gcDescriptorCount = 0u,\n"
            "};\n"
            "static const ZrAotCompiledModule kModule = {\n"
            "    .abiVersion = ZR_VM_AOT_ABI_VERSION,\n"
            "    .backendKind = ZR_AOT_BACKEND_KIND_C,\n"
            "    .moduleName = \"main\",\n"
            "    .inputKind = ZR_AOT_INPUT_KIND_BINARY,\n"
            "    .inputHash = \"\",\n"
            "    .runtimeContracts = ZR_NULL,\n"
            "    .embeddedModuleBlob = kBlob,\n"
            "    .embeddedModuleBlobLength = sizeof(kBlob),\n"
            "    .functionThunks = kThunks,\n"
            "    .functionThunkCount = 1u,\n"
            "    .entryThunk = bad_entry,\n"
            "    .methodInfos = ZR_NULL,\n"
            "    .methodInfoCount = 0u,\n"
            "    .methodTokens = ZR_NULL,\n"
            "    .methodTokenCount = 0u,\n"
            "    .memberTokenRemaps = kRemaps,\n"
            "    .memberTokenRemapCount = ";
    const char *descriptorSourceSuffix =
            ",\n"
            "    .typeLayouts = ZR_NULL,\n"
            "    .typeLayoutCount = 0u,\n"
            "    .typeLayoutTokens = ZR_NULL,\n"
            "    .typeLayoutTokenCount = 0u,\n"
            "    .gcDescriptors = ZR_NULL,\n"
            "    .gcDescriptorCount = 0u,\n"
            "    .codeRegistration = &kCodeRegistration,\n"
            "};\n"
            "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void) {\n"
            "    return &kModule;\n"
            "}\n";
    char command[4096];
    FILE *sourceFile;

    TEST_ASSERT_NOT_NULL(descriptorSourcePath);
    TEST_ASSERT_NOT_NULL(sharedLibraryPath);
    TEST_ASSERT_NOT_NULL(embeddedBlobPath);
    TEST_ASSERT_NOT_NULL(remapEntriesSource);
    TEST_ASSERT_NOT_NULL(remapCountText);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(descriptorSourcePath));
    sourceFile = fopen(descriptorSourcePath, "wb");
    TEST_ASSERT_NOT_NULL(sourceFile);
    write_c_text_or_fail(sourceFile, descriptorSourcePrefix);
    write_c_byte_array_from_file_or_fail(sourceFile, embeddedBlobPath);
    write_c_text_or_fail(sourceFile, descriptorSourceMiddle);
    write_c_text_or_fail(sourceFile, remapEntriesSource);
    write_c_text_or_fail(sourceFile, descriptorSourceRegistrationSuffix);
    write_c_text_or_fail(sourceFile, remapCountText);
    write_c_text_or_fail(sourceFile, descriptorSourceModulePrefix);
    write_c_text_or_fail(sourceFile, remapCountText);
    write_c_text_or_fail(sourceFile, descriptorSourceSuffix);
    TEST_ASSERT_EQUAL_INT(0, fclose(sourceFile));
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

static void assert_member_token_remap_descriptor_rejected(const char *artifactDirectory,
                                                          const char *projectFileBase,
                                                          const char *projectName,
                                                          const char *descriptorFileBase,
                                                          const char *remapEntriesSource,
                                                          const char *remapCountText,
                                                          const char *expectedDiagnostic,
                                                          const char *expectedDetail0,
                                                          const char *expectedDetail1,
                                                          const char *expectedDetail2) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)artifactDirectory;
    (void)projectFileBase;
    (void)projectName;
    (void)descriptorFileBase;
    (void)remapEntriesSource;
    (void)remapCountText;
    (void)expectedDiagnostic;
    (void)expectedDetail0;
    (void)expectedDetail1;
    (void)expectedDetail2;
    TEST_IGNORE_MESSAGE("AOT C descriptor diagnostic test currently validates the Unix dlopen toolchain path");
#else
    const char *source = "return 1;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrTypeValue result;
    const TZrChar *lastError;
    TZrChar projectJson[256];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceArtifactDirectory[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar binaryArtifactDirectory[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar descriptorArtifactDirectory[ZR_TESTS_PATH_MAX];
    TZrChar descriptorSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar libraryArtifactDirectory[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    int written;

    TEST_ASSERT_NOT_NULL(artifactDirectory);
    TEST_ASSERT_NOT_NULL(projectFileBase);
    TEST_ASSERT_NOT_NULL(projectName);
    TEST_ASSERT_NOT_NULL(descriptorFileBase);
    TEST_ASSERT_NOT_NULL(remapEntriesSource);
    TEST_ASSERT_NOT_NULL(remapCountText);
    TEST_ASSERT_NOT_NULL(expectedDiagnostic);
    TEST_ASSERT_NOT_NULL(state);

    written = snprintf(projectJson,
                       sizeof(projectJson),
                       "{\"name\":\"%s\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}",
                       projectName);
    TEST_ASSERT_TRUE(written > 0 && written < (int)sizeof(projectJson));
    written = snprintf(sourceArtifactDirectory, sizeof(sourceArtifactDirectory), "%s/src", artifactDirectory);
    TEST_ASSERT_TRUE(written > 0 && written < (int)sizeof(sourceArtifactDirectory));
    written = snprintf(binaryArtifactDirectory, sizeof(binaryArtifactDirectory), "%s/bin", artifactDirectory);
    TEST_ASSERT_TRUE(written > 0 && written < (int)sizeof(binaryArtifactDirectory));
    written = snprintf(descriptorArtifactDirectory,
                       sizeof(descriptorArtifactDirectory),
                       "%s/bin/aot_c/src",
                       artifactDirectory);
    TEST_ASSERT_TRUE(written > 0 && written < (int)sizeof(descriptorArtifactDirectory));
    written = snprintf(libraryArtifactDirectory,
                       sizeof(libraryArtifactDirectory),
                       "%s/bin/aot_c/lib",
                       artifactDirectory);
    TEST_ASSERT_TRUE(written > 0 && written < (int)sizeof(libraryArtifactDirectory));

    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       artifactDirectory,
                                                       projectFileBase,
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       sourceArtifactDirectory,
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       binaryArtifactDirectory,
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       descriptorArtifactDirectory,
                                                       descriptorFileBase,
                                                       ".c",
                                                       descriptorSourcePath,
                                                       sizeof(descriptorSourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       libraryArtifactDirectory,
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    write_member_token_remap_descriptor_library(descriptorSourcePath,
                                                sharedLibraryPath,
                                                zroPath,
                                                remapEntriesSource,
                                                remapCountText);

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
    TEST_ASSERT_NOT_NULL(strstr(lastError, expectedDiagnostic));
    if (expectedDetail0 != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(strstr(lastError, expectedDetail0));
    }
    if (expectedDetail1 != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(strstr(lastError, expectedDetail1));
    }
    if (expectedDetail2 != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(strstr(lastError, expectedDetail2));
    }

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
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
    TZrChar expectedAbiText[32];

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
    snprintf(expectedAbiText, sizeof(expectedAbiText), "expected=%u", (unsigned)ZR_VM_AOT_ABI_VERSION);
    TEST_ASSERT_NOT_NULL(strstr(lastError, "AOT descriptor validation failed for module 'main'"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "abiVersion"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, expectedAbiText));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "actual=1"));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_descriptor_diagnostic_rejects_missing_code_registration(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C descriptor diagnostic test currently validates the Unix dlopen toolchain path");
#else
    const char *source = "return 1;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-descriptor-code-registration\","
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
                                                       "runtime_project_missing_code_registration",
                                                       "aot_descriptor_code_registration",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project_missing_code_registration/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project_missing_code_registration/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project_missing_code_registration/bin/aot_c/src",
                                                       "missing_code_registration_descriptor",
                                                       ".c",
                                                       descriptorSourcePath,
                                                       sizeof(descriptorSourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_descriptor_diagnostics",
                                                       "runtime_project_missing_code_registration/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    write_missing_code_registration_descriptor_library(descriptorSourcePath, sharedLibraryPath);

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
    TEST_ASSERT_NOT_NULL(strstr(lastError, "codeRegistration"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "null"));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_descriptor_diagnostic_rejects_invalid_member_token_remap_entry(void) {
    assert_member_token_remap_descriptor_rejected(
            "runtime_project_bad_member_token_remap",
            "aot_descriptor_member_token_remap",
            "aot-descriptor-member-token-remap",
            "invalid_member_token_remap_descriptor",
            "    { .sourceToken = 0x02000001u, .targetToken = 0x03000001u }\n",
            "1u",
            "member token remap entry invalid",
            "index=0",
            "sourceToken=0x02000001",
            "targetToken=0x03000001");
}

static void test_aot_c_descriptor_diagnostic_rejects_duplicate_member_token_remap_source(void) {
    assert_member_token_remap_descriptor_rejected(
            "runtime_project_duplicate_member_token_remap_source",
            "aot_descriptor_member_token_remap_duplicate_source",
            "aot-descriptor-member-token-remap-duplicate-source",
            "duplicate_member_token_remap_source_descriptor",
            "    { .sourceToken = 0x03000001u, .targetToken = 0x03000001u },\n"
            "    { .sourceToken = 0x03000001u, .targetToken = 0x03000002u }\n",
            "2u",
            "member token remap duplicate sourceToken",
            "index=1",
            "previousIndex=0",
            "sourceToken=0x03000001");
}

static void test_aot_c_descriptor_diagnostic_rejects_duplicate_member_token_remap_target(void) {
    assert_member_token_remap_descriptor_rejected(
            "runtime_project_duplicate_member_token_remap_target",
            "aot_descriptor_member_token_remap_duplicate_target",
            "aot-descriptor-member-token-remap-duplicate-target",
            "duplicate_member_token_remap_target_descriptor",
            "    { .sourceToken = 0x03000001u, .targetToken = 0x03000001u },\n"
            "    { .sourceToken = 0x03000002u, .targetToken = 0x03000001u }\n",
            "2u",
            "member token remap duplicate targetToken",
            "index=1",
            "previousIndex=0",
            "targetToken=0x03000001");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_descriptor_diagnostic_names_abi_version_mismatch);
    RUN_TEST(test_aot_c_descriptor_diagnostic_rejects_missing_code_registration);
    RUN_TEST(test_aot_c_descriptor_diagnostic_rejects_invalid_member_token_remap_entry);
    RUN_TEST(test_aot_c_descriptor_diagnostic_rejects_duplicate_member_token_remap_source);
    RUN_TEST(test_aot_c_descriptor_diagnostic_rejects_duplicate_member_token_remap_target);
    return UNITY_END();
}
