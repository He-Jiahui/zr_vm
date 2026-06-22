#include "unity.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
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
    TZrBytePtr bytes = ZR_NULL;
    TZrSize byteLength = 0u;
    char *text;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &byteLength));
    TEST_ASSERT_NOT_NULL(bytes);

    text = (char *)malloc(byteLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memcpy(text, bytes, byteLength);
    text[byteLength] = '\0';
    free(bytes);
    return text;
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    const char *found;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);

    found = strstr(text, needle);
    if (found != ZR_NULL) {
        printf("Generated C still contains forbidden token '%s' at byte offset %td\n",
               needle,
               (ptrdiff_t)(found - text));
        TEST_FAIL_MESSAGE("generated typed scalar AOT C contains forbidden runtime token");
    }
}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
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

static TZrInt64 execute_interpreter_i64(const char *source) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "typed_scalar_interpreter.zr");
    TEST_ASSERT_NOT_NULL(function);

    if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result)) {
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
        TEST_FAIL_MESSAGE("interpreter typed scalar execution did not produce an int64 result");
        return 0;
    }

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return result;
}

static void test_aot_c_typed_i64_scalar_uses_plain_c_and_matches_interpreter(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C typed scalar lowering currently validates the Unix shared-library toolchain path");
#else
    const char *source =
            "var left: int = 21;\n"
            "var right: int = 2;\n"
            "var product: int = left * right;\n"
            "var delta: int = 3;\n"
            "var threshold: int = 40;\n"
            "product > threshold;\n"
            "if (product > threshold) {\n"
            "    product + delta;\n"
            "} else {\n"
            "    product - delta;\n"
            "}\n"
            "var branchFlag: bool = product > threshold;\n"
            "if (branchFlag) {\n"
            "    product + right;\n"
            "} else {\n"
            "    product - right;\n"
            "}\n"
            "var unsignedLeft: uint = 9;\n"
            "var unsignedRight: uint = 4;\n"
            "var unsignedSum: uint = unsignedLeft + unsignedRight;\n"
            "var unsignedMasked: uint = unsignedSum & unsignedRight;\n"
            "var unsignedShifted: uint = unsignedSum << right;\n"
            "var unsignedShiftedBack: uint = unsignedShifted >> right;\n"
            "unsignedSum > unsignedRight;\n"
            "var maskBase: int = 15;\n"
            "var masked: int = maskBase & left;\n"
            "var joined: int = masked | right;\n"
            "var toggled: int = joined ^ delta;\n"
            "var shifted: int = toggled << right;\n"
            "var shiftedBack: int = shifted >> right;\n"
            "var inverted: int = ~right;\n"
            "var floatLeft: float = 1.5;\n"
            "var floatRight: float = 2.0;\n"
            "floatLeft * floatRight;\n"
            "<float> product;\n"
            "<float> unsignedSum;\n"
            "<int> floatLeft;\n"
            "<int> unsignedSum;\n"
            "<uint> product;\n"
            "<uint> floatLeft;\n"
            "if (product == 42) {\n"
            "    product + left;\n"
            "} else {\n"
            "    product - left;\n"
            "}\n"
            "var unsignedInverted: uint = ~unsignedRight;\n"
            "if (unsignedInverted <= unsignedRight) {\n"
            "    return product - left;\n"
            "}\n"
            "var floatCopy: float = floatLeft;\n"
            "return product + delta + shiftedBack + inverted + <int> unsignedMasked + <int> unsignedShiftedBack;\n";
    const char *projectJson =
            "{"
            "\"name\":\"aot-typed-scalar\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    TZrInt64 interpreterResult = execute_interpreter_i64(source);
    SZrState *aotState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *aotFunction;
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
    char command[4096];

    TEST_ASSERT_NOT_NULL(aotState);
    aotFunction = compile_source(aotState, source, "main.zr");
    TEST_ASSERT_NOT_NULL(aotFunction);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_scalar",
                                                       "project",
                                                       "typed_scalar",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_scalar",
                                                       "project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_scalar",
                                                       "project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_scalar",
                                                       "project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_typed_scalar",
                                                       "project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(aotState, aotFunction, zroPath, &binaryOptions));
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
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(aotState, aotFunction, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_text_contains(generatedCText, "const TZrUInt32 zr_aot_function_index = 0u;");
    assert_text_does_not_contain(generatedCText, "/* zr_aot_generated_frame_setup */");
    assert_text_does_not_contain(generatedCText, "ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted");
    assert_text_does_not_contain(generatedCText, "ZrAotGeneratedFrame frame = {0};");
    assert_text_does_not_contain(generatedCText, "TZrBool zr_aot_frame_started = ZR_FALSE;");
    assert_text_does_not_contain(generatedCText, "zr_aot_frame_started = ZR_TRUE;");
    assert_text_does_not_contain(generatedCText, "if (zr_aot_frame_started) {");
    assert_text_does_not_contain(generatedCText,
                                 "TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;");
    assert_text_does_not_contain(generatedCText, "zr_aot_begin_instruction");
    assert_text_does_not_contain(generatedCText, "frame.recordHandle");
    assert_text_does_not_contain(generatedCText, "frame.functionIndex");
    assert_text_does_not_contain(generatedCText, "frame.module");
    assert_text_does_not_contain(generatedCText, "frame.moduleExecuted");
    assert_text_does_not_contain(generatedCText, "frame.functionTable");
    assert_text_does_not_contain(generatedCText, "frame.functionCount");
    assert_text_does_not_contain(generatedCText, "frame.functionThunks");
    assert_text_does_not_contain(generatedCText, "frame.functionThunkCount");
    assert_text_does_not_contain(generatedCText, "frame.function = zr_aot_context.metadataFunction;");
    assert_text_does_not_contain(generatedCText, "frame.callInfo = zr_aot_call_info;");
    assert_text_does_not_contain(generatedCText, "frame.slotBase = zr_aot_slot_base;");
    assert_text_does_not_contain(generatedCText, "frame.generatedFrameSlotCount = zr_aot_context.generatedFrameSlotCount;");
    assert_text_does_not_contain(generatedCText, "ZrAotGeneratedModuleContext zr_aot_context;");
    assert_text_does_not_contain(generatedCText,
                                 "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, 0, &zr_aot_context)");
    assert_text_does_not_contain(generatedCText, "TZrStackValuePointer zr_aot_function_base;");
    assert_text_does_not_contain(generatedCText, "TZrStackValuePointer zr_aot_slot_base;");
    assert_text_does_not_contain(generatedCText, "TZrStackValuePointer zr_aot_frame_top;");
    assert_text_does_not_contain(generatedCText, "TZrSize zr_aot_argument_count;");
    assert_text_does_not_contain(generatedCText, "TZrSize zr_aot_frame_slot_count;");
    assert_text_does_not_contain(generatedCText, "SZrFunctionStackAnchor zr_aot_base_anchor;");
    assert_text_does_not_contain(generatedCText, "SZrFunctionStackAnchor zr_aot_return_anchor;");
    assert_text_does_not_contain(generatedCText, "TZrBool zr_aot_has_return_anchor = ZR_FALSE;");
    assert_text_does_not_contain(generatedCText, "ZrCore_Function_CheckStackAndGc(");
    assert_text_does_not_contain(generatedCText, "ZrCore_Value_ResetAsNull(&zr_aot_slot_base");
    assert_text_does_not_contain(generatedCText, "frame.currentInstructionIndex");
    assert_text_does_not_contain(generatedCText, "frame.currentInstructionIndex = 0;");
    assert_text_does_not_contain(generatedCText, "frame.lastObservedInstructionIndex");
    assert_text_does_not_contain(generatedCText, "frame.lastObservedLine");
    assert_text_does_not_contain(generatedCText, "frame.observationMask = state->hasAotObservationPolicyOverride");
    assert_text_does_not_contain(generatedCText, "frame.publishAllInstructions = state->hasAotObservationPolicyOverride");
    assert_text_does_not_contain(generatedCText, "state->debugHookSignal");
    assert_text_does_not_contain(generatedCText, "ZrCore_Stack_GetValue(");
    assert_text_does_not_contain(generatedCText, "ZR_VALUE_FAST_SET(");
    assert_text_contains(generatedCText, "static const SZrAotMethodInfo zr_aot_method_info_0 = {");
    assert_text_contains(generatedCText, "static const SZrAotSignature zr_aot_signature_0 = {");
    assert_text_contains(generatedCText, "    .functionIndex = 0u,");
    assert_text_contains(generatedCText, "    .metadataFunction = ZR_NULL,");
    assert_text_contains(generatedCText, "    .registerFrameBytes = 0u,");
    assert_text_does_not_contain(generatedCText, "    .registerFrameBytes = 6272u,");
    assert_text_does_not_contain(generatedCText, "TZrSize zr_aot_frame_byte_size;");
    assert_text_does_not_contain(generatedCText, "TZrSize zr_aot_frame_byte_slot_count = 0;");
    assert_text_does_not_contain(generatedCText, "zr_aot_frame_byte_size = (TZrSize)0u;");
    assert_text_does_not_contain(generatedCText, "zr_aot_frame_byte_size = (TZrSize)6272u;");
    assert_text_contains(generatedCText, "value SemIR lowering frameByteSize=0");
    assert_text_does_not_contain(generatedCText, "value SemIR lowering frameByteSize=6272");
    assert_text_contains(generatedCText, "    .gcRootMap = ZR_NULL,");
    assert_text_contains(generatedCText, "    .signature = &zr_aot_signature_0,");
    assert_text_contains(generatedCText, "    .observationPolicy = 0u,");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_i64_binary");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_i64_compare");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_to_i64");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_to_u64");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_u64_binary");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_u64_compare");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_u64_bitwise");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_u64_bit_not");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_u64_shift");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_to_f64");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_f64_binary");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_i64_bit_not");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_i64_bitwise");
    assert_text_contains(generatedCText, "zr_aot_scalar_exec_i64_shift");
    assert_text_contains(generatedCText, "zr_aot_scalar_stack_copy_i64");
    assert_text_contains(generatedCText, "zr_aot_scalar_stack_copy_f64");
    assert_text_contains(generatedCText, "zr_aot_jump_if_signed_compare");
    assert_text_contains(generatedCText, "zr_aot_jump_if_bool_false");
    assert_text_contains(generatedCText, "zr_aot_scalar_locals_begin");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s0 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrUInt64 zr_aot_u6 = (TZrUInt64)0u;");
    assert_text_contains(generatedCText, "TZrUInt64 zr_aot_u12 = (TZrUInt64)0u;");
    assert_text_contains(generatedCText, "TZrUInt64 zr_aot_u13 = (TZrUInt64)0u;");
    assert_text_contains(generatedCText, "TZrUInt64 zr_aot_u14 = (TZrUInt64)0u;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s16 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s19 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrFloat64 zr_aot_f19 = 0.0;");
    assert_text_contains(generatedCText, "TZrFloat64 zr_aot_f20 = 0.0;");
    assert_text_contains(generatedCText, "TZrFloat64 zr_aot_f22 = 0.0;");
    assert_text_contains(generatedCText, "TZrFloat64 zr_aot_f40 = 0.0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s31 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrUInt64 zr_aot_u31 = (TZrUInt64)0u;");
    assert_text_contains(generatedCText, "TZrFloat64 zr_aot_f31 = 0.0;");
    assert_text_contains(generatedCText, "TZrFloat64 zr_aot_f32 = 0.0;");
    assert_text_contains(generatedCText, "TZrBool zr_aot_b5 = ZR_FALSE;");
    assert_text_contains(generatedCText, "TZrBool zr_aot_b23 = ZR_FALSE;");
    assert_text_contains(generatedCText, "zr_aot_scalar_locals_end");
    assert_text_contains(generatedCText,
                         "/* zr_aot_scalar_constant_i64_local */\n"
                         "        zr_aot_s0 = (TZrInt64)21;");
    assert_text_contains(generatedCText,
                         "/* zr_aot_scalar_constant_i64_local */\n"
                         "        zr_aot_s1 = (TZrInt64)2;");
    assert_text_contains(generatedCText, "zr_aot_s0 = (TZrInt64)21;");
    assert_text_contains(generatedCText, "zr_aot_s1 = (TZrInt64)2;");
    assert_text_contains(generatedCText, "zr_aot_s4 = (TZrInt64)40;");
    assert_text_contains(generatedCText, "zr_aot_s6 = (TZrInt64)9;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_constant_i64_local */\n"
            "        zr_aot_s0 = (TZrInt64)21;\n"
            "    }\n"
            "    {\n"
            "        /* zr_aot_value_exec_primitive_constant */");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[0].value;\n"
                                 "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
                                 "            zr_aot_destination->isGarbageCollectable) {\n"
                                 "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
                                 "        }\n"
                                 "        zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeInt64 = (TZrInt64)21;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_constant_i64_local */\n"
            "        zr_aot_s1 = (TZrInt64)2;\n"
            "    }\n"
            "    {\n"
            "        /* zr_aot_value_exec_primitive_constant */");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[1].value;\n"
                                 "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
                                 "            zr_aot_destination->isGarbageCollectable) {\n"
                                 "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
                                 "        }\n"
                                 "        zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeInt64 = (TZrInt64)2;");
    assert_text_contains(generatedCText, "zr_aot_f19 = (TZrFloat64)1.5;");
    assert_text_contains(generatedCText, "zr_aot_f20 = (TZrFloat64)2;");
    assert_text_contains(generatedCText,
                         "/* zr_aot_scalar_constant_f64_local */\n"
                         "        zr_aot_f19 = (TZrFloat64)1.5;");
    assert_text_contains(generatedCText,
                         "/* zr_aot_scalar_constant_f64_local */\n"
                         "        zr_aot_f20 = (TZrFloat64)2;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_constant_f64_local */\n"
            "        zr_aot_f19 = (TZrFloat64)1.5;\n"
            "    }\n"
            "    {\n"
            "        /* zr_aot_value_exec_primitive_constant */");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[19].value;\n"
                                 "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
                                 "            zr_aot_destination->isGarbageCollectable) {\n"
                                 "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
                                 "        }\n"
                                 "        zr_aot_destination->type = ZR_VALUE_TYPE_DOUBLE;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeDouble = (TZrFloat64)1.5;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_constant_f64_local */\n"
            "        zr_aot_f20 = (TZrFloat64)2;\n"
            "    }\n"
            "    {\n"
            "        /* zr_aot_value_exec_primitive_constant */");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[20].value;\n"
                                 "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
                                 "            zr_aot_destination->isGarbageCollectable) {\n"
                                 "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
                                 "        }\n"
                                 "        zr_aot_destination->type = ZR_VALUE_TYPE_DOUBLE;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeDouble = (TZrFloat64)2;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s0 = zr_aot_s_left;");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;\n"
            "        zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;\n"
            "        zr_aot_s2 = zr_aot_s0 * zr_aot_s1;");
    assert_text_does_not_contain(
            generatedCText,
            "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[0].value.type) ||\n"
            "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)");
    assert_text_contains(generatedCText, "zr_aot_s2 = zr_aot_s0 * zr_aot_s1;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_i64_binary semirOpcode=29 dstSlot=2 leftSlot=0 rightSlot=1 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[2].value;\n"
                                 "        zr_aot_s2 = zr_aot_s0 * zr_aot_s1;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s2 = zr_aot_s0 * zr_aot_s1;\n"
                                 "        zr_aot_s_result = zr_aot_s2;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s2 = zr_aot_s_left;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s4 = zr_aot_s_right;");
    assert_text_does_not_contain(
            generatedCText,
            "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[2].value.type) ||\n"
            "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[4].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_s2 = frame.slotBase[2].value.value.nativeObject.nativeInt64;\n"
            "        zr_aot_s4 = frame.slotBase[4].value.value.nativeObject.nativeInt64;\n"
            "        zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);");
    assert_text_contains(generatedCText, "zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_i64_compare semirOpcode=36 dstSlot=27 leftSlot=2 rightSlot=4 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[27].value;\n"
                                 "        zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);\n"
                                 "        zr_aot_s_result = zr_aot_b27;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s2 = zr_aot_left->value.nativeObject.nativeInt64;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s4 = zr_aot_right->value.nativeObject.nativeInt64;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s2 = zr_aot_left_scalar;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s4 = zr_aot_right_scalar;");
    assert_text_does_not_contain(
            generatedCText,
            "if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type))");
    assert_text_contains(generatedCText, "if (zr_aot_s2 <= zr_aot_s4) {");
    assert_text_does_not_contain(
            generatedCText,
            "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[2].value.type) ||\n"
            "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[3].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_s2 = frame.slotBase[2].value.value.nativeObject.nativeInt64;\n"
            "        zr_aot_s3 = frame.slotBase[3].value.value.nativeObject.nativeInt64;\n"
            "        zr_aot_s6 = zr_aot_s2 + zr_aot_s3;");
    assert_text_contains(generatedCText, "zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_i64_compare semirOpcode=36 dstSlot=7 leftSlot=2 rightSlot=4 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[7].value;\n"
                                 "        zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);\n"
                                 "        zr_aot_s_result = zr_aot_b7;");
    assert_text_contains(generatedCText, "zr_aot_b5 = (TZrBool)(zr_aot_b7 != 0u);");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[7].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[5].value;\n"
                                 "        zr_aot_source = &frame.slotBase[7].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_BOOL;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeBool = zr_aot_b_value;\n"
                                 "        zr_aot_b5 = (TZrBool)(zr_aot_b_value != 0u);");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_b_value = zr_aot_source->value.nativeObject.nativeBool;");
    assert_text_contains(generatedCText, "zr_aot_u9 = zr_aot_u12;");
    assert_text_contains(generatedCText, "zr_aot_u21 = zr_aot_u33;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[12].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[33].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[9].value;\n"
                                 "        zr_aot_source = &frame.slotBase[12].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[21].value;\n"
                                 "        zr_aot_source = &frame.slotBase[33].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_UINT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeUInt64 = zr_aot_u_value;\n"
                                 "        zr_aot_u9 = zr_aot_u_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_UINT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeUInt64 = zr_aot_u_value;\n"
                                 "        zr_aot_u21 = zr_aot_u_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u_value = zr_aot_source->value.nativeObject.nativeUInt64;");
    assert_text_contains(generatedCText, "zr_aot_s13 = zr_aot_s16;");
    assert_text_contains(generatedCText, "zr_aot_s18 = zr_aot_s19;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[16].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[19].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[13].value;\n"
                                 "        zr_aot_source = &frame.slotBase[16].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[18].value;\n"
                                 "        zr_aot_source = &frame.slotBase[19].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeInt64 = zr_aot_s_value;\n"
                                 "        zr_aot_s13 = zr_aot_s_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeInt64 = zr_aot_s_value;\n"
                                 "        zr_aot_s18 = zr_aot_s_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s_value = zr_aot_source->value.nativeObject.nativeInt64;");
    assert_text_contains(generatedCText, "zr_aot_s16 = zr_aot_s12 & zr_aot_s0;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_i64_bitwise semirOpcode=42 dstSlot=16 leftSlot=12 rightSlot=0 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[16].value;\n"
                                 "        zr_aot_s16 = zr_aot_s12 & zr_aot_s0;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s16 = zr_aot_s12 & zr_aot_s0;\n"
                                 "        zr_aot_s_result = zr_aot_s16;");
    assert_text_contains(generatedCText, "zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_i64_shift semirOpcode=45 dstSlot=19 leftSlot=15 rightSlot=1 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[19].value;\n"
                                 "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);\n"
                                 "        zr_aot_s_result = zr_aot_s19;");
    assert_text_contains(generatedCText, "zr_aot_s19 = ~zr_aot_s1;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_i64_bit_not semirOpcode=41 dstSlot=19 sourceSlot=1 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[19].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type))");
    assert_text_does_not_contain(generatedCText,
                                 "if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)) {\n"
                                 "            ZR_AOT_C_FAIL();\n"
                                 "        }\n"
                                 "        zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_s19 = ~zr_aot_s1;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s19 = ~zr_aot_s1;\n"
                                 "        zr_aot_s_result = zr_aot_s19;");
    assert_text_contains(generatedCText, "zr_aot_f40 = zr_aot_f19;");
    assert_text_contains(generatedCText, "zr_aot_f22 = zr_aot_f40;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[19].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[40].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[40].value;\n"
                                 "        zr_aot_source = &frame.slotBase[19].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[22].value;\n"
                                 "        zr_aot_source = &frame.slotBase[40].value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_DOUBLE;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeDouble = zr_aot_f_value;\n"
                                 "        zr_aot_f40 = zr_aot_f_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination->type = ZR_VALUE_TYPE_DOUBLE;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeDouble = zr_aot_f_value;\n"
                                 "        zr_aot_f22 = zr_aot_f_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_f_value = zr_aot_source->value.nativeObject.nativeDouble;");
    assert_text_contains(generatedCText, "zr_aot_jump_if_bool_false_scalar_local");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_b5 = (TZrBool)(zr_aot_condition->value.nativeObject.nativeBool != 0u);");
    assert_text_contains(generatedCText, "if (!zr_aot_b5) {");
    assert_text_contains(generatedCText, "zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_u64_compare semirOpcode=36 dstSlot=14 leftSlot=8 rightSlot=7 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[14].value;\n"
                                 "        zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);\n"
                                 "        zr_aot_u_result = zr_aot_b14;");
    assert_text_contains(generatedCText, "zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_u64_compare semirOpcode=35 dstSlot=23 leftSlot=21 rightSlot=7 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[23].value;\n"
                                 "        zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);\n"
                                 "        zr_aot_u_result = zr_aot_b23;");
    assert_text_contains(generatedCText, "if (!zr_aot_b23) {");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[21].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[7].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_u21 = frame.slotBase[21].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);");
    assert_text_does_not_contain(generatedCText, "if (!zr_aot_condition_bool) {");
    assert_text_does_not_contain(generatedCText,
                                 "const SZrTypeValue *zr_aot_left = ZR_NULL;\n"
                                 "        TZrInt64 zr_aot_right_literal = (TZrInt64)42;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_left = &frame.slotBase[2].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type))");
    assert_text_does_not_contain(generatedCText, "if (zr_aot_left_scalar != zr_aot_right_literal) {");
    assert_text_contains(generatedCText, "if (zr_aot_s2 != zr_aot_right_literal) {");
    assert_text_contains(generatedCText, "zr_aot_u6 = (TZrUInt64)zr_aot_s6;");
    assert_text_contains(generatedCText, "zr_aot_u7 = (TZrUInt64)4;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[6].value;\n"
                                 "        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[7].value;\n"
                                 "        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_to_u64 opcode=28 dstSlot=6 srcSlot=6 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_to_u64 opcode=28 dstSlot=7 srcSlot=7 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[6].value;\n"
                                 "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
                                 "            zr_aot_destination->isGarbageCollectable) {\n"
                                 "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
                                 "        }\n"
                                 "        zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeInt64 = (TZrInt64)9;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[7].value;\n"
                                 "        if (zr_aot_destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||\n"
                                 "            zr_aot_destination->isGarbageCollectable) {\n"
                                 "            ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);\n"
                                 "        }\n"
                                 "        zr_aot_destination->type = ZR_VALUE_TYPE_INT64;\n"
                                 "        zr_aot_destination->value.nativeObject.nativeInt64 = (TZrInt64)4;");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[6].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[7].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_u6 = frame.slotBase[6].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_u8 = zr_aot_u6 + zr_aot_u7;");
    assert_text_does_not_contain(generatedCText, "zr_aot_u6 = zr_aot_u_left;");
    assert_text_contains(generatedCText, "zr_aot_u8 = zr_aot_u6 + zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_u64_binary semirOpcode=27 dstSlot=8 leftSlot=6 rightSlot=7 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[8].value;\n"
                                 "        zr_aot_u8 = zr_aot_u6 + zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u8 = zr_aot_u6 + zr_aot_u7;\n"
                                 "        zr_aot_u_result = zr_aot_u8;");
    assert_text_does_not_contain(generatedCText, "zr_aot_u8 = frame.slotBase[8].value.value.nativeObject.nativeUInt64;");
    assert_text_does_not_contain(generatedCText, "zr_aot_u8 = zr_aot_u_left;");
    assert_text_does_not_contain(generatedCText, "zr_aot_u7 = zr_aot_u_right;");
    assert_text_contains(generatedCText, "zr_aot_u12 = zr_aot_u8 & zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_u64_bitwise semirOpcode=42 dstSlot=12 leftSlot=8 rightSlot=7 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[12].value;\n"
                                 "        zr_aot_u12 = zr_aot_u8 & zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u12 = zr_aot_u8 & zr_aot_u7;\n"
                                 "        zr_aot_u_result = zr_aot_u12;");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[8].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_u8 = frame.slotBase[8].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;\n"
            "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_contains(generatedCText, "zr_aot_u13 = zr_aot_u8 << zr_aot_s1;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_u64_shift semirOpcode=45 dstSlot=13 leftSlot=8 rightSlot=1 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[13].value;\n"
                                 "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u13 = zr_aot_u8 << zr_aot_s1;\n"
                                 "        zr_aot_u_result = zr_aot_u13;");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[10].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_u10 = frame.slotBase[10].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;\n"
            "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_contains(generatedCText, "zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_u64_shift semirOpcode=46 dstSlot=14 leftSlot=10 rightSlot=1 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[14].value;\n"
                                 "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;\n"
                                 "        zr_aot_u_result = zr_aot_u14;");
    assert_text_contains(generatedCText, "zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[8].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[7].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_u8 = frame.slotBase[8].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;\n"
            "        zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_FLOAT(frame.slotBase[19].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_FLOAT(frame.slotBase[20].value.type)");
    assert_text_does_not_contain(
            generatedCText,
            "zr_aot_f19 = frame.slotBase[19].value.value.nativeObject.nativeDouble;\n"
            "        zr_aot_f20 = frame.slotBase[20].value.value.nativeObject.nativeDouble;\n"
            "        zr_aot_f32 = zr_aot_f19 * zr_aot_f20;");
    assert_text_does_not_contain(generatedCText, "zr_aot_f19 = zr_aot_f_left;");
    assert_text_contains(generatedCText, "zr_aot_f32 = zr_aot_f19 * zr_aot_f20;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_f64_binary semirOpcode=29 dstSlot=32 leftSlot=19 rightSlot=20 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[32].value;\n"
                                 "        zr_aot_f32 = zr_aot_f19 * zr_aot_f20;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_f32 = zr_aot_f19 * zr_aot_f20;\n"
                                 "        zr_aot_f_result = zr_aot_f32;");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[12].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[0].value.type)");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s12 = frame.slotBase[12].value.value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_s16 = zr_aot_s12 & zr_aot_s0;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s12 = zr_aot_s_left;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s0 = zr_aot_s_right;");
    assert_text_contains(generatedCText, "zr_aot_s16 = zr_aot_s12 & zr_aot_s0;");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[15].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)");
    assert_text_does_not_contain(generatedCText,
                                 "!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[16].value.type) ||\n"
                                 "            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s15 = frame.slotBase[15].value.value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;\n"
                                 "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s16 = frame.slotBase[16].value.value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;\n"
                                 "        if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_contains(generatedCText, "if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {");
    assert_text_does_not_contain(generatedCText, "zr_aot_s15 = zr_aot_s_left;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s16 = zr_aot_s_left;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s1 = zr_aot_s_shift;");
    assert_text_contains(generatedCText, "zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);");
    assert_text_contains(generatedCText, "zr_aot_s20 = zr_aot_s16 >> zr_aot_s1;");
    assert_text_does_not_contain(generatedCText, "zr_aot_s1 = zr_aot_s_source;");
    assert_text_contains(generatedCText, "zr_aot_s19 = ~zr_aot_s1;");
    assert_text_contains(generatedCText, "zr_aot_u33 = ~zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_scalar_exec_u64_bit_not semirOpcode=41 dstSlot=33 sourceSlot=7 */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[33].value;\n"
                                 "        zr_aot_u33 = ~zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u33 = ~zr_aot_u7;\n"
                                 "        zr_aot_u_result = zr_aot_u33;");
    assert_text_does_not_contain(generatedCText,
                                 "if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(frame.slotBase[7].value.type)) {\n"
                                 "            ZR_AOT_C_FAIL();\n"
                                 "        }\n"
                                 "        zr_aot_u7 = frame.slotBase[7].value.value.nativeObject.nativeUInt64;\n"
                                 "        zr_aot_u33 = ~zr_aot_u7;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[2].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s2 = zr_aot_source->value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_f31 = (TZrFloat64)zr_aot_s2;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s2 = zr_aot_source->value.nativeObject.nativeInt64;\n"
                                 "        zr_aot_u31 = (TZrUInt64)zr_aot_s2;");
    assert_text_contains(generatedCText, "zr_aot_f31 = (TZrFloat64)zr_aot_s2;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_to_f64 opcode=30 dstSlot=31 srcSlot=2 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[31].value;\n"
                                 "        zr_aot_f31 = (TZrFloat64)zr_aot_s2;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_f31 = (TZrFloat64)zr_aot_s2;\n"
                                 "        zr_aot_f_result = zr_aot_f31;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_f19 = zr_aot_source->value.nativeObject.nativeDouble;\n"
                                 "        zr_aot_s31 = (TZrInt64)zr_aot_f19;");
    assert_text_contains(generatedCText, "zr_aot_s31 = (TZrInt64)zr_aot_f19;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_to_i64 opcode=32 dstSlot=31 srcSlot=19 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[31].value;\n"
                                 "        zr_aot_s31 = (TZrInt64)zr_aot_f19;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s31 = (TZrInt64)zr_aot_f19;\n"
                                 "        zr_aot_s_result = zr_aot_s31;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_source = &frame.slotBase[8].value;\n"
                                 "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type))");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u8 = zr_aot_source->value.nativeObject.nativeUInt64;\n"
                                 "        zr_aot_f31 = (TZrFloat64)zr_aot_u8;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u8 = zr_aot_source->value.nativeObject.nativeUInt64;\n"
                                 "        {\n"
                                 "            TZrUInt64 zr_aot_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;");
    assert_text_contains(generatedCText, "zr_aot_f31 = (TZrFloat64)zr_aot_u8;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_to_f64 opcode=31 dstSlot=31 srcSlot=8 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[31].value;\n"
                                 "        zr_aot_f31 = (TZrFloat64)zr_aot_u8;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_f31 = (TZrFloat64)zr_aot_u8;\n"
                                 "        zr_aot_f_result = zr_aot_f31;");
    assert_text_contains(generatedCText, "zr_aot_s31 = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u8 - zr_aot_limit);");
    assert_text_contains(generatedCText, "zr_aot_s31 = (TZrInt64)zr_aot_u8;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_to_i64 opcode=33 dstSlot=31 srcSlot=8 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s31 = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u8 - zr_aot_limit);\n"
                                 "                zr_aot_s_result = zr_aot_s31;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s31 = (TZrInt64)zr_aot_u8;\n"
                                 "                zr_aot_s_result = zr_aot_s31;");
    assert_text_contains(generatedCText, "zr_aot_u31 = (TZrUInt64)zr_aot_s2;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_to_u64 opcode=35 dstSlot=31 srcSlot=2 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[31].value;\n"
                                 "        zr_aot_u31 = (TZrUInt64)zr_aot_s2;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u31 = (TZrUInt64)zr_aot_s2;\n"
                                 "        zr_aot_u_result = zr_aot_u31;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_f19 = zr_aot_source->value.nativeObject.nativeDouble;\n"
                                 "        zr_aot_u31 = (TZrUInt64)zr_aot_f19;");
    assert_text_contains(generatedCText, "zr_aot_u31 = (TZrUInt64)zr_aot_f19;");
    assert_text_does_not_contain(
            generatedCText,
            "/* zr_aot_scalar_exec_to_u64 opcode=34 dstSlot=31 srcSlot=19 */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[31].value;\n"
                                 "        zr_aot_u31 = (TZrUInt64)zr_aot_f19;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_u31 = (TZrUInt64)zr_aot_f19;\n"
                                 "        zr_aot_u_result = zr_aot_u31;");
    assert_text_contains(generatedCText, "/* zr_aot_direct_return_i64_local */");
    assert_text_contains(generatedCText,
                         "/* zr_aot_direct_return_i64_local */\n"
                         "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s23));");
    assert_text_contains(generatedCText,
                         "/* zr_aot_direct_return_i64_local */\n"
                         "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s48));");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_direct_return_i64_local */\n"
                                 "        SZrCallInfo *zr_aot_call_info = frame.callInfo;");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_direct_return_i64_local */\n"
                                 "        SZrCallInfo *zr_aot_call_info = ZR_NULL;");
    assert_text_does_not_contain(generatedCText, "SZrTypeValue *zr_aot_caller_result_value;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_call_info = state->callInfoList;\n"
                                 "        if (zr_aot_call_info == ZR_NULL");
    assert_text_does_not_contain(generatedCText,
                                 "execution_discard_exception_handlers_for_callinfo(state, zr_aot_call_info);");
    assert_text_does_not_contain(generatedCText, "ZrCore_Closure_CloseClosure(state,");
    assert_text_does_not_contain(generatedCText,
                                 "ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, zr_aot_call_info);");
    assert_text_does_not_contain(generatedCText,
                                 "ZrCore_Ownership_ReleaseValue(state, zr_aot_caller_result_value);");
    assert_text_does_not_contain(generatedCText,
                                 "if (state == ZR_NULL || frame.function == ZR_NULL) {\n"
                                 "            ZR_AOT_C_FAIL();\n"
                                 "        }\n"
                                 "        if (zr_aot_call_info == ZR_NULL)");
    assert_text_does_not_contain(generatedCText,
                                 "if (frame.function->functionName == ZR_NULL ||\n"
                                 "            ZrCore_NativeString_Compare(ZrCore_String_GetNativeString(frame.function->functionName), \"constructor\") != 0)");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[23].value;\n"
                                 "        zr_aot_s23 = zr_aot_s2 - zr_aot_s0;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s23 = zr_aot_s2 - zr_aot_s0;\n"
                                 "        zr_aot_s_result = zr_aot_s23;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_result_slot = frame.slotBase + 23;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_destination = &frame.slotBase[48].value;\n"
                                 "        zr_aot_s48 = zr_aot_s45 + zr_aot_s47;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_s48 = zr_aot_s45 + zr_aot_s47;\n"
                                 "        zr_aot_s_result = zr_aot_s48;");
    assert_text_does_not_contain(generatedCText,
                                 "zr_aot_result_slot = frame.slotBase + 48;");
    assert_text_contains(generatedCText, "/* zr_aot_reset_stack_null_scalar_local_skip slot=21 */");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_value_exec_reset_stack_null */\n"
                                 "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
                                 "        if (frame.slotBase == ZR_NULL || 21 >= frame.generatedFrameSlotCount)");
    assert_text_contains(generatedCText, "/* zr_aot_reset_stack_null2_scalar_local_skip slots=3,4 */");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_value_exec_reset_stack_null2 */\n"
                                 "        SZrTypeValue *zr_aot_first = ZR_NULL;\n"
                                 "        SZrTypeValue *zr_aot_second = ZR_NULL;\n"
                                 "        if (frame.slotBase == ZR_NULL || 3 >= frame.generatedFrameSlotCount ||\n"
                                 "            4 >= frame.generatedFrameSlotCount)");
    assert_text_contains(generatedCText, "/* zr_aot_reset_stack_null2_scalar_local_skip slots=5,6 */");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_value_exec_reset_stack_null2 */\n"
                                 "        SZrTypeValue *zr_aot_first = ZR_NULL;\n"
                                 "        SZrTypeValue *zr_aot_second = ZR_NULL;\n"
                                 "        if (frame.slotBase == ZR_NULL || 5 >= frame.generatedFrameSlotCount ||\n"
                                 "            6 >= frame.generatedFrameSlotCount)");
    assert_text_contains(generatedCText, "/* zr_aot_reset_stack_null2_scalar_local_skip slots=6,7 */");
    assert_text_does_not_contain(generatedCText,
                                 "/* zr_aot_value_exec_reset_stack_null2 */\n"
                                 "        SZrTypeValue *zr_aot_first = ZR_NULL;\n"
                                 "        SZrTypeValue *zr_aot_second = ZR_NULL;\n"
                                 "        if (frame.slotBase == ZR_NULL || 6 >= frame.generatedFrameSlotCount ||\n"
                                 "            7 >= frame.generatedFrameSlotCount)");
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lzr_utf8proc -lzr_xx_hash -lm "
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

    project = ZrLibrary_Project_New(aotState, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    aotState->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(aotState->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&aotResult);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(aotState, ZR_AOT_BACKEND_KIND_C, &aotResult),
                             ZrLibrary_AotRuntime_GetLastError(aotState->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(aotResult.type));
    TEST_ASSERT_EQUAL_INT64(interpreterResult, aotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C,
                          ZrLibrary_AotRuntime_GetExecutedVia(aotState->global));

    aotState->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(aotState, project);
    free(embeddedBlob);
    ZrCore_Function_Free(aotState, aotFunction);
    ZrTests_Runtime_State_Destroy(aotState);
#endif
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_typed_i64_scalar_uses_plain_c_and_matches_interpreter);
    return UNITY_END();
}
