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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_value_semir_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_value_semir_contracts.c");
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

    TEST_ASSERT_NOT_NULL(text);
    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void assert_text_contains_none(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    TEST_ASSERT_NOT_NULL(text);
    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) != NULL) {
            printf("Unexpected source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("found forbidden source contract text");
        }
    }
}

static void test_aot_c_value_semir_field_lowering_lives_in_focused_module(void) {
    static const char *const fieldHeaderNeedles[] = {
            "backend_aot_write_c_value_semir_field_addr(",
            "backend_aot_write_c_value_semir_load(",
            "backend_aot_write_c_value_semir_store(",
            "backend_aot_try_write_c_value_semir_field_load_exec(",
            "backend_aot_try_write_c_value_semir_field_store_exec(",
    };
    static const char *const fieldSourceNeedles[] = {
            "#include \"backend_aot_c_value_semir_fields.h\"",
            "backend_aot_c_value_field_resolve_layout(",
            "backend_aot_c_value_field_layout_can_primitive_exec(",
            "backend_aot_c_value_field_layout_can_value_slot_exec(",
            "backend_aot_c_value_field_layout_can_inline_struct_exec(",
            "zr_aot_value_exec_field_load",
            "zr_aot_value_exec_field_store",
            "zr_aot_value_exec_field_value_slot_load",
            "zr_aot_value_exec_field_value_slot_store",
            "zr_aot_dense_destination",
            "zr_aot_dense_source",
            "ZrCore_Value_Copy(state, zr_aot_dense_destination, zr_aot_destination);",
            "if (!ZR_VALUE_IS_TYPE_NULL(zr_aot_dense_source->type))",
            "zr_aot_value_exec_field_inline_struct_load",
            "zr_aot_value_exec_field_inline_struct_store",
            "zr_aot_value_unsupported_field_load",
            "zr_aot_value_unsupported_field_store",
            "unsupported AOT value SemIR field load",
            "unsupported AOT value SemIR field store",
    };
    static const char *const orchestratorNeedles[] = {
            "#include \"backend_aot_c_value_semir_fields.h\"",
            "backend_aot_write_c_value_semir_field_addr(file, state, functionIr, frameLayout, instruction);",
            "backend_aot_write_c_value_semir_load(file, state, functionIr, frameLayout, instruction);",
            "backend_aot_write_c_value_semir_store(file, state, functionIr, frameLayout, instruction);",
            "backend_aot_try_write_c_value_semir_field_load_exec(",
            "backend_aot_try_write_c_value_semir_field_store_exec(",
            "backend_aot_try_write_c_value_copy_exec(",
    };
    static const char *const orchestratorForbiddenNeedles[] = {
            "static void backend_aot_write_c_value_field_addr(",
            "static void backend_aot_write_c_value_load(",
            "static void backend_aot_write_c_value_store(",
            "static TZrBool backend_aot_write_c_value_unsupported_field_load_exec(",
            "static TZrBool backend_aot_write_c_value_unsupported_field_store_exec(",
            "static TZrBool backend_aot_try_write_c_value_field_inline_struct_load_exec(",
            "static TZrBool backend_aot_try_write_c_value_field_inline_struct_store_exec(",
            "static TZrBool backend_aot_try_write_c_value_field_value_slot_load_exec(",
            "static TZrBool backend_aot_try_write_c_value_field_value_slot_store_exec(",
            "static TZrBool backend_aot_try_write_c_value_field_load_exec(",
            "static TZrBool backend_aot_try_write_c_value_field_store_exec(",
    };
    char *fieldHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.h");
    char *fieldSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c");
    char *orchestratorText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c");

    TEST_ASSERT_NOT_NULL(fieldHeaderText);
    TEST_ASSERT_NOT_NULL(fieldSourceText);
    TEST_ASSERT_NOT_NULL(orchestratorText);

    assert_text_contains_all(fieldHeaderText, fieldHeaderNeedles, ARRAY_COUNT(fieldHeaderNeedles));
    assert_text_contains_all(fieldSourceText, fieldSourceNeedles, ARRAY_COUNT(fieldSourceNeedles));
    assert_text_contains_all(orchestratorText, orchestratorNeedles, ARRAY_COUNT(orchestratorNeedles));
    assert_text_contains_none(orchestratorText,
                              orchestratorForbiddenNeedles,
                              ARRAY_COUNT(orchestratorForbiddenNeedles));

    free(fieldHeaderText);
    free(fieldSourceText);
    free(orchestratorText);
}

