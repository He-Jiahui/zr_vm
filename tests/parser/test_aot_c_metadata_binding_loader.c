#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/aot_c_link_support.h"
#include "harness/runtime_support.h"
#include "harness/module_fixture_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
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

#define TEST_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u)
#define TEST_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_RESOLVED_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u)
#define TEST_RESOLVED_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 8u)
#define TEST_REF_SIGNATURE_HASH ((TZrUInt64)0x0102030405060708ULL)
#define TEST_EXPECTED_SIGNATURE_HASH ((TZrUInt64)0x1112131415161718ULL)
#define TEST_ACTUAL_SIGNATURE_HASH ((TZrUInt64)0x2122232425262728ULL)
#define TEST_MODULE_SIGNATURE_HASH ((TZrUInt64)0x3132333435363738ULL)
#define TEST_LAYOUT_VERSION 3u
#define TEST_LAYOUT_HASH ((TZrUInt64)0x4142434445464748ULL)

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
        for (TZrSize index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static void inject_signature_hash_drift(SZrState *state, SZrFunction *function) {
    SZrMetadataTokenBinding *binding;

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state, function, TEST_REF_TOKEN);
    TEST_ASSERT_NOT_NULL(binding);
    binding->refToken = TEST_REF_TOKEN;
    binding->refSignatureToken = TEST_REF_SIGNATURE_TOKEN;
    binding->refSignatureHash = TEST_REF_SIGNATURE_HASH;
    binding->expectedMetadataToken = TEST_RESOLVED_TOKEN;
    binding->expectedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;
    binding->expectedSignatureHash = TEST_EXPECTED_SIGNATURE_HASH;
    binding->expectedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding->expectedLayoutVersion = TEST_LAYOUT_VERSION;
    binding->expectedLayoutHash = TEST_LAYOUT_HASH;
    binding->resolvedMetadataToken = TEST_RESOLVED_TOKEN;
    binding->resolvedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;
    binding->resolvedSignatureHash = TEST_ACTUAL_SIGNATURE_HASH;
    binding->resolvedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding->resolvedLayoutVersion = TEST_LAYOUT_VERSION;
    binding->resolvedLayoutHash = TEST_LAYOUT_HASH;
}

