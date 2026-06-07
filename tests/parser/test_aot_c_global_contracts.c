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

static void test_aot_c_source_lowers_get_global_to_direct_c_value_copy(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_get_global(FILE *file, TZrUInt32 destinationSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/global.h\\\"\\n",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_get_global(",
            "zr_aot_global_object",
            "ZrCore_Stack_GetValue(frame.slotBase + %u)",
            "state->global != ZR_NULL",
            "state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT",
            "ZrCore_Value_Copy(state, zr_aot_destination, &state->global->zrObject);",
            "ZrCore_Value_ResetAsNull(zr_aot_destination);",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_GLOBAL):",
            "backend_aot_write_c_direct_get_global(file, destinationSlot);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_GetGlobal",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_GetGlobal(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_typeof_to_direct_reflection_call(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_typeof(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/reflection.h\\\"\\n",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_typeof(",
            "zr_aot_value_exec_typeof",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "ZrCore_Reflection_TypeOfValue(state, zr_aot_source, zr_aot_destination)",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TYPEOF):",
            "backend_aot_write_c_direct_typeof(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_TypeOf",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_TypeOf(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_object_struct_conversions_to_core_calls(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_to_object(FILE *file,",
            "backend_aot_write_c_direct_to_struct(FILE *file,",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/execution.h\\\"\\n",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_to_object(",
            "backend_aot_write_c_direct_to_struct(",
            "zr_aot_value_exec_to_object",
            "zr_aot_value_exec_to_struct",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "const SZrTypeValue *zr_aot_type_name = %u < frame.function->constantValueLength",
            "&frame.function->constantValueList[%u]",
            "ZrCore_Execution_ToObject(state,",
            "ZrCore_Execution_ToStruct(state,",
            "frame.callInfo,",
            "zr_aot_destination,",
            "zr_aot_source,",
            "zr_aot_type_name",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TO_STRUCT):",
            "backend_aot_write_c_direct_to_struct(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(TO_OBJECT):",
            "backend_aot_write_c_direct_to_object(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_ToObject",
            "ZrLibrary_AotRuntime_ToStruct",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ToObject(state, &frame",
            "ZrLibrary_AotRuntime_ToStruct(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_to_string_to_direct_core_conversion(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);",
    };
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/string.h\\\"\\n",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_to_string(",
            "zr_aot_value_exec_to_string",
            "SZrTypeValue *zr_aot_destination = ZR_NULL;",
            "SZrTypeValue *zr_aot_source = ZR_NULL;",
            "SZrString *zr_aot_result_string = ZR_NULL;",
            "SZrCallInfo *zr_aot_call_info = ZR_NULL;",
            "zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_result_string = ZrCore_Value_ConvertToString(state, zr_aot_source);",
            "zr_aot_call_info = frame.callInfo != ZR_NULL ? frame.callInfo : state->callInfoList;",
            "frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;",
            "state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;",
            "ZrCore_Value_InitAsRawObject(state, zr_aot_destination, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_result_string));",
            "zr_aot_destination->type = ZR_VALUE_TYPE_STRING;",
            "ZrCore_Value_ResetAsNull(zr_aot_destination);",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TO_STRING):",
            "backend_aot_write_c_direct_to_string(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_ToString",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ToString(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
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
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_dynamic_value_access(",
            "zr_aot_value_unsupported_dynamic_value_access",
            "const char *zr_aot_opcode_name",
            "SZrTypeValue *zr_aot_primary = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "SZrTypeValue *zr_aot_secondary = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "const TZrUInt32 zr_aot_operand_index = %u;",
            "unsupported AOT dynamic value access",
            "ZrCore_Debug_RunError(state,",
            "ZR_AOT_C_FAIL();",
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
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
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
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_meta_value_access(",
            "zr_aot_value_unsupported_meta_value_access",
            "const char *zr_aot_opcode_name",
            "SZrTypeValue *zr_aot_primary = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "SZrTypeValue *zr_aot_secondary = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "const TZrUInt32 zr_aot_member_or_cache_index = %u;",
            "unsupported AOT meta value access",
            "ZrCore_Debug_RunError(state,",
            "ZR_AOT_C_FAIL();",
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
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(valueLoweringText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_get_global_to_direct_c_value_copy);
    RUN_TEST(test_aot_c_source_lowers_typeof_to_direct_reflection_call);
    RUN_TEST(test_aot_c_source_lowers_object_struct_conversions_to_core_calls);
    RUN_TEST(test_aot_c_source_lowers_to_string_to_direct_core_conversion);
    RUN_TEST(test_aot_c_source_makes_dynamic_member_index_access_explicit_boundary);
    RUN_TEST(test_aot_c_source_makes_meta_value_access_explicit_boundary);
    return UNITY_END();
}
