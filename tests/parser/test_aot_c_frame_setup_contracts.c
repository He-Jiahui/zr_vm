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
    static const char *const abiHeaderNeedles[] = {
            "struct SZrFunction;",
            "struct SZrAotGcRootMap;",
            "typedef struct SZrAotSignatureType {",
            "TZrUInt16 baseType;",
            "TZrUInt16 staticCType;",
            "TZrUInt32 staticCTypeId;",
            "TZrUInt32 ownershipQualifier;",
            "TZrUInt16 elementBaseType;",
            "TZrUInt8 isNullable;",
            "TZrUInt8 isArray;",
            "} SZrAotSignatureType;",
            "typedef struct SZrAotSignature {",
            "TZrUInt32 parameterCount;",
            "const SZrAotSignatureType *returnType;",
            "const SZrAotSignatureType *parameterTypes;",
            "TZrUInt8 hasReturnValue;",
            "TZrUInt8 hasVarArgs;",
            "} SZrAotSignature;",
            "typedef struct SZrAotMethodInfo {",
            "TZrUInt32 functionIndex;",
            "const struct SZrFunction *metadataFunction;",
            "TZrUInt32 registerFrameBytes;",
            "const struct SZrAotGcRootMap *gcRootMap;",
            "const SZrAotSignature *signature;",
            "TZrUInt8 observationPolicy;",
            "} SZrAotMethodInfo;",
            "const SZrAotMethodInfo *const *methodInfos;",
            "TZrUInt32 methodInfoCount;",
    };
    static const char *const runtimeSourceNeedles[] = {
            "static void aot_runtime_mark_record_executed(",
            "aot_runtime_mark_record_executed(runtimeState, record);",
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
            "TZrBool includeExportContext",
            "TZrBool includeFrameDescriptor",
    };
    static const char *const frameSetupSourceNeedles[] = {
            "#include \"backend_aot_c_frame_setup.h\"",
            "backend_aot_c_frame_setup_register_frame_bytes(",
            "ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT",
            "TZrBool includeStackFrameSetup",
            "includeStackFrameSetup = (TZrBool)(includeFrameDescriptor || frameByteSize > 0u);",
            "if (!includeStackFrameSetup) {\n        return;\n    }\n\n    fprintf(file,",
            "/* zr_aot_generated_frame_setup */",
            "ZrAotGeneratedModuleContext zr_aot_context;",
            "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, %u, &zr_aot_context)",
            "SZrCallInfo *zr_aot_call_info = state->callInfoList;",
            "SZrFunctionStackAnchor zr_aot_base_anchor;",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_function_base, &zr_aot_base_anchor);",
            "ZrCore_Function_CheckStackAndGc(state, zr_aot_frame_slot_count, zr_aot_function_base + 1);",
            "zr_aot_argument_count =",
            "TZrSize zr_aot_frame_byte_slot_count =",
            "zr_aot_frame_byte_size = (TZrSize)%uu;",
            "(zr_aot_frame_byte_size + sizeof(SZrTypeValue) - 1u) / sizeof(SZrTypeValue)",
            "if (zr_aot_frame_slot_count < zr_aot_frame_byte_slot_count)",
            "ZrCore_Value_ResetAsNull(&zr_aot_slot_base[zr_aot_slot].value);",
            "zr_aot_call_info->functionTop.valuePointer = zr_aot_frame_top;",
            "state->stackTop.valuePointer = zr_aot_frame_top;",
            "ZrCore_Debug_RunError(state,",
            "generated AOT function has no call frame",
            "frame.function = zr_aot_context.metadataFunction;",
            "if (includeFrameDescriptor) {",
            "if (!includeStackFrameSetup) {",
            "if (frameByteSize > 0u) {",
            "if (includeExportContext) {",
            "frame.module = zr_aot_context.module;",
            "frame.moduleExecuted = zr_aot_context.moduleExecuted;",
            "frame.functionTable = zr_aot_context.functionTable;",
            "frame.functionCount = zr_aot_context.functionCount;",
            "frame.functionThunks = zr_aot_context.functionThunks;",
            "frame.functionThunkCount = zr_aot_context.functionThunkCount;",
            "frame.generatedFrameSlotCount = zr_aot_context.generatedFrameSlotCount;",
    };
    static const char *const functionBodyNeedles[] = {
            "#include \"backend_aot_c_frame_setup.h\"",
            "backend_aot_write_c_frame_setup(file,",
            "functionIr != ZR_NULL ? &functionIr->frameLayout : ZR_NULL",
            "entry->flatIndex,",
            "publishExports,",
            "includeFrameDescriptor);",
            "TZrBool includeFrameDescriptor",
            "backend_aot_c_function_body_needs_frame_descriptor(",
            "TZrBool needsFrameCleanup",
            "TZrBool needsSkipDropSlot",
            "needsSkipDropSlot = needsFrameCleanup;",
            "backend_aot_c_frame_cleanup_would_emit(",
            "if (needsFrameCleanup) {",
            "if (needsSkipDropSlot) {",
    };
    static const char *const emitterSourceNeedles[] = {
            "backend_aot_write_c_signature_type(FILE *file,",
            "const SZrFunctionTypedTypeRef *typeRef",
            "backend_aot_write_c_signature(FILE *file,",
            "static const SZrAotSignatureType zr_aot_signature_%u_types[] = {",
            "static const SZrAotSignature zr_aot_signature_%u = {",
            "backend_aot_write_c_method_infos(FILE *file,",
            "backend_aot_c_method_info_register_frame_bytes(",
            "ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT",
            "const SZrAotExecIrFunction *functionIr =",
            "backend_aot_exec_ir_find_function(module, entry->flatIndex);",
            "backend_aot_write_c_signature(file, entry->flatIndex, entry->function);",
            "static const SZrAotMethodInfo zr_aot_method_info_%u = {",
            ".functionIndex = %uu,",
            ".metadataFunction = ZR_NULL,",
            ".registerFrameBytes = %uu,",
            ".gcRootMap = ZR_NULL,",
            ".signature = &zr_aot_signature_%u,",
            ".observationPolicy = 0u,",
            "backend_aot_write_c_method_info_table(FILE *file,",
            "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {",
            "&zr_aot_method_info_%u,",
            "backend_aot_write_c_method_info_table(file, &functionTable);",
            "zr_aot_method_infos,",
            "functionTable.count",
            "backend_aot_write_c_method_infos(file, &functionTable, &module);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_BeginGeneratedFunction(state, %u, &frame)",
    };
    static const char *const forbiddenRuntimeNeedles[] = {
            "ZrLibrary_AotRuntime_ReportGeneratedContextError",
            "ZrLibrary_AotRuntime_GetGeneratedContext",
            "ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted",
            "ZrAotGeneratedContext",
    };
    static const char *const forbiddenFrameSetupNeedles[] = {
            "ZrLibrary_AotRuntime_BeginGeneratedFunction",
            "ZrLibrary_AotRuntime_ReportGeneratedContextError",
            "ZrLibrary_AotRuntime_GetGeneratedContext",
            "ZrAotGeneratedContext",
            "ZrLibrary_AotRuntime_GetObservationPolicy",
            "ZrLibrary_AotRuntime_DefaultObservationMask()",
            "frame.recordHandle = zr_aot_context.recordHandle;",
            "frame.functionIndex = zr_aot_context.resolvedFunctionIndex;",
            "frame.currentInstructionIndex = 0;",
            "frame.lastObservedInstructionIndex = UINT32_MAX;",
            "frame.lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;",
            "frame.observationMask = state->hasAotObservationPolicyOverride",
            "state->aotObservationMask",
            "ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW",
            "ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW",
            "ZR_AOT_GENERATED_STEP_FLAG_CALL",
            "ZR_AOT_GENERATED_STEP_FLAG_RETURN",
            "frame.publishAllInstructions = state->hasAotObservationPolicyOverride",
            "state->aotPublishAllInstructions",
            "state->debugHookSignal",
            "ZR_DEBUG_HOOK_MASK_LINE",
    };
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *abiHeaderText = read_repo_text_file_owned("zr_vm_common/include/zr_vm_common/zr_aot_abi.h");
    char *frameSetupHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.h");
    char *frameSetupSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *emitterSourceText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");

    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(abiHeaderText);
    TEST_ASSERT_NOT_NULL(frameSetupHeaderText);
    TEST_ASSERT_NOT_NULL(frameSetupSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(emitterSourceText);

    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(abiHeaderText, abiHeaderNeedles, ARRAY_COUNT(abiHeaderNeedles));
    assert_text_contains_all(frameSetupHeaderText, frameSetupHeaderNeedles, ARRAY_COUNT(frameSetupHeaderNeedles));
    assert_text_contains_all(frameSetupSourceText, frameSetupSourceNeedles, ARRAY_COUNT(frameSetupSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(emitterSourceText, emitterSourceNeedles, ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_none(runtimeHeaderText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(runtimeSourceText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(frameSetupSourceText, forbiddenFrameSetupNeedles, ARRAY_COUNT(forbiddenFrameSetupNeedles));

    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(abiHeaderText);
    free(frameSetupHeaderText);
    free(frameSetupSourceText);
    free(functionBodyText);
    free(emitterSourceText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_emits_direct_generated_frame_setup);
    return UNITY_END();
}
