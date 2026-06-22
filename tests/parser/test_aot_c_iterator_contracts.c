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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_iterator_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_iterator_contracts.c");
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

static void test_aot_c_source_lowers_iterator_ops_to_boundary_helpers(void) {
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_IterInit(",
            "ZrLibrary_AotRuntime_IterMoveNext(",
            "ZrLibrary_AotRuntime_IterCurrent(",
            "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_IterInit(",
            "ZrCore_Object_IterInit(state,",
            "ZrLibrary_AotRuntime_IterMoveNext(",
            "ZrCore_Object_IterMoveNext(state,",
            "ZrLibrary_AotRuntime_IterCurrent(",
            "ZrCore_Object_IterCurrent(state,",
            "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(",
            "ZR_VALUE_IS_TYPE_BOOL(destinationValue->type)",
            "*outJumpIfFalse = !destinationValue->value.nativeObject.nativeBool",
    };
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_direct_iter_init(FILE *file,",
            "backend_aot_write_c_direct_iter_move_next(FILE *file,",
            "backend_aot_write_c_direct_iter_current(FILE *file,",
            "backend_aot_write_c_direct_iter_move_next_jump_if_false(FILE *file,",
    };
    static const char *const loweringNeedles[] = {
            "backend_aot_write_c_direct_iter_init(",
            "backend_aot_write_c_direct_iter_move_next(",
            "backend_aot_write_c_direct_iter_current(",
            "backend_aot_write_c_direct_iter_move_next_jump_if_false(",
            "zr_aot_value_exec_iter_init",
            "zr_aot_value_exec_iter_move_next",
            "zr_aot_value_exec_iter_current",
            "zr_aot_value_exec_iter_move_next_jump_if_false",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterInit(state, &frame, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterMoveNext(state, &frame, %u, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterCurrent(state, &frame, %u, %u));",
            "TZrBool zr_aot_branch_taken = ZR_FALSE;",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, %u, %u, &zr_aot_branch_taken));",
            "if (zr_aot_branch_taken) {",
            "goto zr_aot_fn_%u_ins_%u;",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ITER_INIT):",
            "case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):",
            "backend_aot_write_c_direct_iter_init(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):",
            "case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):",
            "backend_aot_write_c_direct_iter_move_next(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(ITER_CURRENT):",
            "backend_aot_write_c_direct_iter_current(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):",
            "backend_aot_write_c_direct_iter_move_next_jump_if_false(",
    };
    static const char *const forbiddenLoweringNeedles[] = {
            "backend_aot_c_write_iterator_unsupported(",
            "backend_aot_c_write_iterator_slot_header(",
            "SZrTypeValue *zr_aot_",
            "SZrTypeValue zr_aot_stable_",
            "ZrCore_Stack_GetValue(frame.slotBase",
            "frame.slotBase + %u",
            "ZrCore_Object_TryIterMoveNextCachedArrayFast(",
            "ZrCore_Object_TryIterCurrentCachedMemberFastStackResult(",
            "ZrCore_Object_IterInit(",
            "ZrCore_Object_IterMoveNext(",
            "ZrCore_Object_IterCurrent(",
            "unsupported AOT iterator core path",
            "unsupported AOT iterator branch result",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_IterInit",
            "ZrLibrary_AotRuntime_IterMoveNext",
            "ZrLibrary_AotRuntime_IterCurrent",
            "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse",
            "ZrLibrary_AotRuntime_IsTruthy",
    };
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_iterators.c");
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
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

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
    RUN_TEST(test_aot_c_source_lowers_iterator_ops_to_boundary_helpers);
    return UNITY_END();
}