static void test_aot_c_value_semir_inline_struct_field_transfer_uses_layout_copy_for_non_pod(void) {
    static const char *const fieldSourceNeedles[] = {
            "const SZrTypeLayout *zr_aot_field_layout =",
            "ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function",
            "fieldLayout->typeLayoutId",
            "zr_aot_field_layout->byteSize != %u",
            "ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)",
            "zr_aot_value_exec_field_inline_struct_copy",
            "ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(frame.function",
            "ZrCore_TypeLayout_CopyInlineWithRegistry(state,",
    };
    static const char *const fieldSourceForbiddenNeedles[] = {
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function",
            "ZrCore_TypeLayout_CopyInline(state,",
    };
    char *fieldSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c");

    TEST_ASSERT_NOT_NULL(fieldSourceText);
    assert_text_contains_all(fieldSourceText, fieldSourceNeedles, ARRAY_COUNT(fieldSourceNeedles));
    assert_text_contains_none(fieldSourceText, fieldSourceForbiddenNeedles, ARRAY_COUNT(fieldSourceForbiddenNeedles));

    free(fieldSourceText);
}

static void test_aot_c_value_semir_field_resolver_uses_stable_member_entries(void) {
    static const char *const fieldSourceNeedles[] = {
            "backend_aot_c_value_field_resolve_stable_member_name(",
            "backend_aot_c_value_field_resolve_cache_member_name(",
            "SemIR field operands are stable member-entry indexes",
            "memberEntryIndex >= function->memberEntryLength",
    };
    char *fieldSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c");

    TEST_ASSERT_NOT_NULL(fieldSourceText);
    assert_text_contains_all(fieldSourceText, fieldSourceNeedles, ARRAY_COUNT(fieldSourceNeedles));

    free(fieldSourceText);
}

static void test_aot_c_type_layout_descriptor_preserves_canonical_identity_and_maps(void) {
    static const char *const typeLayoutNeedles[] = {
            "backend_aot_c_type_layout_write_ref_offsets(",
            "typeLayout->layoutVersion",
            "typeLayout->layoutHash",
            "typeLayout->gcScanKind",
            "typeLayout->refFieldCount",
            ".refFieldOffsets = ZrRefOffsets_",
            ".customDrop = ZR_NULL",
            ".customDropUserData = ZR_NULL",
    };
    static const char *const forbiddenNeedles[] = {
            ".reserved0 =",
    };
    char *typeLayoutSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c");

    TEST_ASSERT_NOT_NULL(typeLayoutSourceText);
    assert_text_contains_all(typeLayoutSourceText,
                             typeLayoutNeedles,
                             ARRAY_COUNT(typeLayoutNeedles));
    assert_text_contains_none(typeLayoutSourceText,
                              forbiddenNeedles,
                              ARRAY_COUNT(forbiddenNeedles));
    free(typeLayoutSourceText);
}

static void test_aot_c_frame_alias_preserves_addressing_without_duplicate_lifecycle(void) {
    static const char *const execIrNeedles[] = {
            "destinationLayout->reserved0 = sourceLayout->reserved0;",
    };
    static const char *const cleanupNeedles[] = {
            "layout->reserved0 & ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS",
            "ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(frame.function",
            "ZrCore_TypeLayout_DropInlineWithRegistry(state,",
    };
    static const char *const methodMetadataNeedles[] = {
            "slotLayout->reserved0 & ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS",
    };
    static const char *const valueSemIrNeedles[] = {
            "ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(frame.function",
            "ZrCore_TypeLayout_CopyInlineWithRegistry(state,",
    };
    static const char *const lifecycleForbiddenNeedles[] = {
            "ZrCore_TypeLayout_CopyInline(state,",
            "ZrCore_TypeLayout_DropInline(state,",
    };
    char *execIrSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c");
    char *cleanupSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c");
    char *methodMetadataSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c");
    char *valueSemIrSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c");

    TEST_ASSERT_NOT_NULL(execIrSourceText);
    TEST_ASSERT_NOT_NULL(cleanupSourceText);
    TEST_ASSERT_NOT_NULL(methodMetadataSourceText);
    TEST_ASSERT_NOT_NULL(valueSemIrSourceText);
    assert_text_contains_all(execIrSourceText, execIrNeedles, ARRAY_COUNT(execIrNeedles));
    assert_text_contains_all(cleanupSourceText, cleanupNeedles, ARRAY_COUNT(cleanupNeedles));
    assert_text_contains_all(methodMetadataSourceText,
                             methodMetadataNeedles,
                             ARRAY_COUNT(methodMetadataNeedles));
    assert_text_contains_all(valueSemIrSourceText,
                             valueSemIrNeedles,
                             ARRAY_COUNT(valueSemIrNeedles));
    assert_text_contains_none(cleanupSourceText,
                              lifecycleForbiddenNeedles,
                              ARRAY_COUNT(lifecycleForbiddenNeedles));
    assert_text_contains_none(valueSemIrSourceText,
                              lifecycleForbiddenNeedles,
                              ARRAY_COUNT(lifecycleForbiddenNeedles));

    free(execIrSourceText);
    free(cleanupSourceText);
    free(methodMetadataSourceText);
    free(valueSemIrSourceText);
}

