#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
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

static unsigned count_substring(const char *text, const char *needle) {
    unsigned count = 0u;
    const char *cursor;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);

    cursor = text;
    while ((cursor = strstr(cursor, needle)) != ZR_NULL) {
        count++;
        cursor += strlen(needle);
    }
    return count;
}

static void test_aot_runtime_generic_dictionary_lazily_resolves_type_layout_and_sizeof(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayout layout;
    SZrAotGenericSlot slots[2];
    SZrAotGenericResolvedSlot cache[2];
    SZrAotGenericDictionary dictionary;
    const SZrTypeLayout *resolvedLayout;
    TZrSize resolvedSize = 0u;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_TypeLayout_InitStruct(&layout,
                                 24u,
                                 8u,
                                 ZR_TYPE_LAYOUT_COPY_KIND_POD,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 ZR_NULL,
                                 0u);

    memset(slots, 0, sizeof(slots));
    slots[0].kind = ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT;
    slots[0].debugName = "Box<RefA>";
    slots[0].staticTypeLayout = &layout;
    slots[1].kind = ZR_AOT_GENERIC_SLOT_SIZEOF;
    slots[1].debugName = "sizeof(Box<RefA>)";
    slots[1].staticTypeLayout = &layout;

    memset(cache, 0, sizeof(cache));
    dictionary.slotCount = 2u;
    dictionary.slots = slots;
    dictionary.resolvedSlots = cache;

    TEST_ASSERT_EQUAL_UINT8(0u, cache[0].isResolved);
    resolvedLayout = ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, &dictionary, ZR_NULL, 0u);
    TEST_ASSERT_EQUAL_PTR(&layout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT8(1u, cache[0].isResolved);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT, cache[0].kind);
    TEST_ASSERT_EQUAL_PTR(&layout, cache[0].value.typeLayout);
    TEST_ASSERT_EQUAL_PTR(&layout, ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, &dictionary, ZR_NULL, 0u));

    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(state,
                                                                   &dictionary,
                                                                   ZR_NULL,
                                                                   1u,
                                                                   &resolvedSize));
    TEST_ASSERT_EQUAL_UINT64(24u, resolvedSize);
    TEST_ASSERT_EQUAL_UINT8(1u, cache[1].isResolved);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_GENERIC_SLOT_SIZEOF, cache[1].kind);
    TEST_ASSERT_EQUAL_UINT64(24u, cache[1].value.sizeOfValue);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_runtime_generic_dictionary_resolves_type_layout_from_metadata_runtime(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    SZrTypeLayout registeredLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrAotGenericSlot slots[2];
    SZrAotGenericResolvedSlot cache[2];
    SZrAotGenericDictionary dictionary;
    SZrMetadataRuntime *metadataRuntime;
    const SZrTypeLayout *resolvedLayout;
    TZrSize resolvedSize = 0u;

    TEST_ASSERT_NOT_NULL(state);

    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 96u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;

    registeredLayout.cTypeId = 42u;
    registeredLayout.byteSize = 24u;
    registeredLayouts[42] = &registeredLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;

    metadataRuntime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(metadataRuntime);

    memset(slots, 0, sizeof(slots));
    slots[0].kind = ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT;
    slots[0].typeLayoutId = 42u;
    slots[0].debugName = "Box<RefA>";
    slots[1].kind = ZR_AOT_GENERIC_SLOT_SIZEOF;
    slots[1].typeLayoutId = 42u;
    slots[1].debugName = "sizeof(Box<RefA>)";

    memset(cache, 0, sizeof(cache));
    dictionary.slotCount = 2u;
    dictionary.slots = slots;
    dictionary.resolvedSlots = cache;

    resolvedLayout = ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, &dictionary, metadataRuntime, 0u);
    TEST_ASSERT_EQUAL_PTR(&registeredLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT64(24u, resolvedLayout->byteSize);
    TEST_ASSERT_EQUAL_PTR(&registeredLayout, cache[0].value.typeLayout);

    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(state,
                                                                   &dictionary,
                                                                   metadataRuntime,
                                                                   1u,
                                                                   &resolvedSize));
    TEST_ASSERT_EQUAL_UINT64(24u, resolvedSize);
    TEST_ASSERT_EQUAL_UINT64(24u, cache[1].value.sizeOfValue);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_runtime_generic_dictionary_type_layout_does_not_fallback_to_prototype_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[8] = {0};
    SZrAotGenericSlot slots[2];
    SZrAotGenericResolvedSlot cache[2];
    SZrAotGenericDictionary dictionary;
    SZrMetadataRuntime *metadataRuntime;
    TZrSize resolvedSize = 123u;

    TEST_ASSERT_NOT_NULL(state);

    stalePrototypeLayouts[7].cTypeId = 7u;
    stalePrototypeLayouts[7].byteSize = 64u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 8u;

    metadataRuntime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(metadataRuntime);

    memset(slots, 0, sizeof(slots));
    slots[0].kind = ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT;
    slots[0].typeLayoutId = 7u;
    slots[1].kind = ZR_AOT_GENERIC_SLOT_SIZEOF;
    slots[1].typeLayoutId = 7u;

    memset(cache, 0, sizeof(cache));
    dictionary.slotCount = 2u;
    dictionary.slots = slots;
    dictionary.resolvedSlots = cache;

    TEST_ASSERT_NULL(ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, &dictionary, metadataRuntime, 0u));
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf(state,
                                                                    &dictionary,
                                                                    metadataRuntime,
                                                                    1u,
                                                                    &resolvedSize));
    TEST_ASSERT_EQUAL_UINT64(0u, resolvedSize);
    TEST_ASSERT_EQUAL_UINT8(0u, cache[0].isResolved);
    TEST_ASSERT_EQUAL_UINT8(0u, cache[1].isResolved);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_reference_generic_instances_share_dictionary_backed_code(void) {
    const char *source =
            "class RefA { }\n"
            "class RefB { }\n"
            "class Box<T> where T: class { var value: T; }\n"
            "var first: Box<RefA>;\n"
            "var second: Box<RefB>;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
#if defined(ZR_PLATFORM_UNIX)
    char command[4096];
#endif

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "aot_c_generic_reference_sharing.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_reference_sharing",
                                                       "aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_reference_sharing",
                                                       "aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_reference_sharing";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "reference-sharing";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_generic_dictionary_table */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "type=Box<RefA> share=shared"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "type=Box<RefB> share=shared"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "#define ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, slotIndex)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, (dict), (metadataRuntime), (slotIndex))"));
    TEST_ASSERT_EQUAL_UINT(1u,
                           count_substring(generatedCText,
                                           "static TZrInt64 zr_fn_box__shared(struct SZrState *state, SZrMetadataRuntime *metadataRuntime, const SZrAotGenericDictionary *dict)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "const SZrAotGenericDictionary *dict"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, 0u)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".genericDictionary = &zr_aot_generic_dict_1,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "share=monomorphized"));

#if defined(ZR_PLATFORM_UNIX)
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
#endif

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_runtime_generic_dictionary_lazily_resolves_type_layout_and_sizeof);
    RUN_TEST(test_aot_runtime_generic_dictionary_resolves_type_layout_from_metadata_runtime);
    RUN_TEST(test_aot_runtime_generic_dictionary_type_layout_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_aot_c_reference_generic_instances_share_dictionary_backed_code);
    return UNITY_END();
}
