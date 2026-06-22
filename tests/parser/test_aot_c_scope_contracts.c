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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_scope_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_scope_contracts.c");
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

static void test_aot_c_source_lowers_scope_lifecycle_to_boundary_helpers(void) {
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_MarkToBeClosed(",
            "ZrLibrary_AotRuntime_CloseScope(",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_MarkToBeClosed(",
            "aot_runtime_frame_slot(frame, slotIndex)",
            "ZrCore_Closure_ToBeClosedValueClosureNew(state, slotPointer);",
            "ZrLibrary_AotRuntime_CloseScope(",
            "aot_runtime_close_scope_registrations(state, cleanupCount);",
    };
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_direct_mark_to_be_closed(FILE *file,",
            "backend_aot_write_c_direct_close_scope(FILE *file,",
    };
    static const char *const loweringNeedles[] = {
            "backend_aot_write_c_direct_mark_to_be_closed(",
            "backend_aot_write_c_direct_close_scope(",
            "zr_aot_scope_mark_to_be_closed",
            "zr_aot_scope_close_scope",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, %u));",
            "ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CloseScope(state, &frame, %u));",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):",
            "backend_aot_write_c_direct_mark_to_be_closed(file, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):",
            "backend_aot_write_c_direct_close_scope(file, destinationSlot);",
    };
    static const char *const forbiddenNeedles[] = {
            "TZrStackValuePointer zr_aot_slot_pointer",
            "ZrCore_Closure_ToBeClosedValueClosureNew(state, zr_aot_slot_pointer);",
            "TZrMemoryOffset zr_aot_saved_stack_top_offset",
            "ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer)",
            "ZrCore_Closure_CloseStackValue(state, zr_aot_to_be_closed.valuePointer);",
            "ZrCore_Closure_CloseRegisteredValues(state, 1, ZR_THREAD_STATUS_INVALID, ZR_FALSE);",
            "ZrCore_Stack_LoadOffsetToPointer(state, zr_aot_saved_stack_top_offset)",
    };
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_scope.c");
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
    assert_text_contains_none(loweringText, forbiddenNeedles, ARRAY_COUNT(forbiddenNeedles));

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
    RUN_TEST(test_aot_c_source_lowers_scope_lifecycle_to_boundary_helpers);
    return UNITY_END();
}
