#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

void setUp(void) {}

void tearDown(void) {}

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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_control_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_control_contracts.c");
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

static void test_aot_c_source_lowers_exception_control_to_direct_core_calls(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_try(FILE *file,",
            "backend_aot_write_c_end_try(FILE *file,",
            "backend_aot_write_c_throw(FILE *file,",
            "backend_aot_write_c_catch(FILE *file,",
            "backend_aot_write_c_end_finally(FILE *file,",
    };
    static const char *const controlNeedles[] = {
            "/* zr_aot_try_direct */",
            "execution_push_exception_handler(state, zr_aot_call_info, %u)",
            "/* zr_aot_end_try_direct */",
            "execution_find_handler_state(state, zr_aot_call_info, %u)",
            "handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;",
            "execution_pop_exception_handler(state, handlerState);",
            "/* zr_aot_throw_direct */",
            "ZrCore_Exception_NormalizeThrownValue(state,",
            "execution_unwind_exception_to_handler(state, &zr_aot_call_info)",
            "ZrCore_Exception_Throw(state, state->currentExceptionStatus);",
            "/* zr_aot_catch_direct */",
            "ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException);",
            "ZrCore_Exception_ClearCurrent(state);",
            "ZrCore_Value_ResetAsNull(zr_aot_destination);",
            "/* zr_aot_end_finally_direct */",
            "switch (state->pendingControl.kind)",
            "case ZR_VM_PENDING_CONTROL_EXCEPTION:",
            "case ZR_VM_PENDING_CONTROL_RETURN:",
            "case ZR_VM_PENDING_CONTROL_BREAK:",
            "case ZR_VM_PENDING_CONTROL_CONTINUE:",
            "ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);",
            "execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)",
            "execution_jump_to_instruction_offset(state,",
            "ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)",
            "execution_clear_pending_control(state);",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TRY):",
            "backend_aot_write_c_try(file, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(END_TRY):",
            "backend_aot_write_c_end_try(file, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(THROW):",
            "backend_aot_write_c_throw(file, entry->flatIndex, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(CATCH):",
            "backend_aot_write_c_catch(file, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(END_FINALLY):",
            "backend_aot_write_c_end_finally(file, entry->flatIndex, destinationSlot);",
    };
    static const char *const forbiddenNeedles[] = {
            "ZrLibrary_AotRuntime_Try(state, &frame",
            "ZrLibrary_AotRuntime_EndTry(state, &frame",
            "ZrLibrary_AotRuntime_Throw",
            "ZrLibrary_AotRuntime_Catch(state, &frame",
            "ZrLibrary_AotRuntime_EndFinally",
            "backend_aot_write_c_control_transfer_call",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *controlText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(controlText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(controlText, forbiddenNeedles, ARRAY_COUNT(forbiddenNeedles));
    assert_text_contains_none(functionBodyText, forbiddenNeedles, ARRAY_COUNT(forbiddenNeedles));

    free(emitterHeaderText);
    free(controlText);
    free(functionBodyText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_exception_control_to_direct_core_calls);
    return UNITY_END();
}