static void test_aot_runtime_rejects_embedded_module_with_incompatible_metadata_binding(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT metadata binding loader test currently validates the Unix shared-library loader path");
#else
    const char *source = "return 42;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-metadata-binding-loader\","
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
    const TZrChar *lastError;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);
    inject_signature_hash_drift(state, function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_metadata_binding_loader",
                                                       "runtime_project",
                                                       "aot_metadata_binding_loader",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_metadata_binding_loader",
                                                       "runtime_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_metadata_binding_loader",
                                                       "runtime_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_metadata_binding_loader",
                                                       "runtime_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_metadata_binding_loader",
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

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
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
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result));
    lastError = ZrLibrary_AotRuntime_GetLastError(state->global);
    TEST_ASSERT_NOT_NULL(lastError);
    TEST_ASSERT_NOT_NULL(strstr(lastError, "AOT metadata binding compatibility failed"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "module 'main'"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "SIGNATURE_HASH_MISMATCH"));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_NONE, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void assert_generated_call_binding_rows_backend(const char *source, const char *artifactSuite,
        EZrAotBackendKind backend,
        TZrUInt32 expectedGetters, TZrUInt32 expectedSetters, TZrUInt32 expectedDeferred) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)source;
    (void)artifactSuite;
    (void)backend;
    (void)expectedGetters;
    (void)expectedSetters;
    (void)expectedDeferred;
    TEST_IGNORE_MESSAGE("AOT generated call-binding loader test currently validates the Unix shared-library loader path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-call-binding-loader\","
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
    SZrString *moduleName;
    SZrObjectModule *module;
    SZrMetadataRuntime *metadataRuntime;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    TZrUInt32 getters = 0u;
    TZrUInt32 setters = 0u;
    TZrUInt32 deferred = 0u;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactSuite,
                                                       "runtime_project",
                                                       "aot_call_binding_loader",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactSuite,
                                                       "runtime_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactSuite,
                                                       "runtime_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactSuite,
                                                       backend == ZR_AOT_BACKEND_KIND_C ? "runtime_project/bin/aot_c/src" : "runtime_project/bin/aot_llvm/src",
                                                       "main",
                                                       backend == ZR_AOT_BACKEND_KIND_C ? ".c" : ".ll",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(artifactSuite,
                                                       backend == ZR_AOT_BACKEND_KIND_C ? "runtime_project/bin/aot_c/lib" : "runtime_project/bin/aot_llvm/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

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
    TEST_ASSERT_TRUE(backend == ZR_AOT_BACKEND_KIND_C
            ? ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions)
            : ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, generatedCPath, &aotOptions));

    snprintf(command,
             sizeof(command),
             "\"%s\" %s -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-o \"%s\"",
             backend == ZR_AOT_BACKEND_KIND_C ? ZR_VM_TESTS_C_COMPILER : "clang",
             backend == ZR_AOT_BACKEND_KIND_C ? "-std=c11" : "-mllvm -opaque-pointers",
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    function = ZR_NULL;
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          backend == ZR_AOT_BACKEND_KIND_C ? ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C : ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    {
        TZrBool executed = ZrLibrary_AotRuntime_ExecuteEntry(state, backend, &result);
        if (!executed) fprintf(stderr, "call-binding AOT failure (%s): %s\n", artifactSuite,
                ZrLibrary_AotRuntime_GetLastError(state->global));
        TEST_ASSERT_TRUE_MESSAGE(executed, ZrLibrary_AotRuntime_GetLastError(state->global));
    }
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(backend == ZR_AOT_BACKEND_KIND_C ? ZR_LIBRARY_EXECUTED_VIA_AOT_C : ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    moduleName = ZrCore_String_CreateFromNative(state, "main");
    TEST_ASSERT_NOT_NULL(moduleName);
    module = ZrLibrary_AotRuntime_ModuleLoader(state, moduleName, project->aotRuntime);
    TEST_ASSERT_NOT_NULL(module);
    metadataRuntime = ZrCore_Module_GetMetadataRuntime(module);
    TEST_ASSERT_NOT_NULL(metadataRuntime);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, metadataRuntime->codeRegistration->callBindingRowCount);
    for (TZrUInt32 rowIndex = 0u;
         rowIndex < metadataRuntime->codeRegistration->callBindingRowCount;
         ++rowIndex) {
        SZrMetadataRuntimeCallBindingView view;
        SZrFunction *caller;
        SZrFunctionCallSiteCacheEntry *entry;
        TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadCallBindingView(metadataRuntime, rowIndex, &view));
        caller = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                state, metadataRuntime->metadataFunction, view.functionIndex);
        TEST_ASSERT_NOT_NULL(caller);
        TEST_ASSERT_LESS_THAN_UINT32(caller->callSiteCacheLength, view.cacheIndex);
        entry = &caller->callSiteCaches[view.cacheIndex];
        TEST_ASSERT_EQUAL_UINT64(caller->callBindingGeneration, entry->binding.generation);
        if (view.location.kind == ZR_CALL_BINDING_RELOCATION_MODULE) {
            TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, view.targetFunctionIndex);
            if (entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_NATIVE) {
                TEST_ASSERT_NOT_NULL(entry->binding.target.native.function);
                TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
                if (view.contract.operation == ZR_CALL_BINDING_OPERATION_GET) ++getters;
                if (view.contract.operation == ZR_CALL_BINDING_OPERATION_SET) ++setters;
                continue;
            }
            TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_VM, entry->binding.target.targetKind);
            TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
            TEST_ASSERT_NULL(view.functionPointer);
            ++deferred;
            continue;
        }
        TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_AOT, entry->binding.target.targetKind);
        TEST_ASSERT_TRUE(entry->binding.target.aot.thunk == view.functionPointer);
        TEST_ASSERT_NOT_NULL(entry->binding.target.aot.methodInfo);
        TEST_ASSERT_NOT_NULL(entry->binding.target.callableObject);
        if (view.contract.operation == ZR_CALL_BINDING_OPERATION_META)
            TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
        if (view.contract.operation == ZR_CALL_BINDING_OPERATION_GET) ++getters;
        if (view.contract.operation == ZR_CALL_BINDING_OPERATION_SET) ++setters;
    }
    TEST_ASSERT_EQUAL_UINT32(expectedGetters, getters);
    TEST_ASSERT_EQUAL_UINT32(expectedSetters, setters);
    TEST_ASSERT_EQUAL_UINT32(expectedDeferred, deferred);

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void assert_generated_call_binding_rows(const char *source, const char *artifactSuite,
        TZrUInt32 getters, TZrUInt32 setters, TZrUInt32 deferred) {
    assert_generated_call_binding_rows_backend(source, artifactSuite, ZR_AOT_BACKEND_KIND_C,
            getters, setters, deferred);
}