static void test_aot_c_indirect_alias_field_access_uses_canonical_frame_member_transfer(void) {
    static const char *const fieldSourceNeedles[] = {
            "ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS",
            "backend_aot_c_value_field_layout_is_indirect_alias(",
    };
    static const char *const coreHeaderNeedles[] = {
            "ZrCore_Function_GetFrameSlotInlineMember(",
            "ZrCore_Function_SetFrameSlotInlineMember(",
    };
    static const char *const runtimeNeedles[] = {
            "ZrCore_Function_GetFrameSlotInlineMember(",
            "ZrCore_Function_SetFrameSlotInlineMember(",
    };
    char *fieldSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c");
    char *coreHeaderText = read_repo_text_file_owned(
            "zr_vm_core/include/zr_vm_core/function.h");
    char *rootRuntimeText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *aotRuntimeText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c");

    TEST_ASSERT_NOT_NULL(fieldSourceText);
    TEST_ASSERT_NOT_NULL(coreHeaderText);
    TEST_ASSERT_NOT_NULL(rootRuntimeText);
    TEST_ASSERT_NOT_NULL(aotRuntimeText);
    assert_text_contains_all(fieldSourceText, fieldSourceNeedles, ARRAY_COUNT(fieldSourceNeedles));
    assert_text_contains_all(coreHeaderText, coreHeaderNeedles, ARRAY_COUNT(coreHeaderNeedles));
    assert_text_contains_all(rootRuntimeText, runtimeNeedles, ARRAY_COUNT(runtimeNeedles));
    assert_text_contains_all(aotRuntimeText, runtimeNeedles, ARRAY_COUNT(runtimeNeedles));

    free(fieldSourceText);
    free(coreHeaderText);
    free(rootRuntimeText);
    free(aotRuntimeText);
}

