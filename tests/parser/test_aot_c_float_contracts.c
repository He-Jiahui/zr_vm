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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_float_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_float_contracts.c");
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

static void test_aot_c_source_lowers_typed_float_mod_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_mod_float(FILE *file",
    };
    static const char *const emitterNeedles[] = {
            "#include <math.h>\\n",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_write_c_direct_mod_float(",
            "zr_aot_arith_exec_float_mod",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)",
            "zr_aot_right_scalar == 0.0",
            "ZrCore_Debug_RunError(state, \\\"modulo by zero\\\")",
            "fmod(zr_aot_left_scalar, zr_aot_right_scalar)",
            "ZR_VALUE_FAST_SET(zr_aot_destination",
            "ZR_VALUE_TYPE_DOUBLE",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(MOD_FLOAT):",
            "backend_aot_write_c_direct_mod_float(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_ModFloat",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ModFloat(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_float_mod.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(moduleText);
    free(functionBodyText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_typed_float_mod_to_direct_c);
    return UNITY_END();
}
