#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == NULL) {
        return NULL;
    }

    marker = strstr(sourceFile, "tests/parser/test_aot_c_call_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_call_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void assert_text_contains_none(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) != NULL) {
            printf("Unexpected source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("found forbidden source contract text");
        }
    }
}

static void test_aot_c_source_lowers_quickened_dynamic_calls_to_direct_core_calls(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_dynamic_function_call(FILE *file,",
    };
    static const char *const functionNeedles[] = {
            "ZrCore_Function_StackAnchorInit(",
            "ZrCore_Function_CallAndRestoreAnchor(",
    };
    static const char *const stackNeedles[] = {
            "ZrCore_Stack_GetValue(",
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_dynamic_function_call(",
            "zr_aot_direct_dynamic_function_call",
            "TZrStackValuePointer zr_aot_call_base;",
            "TZrStackValuePointer zr_aot_destination_pointer;",
            "zr_aot_call_base = frame.slotBase + %u;",
            "zr_aot_destination_pointer = frame.slotBase + %u;",
            "SZrTypeValue *zr_aot_destination_value;",
            "SZrTypeValue *zr_aot_result_value;",
            "SZrFunctionStackAnchor zr_aot_call_anchor;",
            "SZrFunctionStackAnchor zr_aot_destination_anchor;",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_call_base, &zr_aot_call_anchor);",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_destination_pointer, &zr_aot_destination_anchor);",
            "state->stackTop.valuePointer = zr_aot_call_base + 1 + %u;",
            "ZrCore_Function_CallAndRestoreAnchor(state, &zr_aot_call_anchor, 1)",
            "zr_aot_destination_pointer = ZrCore_Function_StackAnchorRestore(state, &zr_aot_destination_anchor);",
            "zr_aot_destination_value = ZrCore_Stack_GetValue(zr_aot_destination_pointer);",
            "zr_aot_result_value = ZrCore_Stack_GetValue(zr_aot_result_base);",
            "generated AOT %s has invalid result slot",
            "*zr_aot_destination_value = *zr_aot_result_value;",
            "frame.callInfo = state->callInfoList;",
            "frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;",
            "state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_find_exec_ir_instruction(",
            "backend_aot_exec_ir_instruction_is_dynamic_call(",
            "ZR_SEMIR_OPCODE_DYN_CALL",
            "ZR_SEMIR_OPCODE_DYN_TAIL_CALL",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):",
            "backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, 0);",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):",
            "ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):",
            "ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL",
            "case ZR_INSTRUCTION_ENUM(DYN_CALL):",
            "semirDynamicCall ||",
            "instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||",
            "backend_aot_write_c_dynamic_function_call(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_Call(state, &frame",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *functionHeaderText = read_repo_text_file_owned("zr_vm_core/include/zr_vm_core/function.h");
    char *stackHeaderText = read_repo_text_file_owned("zr_vm_core/include/zr_vm_core/stack.h");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(functionHeaderText);
    TEST_ASSERT_NOT_NULL(stackHeaderText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(functionHeaderText, functionNeedles, ARRAY_COUNT(functionNeedles));
    assert_text_contains_all(stackHeaderText, stackNeedles, ARRAY_COUNT(stackNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(callLoweringText, forbiddenCallLoweringNeedles, ARRAY_COUNT(forbiddenCallLoweringNeedles));

    free(emitterHeaderText);
    free(functionHeaderText);
    free(stackHeaderText);
    free(callLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_generic_function_calls_to_direct_core_calls(void) {
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_direct_function_call(FILE *file,",
            "zr_aot_direct_function_call",
            "TZrStackValuePointer zr_aot_call_base;",
            "TZrStackValuePointer zr_aot_destination_pointer;",
            "SZrFunctionStackAnchor zr_aot_call_anchor;",
            "SZrFunctionStackAnchor zr_aot_destination_anchor;",
            "SZrTypeValue *zr_aot_destination_value;",
            "SZrTypeValue *zr_aot_result_value;",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_call_base, &zr_aot_call_anchor);",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_destination_pointer, &zr_aot_destination_anchor);",
            "state->stackTop.valuePointer = zr_aot_call_base + 1 + %u;",
            "ZrCore_Function_CallAndRestoreAnchor(state, &zr_aot_call_anchor, 1)",
            "zr_aot_destination_pointer = ZrCore_Function_StackAnchorRestore(state, &zr_aot_destination_anchor);",
            "zr_aot_destination_value = ZrCore_Stack_GetValue(zr_aot_destination_pointer);",
            "zr_aot_result_value = ZrCore_Stack_GetValue(zr_aot_result_base);",
            "generated AOT %s has invalid result slot",
            "*zr_aot_destination_value = *zr_aot_result_value;",
            "frame.callInfo = state->callInfoList;",
            "frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;",
            "state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;",
            "\"zr_aot_direct_function_call\"",
            "\"function call\"",
            "generated AOT %s has invalid stack range",
            "generated AOT %s failed",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_PrepareDirectCall",
    };
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");

    TEST_ASSERT_NOT_NULL(callLoweringText);

    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_none(callLoweringText, forbiddenCallLoweringNeedles, ARRAY_COUNT(forbiddenCallLoweringNeedles));

    free(callLoweringText);
}

static void test_aot_c_source_lowers_static_direct_calls_to_direct_core_calls(void) {
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_static_direct_function_call(FILE *file,",
            "zr_aot_direct_static_function_call",
            "TZrStackValuePointer zr_aot_call_base;",
            "TZrStackValuePointer zr_aot_destination_pointer;",
            "SZrCallInfo *zr_aot_call_info;",
            "SZrFunction *zr_aot_metadata_function;",
            "SZrTypeValue *zr_aot_callable_value;",
            "zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);",
            "zr_aot_call_base = ZrCore_Function_CheckStackAndGc(state, 1u + %u, zr_aot_call_base);",
            "zr_aot_destination_pointer = frame.slotBase + %u;",
            "zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_callable_value);",
            "ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(zr_aot_call_base), zr_aot_callable_value);",
            "state->stackTop.valuePointer = zr_aot_call_base + 1 + %u;",
            "ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(",
            "ZR_AOT_C_GUARD(zr_aot_fn_%u(state));",
            "ZrCore_Function_PostCall(state, zr_aot_call_info, 1);",
            "frame.callInfo = state->callInfoList;",
            "frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;",
            "zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);",
            "generated AOT static direct call has invalid stack range",
            "generated AOT static direct call failed",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_PrepareStaticDirectCall",
            "ZrLibrary_AotRuntime_FinishDirectCall",
    };
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");

    TEST_ASSERT_NOT_NULL(callLoweringText);

    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_none(callLoweringText, forbiddenCallLoweringNeedles, ARRAY_COUNT(forbiddenCallLoweringNeedles));

    free(callLoweringText);
}

static void test_aot_c_source_makes_meta_calls_explicit_boundary(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_unsupported_meta_call(FILE *file,",
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_meta_call(FILE *file,",
            "zr_aot_unsupported_meta_call",
            "const TZrUInt32 zr_aot_destination_slot = %u;",
            "const TZrUInt32 zr_aot_receiver_slot = %u;",
            "const TZrUInt32 zr_aot_argument_count = %u;",
            "SZrTypeValue *zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "unsupported AOT meta call",
            "ZrCore_Debug_RunError(state,",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):",
            "backend_aot_write_c_unsupported_meta_call(file, destinationSlot, operandA1, 0);",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):",
            "ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):",
            "case ZR_INSTRUCTION_ENUM(META_CALL):",
            "case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):",
            "backend_aot_write_c_unsupported_meta_call(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "backend_aot_write_c_direct_meta_call",
            "ZrAotGeneratedDirectCall zr_aot_direct_call",
            "ZrLibrary_AotRuntime_PrepareMetaCall",
            "ZrLibrary_AotRuntime_CallPreparedOrGeneric",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "backend_aot_write_c_direct_meta_call",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(callLoweringText, forbiddenCallLoweringNeedles, ARRAY_COUNT(forbiddenCallLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(emitterHeaderText);
    free(callLoweringText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_quickened_dynamic_calls_to_direct_core_calls);
    RUN_TEST(test_aot_c_source_lowers_generic_function_calls_to_direct_core_calls);
    RUN_TEST(test_aot_c_source_lowers_static_direct_calls_to_direct_core_calls);
    RUN_TEST(test_aot_c_source_makes_meta_calls_explicit_boundary);
    return UNITY_END();
}