static void test_aot_c_value_semir_typed_call_return_lives_in_focused_module(void) {
    static const char *const callHeaderNeedles[] = {
            "backend_aot_write_c_value_semir_call_typed(",
            "backend_aot_write_c_value_semir_return_typed(",
            "backend_aot_try_write_c_value_semir_call_typed_exec(",
            "backend_aot_try_write_c_value_semir_return_typed_exec(",
    };
    static const char *const callSourceNeedles[] = {
            "#include \"backend_aot_c_value_semir_calls.h\"",
            "backend_aot_c_value_call_find_frame_slot_layout(",
            "backend_aot_c_value_call_layout_can_inline_struct(",
            "calleeFunctionIr->frameLayout",
            "parameterLayoutCount != argumentCount",
            "ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER",
            "argumentIndex != 0u",
            "!isReferenceParameter",
            "zr_aot_value_call_typed",
            "zr_aot_value_return_typed",
            "zr_aot_value_exec_call_typed",
            "zr_aot_value_exec_call_typed_metadata_guard",
            "ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)",
            "zr_aot_value_exec_return_typed",
            "ZrLibrary_AotRuntime_CallInlineStruct(state,",
            "ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,",
            "typed inline struct direct call metadata drift",
            "zr_aot_fn_%u",
            "ZrCore_Function_TryCopyInlineFrameReturnValue(state,",
            "ZrLibrary_AotRuntime_ReturnInlineStruct(state,",
            "&zr_aot_skip_drop_slot",
    };
    static const char *const callSourceForbiddenNeedles[] = {
            "calleeFunction->parameterMetadata",
            "parameterOffset",
            "SZrCallInfo *zr_aot_call_info = frame.callInfo;",
            "SZrCallInfo *zr_aot_call_info;",
            "SZrFunction *zr_aot_metadata_function;",
            "SZrTypeValue *zr_aot_callable_value;",
            "ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);",
            "ZrCore_Function_CheckStackAndGc(state, 1u + %u, zr_aot_call_base);",
            "ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_callable_value);",
            "ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(",
            "ZrCore_Function_PostCall(state, zr_aot_call_info, 1);",
            "state->stackTop.valuePointer = zr_aot_return_source + 1;",
            "zr_aot_skip_drop_slot = %u;",
    };
    static const char *const orchestratorNeedles[] = {
            "#include \"backend_aot_c_value_semir_calls.h\"",
            "backend_aot_exec_ir_find_function(module, calleeFunctionIndex)",
            "backend_aot_write_c_value_semir_call_typed(file, frameLayout, instruction);",
            "backend_aot_write_c_value_semir_return_typed(file, frameLayout, instruction);",
            "backend_aot_try_write_c_value_semir_call_typed_exec(",
            "backend_aot_try_write_c_value_semir_return_typed_exec(",
    };
    static const char *const orchestratorForbiddenNeedles[] = {
            "static TZrBool backend_aot_try_write_c_value_call_typed_exec(",
            "static TZrBool backend_aot_try_write_c_value_return_typed_exec(",
            "static void backend_aot_write_c_value_call_typed(",
            "static void backend_aot_write_c_value_return_typed(",
    };
    char *callHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.h");
    char *callSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c");
    char *orchestratorText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c");

    TEST_ASSERT_NOT_NULL(callHeaderText);
    TEST_ASSERT_NOT_NULL(callSourceText);
    TEST_ASSERT_NOT_NULL(orchestratorText);

    assert_text_contains_all(callHeaderText, callHeaderNeedles, ARRAY_COUNT(callHeaderNeedles));
    assert_text_contains_all(callSourceText, callSourceNeedles, ARRAY_COUNT(callSourceNeedles));
    assert_text_contains_none(callSourceText, callSourceForbiddenNeedles, ARRAY_COUNT(callSourceForbiddenNeedles));
    assert_text_contains_all(orchestratorText, orchestratorNeedles, ARRAY_COUNT(orchestratorNeedles));
    assert_text_contains_none(orchestratorText,
                              orchestratorForbiddenNeedles,
                              ARRAY_COUNT(orchestratorForbiddenNeedles));

    free(callHeaderText);
    free(callSourceText);
    free(orchestratorText);
}

static void test_aot_c_constructor_failure_uses_partial_inline_frame_cleanup(void) {
    static const char *const cleanupNeedles[] = {
            "ZrCore_Function_DropInlineFrameValuesOnUnwind(",
            "state->hasCurrentException",
            "zr_aot_constructor_unwind_dropped",
    };
    static const char *const setupNeedles[] = {
            "layout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT",
            "slotEnd = layout->byteOffset + layout->byteSize",
            "registerFrameBytes",
    };
    char *cleanupText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c");
    char *setupText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c");

    TEST_ASSERT_NOT_NULL(cleanupText);
    TEST_ASSERT_NOT_NULL(setupText);
    assert_text_contains_all(cleanupText, cleanupNeedles, ARRAY_COUNT(cleanupNeedles));
    assert_text_contains_all(setupText, setupNeedles, ARRAY_COUNT(setupNeedles));

    free(cleanupText);
    free(setupText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_value_semir_field_lowering_lives_in_focused_module);
    RUN_TEST(test_aot_c_value_semir_inline_struct_field_transfer_uses_layout_copy_for_non_pod);
    RUN_TEST(test_aot_c_value_semir_field_resolver_uses_stable_member_entries);
    RUN_TEST(test_aot_c_type_layout_descriptor_preserves_canonical_identity_and_maps);
    RUN_TEST(test_aot_c_frame_alias_preserves_addressing_without_duplicate_lifecycle);
    RUN_TEST(test_aot_c_indirect_alias_field_access_uses_canonical_frame_member_transfer);
    RUN_TEST(test_aot_c_value_semir_typed_call_return_lives_in_focused_module);
    RUN_TEST(test_aot_c_constructor_failure_uses_partial_inline_frame_cleanup);
    return UNITY_END();
}
