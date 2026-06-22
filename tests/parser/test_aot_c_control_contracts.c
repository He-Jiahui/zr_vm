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
            "ZrLibrary_AotRuntime_Try(state, &frame, %u)",
            "/* zr_aot_end_try_direct */",
            "ZrLibrary_AotRuntime_EndTry(state, &frame, %u)",
            "/* zr_aot_throw_direct */",
            "ZrLibrary_AotRuntime_Throw(state, &frame, %u, &zr_aot_next_instruction)",
            "if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH)",
            "/* zr_aot_catch_direct */",
            "ZrLibrary_AotRuntime_Catch(state, &frame, %u)",
            "/* zr_aot_end_finally_direct */",
            "ZrLibrary_AotRuntime_EndFinally(state, &frame, %u, &zr_aot_next_instruction)",
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
            "backend_aot_write_c_control_transfer_call",
            "execution_push_exception_handler(state, zr_aot_call_info",
            "generated AOT TRY failed to push exception handler",
            "generated AOT END_TRY has invalid handler index",
            "handlerInfo = &frame.function->exceptionHandlerList[%u];",
            "handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;",
            "SZrCallInfo *resumeCallInfo;",
            "SZrVmExceptionHandlerState *handlerState;",
            "TZrStackValuePointer targetSlot;",
            "switch (state->pendingControl.kind)",
            "generated AOT END_FINALLY is missing call frame",
            "ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);",
            "ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)",
            "SZrTypeValue *zr_aot_destination;",
            "generated AOT CATCH has invalid destination slot",
            "ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException);",
            "ZrCore_Exception_ClearCurrent(state);",
            "ZrCore_Value_ResetAsNull(zr_aot_destination);",
            "SZrTypeValue *zr_aot_source_value;",
            "SZrTypeValue zr_aot_payload;",
            "ZrCore_Exception_NormalizeThrownValue(state,",
            "generated AOT THROW has invalid payload slot",
            "generated AOT THROW has missing payload value",
            "generated AOT THROW failed to normalize exception",
    };
    static const char *const runtimeNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_Throw(SZrState *state,",
            "TZrBool ZrLibrary_AotRuntime_EndFinally(SZrState *state,",
            "if (!execution_unwind_exception_to_handler(state, &resumeCallInfo))",
            "ZrCore_Exception_Throw(state, state->currentExceptionStatus);",
    };
    static const char *const runtimeForbiddenNeedles[] = {
            "return aot_runtime_resume_exception_in_current_frame(state, frame, outResumeInstructionIndex);",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *controlText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *runtimeText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(runtimeText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(controlText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(runtimeText, runtimeNeedles, ARRAY_COUNT(runtimeNeedles));
    assert_text_contains_none(controlText, forbiddenNeedles, ARRAY_COUNT(forbiddenNeedles));
    assert_text_contains_none(functionBodyText, forbiddenNeedles, ARRAY_COUNT(forbiddenNeedles));
    assert_text_contains_none(runtimeText, runtimeForbiddenNeedles, ARRAY_COUNT(runtimeForbiddenNeedles));

    free(emitterHeaderText);
    free(controlText);
    free(functionBodyText);
    free(runtimeText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_exception_control_to_direct_core_calls);
    return UNITY_END();
}
