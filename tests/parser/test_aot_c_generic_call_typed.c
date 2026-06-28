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

static void compile_generated_c_shared_library_or_fail(const TZrChar *generatedCPath, const TZrChar *sharedLibraryPath) {
    char command[4096];

    TEST_ASSERT_NOT_NULL(generatedCPath);
    TEST_ASSERT_NOT_NULL(sharedLibraryPath);
    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
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

static void assert_generated_c_reports_embedded_module_bytes(const char *generatedCText, TZrSize embeddedBlobLength) {
    char marker[128];
    int markerLength;

    TEST_ASSERT_NOT_NULL(generatedCText);
    markerLength = snprintf(marker,
                            sizeof(marker),
                            "/* aot_size.embeddedModuleBytes = %llu */",
                            (unsigned long long)embeddedBlobLength);
    TEST_ASSERT_TRUE(markerLength > 0);
    TEST_ASSERT_TRUE((size_t)markerLength < sizeof(marker));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, marker));
}

static void assert_generated_c_reports_method_metadata_bytes(const char *generatedCText) {
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* aot_size.methodMetadataBytes[0] = "));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* aot_size.methodMetadataBytesTotal = "));
}

static char *replace_first_static_method_with_null_owned_or_fail(const char *text) {
    const char *needle = ".staticMethod = zr_aot_fn_";
    const char *replacement = ".staticMethod = ZR_NULL";
    const char *start;
    const char *end;
    char *buffer;
    size_t prefixLength;
    size_t replacementLength;
    size_t suffixLength;

    TEST_ASSERT_NOT_NULL(text);
    start = strstr(text, needle);
    TEST_ASSERT_NOT_NULL(start);
    end = strstr(start, " },");
    TEST_ASSERT_NOT_NULL(end);

    prefixLength = (size_t)(start - text);
    replacementLength = strlen(replacement);
    suffixLength = strlen(end);
    buffer = (char *)malloc(prefixLength + replacementLength + suffixLength + 1u);
    TEST_ASSERT_NOT_NULL(buffer);

    memcpy(buffer, text, prefixLength);
    memcpy(buffer + prefixLength, replacement, replacementLength);
    memcpy(buffer + prefixLength + replacementLength, end, suffixLength + 1u);
    return buffer;
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
        for (index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static TZrInt64 execute_interpreter_i64(const char *source) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "generic_call_typed_interpreter.zr");
    TEST_ASSERT_NOT_NULL(function);

    if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result)) {
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
        TEST_FAIL_MESSAGE("interpreter generic call typed execution did not produce an int64 result");
        return 0;
    }

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return result;
}

static TZrInt64 aot_test_generic_method(struct SZrState *state) {
    (void)state;
    return 1234;
}

static void test_aot_runtime_generic_dictionary_lazily_resolves_method_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGenericSlot slot;
    SZrAotGenericResolvedSlot cache;
    SZrAotGenericDictionary dictionary;
    FZrAotEntryThunk resolvedMethod;

    TEST_ASSERT_NOT_NULL(state);

    memset(&slot, 0, sizeof(slot));
    slot.kind = ZR_AOT_GENERIC_SLOT_METHOD;
    slot.debugName = "identity<RefA>";
    slot.staticMethod = aot_test_generic_method;

    memset(&cache, 0, sizeof(cache));
    dictionary.slotCount = 1u;
    dictionary.slots = &slot;
    dictionary.resolvedSlots = &cache;

    TEST_ASSERT_EQUAL_UINT8(0u, cache.isResolved);
    resolvedMethod = ZrLibrary_AotRuntime_GenericSlot_Method(state, &dictionary, ZR_NULL, 0u);
    TEST_ASSERT_TRUE(resolvedMethod == aot_test_generic_method);
    TEST_ASSERT_EQUAL_INT64(1234, resolvedMethod(state));
    TEST_ASSERT_EQUAL_UINT8(1u, cache.isResolved);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_GENERIC_SLOT_METHOD, cache.kind);
    TEST_ASSERT_TRUE(cache.value.method == aot_test_generic_method);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GenericSlot_Method(state, &dictionary, ZR_NULL, 0u) ==
                     aot_test_generic_method);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_runtime_generic_dictionary_returns_null_for_missing_method_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrAotGenericSlot slot;
    SZrAotGenericResolvedSlot cache;
    SZrAotGenericDictionary dictionary;

    TEST_ASSERT_NOT_NULL(state);

    memset(&slot, 0, sizeof(slot));
    slot.kind = ZR_AOT_GENERIC_SLOT_METHOD;
    slot.debugName = "missing<RefA>";
    slot.staticMethod = ZR_NULL;

    memset(&cache, 0, sizeof(cache));
    dictionary.slotCount = 1u;
    dictionary.slots = &slot;
    dictionary.resolvedSlots = &cache;

    TEST_ASSERT_NULL(ZrLibrary_AotRuntime_GenericSlot_Method(state, &dictionary, ZR_NULL, 0u));
    TEST_ASSERT_EQUAL_UINT8(0u, cache.isResolved);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_generic_call_typed_emits_monomorphized_and_shared_method_forms(void) {
    const char *source =
            "struct Pair<TLeft, TRight> {\n"
            "    pub var left: TLeft;\n"
            "    pub var right: TRight;\n"
            "    pub @constructor(left: TLeft, right: TRight) {\n"
            "        this.left = left;\n"
            "        this.right = right;\n"
            "    }\n"
            "}\n"
            "class RefA { }\n"
            "class RefB { }\n"
            "class Box<T> where T: class { var value: T; }\n"
            "pub makePair(seed: int): Pair<int, int> {\n"
            "    var local: Pair<int, int> = $Pair<int, int>(seed, seed + 1);\n"
            "    return local;\n"
            "}\n"
            "var first: Box<RefA>;\n"
            "var second: Box<RefB>;\n"
            "var returned: Pair<int, int> = makePair(7);\n"
            "return returned.left + returned.right;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_c_generic_call_typed.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_call_typed",
                                                       "aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_call_typed",
                                                       "aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_call_typed";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-call-typed";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_generated_c_reports_method_metadata_bytes(generatedCText);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_monomorphized_direct */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "return zr_aot_fn_"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* symbol_stripping.generatedSymbols = 0 */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_fn_pair__1"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_shared_method_slot */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrAot_GenericSlot_Method(dict, 1u)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_method_1(state)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZR_AOT_GENERIC_SLOT_METHOD"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".staticMethod = zr_aot_generic_dict_1_method_0"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_fn_box__shared"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".debugName = \"Box<RefA>\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const TZrUInt32 zr_aot_type_layout_tokens[]"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "0x07000001u"));

