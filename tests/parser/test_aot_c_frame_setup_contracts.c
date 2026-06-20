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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_frame_setup_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_frame_setup_contracts.c");
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

static void test_aot_c_source_emits_direct_generated_frame_setup(void) {
    static const char *const runtimeHeaderNeedles[] = {
            "typedef struct ZrAotGeneratedModuleContext",
            "struct SZrFunction *metadataFunction;",
            "TZrUInt32 generatedFrameSlotCount;",
            "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "TZrBool ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(SZrState *state,",
            "context->recordHandle = record;",
            "context->metadataFunction = metadataFunction;",
            "context->module = record->module;",
            "context->generatedFrameSlotCount = generatedFrameSlotCount;",
            "runtimeState->executedVia = aot_runtime_backend_to_executed_via(record->backendKind);",
    };
    static const char *const frameSetupHeaderNeedles[] = {
            "backend_aot_write_c_frame_setup(FILE *file,",
            "const SZrAotExecIrFrameLayout *frameLayout",
            "TZrUInt32 functionIndex",
    };
    static const char *const frameSetupSourceNeedles[] = {
            "#include \"backend_aot_c_frame_setup.h\"",
            "/* zr_aot_generated_frame_setup */",
            "ZrAotGeneratedModuleContext zr_aot_context;",
            "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, %u, &zr_aot_context)",
            "SZrCallInfo *zr_aot_call_info = state->callInfoList;",
            "SZrFunctionStackAnchor zr_aot_base_anchor;",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_function_base, &zr_aot_base_anchor);",
            "ZrCore_Function_CheckStackAndGc(state, zr_aot_frame_slot_count, zr_aot_function_base + 1);",
            "zr_aot_argument_count =",
            "TZrSize zr_aot_frame_byte_slot_count = 0;",
            "zr_aot_frame_byte_size = (TZrSize)%u;",
            "(zr_aot_frame_byte_size + sizeof(SZrTypeValue) - 1u) / sizeof(SZrTypeValue)",
            "if (zr_aot_frame_slot_count < zr_aot_frame_byte_slot_count)",
            "ZrCore_Value_ResetAsNull(&zr_aot_slot_base[zr_aot_slot].value);",
            "zr_aot_call_info->functionTop.valuePointer = zr_aot_frame_top;",
            "state->stackTop.valuePointer = zr_aot_frame_top;",
            "ZrCore_Debug_RunError(state,",
            "generated AOT function has no call frame",
            "frame.recordHandle = zr_aot_context.recordHandle;",
            "frame.function = zr_aot_context.metadataFunction;",
            "frame.generatedFrameSlotCount = zr_aot_context.generatedFrameSlotCount;",
            "frame.currentInstructionIndex = 0;",
            "frame.lastObservedInstructionIndex = UINT32_MAX;",
            "frame.observationMask = state->hasAotObservationPolicyOverride",
            "state->aotObservationMask",
            "ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW",
            "ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW",
            "ZR_AOT_GENERATED_STEP_FLAG_CALL",
            "ZR_AOT_GENERATED_STEP_FLAG_RETURN",
            "frame.publishAllInstructions = state->hasAotObservationPolicyOverride",
            "state->aotPublishAllInstructions",
    };
    static const char *const functionBodyNeedles[] = {
            "#include \"backend_aot_c_frame_setup.h\"",
            "backend_aot_write_c_frame_setup(file,",
            "functionIr != ZR_NULL ? &functionIr->frameLayout : ZR_NULL",
            "entry->flatIndex);",
            "zr_aot_frame_started = ZR_TRUE;",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_BeginGeneratedFunction(state, %u, &frame)",
    };
    static const char *const forbiddenRuntimeNeedles[] = {
            "ZrLibrary_AotRuntime_ReportGeneratedContextError",
            "ZrLibrary_AotRuntime_GetGeneratedContext",
            "ZrAotGeneratedContext",
    };
    static const char *const forbiddenFrameSetupNeedles[] = {
            "ZrLibrary_AotRuntime_BeginGeneratedFunction",
            "ZrLibrary_AotRuntime_ReportGeneratedContextError",
            "ZrLibrary_AotRuntime_GetGeneratedContext",
            "ZrAotGeneratedContext",
            "ZrLibrary_AotRuntime_GetObservationPolicy",
            "ZrLibrary_AotRuntime_DefaultObservationMask()",
    };
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *frameSetupHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.h");
    char *frameSetupSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(frameSetupHeaderText);
    TEST_ASSERT_NOT_NULL(frameSetupSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(frameSetupHeaderText, frameSetupHeaderNeedles, ARRAY_COUNT(frameSetupHeaderNeedles));
    assert_text_contains_all(frameSetupSourceText, frameSetupSourceNeedles, ARRAY_COUNT(frameSetupSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(runtimeHeaderText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(runtimeSourceText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(frameSetupSourceText, forbiddenFrameSetupNeedles, ARRAY_COUNT(forbiddenFrameSetupNeedles));

    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(frameSetupHeaderText);
    free(frameSetupSourceText);
    free(functionBodyText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_emits_direct_generated_frame_setup);
    return UNITY_END();
}
