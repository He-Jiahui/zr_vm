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

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_global_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_global_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1 >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1);
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

static void test_aot_c_source_lowers_get_global_to_runtime_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_get_global(FILE *file, TZrUInt32 destinationSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_library/aot_runtime.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GetGlobal(struct SZrState *state,",
            "TZrUInt32 destinationSlot);",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_GetGlobal(SZrState *state,",
            "aot_runtime_frame_slot(frame, destinationSlot)",
            "ZrCore_Value_Copy(state, destinationValue, &state->global->zrObject);",
            "ZrCore_Value_ResetAsNull(destinationValue);",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_get_global(",
            "zr_aot_value_exec_get_global",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetGlobal(state, &frame, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_GLOBAL):",
            "backend_aot_write_c_direct_get_global(file, destinationSlot);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "zr_aot_global_object",
            "state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT",
            "ZrCore_Value_Copy(state, zr_aot_destination, &state->global->zrObject);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_GetGlobal(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_object_array_creation_to_runtime_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_create_object(FILE *file, TZrUInt32 destinationSlot);",
            "backend_aot_write_c_direct_create_array(FILE *file, TZrUInt32 destinationSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_library/aot_runtime.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_CreateObject(struct SZrState *state,",
            "ZrLibrary_AotRuntime_CreateArray(struct SZrState *state,",
            "TZrUInt32 destinationSlot);",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_CreateObject(SZrState *state,",
            "TZrBool ZrLibrary_AotRuntime_CreateArray(SZrState *state,",
            "aot_runtime_frame_slot(frame, destinationSlot)",
            "ZrCore_Object_New(state, ZR_NULL);",
            "ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);",
            "ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(objectValue));",
            "ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayValue));",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_create_object(",
            "backend_aot_write_c_direct_create_array(",
            "zr_aot_value_exec_create_object",
            "zr_aot_value_exec_create_array",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateObject(state, &frame, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateArray(state, &frame, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):",
            "backend_aot_write_c_direct_create_object(file, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):",
            "backend_aot_write_c_direct_create_array(file, destinationSlot);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "SZrObject *zr_aot_object = ZrCore_Object_New(state, ZR_NULL);",
            "SZrObject *zr_aot_array = ZrCore_Object_NewCustomized(state,",
            "ZR_OBJECT_INTERNAL_TYPE_ARRAY);",
            "zr_aot_destination->type = ZR_VALUE_TYPE_ARRAY;",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_CreateObject(state, &frame",
            "ZrLibrary_AotRuntime_CreateArray(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_typeof_to_runtime_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_typeof(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_library/aot_runtime.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_TypeOf(struct SZrState *state,",
            "TZrUInt32 destinationSlot,",
            "TZrUInt32 sourceSlot);",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_TypeOf(SZrState *state,",
            "aot_runtime_frame_slot(frame, destinationSlot)",
            "aot_runtime_frame_slot(frame, sourceSlot)",
            "ZrCore_Reflection_TypeOfValue(state, sourceValue, destinationValue)",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_typeof(",
            "zr_aot_value_exec_typeof",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_TypeOf(state, &frame, %u, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TYPEOF):",
            "backend_aot_write_c_direct_typeof(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "ZrCore_Reflection_TypeOfValue(state, zr_aot_source, zr_aot_destination)",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_TypeOf(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_object_struct_conversions_to_runtime_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_to_object(FILE *file,",
            "backend_aot_write_c_direct_to_struct(FILE *file,",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_library/aot_runtime.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_ToObject(struct SZrState *state,",
            "TZrUInt32 destinationSlot,",
            "TZrUInt32 sourceSlot,",
            "TZrUInt32 typeNameConstantIndex);",
            "ZrLibrary_AotRuntime_ToStruct(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_ToObject(SZrState *state,",
            "TZrBool ZrLibrary_AotRuntime_ToStruct(SZrState *state,",
            "aot_runtime_frame_slot(frame, destinationSlot)",
            "aot_runtime_frame_slot(frame, sourceSlot)",
            "typeNameConstantIndex >= function->constantValueLength",
            "&function->constantValueList[typeNameConstantIndex]",
            "ZrCore_Execution_ToObject(state,",
            "ZrCore_Execution_ToStruct(state,",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_to_object(",
            "backend_aot_write_c_direct_to_struct(",
            "zr_aot_value_exec_to_object",
            "zr_aot_value_exec_to_struct",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToObject(state, &frame, %u, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToStruct(state, &frame, %u, %u, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TO_STRUCT):",
            "backend_aot_write_c_direct_to_struct(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(TO_OBJECT):",
            "backend_aot_write_c_direct_to_object(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "const SZrTypeValue *zr_aot_type_name = %u < frame.function->constantValueLength",
            "ZrCore_Execution_ToObject(state,",
            "ZrCore_Execution_ToStruct(state,",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ToObject(state, &frame",
            "ZrLibrary_AotRuntime_ToStruct(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_to_string_to_runtime_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_library/aot_runtime.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_ToString(struct SZrState *state,",
            "TZrUInt32 destinationSlot,",
            "TZrUInt32 sourceSlot);",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_ToString(SZrState *state,",
            "aot_runtime_frame_slot(frame, destinationSlot)",
            "aot_runtime_frame_slot(frame, sourceSlot)",
            "ZrCore_Value_ConvertToString(state, sourceValue);",
            "aot_runtime_refresh_frame_from_callinfo(state, frame, callInfo)",
            "ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(resultString));",
            "ZrCore_Value_ResetAsNull(destinationValue);",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_to_string(",
            "zr_aot_value_exec_to_string",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, %u, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TO_STRING):",
            "backend_aot_write_c_direct_to_string(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "SZrString *zr_aot_result_string = ZR_NULL;",
            "zr_aot_result_string = ZrCore_Value_ConvertToString(state, zr_aot_source);",
            "frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;",
            "ZrCore_Value_InitAsRawObject(state, zr_aot_destination, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_result_string));",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ToString(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_makes_dynamic_member_index_access_explicit_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_unsupported_dynamic_value_access(FILE *file,",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/debug.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(struct SZrState *state,",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_dynamic_value_access(",
            "zr_aot_value_unsupported_dynamic_value_access",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(state,",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_MEMBER):",
            "\"GET_MEMBER\"",
            "case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):",
            "\"GET_MEMBER_SLOT\"",
            "case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):",
            "\"GET_BY_INDEX\"",
            "case ZR_INSTRUCTION_ENUM(SET_MEMBER):",
            "\"SET_MEMBER\"",
            "case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):",
            "\"SET_MEMBER_SLOT\"",
            "case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):",
            "\"SET_BY_INDEX\"",
            "backend_aot_write_c_unsupported_dynamic_value_access(file,",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "const TZrUInt32 zr_aot_operand_index = %u;",
            "ZrCore_Debug_RunError(state, \\\"unsupported AOT dynamic value access: %%s\\\", zr_aot_opcode_name);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_GetMember(state, &frame",
            "ZrLibrary_AotRuntime_GetMemberSlot(state, &frame",
            "ZrLibrary_AotRuntime_GetByIndex(state, &frame",
            "ZrLibrary_AotRuntime_SetMember(state, &frame",
            "ZrLibrary_AotRuntime_SetMemberSlot(state, &frame",
            "ZrLibrary_AotRuntime_SetByIndex(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_makes_meta_value_access_explicit_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_unsupported_meta_value_access(FILE *file,",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/debug.h\\\"\\n",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(struct SZrState *state,",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_meta_value_access(",
            "zr_aot_value_unsupported_meta_value_access",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(META_GET):",
            "\"META_GET\"",
            "case ZR_INSTRUCTION_ENUM(META_SET):",
            "\"META_SET\"",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):",
            "\"SUPER_META_GET_CACHED\"",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):",
            "\"SUPER_META_SET_CACHED\"",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):",
            "\"SUPER_META_GET_STATIC_CACHED\"",
            "case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):",
            "\"SUPER_META_SET_STATIC_CACHED\"",
            "backend_aot_write_c_unsupported_meta_value_access(file,",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_MetaGet",
            "ZrLibrary_AotRuntime_MetaSet",
            "ZrLibrary_AotRuntime_MetaGetCached",
            "ZrLibrary_AotRuntime_MetaSetCached",
            "ZrLibrary_AotRuntime_MetaGetStaticCached",
            "ZrLibrary_AotRuntime_MetaSetStaticCached",
            "const TZrUInt32 zr_aot_member_or_cache_index = %u;",
            "ZrCore_Debug_RunError(state, \\\"unsupported AOT meta value access: %%s\\\", zr_aot_opcode_name);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_MetaGet(state, &frame",
            "ZrLibrary_AotRuntime_MetaSet(state, &frame",
            "ZrLibrary_AotRuntime_MetaGetCached(state, &frame",
            "ZrLibrary_AotRuntime_MetaSetCached(state, &frame",
            "ZrLibrary_AotRuntime_MetaGetStaticCached(state, &frame",
            "ZrLibrary_AotRuntime_MetaSetStaticCached(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(runtimeHeaderText);
    free(valueLoweringText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_get_global_to_runtime_boundary);
    RUN_TEST(test_aot_c_source_lowers_object_array_creation_to_runtime_boundary);
    RUN_TEST(test_aot_c_source_lowers_typeof_to_runtime_boundary);
    RUN_TEST(test_aot_c_source_lowers_object_struct_conversions_to_runtime_boundary);
    RUN_TEST(test_aot_c_source_lowers_to_string_to_runtime_boundary);
    RUN_TEST(test_aot_c_source_makes_dynamic_member_index_access_explicit_boundary);
    RUN_TEST(test_aot_c_source_makes_meta_value_access_explicit_boundary);
    return UNITY_END();
}