#if defined(ZR_PLATFORM_UNIX)
    compile_generated_c_shared_library_or_fail(generatedCPath, sharedLibraryPath);
#endif

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_generic_call_typed_strips_generated_symbol_names_when_requested(void) {
    const char *source =
            "struct Pair<TLeft, TRight> {\n"
            "    pub var left: TLeft;\n"
            "    pub var right: TRight;\n"
            "    pub @constructor(left: TLeft, right: TRight) {\n"
            "        this.left = left;\n"
            "        this.right = right;\n"
            "    }\n"
            "}\n"
            "class RefA { }\n"
            "class RefB { }\n"
            "class Box<T> where T: class { var value: T; }\n"
            "pub makePair(seed: int): Pair<int, int> {\n"
            "    var local: Pair<int, int> = $Pair<int, int>(seed, seed + 1);\n"
            "    return local;\n"
            "}\n"
            "var first: Box<RefA>;\n"
            "var second: Box<RefB>;\n"
            "var returned: Pair<int, int> = makePair(7);\n"
            "return returned.left + returned.right;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_c_generic_call_typed_strip_symbols.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_call_typed_strip_symbols",
                                                       "aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_call_typed_strip_symbols";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-call-typed-strip-symbols";
    options.requireExecutableLowering = ZR_TRUE;
    options.stripGeneratedSymbols = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* symbol_stripping.generatedSymbols = 1 */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static TZrInt64 zr_fn_g1__1(struct SZrState *state)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "target=zr_fn_g1__shared"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "static TZrInt64 zr_fn_g1__shared(struct SZrState *state, SZrMetadataRuntime *metadataRuntime, const SZrAotGenericDictionary *dict)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".debugName = \"generic#1\""));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_fn_pair__"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_fn_box__shared"));
    TEST_ASSERT_NULL(strstr(generatedCText, "type=Pair"));
    TEST_ASSERT_NULL(strstr(generatedCText, "type=Box"));
    TEST_ASSERT_NULL(strstr(generatedCText, ".debugName = \"Box<"));

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_reference_generic_call_typed_uses_shared_method_slot_callsite(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic call typed shared callsite currently validates the Unix shared-library path");
#else
    const char *source =
            "struct Stamp {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "class RefA { }\n"
            "func stamp<T>(value: T): Stamp where T: class {\n"
            "    var local: Stamp = $Stamp(42);\n"
            "    return local;\n"
            "}\n"
            "var input: RefA;\n"
            "var returned: Stamp = stamp<RefA>(input);\n"
            "return returned.value;";
    const char *projectJson =
            "{"
            "\"name\":\"aot-reference-generic-call-typed\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    TZrInt64 interpreterResult = execute_interpreter_i64(source);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue aotResult;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed",
                                                       "project",
                                                       "reference_generic_call_typed",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed",
                                                       "project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed",
                                                       "project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed",
                                                       "project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed",
                                                       "project/bin/aot_c/lib",
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
    assert_generated_c_reports_embedded_module_bytes(generatedCText, embeddedBlobLength);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrAot_GenericSlot_Method(&zr_aot_generic_call_typed_"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZR_AOT_GENERIC_SLOT_METHOD"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".staticMethod = zr_aot_fn_"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallInlineStruct(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_missing_instance_deopt deopt="));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "\"generic call typed missing AOT instance\""));
    free(generatedCText);

    compile_generated_c_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&aotResult);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &aotResult),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(aotResult.type));
    TEST_ASSERT_EQUAL_INT64(interpreterResult, aotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_reference_generic_call_typed_full_aot_omits_missing_instance_deopt(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic call typed full-AOT mode currently validates the Unix shared-library path");
#else
    const char *source =
            "struct Stamp {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "class RefA { }\n"
            "func stamp<T>(value: T): Stamp where T: class {\n"
            "    var local: Stamp = $Stamp(42);\n"
            "    return local;\n"
            "}\n"
            "var input: RefA;\n"
            "var returned: Stamp = stamp<RefA>(input);\n"
            "return returned.value;";
    const char *projectJson =
            "{"
            "\"name\":\"aot-reference-generic-call-typed-full-aot\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    TZrInt64 interpreterResult = execute_interpreter_i64(source);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue aotResult;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_full_aot",
                                                       "project",
                                                       "reference_generic_call_typed_full_aot",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_full_aot",
                                                       "project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_full_aot",
                                                       "project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_full_aot",
                                                       "project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_full_aot",
                                                       "project/bin/aot_c/lib",
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
    aotOptions.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_full_aot_no_deopt */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "if (zr_aot_generic_call_typed_method == ZR_NULL)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_missing_instance_deopt deopt="));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_value_exec_call_typed_metadata_guard */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "\"typed inline struct direct call metadata drift\""));
    TEST_ASSERT_NULL(strstr(generatedCText, "\"generic call typed missing AOT instance\""));
    free(generatedCText);

    compile_generated_c_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&aotResult);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &aotResult),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(aotResult.type));
    TEST_ASSERT_EQUAL_INT64(interpreterResult, aotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_reference_generic_call_typed_missing_instance_deopts_to_interpreter(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic call typed missing-instance deopt currently validates the Unix shared-library path");
#else
    const char *source =
            "struct Stamp {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "class RefA { }\n"
            "func stamp<T>(value: T): Stamp where T: class {\n"
            "    var local: Stamp = $Stamp(42);\n"
            "    return local;\n"
            "}\n"
            "var input: RefA;\n"
            "var returned: Stamp = stamp<RefA>(input);\n"
            "return returned.value;";
    const char *projectJson =
            "{"
            "\"name\":\"aot-reference-generic-call-typed-deopt\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    TZrInt64 interpreterResult = execute_interpreter_i64(source);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue aotResult;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char *deoptGeneratedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_deopt",
                                                       "project",
                                                       "reference_generic_call_typed_deopt",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_deopt",
                                                       "project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_deopt",
                                                       "project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_deopt",
                                                       "project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_reference_generic_call_typed_deopt",
                                                       "project/bin/aot_c/lib",
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
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_call_typed_missing_instance_deopt deopt="));
    deoptGeneratedCText = replace_first_static_method_with_null_owned_or_fail(generatedCText);
    TEST_ASSERT_NOT_NULL(strstr(deoptGeneratedCText, ".staticMethod = ZR_NULL"));
    TEST_ASSERT_NOT_NULL(strstr(deoptGeneratedCText, "ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,"));
    write_text_file_or_fail(generatedCPath, deoptGeneratedCText);
    free(generatedCText);
    free(deoptGeneratedCText);

    compile_generated_c_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&aotResult);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &aotResult),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(aotResult.type));
    TEST_ASSERT_EQUAL_INT64(interpreterResult, aotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_runtime_generic_dictionary_lazily_resolves_method_slot);
    RUN_TEST(test_aot_runtime_generic_dictionary_returns_null_for_missing_method_slot);
    RUN_TEST(test_aot_c_generic_call_typed_emits_monomorphized_and_shared_method_forms);
    RUN_TEST(test_aot_c_generic_call_typed_strips_generated_symbol_names_when_requested);
    RUN_TEST(test_aot_c_reference_generic_call_typed_uses_shared_method_slot_callsite);
    RUN_TEST(test_aot_c_reference_generic_call_typed_full_aot_omits_missing_instance_deopt);
    RUN_TEST(test_aot_c_reference_generic_call_typed_missing_instance_deopts_to_interpreter);
    return UNITY_END();
}