static void test_aot_runtime_generated_call_binding_row_selects_registered_thunk(void) {
    assert_generated_call_binding_rows(
            "class Math { pub static fn answer(): int { return 42; } }\n"
            "return Math.answer();\n", "aot_c_call_binding_loader", 0u, 0u, 0u);
}

static void assert_generated_property_binding_rows(EZrAotBackendKind backend) {
    assert_generated_call_binding_rows_backend(
            "class Math { pub static fn answer(): int { return 42; } }\n"
            "class Box { pri var stored: int = 1;\n"
            "pub property value: int { get { return this.stored; } set { this.stored = value; } } }\n"
            "fn access(box: Box): int { box.value = 42; return box.value; }\n"
            "return access(new Box());\n",
            backend == ZR_AOT_BACKEND_KIND_C ? "aot_c_property_binding_loader" : "aot_llvm_property_binding_loader",
            backend, 1u, 1u, 0u);
}

static void test_aot_runtime_generated_property_rows_select_registered_thunks(void) {
    assert_generated_property_binding_rows(ZR_AOT_BACKEND_KIND_C);
}

static void test_aot_llvm_property_binding_executes_accessors(void) {
    assert_generated_property_binding_rows(ZR_AOT_BACKEND_KIND_LLVM);
}

static void test_aot_runtime_generated_interface_row_preserves_deferred_slot(void) {
    assert_generated_call_binding_rows(
            "interface Readable { fn read(): int; }\n"
            "class Box : Readable { pub fn read(): int { return 42; } }\n"
            "var box: Readable = new Box(); return box.read();\n",
            "aot_c_interface_binding_loader", 0u, 0u, 1u);
}

static void test_aot_meta_call_consumes_registered_binding(void) {
    assert_generated_call_binding_rows(
            "class Callable { pub @call(value: int): int { return value + 1; } } "
            "var value = new Callable(); return value(41);",
            "aot_c_meta_binding_loader", 0u, 0u, 0u);
}

static void test_aot_zero_argument_meta_call_consumes_binding(void) {
    assert_generated_call_binding_rows(
            "class Callable { pub @call(): int { return 42; } } "
            "var value = new Callable(); return value();",
            "aot_c_meta_noargs_binding_loader", 0u, 0u, 0u);
}

static void test_aot_llvm_meta_call_consumes_binding(void) {
    assert_generated_call_binding_rows_backend(
            "class Callable { pub @call(value: int): int { return value + 1; } } "
            "var value = new Callable(); return value(41);",
            "aot_llvm_meta_binding_loader", ZR_AOT_BACKEND_KIND_LLVM, 0u, 0u, 0u);
}

#include "call_binding_aot_module_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_runtime_rejects_embedded_module_with_incompatible_metadata_binding);
    RUN_TEST(test_aot_runtime_generated_call_binding_row_selects_registered_thunk);
    RUN_TEST(test_aot_runtime_generated_property_rows_select_registered_thunks);
    RUN_TEST(test_aot_llvm_property_binding_executes_accessors);
    RUN_TEST(test_aot_runtime_generated_interface_row_preserves_deferred_slot);
    RUN_TEST(test_aot_meta_call_consumes_registered_binding);
    RUN_TEST(test_aot_zero_argument_meta_call_consumes_binding);
    RUN_TEST(test_aot_llvm_meta_call_consumes_binding);
    RUN_TEST(test_aot_imported_module_function_uses_provider_thunk);
    RUN_TEST(test_aot_imported_static_method_uses_provider_thunk);
    RUN_TEST(test_aot_imported_function_preserves_module_capture);
    RUN_TEST(test_aot_llvm_imports_provider_without_internal_bindings);
    return UNITY_END();
}
