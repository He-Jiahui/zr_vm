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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_shift_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_shift_contracts.c");
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

static void test_aot_c_source_lowers_generic_integer_shift_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_shift_left(FILE *file",
            "backend_aot_write_c_direct_shift_right(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_write_c_direct_generic_shift",
            "backend_aot_write_c_direct_shift_left(",
            "backend_aot_write_c_direct_shift_right(",
            "zr_aot_shift_exec_generic_left",
            "zr_aot_shift_exec_generic_right",
            "ZR_VALUE_IS_TYPE_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_INT(zr_aot_right->type)",
            "backend_aot_write_c_unsigned_integer_like_extract(file, \"zr_aot_left\", \"zr_aot_left_unsigned\")",
            "backend_aot_write_c_integer_like_extract(file, \"zr_aot_left\", \"zr_aot_left_scalar\")",
            "backend_aot_write_c_integer_like_extract(file, \"zr_aot_right\", \"zr_aot_shift_count\")",
            "backend_aot_write_c_shift_count_guard(file, \"zr_aot_shift_count\")",
            "(TZrInt64)(zr_aot_left_unsigned << zr_aot_shift_count)",
            "zr_aot_left_scalar >> zr_aot_shift_count",
            "unsupported AOT generic integer shift",
            "ZR_VALUE_FAST_SET(zr_aot_destination",
            "ZR_VALUE_TYPE_INT64",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):",
            "backend_aot_write_c_direct_shift_left(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):",
            "backend_aot_write_c_direct_shift_right(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_ShiftLeft",
            "ZrLibrary_AotRuntime_ShiftRight",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ShiftLeft(state, &frame",
            "ZrLibrary_AotRuntime_ShiftRight(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bitwise.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(moduleText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_generic_integer_shift_to_direct_c);
    return UNITY_END();
}
