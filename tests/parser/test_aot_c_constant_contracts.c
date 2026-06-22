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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_constant_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_constant_contracts.c");
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

static void test_aot_c_source_makes_unresolved_callable_constant_explicit_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_unsupported_callable_constant_materialization(",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_callable_constant_materialization(",
            "zr_aot_value_unsupported_callable_constant_materialization",
            "const SZrTypeValue *zr_aot_source = %u < frame.function->constantValueLength",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_source)",
            "unsupported AOT callable constant materialization",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_CONSTANT):",
            "backend_aot_write_c_unsupported_callable_constant_materialization(file,",
            "backend_aot_c_constant_requires_materialization(state, entry->function, operandA2)",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_CopyConstant",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_makes_create_closure_explicit_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_unsupported_create_closure_materialization(",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_unsupported_create_closure_materialization(",
            "zr_aot_value_unsupported_create_closure_materialization",
            "const TZrUInt32 zr_aot_capture_count = %u;",
            "const SZrTypeValue *zr_aot_source = %u < frame.function->constantValueLength",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_source)",
            "unsupported AOT CREATE_CLOSURE materialization",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):",
            "backend_aot_write_c_unsupported_create_closure_materialization(file,",
            "closureCaptureCount",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_CreateClosure",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_get_sub_function_to_native_closure_boundary_helper(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_get_sub_function(",
            "backend_aot_write_c_unsupported_get_sub_function_materialization(",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_direct_get_sub_function(",
            "zr_aot_value_get_sub_function_native_closure_boundary",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state,",
            "zr_aot_fn_%u));",
            "backend_aot_write_c_unsupported_get_sub_function_materialization(",
            "zr_aot_value_unsupported_get_sub_function_materialization",
            "const TZrUInt32 zr_aot_capture_count = %u;",
            "unsupported AOT GET_SUB_FUNCTION materialization",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(struct SZrState *state,",
            "FZrAotEntryThunk nativeThunk);",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(SZrState *state,",
            "FZrAotEntryThunk nativeThunk)",
            "childFunctionIndex >= ownerFunction->childFunctionLength",
            "metadataFunction = &ownerFunction->childFunctionList[childFunctionIndex];",
            "closure = ZrCore_ClosureNative_New(state, 0);",
            "closure->nativeFunction = (FZrNativeFunction)nativeThunk;",
            "closure->aotShimFunction = metadataFunction;",
            "ZrCore_Value_InitAsRawObject(state, destinationValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));",
            "destinationValue->isNative = ZR_TRUE;",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):",
            "SZrFunction *childFunction = ZR_NULL;",
            "backend_aot_find_function_table_index(functionTable, childFunction)",
            "childFunction->closureValueLength == 0",
            "backend_aot_write_c_direct_get_sub_function(file,",
            "backend_aot_write_c_unsupported_get_sub_function_materialization(file,",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "zr_aot_value_exec_get_sub_function_native_closure",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));

    free(headerText);
    free(valueLoweringText);
    free(functionBodyText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
}

static void test_aot_c_source_lowers_closure_value_access_to_runtime_boundary(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_get_closure_value(",
            "backend_aot_write_c_set_closure_value(",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GetClosureValue(struct SZrState *state,",
            "ZrLibrary_AotRuntime_SetClosureValue(struct SZrState *state,",
            "TZrUInt32 closureIndex);",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_GetClosureValue(SZrState *state,",
            "TZrBool ZrLibrary_AotRuntime_SetClosureValue(SZrState *state,",
            "aot_runtime_resolve_current_closure_capture(state, frame, closureIndex",
            "ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(destinationPointer), closureValue);",
            "ZrCore_Value_Copy(state, targetValue, sourceValue);",
            "ZrCore_Value_Barrier(state, barrierObject, sourceValue);",
    };
    static const char *const valueLoweringNeedles[] = {
            "backend_aot_write_c_get_closure_value(",
            "zr_aot_value_exec_get_closure_value",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetClosureValue(state, &frame, %u, %u));",
            "backend_aot_write_c_set_closure_value(",
            "zr_aot_value_exec_set_closure_value",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SetClosureValue(state, &frame, %u, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_CLOSURE):",
            "backend_aot_write_c_get_closure_value(file, destinationSlot, (TZrUInt32)operandA2);",
            "case ZR_INSTRUCTION_ENUM(SET_CLOSURE):",
            "backend_aot_write_c_set_closure_value(file, destinationSlot, (TZrUInt32)operandA2);",
            "case ZR_INSTRUCTION_ENUM(GETUPVAL):",
            "backend_aot_write_c_get_closure_value(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(SETUPVAL):",
            "backend_aot_write_c_set_closure_value(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueLoweringNeedles[] = {
            "const SZrTypeValue *zr_aot_current_closure_value = ZR_NULL;",
            "ZrCore_Stack_GetValue(frame.slotBase - 1)",
            "ZR_CAST_NATIVE_CLOSURE(state, zr_aot_current_closure_value->value.object)",
            "ZrCore_ClosureNative_GetCaptureValue(zr_aot_native_closure,",
            "ZR_CAST_VM_CLOSURE(state, zr_aot_current_closure_value->value.object)",
            "ZrCore_ClosureValue_GetValue(zr_aot_vm_closure_value)",
            "ZrCore_Value_Copy(state, zr_aot_destination, zr_aot_closure_value);",
            "ZrCore_Value_Copy(state, zr_aot_closure_value, zr_aot_source);",
            "ZrCore_Value_Barrier(state, zr_aot_barrier_object, zr_aot_source);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_GetClosureValue(state, &frame",
            "ZrLibrary_AotRuntime_SetClosureValue(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(valueLoweringText, valueLoweringNeedles, ARRAY_COUNT(valueLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueLoweringNeedles, ARRAY_COUNT(forbiddenValueLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(valueLoweringText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_makes_unresolved_callable_constant_explicit_boundary);
    RUN_TEST(test_aot_c_source_makes_create_closure_explicit_boundary);
    RUN_TEST(test_aot_c_source_lowers_get_sub_function_to_native_closure_boundary_helper);
    RUN_TEST(test_aot_c_source_lowers_closure_value_access_to_runtime_boundary);
    return UNITY_END();
}
