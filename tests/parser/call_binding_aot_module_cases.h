#ifndef ZR_TESTS_CALL_BINDING_AOT_MODULE_CASES_H
#define ZR_TESTS_CALL_BINDING_AOT_MODULE_CASES_H

#include "harness/module_fixture_support.h"

#if defined(ZR_PLATFORM_UNIX)
static ZrTestsFixtureSource binding_aot_provider;

static TZrBool binding_aot_source_loader(SZrState *state, TZrNativeString path,
        TZrNativeString hash, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(state, path, hash, io, &binding_aot_provider, 1u);
}

static void write_binding_aot_module(SZrState *state, const char *suite,
        const char *module, const char *source, EZrAotBackendKind backend) {
    TZrChar sourcePath[ZR_TESTS_PATH_MAX], binaryPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedPath[ZR_TESTS_PATH_MAX], libraryPath[ZR_TESTS_PATH_MAX];
    TZrChar hash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH], libraryName[128], sourceName[128];
    TZrBytePtr blob = ZR_NULL;
    TZrSize blobLength = 0u;
    SZrBinaryWriterOptions binaryOptions = {0};
    SZrAotWriterOptions options = {0};
    SZrFunction *function;
    char command[4096];
    snprintf(sourceName, sizeof(sourceName), "%s.zr", module);
    snprintf(libraryName, sizeof(libraryName), "zrvm_aot_%s", module);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(suite, "runtime_project/src", module,
            ".zr", sourcePath, sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(suite, "runtime_project/bin", module,
            ".zro", binaryPath, sizeof(binaryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(suite,
            backend == ZR_AOT_BACKEND_KIND_C ? "runtime_project/bin/aot_c/src" : "runtime_project/bin/aot_llvm/src", module,
            backend == ZR_AOT_BACKEND_KIND_C ? ".c" : ".ll", generatedPath, sizeof(generatedPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(suite,
            backend == ZR_AOT_BACKEND_KIND_C ? "runtime_project/bin/aot_c/lib" : "runtime_project/bin/aot_llvm/lib", libraryName,
            ".so", libraryPath, sizeof(libraryPath)));
    write_text_file_or_fail(sourcePath, source);
    function = compile_source(state, source, sourceName);
    TEST_ASSERT_NOT_NULL(function);
    binaryOptions.moduleName = module;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &binaryOptions));
    hash_file_or_fail(binaryPath, hash, sizeof(hash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(binaryPath, &blob, &blobLength));
    options.moduleName = module;
    options.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    options.inputHash = hash;
    options.embeddedModuleBlob = blob;
    options.embeddedModuleBlobLength = blobLength;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(backend == ZR_AOT_BACKEND_KIND_C
            ? ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedPath, &options)
            : ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, generatedPath, &options));
    snprintf(command, sizeof(command),
            "\"%s\" %s -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
            "-I\"%s/zr_vm_common/include\" -I\"%s/zr_vm_core/include\" -I\"%s/zr_vm_library/include\" "
            "\"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
            ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS "-o \"%s\"",
            backend == ZR_AOT_BACKEND_KIND_C ? ZR_VM_TESTS_C_COMPILER : "clang",
            backend == ZR_AOT_BACKEND_KIND_C ? "-std=c11" : "-mllvm -opaque-pointers",
            ZR_VM_TESTS_REPO_ROOT, ZR_VM_TESTS_REPO_ROOT,
            ZR_VM_TESTS_REPO_ROOT, generatedPath, ZR_VM_TESTS_BUILD_LIB_DIR,
            ZR_VM_TESTS_BUILD_LIB_DIR, libraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));
    free(blob);
    ZrCore_Function_Free(state, function);
}
#endif

static void assert_aot_module_binding_backend(const char *suite, const char *providerSource,
        const char *consumerSource, EZrAotBackendKind backend) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)suite; (void)providerSource; (void)consumerSource; (void)backend;
    TEST_IGNORE_MESSAGE("AOT module binding execution uses the Unix shared-library loader");
#else
    const char *projectJson = "{\"name\":\"call-binding-modules\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    SZrObjectModule *module;
    SZrMetadataRuntime *metadata;
    SZrTypeValue result;
    TZrUInt32 boundCalls = 0u;
    TEST_ASSERT_NOT_NULL(state);
    binding_aot_provider = (ZrTestsFixtureSource)ZR_TESTS_FIXTURE_SOURCE_TEXT("provider", providerSource);
    state->global->sourceLoader = binding_aot_source_loader;
    state->global->compileSource = ZrParser_Source_Compile;
    write_binding_aot_module(state, suite, "provider", providerSource, backend);
    write_binding_aot_module(state, suite, "main", consumerSource, backend);
    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(suite, "runtime_project", "project",
            ".zrp", projectPath, sizeof(projectPath)));
    write_text_file_or_fail(projectPath, projectJson);
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
            backend == ZR_AOT_BACKEND_KIND_C ? ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C : ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
            ZR_TRUE));
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, backend, &result),
            ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    module = ZrLibrary_AotRuntime_ModuleLoader(state, ZrCore_String_CreateFromNative(state, "main"), project->aotRuntime);
    TEST_ASSERT_NOT_NULL(module);
    metadata = ZrCore_Module_GetMetadataRuntime(module);
    TEST_ASSERT_NOT_NULL(metadata);
    for (TZrUInt32 index = 0u; index < metadata->metadataFunction->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &metadata->metadataFunction->callSiteCaches[index];
        if (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_VM_MODULE) continue;
        TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_AOT, entry->binding.target.targetKind);
        TEST_ASSERT_NOT_NULL(entry->binding.target.aot.thunk);
        TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
        ++boundCalls;
    }
    TEST_ASSERT_EQUAL_UINT32(1u, boundCalls);
    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void assert_aot_module_binding(const char *suite, const char *providerSource, const char *consumerSource) {
    assert_aot_module_binding_backend(suite, providerSource, consumerSource, ZR_AOT_BACKEND_KIND_C);
}

static void test_aot_llvm_imports_provider_without_internal_bindings(void) {
    assert_aot_module_binding_backend("aot_llvm_binding_module_function", "pub fn answer(): int { return 42; }",
            "var provider = import(\"provider\"); return provider.answer();", ZR_AOT_BACKEND_KIND_LLVM);
}

static void test_aot_imported_module_function_uses_provider_thunk(void) {
    assert_aot_module_binding("aot_binding_module_function", "pub fn answer(): int { return 42; }",
            "var provider = import(\"provider\"); return provider.answer();");
}

static void test_aot_imported_static_method_uses_provider_thunk(void) {
    assert_aot_module_binding("aot_binding_module_static", "pub class Math { pub static fn answer(): int { return 42; } }",
            "var provider = import(\"provider\"); return provider.Math.answer();");
}

static void test_aot_imported_function_preserves_module_capture(void) {
    assert_aot_module_binding("aot_binding_module_capture", "var offset: int = 42; pub fn answer(): int { return offset; }",
            "var provider = import(\"provider\"); return provider.answer();");
}

#endif
