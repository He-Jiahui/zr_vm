#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "compiler/compiler_aot_exports.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
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
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
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
    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        TZrSize index;

        for (index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static void normalize_path_text(TZrChar *path) {
    if (path == NULL) {
        return;
    }

    for (; *path != '\0'; path++) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
}

static void assert_manifest_export_member_token(TZrMetadataToken token) {
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MEMBER_DEF, ZR_METADATA_TOKEN_TABLE(token));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, ZR_METADATA_TOKEN_RID(token));
}

static void assert_provider_module_imports_with_manifest_exports(SZrGlobalState *runtimeGlobal,
                                                                 SZrState *runtimeState,
                                                                 TZrNativeString expectedModuleKey) {
    SZrString *moduleName;
    SZrObjectModule *module;
    SZrObjectModule *cachedModule;
    SZrMetadataRuntime *metadataRuntime;
    SZrMetadataRuntimeManifestExportView manifestExportView;
    SZrString *seedName;
    const SZrTypeValue *seedValue;

    TEST_ASSERT_NOT_NULL(runtimeGlobal);
    TEST_ASSERT_NOT_NULL(runtimeState);
    TEST_ASSERT_NOT_NULL(expectedModuleKey);

    moduleName = ZrCore_String_CreateFromNative(runtimeState, expectedModuleKey);
    TEST_ASSERT_NOT_NULL(moduleName);
    module = ZrCore_Module_ImportByPath(runtimeState, moduleName);
    TEST_ASSERT_NOT_NULL_MESSAGE(module, ZrLibrary_AotRuntime_GetLastError(runtimeGlobal));
    TEST_ASSERT_EQUAL_STRING(expectedModuleKey, ZrCore_String_GetNativeString(module->moduleName));
    cachedModule = ZrCore_Module_GetFromCache(runtimeState, moduleName);
    TEST_ASSERT_EQUAL_PTR(module, cachedModule);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(runtimeGlobal));

    metadataRuntime = ZrCore_Module_GetMetadataRuntime(module);
    TEST_ASSERT_NOT_NULL(metadataRuntime);
    TEST_ASSERT_EQUAL_UINT32(2u, metadataRuntime->manifestExportCount);
    TEST_ASSERT_NOT_NULL(metadataRuntime->manifestExports);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadManifestExportView(metadataRuntime,
                                                                   ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD,
                                                                   "add",
                                                                   &manifestExportView));
    TEST_ASSERT_EQUAL_STRING("add", manifestExportView.target);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                             manifestExportView.entry->flags);
    assert_manifest_export_member_token(manifestExportView.memberToken);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadManifestExportView(metadataRuntime,
                                                                   ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD,
                                                                   "seed",
                                                                   &manifestExportView));
    TEST_ASSERT_EQUAL_STRING("seed", manifestExportView.target);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                             manifestExportView.entry->flags);
    assert_manifest_export_member_token(manifestExportView.memberToken);

    seedName = ZrCore_String_CreateFromNative(runtimeState, "seed");
    TEST_ASSERT_NOT_NULL(seedName);
    seedValue = ZrCore_Module_GetPubExport(runtimeState, module, seedName);
    TEST_ASSERT_NOT_NULL_MESSAGE(seedValue, expectedModuleKey);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(seedValue->type));
    TEST_ASSERT_EQUAL_INT64(37, seedValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(ZrLibrary_AotRuntime_GetLastError(runtimeGlobal));
}

