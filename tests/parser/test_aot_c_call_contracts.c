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
            "backend_aot_write_c_dynamic_function_call(FILE *file,",
            "const SZrAotExecIrFunction *functionIr,",
            "zr_aot_direct_dynamic_function_call",
            "ZrLibrary_AotRuntime_CallStackValue(state,",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "zr_aot_direct_dynamic_function_call_sync_u64_local_boundary",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
            "zr_aot_direct_dynamic_function_call_sync_f64_local_boundary",
            "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_find_exec_ir_instruction(",
            "backend_aot_exec_ir_instruction_is_dynamic_call(",
            "ZR_SEMIR_OPCODE_DYN_CALL",
            "ZR_SEMIR_OPCODE_DYN_TAIL_CALL",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):",
            "backend_aot_write_c_dynamic_function_call(file, functionIr, destinationSlot, operandA1, 0);",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):",
            "ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):",
            "ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL",
            "case ZR_INSTRUCTION_ENUM(DYN_CALL):",
            "semirDynamicCall ||",
            "instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||",
            "backend_aot_write_c_dynamic_function_call(file, functionIr, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_Call(state, &frame",
            "SZrFunctionStackAnchor zr_aot_call_anchor;",
            "SZrFunctionStackAnchor zr_aot_destination_anchor;",
            "ZrCore_Function_CallAndRestoreAnchor(state, &zr_aot_call_anchor, 1)",
            "*zr_aot_destination_value = *zr_aot_result_value;",
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
            "const SZrAotExecIrFunction *functionIr,",
            "zr_aot_direct_function_call",
            "ZrLibrary_AotRuntime_CallStackValue(state,",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "zr_aot_direct_function_call_sync_u64_local_boundary",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
            "zr_aot_direct_function_call_sync_f64_local_boundary",
            "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
            "\"zr_aot_direct_function_call\"",
            "\"function call\"",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_PrepareDirectCall",
            "SZrFunctionStackAnchor zr_aot_call_anchor;",
            "SZrFunctionStackAnchor zr_aot_destination_anchor;",
            "ZrCore_Function_CallAndRestoreAnchor(state, &zr_aot_call_anchor, 1)",
            "*zr_aot_destination_value = *zr_aot_result_value;",
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
            "const SZrAotExecIrFunction *functionIr",
            "zr_aot_direct_static_function_call",
            "ZrLibrary_AotRuntime_CallStaticDirect(state,",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "zr_aot_direct_static_function_call_sync_i64_local_boundary",
            "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
            "zr_aot_direct_static_function_call_sync_bool_local_boundary",
            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
            "zr_aot_direct_static_function_call_sync_u64_local_boundary",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
            "zr_aot_direct_static_function_call_sync_f64_local_boundary",
            "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
            "zr_aot_fn_%u",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_SyncSignedIntLocal(struct SZrState *state,",
            "TZrInt64 *outValue",
            "ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,",
            "TZrBool *outValue",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(struct SZrState *state,",
            "TZrUInt64 *outValue",
            "ZrLibrary_AotRuntime_SyncFloatLocal(struct SZrState *state,",
            "TZrFloat64 *outValue",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_SyncSignedIntLocal(SZrState *state,",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)",
            "*outValue = sourceValue->value.nativeObject.nativeInt64;",
            "ZrLibrary_AotRuntime_SyncBoolLocal(SZrState *state,",
            "ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)",
            "*outValue = (TZrBool)(sourceValue->value.nativeObject.nativeBool != 0u);",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(SZrState *state,",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)",
            "*outValue = sourceValue->value.nativeObject.nativeUInt64;",
            "ZrLibrary_AotRuntime_SyncFloatLocal(SZrState *state,",
            "ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)",
            "*outValue = sourceValue->value.nativeObject.nativeDouble;",
    };
    static const char *const scalarLocalsNeedles[] = {
            "backend_aot_c_scalar_locals_instruction_is_call_result_write",
            "case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):",
            "case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):",
            "case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):",
            "case ZR_INSTRUCTION_ENUM(DYN_CALL):",
            "case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):",
            "backend_aot_c_scalar_locals_slot_is_call_result_destination",
            "backend_aot_c_scalar_locals_record_slot_changed(",
            "slotKinds[destinationSlot]",
            "declaredSlotKinds[destinationSlot]",
    };
    static const char *const forbiddenCallLoweringNeedles[] = {
            "const SZrTypeValue *zr_aot_direct_call_result = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_direct_call_result->value.nativeObject.nativeInt64",
            "zr_aot_direct_call_result->value.nativeObject.nativeBool",
            "zr_aot_direct_call_result->value.nativeObject.nativeUInt64",
            "zr_aot_direct_call_result->value.nativeObject.nativeDouble",
            "zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);",
            "zr_aot_call_base = ZrCore_Function_CheckStackAndGc(state, 1u + %u, zr_aot_call_base);",
            "zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_callable_value);",
            "zr_aot_materialize_argument_source_slot",
            "ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(",
            "ZrCore_Function_PostCall(state, zr_aot_call_info, 1);",
            "ZrLibrary_AotRuntime_PrepareStaticDirectCall",
            "ZrLibrary_AotRuntime_FinishDirectCall",
    };
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_sync.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");

    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);

    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalsNeedles, ARRAY_COUNT(scalarLocalsNeedles));
    assert_text_contains_none(callLoweringText, forbiddenCallLoweringNeedles, ARRAY_COUNT(forbiddenCallLoweringNeedles));

    free(callLoweringText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(scalarLocalsText);
}

static void test_aot_c_source_makes_meta_calls_explicit_boundary(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_unsupported_meta_call(FILE *file,",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_UnsupportedMetaCall(struct SZrState *state,",
    };
    static const char *const callLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_meta_call(FILE *file,",
            "zr_aot_unsupported_meta_call",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_UnsupportedMetaCall(state,",
            "unsupported AOT meta call",
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
            "const TZrUInt32 zr_aot_destination_slot = %u;",
            "const TZrUInt32 zr_aot_receiver_slot = %u;",
            "const TZrUInt32 zr_aot_argument_count = %u;",
            "SZrTypeValue *zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "ZrCore_Debug_RunError(state,",
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
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *callLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(callLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(callLoweringText, callLoweringNeedles, ARRAY_COUNT(callLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(callLoweringText, forbiddenCallLoweringNeedles, ARRAY_COUNT(forbiddenCallLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(emitterHeaderText);
    free(runtimeHeaderText);
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
