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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_return_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_return_contracts.c");
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

static void test_aot_c_source_lowers_export_return_to_direct_publication_then_direct_return(void) {
    static const char *const emitterHeaderNeedles[] = {
            "backend_aot_write_c_publish_exports(FILE *file);",
            "backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot);",
            "backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports);",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "struct SZrObjectModule *module;",
            "TZrBool *moduleExecuted;",
            "struct SZrFunction **functionTable;",
            "TZrUInt32 functionCount;",
            "const FZrAotEntryThunk *functionThunks;",
            "TZrUInt32 functionThunkCount;",
            "ZrLibrary_AotRuntime_Return(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "frame->module = record->module;",
            "frame->moduleExecuted = &record->moduleExecuted;",
            "frame->functionTable = record->functionTable;",
            "frame->functionCount = record->functionCount;",
            "frame->functionThunks = record->descriptor != ZR_NULL ? record->descriptor->functionThunks : ZR_NULL;",
            "frame->functionThunkCount = record->descriptor != ZR_NULL ? record->descriptor->functionThunkCount : 0;",
    };
    static const char *const exportNeedles[] = {
            "void backend_aot_write_c_publish_exports(FILE *file)",
            "/* zr_aot_publish_exports_direct */",
            "if (frame.module == ZR_NULL || frame.moduleExecuted == ZR_NULL) {",
            "if (!*frame.moduleExecuted) {",
            "TZrStackValuePointer zr_aot_exported_values_top = frame.slotBase + frame.function->stackSize;",
            "const SZrFunctionExportedVariable *zr_aot_export = &frame.function->exportedVariables[zr_aot_export_index];",
            "SZrFunction *zr_aot_export_metadata_function;",
            "zr_aot_export_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_export_value);",
            "if (zr_aot_export_metadata_function == ZR_NULL) {",
            "ZrCore_Value_Copy(state, &zr_aot_published_value, zr_aot_export_value);",
            "if (zr_aot_export_metadata_function->closureValueLength == 0) {",
            "for (TZrUInt32 zr_aot_candidate_index = 0; zr_aot_candidate_index < frame.functionCount &&",
            "frame.functionTable[zr_aot_candidate_index] == zr_aot_export_metadata_function",
            "SZrClosureNative *zr_aot_export_closure = ZrCore_ClosureNative_New(state, 0);",
            "zr_aot_export_closure->nativeFunction = (FZrNativeFunction)frame.functionThunks[zr_aot_export_function_index];",
            "zr_aot_export_closure->aotShimFunction = zr_aot_export_metadata_function;",
            "zr_aot_export_capture_owners = ZrCore_ClosureNative_GetCaptureOwners(zr_aot_export_closure);",
            "const SZrFunctionClosureVariable *zr_aot_closure_variable = &zr_aot_export_metadata_function->closureValueList[zr_aot_capture_index];",
            "ZrCore_Closure_FindOrCreateValue(state, frame.slotBase + zr_aot_closure_variable->index);",
            "ZrCore_ClosureNative_GetCaptureValue(zr_aot_source_native_closure,",
            "ZrCore_ClosureNative_GetCaptureOwner(zr_aot_source_native_closure,",
            "ZR_CAST_VM_CLOSURE(state, zr_aot_export_value->value.object);",
            "zr_aot_export_closure->closureValuesExtend[zr_aot_capture_index] = zr_aot_capture_value;",
            "ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_export_closure), zr_aot_capture_owner);",
            "ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_export_closure), zr_aot_capture_value);",
            "if (zr_aot_export_capture_count > 0 && !zr_aot_export_captures_bound) {",
            "unsupported AOT module export closure capture materialization",
            "ZrCore_Value_InitAsRawObject(state, &zr_aot_published_value, ZR_CAST_RAW_OBJECT_AS_SUPER(zr_aot_export_closure));",
            "zr_aot_published_value.type = ZR_VALUE_TYPE_CLOSURE;",
            "zr_aot_published_value.isGarbageCollectable = ZR_TRUE;",
            "zr_aot_published_value.isNative = ZR_TRUE;",
            "ZrCore_Module_AddPubExport(state, frame.module, zr_aot_export->name, &zr_aot_published_value);",
            "ZrCore_Module_AddProExport(state, frame.module, zr_aot_export->name, &zr_aot_published_value);",
            "*frame.moduleExecuted = ZR_TRUE;",
    };
    static const char *const controlNeedles[] = {
            "void backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports)",
            "if (publishExports) {",
            "backend_aot_write_c_publish_exports(file);",
            "backend_aot_write_c_direct_return(file, sourceSlot);",
            "/* zr_aot_direct_return */",
    };
    static const char *const functionBodyNeedles[] = {
            "if (publishExports) {\n                    backend_aot_write_c_publish_exports(file);\n                }\n                backend_aot_write_c_direct_return(file, operandA1);",
    };
    static const char *const forbiddenEmitterNeedles[] = {
            "/* zr_aot_publish_exports_boundary */",
            "ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)",
            "ZrLibrary_AotRuntime_Return(state, &frame",
    };
    static const char *const forbiddenControlNeedles[] = {
            "void backend_aot_write_c_publish_exports(FILE *file)",
            "ZrLibrary_AotRuntime_MaterializeModuleExportValue(state, &frame, zr_aot_export_value, &zr_aot_published_value)",
    };
    static const char *const forbiddenExportNeedles[] = {
            "ZrLibrary_AotRuntime_MaterializeModuleExportValue(state, &frame, zr_aot_export_value, &zr_aot_published_value)",
    };
    static const char *const forbiddenRuntimeNeedles[] = {
            "ZrLibrary_AotRuntime_MaterializeModuleExportValue",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *controlText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *exportText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_exports.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(exportText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, emitterHeaderNeedles, ARRAY_COUNT(emitterHeaderNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_none(runtimeHeaderText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(runtimeSourceText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_all(exportText, exportNeedles, ARRAY_COUNT(exportNeedles));
    assert_text_contains_all(controlText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(controlText, forbiddenControlNeedles, ARRAY_COUNT(forbiddenControlNeedles));
    assert_text_contains_none(controlText, forbiddenEmitterNeedles, ARRAY_COUNT(forbiddenEmitterNeedles));
    assert_text_contains_none(functionBodyText, forbiddenEmitterNeedles, ARRAY_COUNT(forbiddenEmitterNeedles));
    assert_text_contains_none(exportText, forbiddenEmitterNeedles, ARRAY_COUNT(forbiddenEmitterNeedles));
    assert_text_contains_none(exportText, forbiddenExportNeedles, ARRAY_COUNT(forbiddenExportNeedles));

    free(emitterHeaderText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(controlText);
    free(exportText);
    free(functionBodyText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_export_return_to_direct_publication_then_direct_return);
    return UNITY_END();
}