static void test_aot_c_provider_import_loads_project_library_from_provider_bin(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C provider shared-library smoke currently validates the Unix dlopen toolchain path");
#else
    const char *rootProjectJson =
            "{"
            "\"manifestVersion\":1,"
            "\"assembly\":{\"name\":\"app.render\",\"version\":\"3.4.5\"},"
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\","
            "\"references\":{"
            "\"mathLocal\":{"
            "\"assembly\":\"zr.math\","
            "\"version\":\"2.1.0\","
            "\"path\":\"deps/math/math.zrp\","
            "\"minVersionInclusive\":\"2.0.0\","
            "\"maxVersionExclusive\":\"3.0.0\""
            "},"
            "\"mathRange\":{"
            "\"assembly\":\"zr.math\","
            "\"minVersionInclusive\":\"2.0.0\","
            "\"maxVersionExclusive\":\"3.0.0\","
            "\"candidates\":["
            "{\"path\":\"deps/math_old/math.zrp\"},"
            "{\"path\":\"deps/math/math.zrp\"}"
            "]"
            "}"
            "}"
            "}";
    const char *providerProjectJson =
            "{"
            "\"manifestVersion\":1,"
            "\"assembly\":{\"name\":\"zr.math\",\"version\":\"2.1.0\"},"
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"index\","
            "\"exports\":["
            "{\"kind\":\"method\",\"target\":\"add\"},"
            "{\"kind\":\"field\",\"target\":\"seed\"}"
            "]"
            "}";
    const char *providerOldProjectJson =
            "{"
            "\"manifestVersion\":1,"
            "\"assembly\":{\"name\":\"zr.math\",\"version\":\"2.0.5\"},"
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"index\""
            "}";
    const char *providerSource =
            "pub func add(value: int): int {\n"
            "    return value + 5;\n"
            "}\n"
            "pub var seed: int = 37;\n"
            "return seed;\n";
    SZrState *compileState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *providerProject;
    SZrCliProjectContext providerProjectContext;
    SZrCliAotPreserveRoots providerAotRoots;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrSize generatedCTextLength = 0;
    char *generatedCText;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar rootProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar providerProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar providerOldProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar providerSourcePath[ZR_TESTS_PATH_MAX];
    TZrChar providerZroPath[ZR_TESTS_PATH_MAX];
    TZrChar providerGeneratedCPath[ZR_TESTS_PATH_MAX];
    TZrChar providerSharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar normalizedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    SZrGlobalState *runtimeGlobal;
    SZrState *runtimeState;

    TEST_ASSERT_NOT_NULL(compileState);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project",
                                                       "root",
                                                       ".zrp",
                                                       rootProjectPath,
                                                       sizeof(rootProjectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project/deps/math",
                                                       "math",
                                                       ".zrp",
                                                       providerProjectPath,
                                                       sizeof(providerProjectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project/deps/math_old",
                                                       "math",
                                                       ".zrp",
                                                       providerOldProjectPath,
                                                       sizeof(providerOldProjectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project/deps/math/src/ops",
                                                       "sum",
                                                       ".zr",
                                                       providerSourcePath,
                                                       sizeof(providerSourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project/deps/math/bin/ops",
                                                       "sum",
                                                       ".zro",
                                                       providerZroPath,
                                                       sizeof(providerZroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project/deps/math/bin/aot_c/src/ops",
                                                       "sum",
                                                       ".c",
                                                       providerGeneratedCPath,
                                                       sizeof(providerGeneratedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_provider_shared_library",
                                                       "runtime_project/deps/math/bin/aot_c/lib",
                                                       "zrvm_aot_ops_sum",
                                                       ".so",
                                                       providerSharedLibraryPath,
                                                       sizeof(providerSharedLibraryPath)));

    write_text_file_or_fail(rootProjectPath, rootProjectJson);
    write_text_file_or_fail(providerProjectPath, providerProjectJson);
    write_text_file_or_fail(providerOldProjectPath, providerOldProjectJson);
    write_text_file_or_fail(providerSourcePath, providerSource);

    providerProject = ZrLibrary_Project_New(compileState,
                                            (TZrNativeString)providerProjectJson,
                                            (TZrNativeString)providerProjectPath);
    TEST_ASSERT_NOT_NULL(providerProject);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)providerProject->exportDeclarationCount);

    function = compile_source(compileState, providerSource, "ops/sum.zr");
    TEST_ASSERT_NOT_NULL(function);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "ops/sum";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(compileState, function, providerZroPath, &binaryOptions));
    hash_file_or_fail(providerZroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(providerZroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "ops/sum";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    memset(&providerProjectContext, 0, sizeof(providerProjectContext));
    memset(&providerAotRoots, 0, sizeof(providerAotRoots));
    providerProjectContext.libraryProject = providerProject;
    ZrCli_Compiler_AotExportDeclarations_Init(&providerAotRoots);
    TEST_ASSERT_TRUE(ZrCli_Compiler_ApplyProjectAotExportDeclarations(&providerProjectContext,
                                                                      function,
                                                                      &aotOptions,
                                                                      &providerAotRoots));
    TEST_ASSERT_EQUAL_UINT32(2u, aotOptions.manifestExportDeclarationCount);
    TEST_ASSERT_EQUAL_STRING("add", aotOptions.manifestExportDeclarations[0].target);
    TEST_ASSERT_TRUE(aotOptions.manifestExportDeclarations[0].hasMemberTokenBinding);
    assert_manifest_export_member_token(aotOptions.manifestExportDeclarations[0].memberToken);
    TEST_ASSERT_EQUAL_STRING("seed", aotOptions.manifestExportDeclarations[1].target);
    TEST_ASSERT_TRUE(aotOptions.manifestExportDeclarations[1].hasMemberTokenBinding);
    assert_manifest_export_member_token(aotOptions.manifestExportDeclarations[1].memberToken);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(compileState,
                                                              function,
                                                              providerGeneratedCPath,
                                                              &aotOptions));
    generatedCText = ZrTests_ReadTextFile(providerGeneratedCPath, &generatedCTextLength);
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, generatedCTextLength);
    assert_text_contains(generatedCText, "/* manifest.exports = 2 */");
    assert_text_contains(generatedCText, "/* manifest.export[0] kind=method target=add */");
    assert_text_contains(generatedCText, "/* manifest.export[1] kind=field target=seed */");
    assert_text_contains(generatedCText, "static const SZrAotManifestExportEntry zr_aot_manifest_exports[]");
    assert_text_contains(generatedCText, "{ .kind = 2u, .flags = 2u, .target = \"add\"");
    assert_text_contains(generatedCText, "{ .kind = 3u, .flags = 2u, .target = \"seed\"");
    assert_text_contains(generatedCText, ".manifestExports = zr_aot_manifest_exports,");
    assert_text_contains(generatedCText, ".manifestExportCount = 2u,");
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
             providerGeneratedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             providerSharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    runtimeGlobal = ZrLibrary_CommonState_CommonGlobalState_New(rootProjectPath);
    TEST_ASSERT_NOT_NULL(runtimeGlobal);
    runtimeState = runtimeGlobal->mainThreadState;
    TEST_ASSERT_NOT_NULL(runtimeState);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(runtimeGlobal,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    assert_provider_module_imports_with_manifest_exports(runtimeGlobal,
                                                         runtimeState,
                                                         "$mathLocal@2.1.0/ops/sum");
    assert_provider_module_imports_with_manifest_exports(runtimeGlobal,
                                                         runtimeState,
                                                         "$mathRange@2.1.0/ops/sum");

    snprintf(normalizedLibraryPath, sizeof(normalizedLibraryPath), "%s", providerSharedLibraryPath);
    normalize_path_text(normalizedLibraryPath);
    TEST_ASSERT_NOT_NULL(strstr(normalizedLibraryPath, "/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.so"));

    ZrLibrary_CommonState_CommonGlobalState_Free(runtimeGlobal);
    ZrCli_Compiler_AotExportDeclarations_Free(&providerAotRoots);
    ZrLibrary_Project_Free(compileState, providerProject);
    free(embeddedBlob);
    ZrCore_Function_Free(compileState, function);
    ZrTests_Runtime_State_Destroy(compileState);
#endif
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_aot_c_provider_import_loads_project_library_from_provider_bin);

    return UNITY_END();
}
