#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include <dlfcn.h>
#endif

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
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

void setUp(void) {}

void tearDown(void) {}

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

static void *load_symbol(void *library, const char *symbolName) {
    void *symbol;

    dlerror();
    symbol = dlsym(library, symbolName);
    if (symbol == NULL) {
        const char *error = dlerror();
        printf("dlsym(%s) failed: %s\n", symbolName, error != NULL ? error : "<unknown>");
    }
    return symbol;
}
#endif

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

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    TEST_ASSERT_NOT_NULL(path);
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    fileSize = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, fileSize);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    buffer = (char *)malloc((size_t)fileSize + 1u);
    TEST_ASSERT_NOT_NULL(buffer);
    if (fileSize > 0) {
        TEST_ASSERT_EQUAL_size_t((size_t)fileSize, fread(buffer, 1, (size_t)fileSize, file));
    }
    buffer[fileSize] = '\0';
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return buffer;
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

static void test_aot_c_generated_shared_library_executes_string_field_value_type_copy_and_return(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C value-type shared-library smoke currently validates the Unix toolchain path");
#else
    const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "pub makeLabel(text: string): Label {\n"
            "    var local: Label = $Label(text);\n"
            "    return local;\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var copied: Label = original;\n"
            "copied.text = \"right\";\n"
            "var returned: Label = makeLabel(copied.text);\n"
            "returned.text = \"done\";\n"
            "if (original.text == \"left\" && copied.text == \"right\") {\n"
            "    return returned.text;\n"
            "}\n"
            "return \"bad\";";
    const char *projectJson =
            "{"
            "\"name\":\"aot-value-type-string-field-smoke\","
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
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "string_field_project",
                                                       "runtime_value_type_string_field_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "string_field_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "string_field_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "string_field_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "string_field_project/bin/aot_c/lib",
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

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_field_value_slot_load"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrCore_Value_Copy(state, zr_aot_dense_destination, "
                                "(const SZrTypeValue *)zr_aot_field);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_field_value_slot_store"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_TypeLayout_CopyInline(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Function_TryCopyInlineFrameReturnValue"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const TZrUInt32 zr_aot_type_layout_tokens[]"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Gc_WriteBarrier("));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT value SemIR field"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT dynamic value access"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
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

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_EQUAL_STRING("done",
                             ZrCore_String_GetNativeString(ZR_CAST_STRING(state, result.value.object)));
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_union_type_layout_token_uses_local_type_def(void) {
    const char *source =
            "pub union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "pub identity(shape: Shape): Shape {\n"
            "    var local: Shape = shape;\n"
            "    return local;\n"
            "}\n"
            "var shape: Shape = Shape.Circle(1.0);\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char *tokenTable;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_c_union_type_layout_token.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "type_layout_token/src",
                                                       "union_shape",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_union_type_layout_token";
    options.sourceHash = "aot-c-union-type-layout-token";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-union-type-layout-token";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const SZrTypeLayout ZrTypeLayout_"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    .kind = 2u,\n"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const SZrTypeLayout *const zr_aot_type_layouts[]"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "&ZrTypeLayout_"));
    tokenTable = strstr(generatedCText, "static const TZrUInt32 zr_aot_type_layout_tokens[]");
    TEST_ASSERT_NOT_NULL(tokenTable);
    TEST_ASSERT_NOT_NULL(strstr(tokenTable, "0x02000001u"));
    TEST_ASSERT_NULL(strstr(generatedCText, "debug.typeLayoutToken"));
    free(generatedCText);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_generated_type_layout_gc_descriptors_are_ref_exact_and_skip_pod(void) {
    const char *refSource =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub var count: int;\n"
            "    pub @constructor(text: string, count: int) {\n"
            "        this.text = text;\n"
            "        this.count = count;\n"
            "    }\n"
            "}\n"
            "var label: Label = $Label(\"live\", 3);\n"
            "return label.text;";
    const char *podSource =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "var point: Point = $Point(1, 2);\n"
            "return point.x + point.y;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *refFunction;
    SZrFunction *podFunction;
    SZrAotWriterOptions options;
    TZrChar refGeneratedCPath[ZR_TESTS_PATH_MAX];
    TZrChar podGeneratedCPath[ZR_TESTS_PATH_MAX];
#if defined(ZR_PLATFORM_UNIX)
    TZrChar refSharedLibraryPath[ZR_TESTS_PATH_MAX];
#endif
    char *refGeneratedCText;
    char *podGeneratedCText;

    TEST_ASSERT_NOT_NULL(state);
    refFunction = compile_source(state, refSource, "aot_c_gc_descriptor_ref.zr");
    TEST_ASSERT_NOT_NULL(refFunction);
    podFunction = compile_source(state, podSource, "aot_c_gc_descriptor_pod.zr");
    TEST_ASSERT_NOT_NULL(podFunction);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "gc_descriptor/src",
                                                       "ref_struct",
                                                       ".c",
                                                       refGeneratedCPath,
                                                       sizeof(refGeneratedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "gc_descriptor/src",
                                                       "pod_struct",
                                                       ".c",
                                                       podGeneratedCPath,
                                                       sizeof(podGeneratedCPath)));
#if defined(ZR_PLATFORM_UNIX)
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "gc_descriptor/lib",
                                                       "libref_struct",
                                                       ".so",
                                                       refSharedLibraryPath,
                                                       sizeof(refSharedLibraryPath)));
#endif

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_gc_descriptor_ref";
    options.sourceHash = "aot-c-gc-descriptor-ref";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-gc-descriptor-ref";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, refFunction, refGeneratedCPath, &options));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_gc_descriptor_pod";
    options.sourceHash = "aot-c-gc-descriptor-pod";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-gc-descriptor-pod";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, podFunction, podGeneratedCPath, &options));

    refGeneratedCText = read_text_file_owned_or_fail(refGeneratedCPath);
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "/* zr_aot_gc_descriptor_offsets layout="));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, " count=1 */"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const TZrUInt32 ZrGcOffsets_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText,
                                "static const SZrAotGcDescriptor ZrGcDescriptor_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "    1u,\n    ZrGcOffsets_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText,
                                "static const SZrAotGcDescriptor *const zr_aot_gc_descriptors[]"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "&ZrGcDescriptor_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const SZrTypeLayoutField ZrTypeLayoutFields_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const SZrTypeLayout ZrTypeLayout_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const SZrTypeLayout *const zr_aot_type_layouts[]"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const TZrUInt32 zr_aot_type_layout_tokens[]"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "&ZrTypeLayout_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "    .typeLayouts = zr_aot_type_layouts,"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "    .typeLayoutTokens = zr_aot_type_layout_tokens,"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const SZrAotGcRootSlot zr_aot_gc_root_slots_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "static const SZrAotGcRootMap zr_aot_gc_root_map_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "    .gcRootMap = &zr_aot_gc_root_map_"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "SZrAotGcRootFrame zr_aot_gc_root_frame;"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "ZrCore_Gc_AotRootFramePush(state,"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "zr_aot_context.methodInfo->gcRootMap"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "ZrCore_Gc_AotRootFramePop(state, &zr_aot_gc_root_frame);"));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "/* aot_size.typeLayoutBytes["));
    TEST_ASSERT_NOT_NULL(strstr(refGeneratedCText, "/* aot_size.typeLayoutBytesTotal = "));
    TEST_ASSERT_NULL(strstr(refGeneratedCText, "zr_aot_gc_descriptor_offsets_failed"));
    free(refGeneratedCText);

#if defined(ZR_PLATFORM_UNIX)
    {
        char command[4096];
        void *library;
        void *symbol;
        FZrVmGetAotCompiledModule getModule;
        const ZrAotCompiledModule *module;
        const SZrAotGcDescriptor *descriptor = ZR_NULL;
        const SZrTypeLayout *layout = ZR_NULL;
        const SZrAotMethodInfo *methodInfo;
        TZrUInt32 descriptorIndex;
        TZrUInt32 rootIndex;
        TZrBool foundDescriptorRoot = ZR_FALSE;

        snprintf(command,
                 sizeof(command),
                 "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
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
                 refGeneratedCPath,
                 ZR_VM_TESTS_BUILD_LIB_DIR,
                 ZR_VM_TESTS_BUILD_LIB_DIR,
                 refSharedLibraryPath);
        TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

        library = dlopen(refSharedLibraryPath, RTLD_NOW | RTLD_LOCAL);
        if (library == NULL) {
            printf("dlopen(%s) failed: %s\n", refSharedLibraryPath, dlerror());
        }
        TEST_ASSERT_NOT_NULL(library);

        symbol = load_symbol(library, "ZrVm_GetAotCompiledModule");
        TEST_ASSERT_NOT_NULL(symbol);
        memcpy(&getModule, &symbol, sizeof(getModule));
        module = getModule();
        TEST_ASSERT_NOT_NULL(module);
        TEST_ASSERT_NOT_NULL(module->codeRegistration);
        TEST_ASSERT_NOT_NULL(module->typeLayouts);
        TEST_ASSERT_GREATER_THAN_UINT32(0u, module->typeLayoutCount);
        TEST_ASSERT_EQUAL_PTR(module->typeLayouts, module->codeRegistration->typeLayouts);
        TEST_ASSERT_EQUAL_UINT32(module->typeLayoutCount, module->codeRegistration->typeLayoutCount);
        TEST_ASSERT_NOT_NULL(module->typeLayoutTokens);
        TEST_ASSERT_EQUAL_UINT32(module->typeLayoutCount, module->typeLayoutTokenCount);
        TEST_ASSERT_EQUAL_PTR(module->typeLayoutTokens, module->codeRegistration->typeLayoutTokens);
        TEST_ASSERT_EQUAL_UINT32(module->typeLayoutTokenCount, module->codeRegistration->typeLayoutTokenCount);
        TEST_ASSERT_NOT_NULL(module->gcDescriptors);
        TEST_ASSERT_GREATER_THAN_UINT32(0u, module->gcDescriptorCount);

        for (descriptorIndex = 0u; descriptorIndex < module->gcDescriptorCount; descriptorIndex++) {
            if (module->gcDescriptors[descriptorIndex] != ZR_NULL) {
                descriptor = module->gcDescriptors[descriptorIndex];
                break;
            }
        }
        TEST_ASSERT_NOT_NULL(descriptor);
        TEST_ASSERT_EQUAL_UINT32(descriptorIndex, descriptor->typeLayoutId);
        TEST_ASSERT_EQUAL_UINT32(1u, descriptor->gcFieldCount);
        TEST_ASSERT_NOT_NULL(descriptor->gcFieldOffsets);
        TEST_ASSERT_TRUE(descriptorIndex < module->typeLayoutCount);
        layout = module->typeLayouts[descriptorIndex];
        TEST_ASSERT_NOT_NULL(layout);
        TEST_ASSERT_EQUAL_UINT32(descriptor->typeLayoutId, layout->cTypeId);
        TEST_ASSERT_EQUAL_UINT32(descriptor->gcFieldCount, layout->gcFieldCount);
        TEST_ASSERT_NOT_NULL(layout->gcFieldOffsets);
        TEST_ASSERT_EQUAL_UINT32(descriptor->gcFieldOffsets[0], layout->gcFieldOffsets[0]);
        TEST_ASSERT_NOT_NULL(module->methodInfos);
        methodInfo = module->methodInfos[0];
        TEST_ASSERT_NOT_NULL(methodInfo);
        TEST_ASSERT_NOT_NULL(methodInfo->gcRootMap);
        TEST_ASSERT_GREATER_THAN_UINT32(0u, methodInfo->gcRootMap->rootCount);
        TEST_ASSERT_NOT_NULL(methodInfo->gcRootMap->roots);
        for (rootIndex = 0u; rootIndex < methodInfo->gcRootMap->rootCount; rootIndex++) {
            const SZrAotGcRootSlot *root = &methodInfo->gcRootMap->roots[rootIndex];

            TEST_ASSERT_EQUAL_UINT32(ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET, root->locationKind);
            if (root->typeLayoutId == descriptor->typeLayoutId) {
                TEST_ASSERT_EQUAL_UINT32(descriptor->gcFieldOffsets[0], root->fieldByteOffset);
                foundDescriptorRoot = ZR_TRUE;
            }
        }
        TEST_ASSERT_TRUE(foundDescriptorRoot);

        dlclose(library);
    }
#endif

    podGeneratedCText = read_text_file_owned_or_fail(podGeneratedCPath);
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "/* zr_aot_gc_descriptor_offsets layout="));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "static const TZrUInt32 ZrGcOffsets_"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "static const TZrUInt32 ZrOwnershipOffsets_"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "static const SZrAotGcDescriptor ZrGcDescriptor_"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "static const SZrAotGcDescriptor *const zr_aot_gc_descriptors[]"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "zr_aot_gc_root_map_"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "zr_aot_gc_root_slots_"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "ZrCore_Gc_AotRootFramePush(state,"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "ZrCore_Gc_AotRootFramePop(state, &zr_aot_gc_root_frame);"));
    TEST_ASSERT_NOT_NULL(strstr(podGeneratedCText, "    .gcRootMap = ZR_NULL,"));
    TEST_ASSERT_NULL(strstr(podGeneratedCText, "zr_aot_gc_descriptor_offsets_failed"));
    TEST_ASSERT_NOT_NULL(strstr(podGeneratedCText, "/* aot_size.typeLayoutBytes["));
    TEST_ASSERT_NOT_NULL(strstr(podGeneratedCText, "/* aot_size.typeLayoutBytesTotal = "));
    free(podGeneratedCText);

    ZrCore_Function_Free(state, refFunction);
    ZrCore_Function_Free(state, podFunction);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_generated_type_layout_emits_ownership_offsets_for_owner_fields(void) {
    const char *source =
            "struct Holder {\n"
            "    pub var handle: Unique<string>;\n"
            "    pub var count: int;\n"
            "}\n"
            "var holder: Holder;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_c_owner_field_layout.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_type_shared_library",
                                                       "ownership_offsets/src",
                                                       "owner_struct",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_owner_field_layout";
    options.sourceHash = "aot-c-owner-field-layout";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-owner-field-layout";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const TZrUInt32 ZrOwnershipOffsets_"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_ownership_offsets layout="));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " count=1 */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    .ownershipFieldCount = 1u,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    .ownershipFieldOffsets = ZrOwnershipOffsets_"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_ownership_offsets_failed"));
    free(generatedCText);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_string_field_value_type_copy_and_return);
    RUN_TEST(test_aot_c_generated_union_type_layout_token_uses_local_type_def);
    RUN_TEST(test_aot_c_generated_type_layout_gc_descriptors_are_ref_exact_and_skip_pod);
    RUN_TEST(test_aot_c_generated_type_layout_emits_ownership_offsets_for_owner_fields);
    return UNITY_END();
}
