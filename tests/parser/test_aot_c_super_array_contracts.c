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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_super_array_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_super_array_contracts.c");
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

static void test_aot_c_source_lowers_super_array_int_ops_to_boundary_helpers(void) {
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_SuperArrayGetInt(",
            "ZrLibrary_AotRuntime_SuperArraySetInt(",
            "ZrLibrary_AotRuntime_SuperArrayAddInt(",
            "ZrLibrary_AotRuntime_SuperArrayAddInt4(",
            "ZrLibrary_AotRuntime_SuperArrayAddInt4Const(",
            "ZrLibrary_AotRuntime_SuperArrayFillInt4Const(",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_SuperArrayGetInt(",
            "ZrCore_Object_SuperArrayGetInt(state,",
            "ZrLibrary_AotRuntime_SuperArraySetInt(",
            "ZrCore_Object_SuperArraySetInt(state,",
            "ZrLibrary_AotRuntime_SuperArrayAddInt(",
            "destinationSlot == ZR_INSTRUCTION_USE_RET_FLAG",
            "ZrCore_Object_SuperArrayAddInt(state,",
            "ZrLibrary_AotRuntime_SuperArrayAddInt4(",
            "ZrLibrary_AotRuntime_SuperArrayAddInt4Const(",
            "ZrLibrary_AotRuntime_SuperArrayFillInt4Const(",
            "ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(state,",
    };
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_direct_super_array_get_int(FILE *file,",
            "backend_aot_write_c_direct_super_array_set_int(FILE *file,",
            "backend_aot_write_c_direct_super_array_add_int(FILE *file,",
            "backend_aot_write_c_direct_super_array_add_int4(FILE *file,",
            "backend_aot_write_c_direct_super_array_add_int4_const(FILE *file,",
            "backend_aot_write_c_direct_super_array_fill_int4_const(FILE *file,",
    };
    static const char *const loweringNeedles[] = {
            "backend_aot_write_c_direct_super_array_get_int(",
            "backend_aot_write_c_direct_super_array_set_int(",
            "backend_aot_write_c_direct_super_array_add_int(",
            "backend_aot_write_c_direct_super_array_add_int4(",
            "backend_aot_write_c_direct_super_array_add_int4_const(",
            "backend_aot_write_c_direct_super_array_fill_int4_const(",
            "zr_aot_value_exec_super_array_get_int",
            "zr_aot_value_exec_super_array_set_int",
            "zr_aot_value_exec_super_array_add_int",
            "zr_aot_value_exec_super_array_add_int4",
            "zr_aot_value_exec_super_array_fill_int4_const",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayGetInt(state, &frame, %u, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArraySetInt(state, &frame, %u, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, %u, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt4(state, &frame, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayAddInt4Const(state, &frame, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayFillInt4Const(state, &frame, %u, %u, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):",
            "backend_aot_write_c_direct_super_array_get_int(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):",
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):",
            "backend_aot_write_c_direct_super_array_set_int(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):",
            "backend_aot_write_c_direct_super_array_add_int(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):",
            "backend_aot_write_c_direct_super_array_add_int4(file, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):",
            "backend_aot_write_c_direct_super_array_add_int4_const(file, entry->function, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):",
            "backend_aot_write_c_direct_super_array_fill_int4_const(file, entry->function, operandA1, operandB1, destinationSlot);",
    };
    static const char *const forbiddenLoweringNeedles[] = {
            "backend_aot_c_write_super_array_unsupported(",
            "backend_aot_c_get_signed_int_constant(",
            "backend_aot_c_write_super_array_invalid_constant(",
            "unsupported AOT super-array integer fast path",
            "SZrTypeValue *zr_aot_",
            "TZrStackValuePointer zr_aot_receiver_base",
            "ZrCore_Stack_GetValue(frame.slotBase",
            "frame.slotBase + %u",
            "ZrCore_Object_SuperArrayTryGetIntFast(",
            "ZrCore_Object_SuperArrayTrySetIntFast(",
            "ZrCore_Object_SuperArrayAddIntAssumeFast(",
            "ZrCore_Object_SuperArrayAddIntDiscardResultAssumeFast(",
            "ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(",
            "ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_SuperArrayGetInt",
            "ZrLibrary_AotRuntime_SuperArraySetInt",
            "ZrLibrary_AotRuntime_SuperArrayAddInt",
            "ZrLibrary_AotRuntime_SuperArrayAddInt4",
            "ZrLibrary_AotRuntime_SuperArrayAddInt4Const",
            "ZrLibrary_AotRuntime_SuperArrayFillInt4Const",
    };
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_super_array.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(loweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(loweringText, loweringNeedles, ARRAY_COUNT(loweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(loweringText, forbiddenLoweringNeedles, ARRAY_COUNT(forbiddenLoweringNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(emitterHeaderText);
    free(loweringText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_super_array_int_ops_to_boundary_helpers);
    return UNITY_END();
}
