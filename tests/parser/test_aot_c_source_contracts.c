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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_source_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_source_contracts.c");
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

static void test_aot_c_source_lowers_value_semir_with_frame_layout(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_value_semir_for_function(",
            "SZrState *state",
            "const SZrAotExecIrFunction *functionIr",
            "const SZrAotExecIrFrameLayout *frameLayout",
            "backend_aot_try_write_c_value_semir_for_exec_instruction(",
    };
    static const char *const sourceNeedles[] = {
            "#include \"backend_aot_c_value_semir_calls.h\"",
            "#include \"backend_aot_c_value_semir_fields.h\"",
            "backend_aot_c_find_frame_slot_layout(",
            "frameLayout->slotLayouts",
            "ZR_SEMIR_OPCODE_FIELD_ADDR",
            "ZR_SEMIR_OPCODE_LOAD_VALUE",
            "ZR_SEMIR_OPCODE_STORE_VALUE",
            "ZR_SEMIR_OPCODE_COPY_VALUE",
            "ZR_SEMIR_OPCODE_CALL_TYPED",
            "ZR_SEMIR_OPCODE_RETURN_TYPED",
            "zr_aot_value_copy",
            "zr_aot_value_expr_inline_copy",
            "memmove((TZrByte *)frame.slotBase +",
            "zr_aot_value_exec_inline_copy",
            "zr_aot_value_exec_inline_field_copy",
            "const SZrTypeLayout *zr_aot_copy_layout =",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function",
            "ZrCore_TypeLayout_CanRawCopy(zr_aot_copy_layout)",
            "ZrCore_TypeLayout_CopyInline(state,",
            "backend_aot_write_c_value_semir_field_addr(file, state, functionIr, frameLayout, instruction);",
            "backend_aot_write_c_value_semir_load(file, state, functionIr, frameLayout, instruction);",
            "backend_aot_write_c_value_semir_store(file, state, functionIr, frameLayout, instruction);",
            "backend_aot_try_write_c_value_semir_field_load_exec(",
            "backend_aot_try_write_c_value_semir_field_store_exec(",
            "case ZR_SEMIR_OPCODE_COPY_VALUE",
            "case ZR_SEMIR_OPCODE_LOAD_VALUE",
            "case ZR_SEMIR_OPCODE_STORE_VALUE",
            "case ZR_SEMIR_OPCODE_CALL_TYPED",
            "case ZR_SEMIR_OPCODE_RETURN_TYPED",
            "backend_aot_write_c_value_semir_call_typed(file, frameLayout, instruction);",
            "backend_aot_write_c_value_semir_return_typed(file, frameLayout, instruction);",
            "backend_aot_try_write_c_value_semir_call_typed_exec(",
            "backend_aot_try_write_c_value_semir_return_typed_exec(",
    };
    static const char *const callHeaderNeedles[] = {
            "backend_aot_write_c_value_semir_call_typed(",
            "backend_aot_write_c_value_semir_return_typed(",
            "backend_aot_try_write_c_value_semir_call_typed_exec(",
            "backend_aot_try_write_c_value_semir_return_typed_exec(",
    };
    static const char *const callSourceNeedles[] = {
            "backend_aot_c_value_call_find_frame_slot_layout(",
            "backend_aot_c_value_call_layout_can_inline_struct(",
            "zr_aot_value_call_typed",
            "zr_aot_value_return_typed",
            "zr_aot_value_exec_call_typed",
            "zr_aot_value_exec_return_typed",
            "SZrFunction *zr_aot_metadata_function;",
            "SZrTypeValue *zr_aot_callable_value;",
            "ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_callable_value);",
            "ZrCore_Function_PreCallPreparedResolvedVmFunction(state,",
            "ZrCore_Function_PostCall(state, zr_aot_call_info, 1);",
            "PostCall routes the callee inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...).",
            "ZrCore_Function_TryCopyInlineFrameReturnValue(state,",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout",
            "state->stackTop.valuePointer = zr_aot_return_source + 1;",
    };
    static const char *const fieldHeaderNeedles[] = {
            "backend_aot_write_c_value_semir_field_addr(",
            "backend_aot_write_c_value_semir_load(",
            "backend_aot_write_c_value_semir_store(",
            "backend_aot_try_write_c_value_semir_field_load_exec(",
            "backend_aot_try_write_c_value_semir_field_store_exec(",
    };
    static const char *const fieldSourceNeedles[] = {
            "backend_aot_c_value_field_resolve_layout(",
            "ZrCore_Function_ResolvePrototypeFrameFieldLayout(state",
            "zr_aot_value_field_addr",
            "zr_aot_value_load",
            "zr_aot_value_store",
            "zr_aot_value_expr_field_load",
            "zr_aot_value_expr_field_store",
            "fieldLayout.byteOffset",
            "zr_aot_value_exec_field_load",
            "zr_aot_value_exec_field_store",
            "backend_aot_c_value_field_layout_can_value_slot_exec",
            "backend_aot_c_value_field_layout_can_value_slot_destination_exec",
            "backend_aot_c_value_field_layout_can_value_slot_source_exec",
            "zr_aot_value_exec_field_value_slot_load",
            "zr_aot_value_exec_field_value_slot_store",
            "SZrTypeValue *zr_aot_destination = (SZrTypeValue *)((TZrByte *)frame.slotBase + %u);",
            "const SZrTypeValue *zr_aot_source = (const SZrTypeValue *)((const TZrByte *)frame.slotBase + %u);",
            "ZrCore_Value_Copy(state, zr_aot_destination, (const SZrTypeValue *)zr_aot_field);",
            "ZrCore_Value_Copy(state, (SZrTypeValue *)zr_aot_field, zr_aot_source);",
            "backend_aot_c_value_field_layout_can_inline_struct_exec",
            "zr_aot_value_exec_field_inline_struct_load",
            "zr_aot_value_exec_field_inline_struct_store",
            "const SZrTypeLayout *zr_aot_field_layout =",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);",
            "ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)",
            "zr_aot_value_exec_field_inline_struct_copy",
            "ZrCore_TypeLayout_CopyInline(state,",
            "memmove((TZrByte *)frame.slotBase + %u, (const TZrByte *)frame.slotBase + %u + %u, %u);",
            "memmove((TZrByte *)frame.slotBase + %u + %u, (const TZrByte *)frame.slotBase + %u, %u);",
            "zr_aot_value_unsupported_field_load",
            "zr_aot_value_unsupported_field_store",
            "unsupported AOT value SemIR field load",
            "unsupported AOT value SemIR field store",
            "fieldLayout.isPrimitivePod",
            "fieldLayout.isValueSlot",
            "fieldLayout.typeLayoutId",
            "const TZrByte *zr_aot_field = (const TZrByte *)frame.slotBase + %u + %u;",
            "SZrTypeValue *zr_aot_destination = (SZrTypeValue *)((TZrByte *)frame.slotBase + %u);",
            "memcpy(&zr_aot_field_value, zr_aot_field, sizeof(zr_aot_field_value));",
            "ZR_VALUE_FAST_SET(zr_aot_destination,",
            "TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;",
            "const SZrTypeValue *zr_aot_source = (const SZrTypeValue *)((const TZrByte *)frame.slotBase + %u);",
            "memcpy(zr_aot_field, &zr_aot_stored_value, sizeof(zr_aot_stored_value));",
    };
    static const char *const valueSourceForbiddenNeedles[] = {
            "ZrAotGeneratedDirectCall zr_aot_direct_call;",
            "ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,",
            "ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1)",
    };
    static const char *const functionBodyNeedles[] = {
            "#include \"backend_aot_c_value_semir.h\"",
            "backend_aot_write_c_value_semir_for_function(",
            "backend_aot_write_c_value_semir_for_function(file, state, module",
            "&functionIr->frameLayout",
            "backend_aot_try_write_c_value_semir_for_exec_instruction(",
            "backend_aot_write_c_direct_stack_copy(file, destinationSlot, (TZrUInt32)operandA2);",
            "case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):",
            "case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):",
            "case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):",
            "case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):",
            "case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):",
            "case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):",
            "case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):",
    };
    char *valueLoweringHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.h");
    char *valueLoweringSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c");
    char *callLoweringHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.h");
    char *callLoweringSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c");
    char *fieldLoweringHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.h");
    char *fieldLoweringSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(valueLoweringHeaderText);
    TEST_ASSERT_NOT_NULL(valueLoweringSourceText);
    TEST_ASSERT_NOT_NULL(callLoweringHeaderText);
    TEST_ASSERT_NOT_NULL(callLoweringSourceText);
    TEST_ASSERT_NOT_NULL(fieldLoweringHeaderText);
    TEST_ASSERT_NOT_NULL(fieldLoweringSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(valueLoweringHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(valueLoweringSourceText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(callLoweringHeaderText, callHeaderNeedles, ARRAY_COUNT(callHeaderNeedles));
    assert_text_contains_all(callLoweringSourceText, callSourceNeedles, ARRAY_COUNT(callSourceNeedles));
    assert_text_contains_all(fieldLoweringHeaderText, fieldHeaderNeedles, ARRAY_COUNT(fieldHeaderNeedles));
    assert_text_contains_all(fieldLoweringSourceText, fieldSourceNeedles, ARRAY_COUNT(fieldSourceNeedles));
    assert_text_contains_none(valueLoweringSourceText,
                              valueSourceForbiddenNeedles,
                              ARRAY_COUNT(valueSourceForbiddenNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(valueLoweringHeaderText);
    free(valueLoweringSourceText);
    free(callLoweringHeaderText);
    free(callLoweringSourceText);
    free(fieldLoweringHeaderText);
    free(fieldLoweringSourceText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_primitive_constants_to_direct_value_writes(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_primitive_constant",
            "backend_aot_write_c_direct_set_constant",
    };
    static const char *const valueSourceNeedles[] = {
            "zr_aot_value_exec_primitive_constant",
            "backend_aot_c_write_direct_null_value",
            "backend_aot_c_write_direct_plain_value_replace_guard",
            "backend_aot_write_c_direct_reset_stack_null(FILE *file, TZrUInt32 destinationSlot)",
            "backend_aot_write_c_direct_reset_stack_null2(FILE *file, TZrUInt32 firstSlot, TZrUInt32 secondSlot)",
            "zr_aot_value_exec_reset_stack_null",
            "zr_aot_value_exec_reset_stack_null2",
            "ZrCore_Ownership_ReleaseValue(state, zr_aot_destination);",
            "ZR_VALUE_FAST_SET(zr_aot_destination",
            "nativeBool",
            "nativeInt64",
            "nativeUInt64",
            "nativeDouble",
            "zr_aot_value_exec_set_constant",
            "*zr_aot_constant = *zr_aot_source;",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(GET_CONSTANT):",
            "backend_aot_write_c_direct_primitive_constant(",
            "case ZR_INSTRUCTION_ENUM(SET_CONSTANT):",
            "backend_aot_write_c_direct_set_constant(file, destinationSlot, (TZrUInt32)operandA2);",
            "case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):",
            "backend_aot_write_c_direct_reset_stack_null(file, destinationSlot);",
            "case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2):",
            "backend_aot_write_c_direct_reset_stack_null2(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenValueSourceNeedles[] = {
            "ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant)",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_SetConstant(state, &frame",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *valueLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(valueLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(valueLoweringText, valueSourceNeedles, ARRAY_COUNT(valueSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(valueLoweringText, forbiddenValueSourceNeedles, ARRAY_COUNT(forbiddenValueSourceNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(emitterHeaderText);
    free(valueLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_legacy_int_arithmetic_to_direct_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add_int",
            "backend_aot_write_c_direct_add_int_const",
            "backend_aot_write_c_direct_sub_int",
            "backend_aot_write_c_direct_sub_int_const",
    };
    static const char *const legacyIntLoweringNeedles[] = {
            "backend_aot_c_lowering_legacy_int_arithmetic.c",
            "zr_aot_arith_exec_int",
            "zr_aot_arith_exec_int_const",
            "backend_aot_c_format_integer_like_literal",
            "zr_aot_left_int + zr_aot_right_int",
            "zr_aot_left_int + zr_aot_right_literal",
            "zr_aot_left_int - zr_aot_right_int",
            "zr_aot_left_int - zr_aot_right_literal",
            "unsupported AOT ADD_INT_CONST constant",
            "unsupported AOT SUB_INT_CONST constant",
            "ZR_AOT_C_FAIL();",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_INT):",
            "backend_aot_write_c_direct_add_int(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_add_int_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUB_INT):",
            "backend_aot_write_c_direct_sub_int(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_sub_int_const(file, entry->function, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenLegacyIntLoweringNeedles[] = {
            "ZrLibrary_AotRuntime_AddIntConst(state, &frame",
            "ZrLibrary_AotRuntime_SubInt(state, &frame",
            "ZrLibrary_AotRuntime_SubIntConst(state, &frame",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *legacyIntLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_legacy_int_arithmetic.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(legacyIntLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(legacyIntLoweringText, legacyIntLoweringNeedles, ARRAY_COUNT(legacyIntLoweringNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(legacyIntLoweringText,
                              forbiddenLegacyIntLoweringNeedles,
                              ARRAY_COUNT(forbiddenLegacyIntLoweringNeedles));

    free(emitterHeaderText);
    free(legacyIntLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_generic_numeric_arithmetic_to_direct_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add",
            "backend_aot_write_c_direct_sub",
            "backend_aot_write_c_direct_mul",
            "backend_aot_write_c_direct_div",
            "backend_aot_write_c_direct_mod",
            "backend_aot_write_c_direct_neg",
    };
    static const char *const genericNumericNeedles[] = {
            "backend_aot_c_lowering_generic_numeric_arithmetic.c",
            "zr_aot_arith_exec_generic_numeric_binary",
            "zr_aot_arith_exec_generic_numeric_unary",
            "backend_aot_c_write_generic_numeric_unsupported",
            "backend_aot_c_write_generic_numeric_zero_guard",
            "backend_aot_c_write_generic_numeric_float_binary",
            "backend_aot_c_write_generic_numeric_int_binary",
            "backend_aot_c_write_generic_numeric_uint_binary",
            "backend_aot_c_write_generic_numeric_extract_float64",
            "backend_aot_c_write_generic_numeric_extract_int64",
            "unsupported AOT generic numeric arithmetic",
            "divide by zero",
            "modulo by zero",
            "zr_aot_left_float + zr_aot_right_float",
            "zr_aot_left_float - zr_aot_right_float",
            "zr_aot_left_float * zr_aot_right_float",
            "zr_aot_left_float / zr_aot_right_float",
            "zr_aot_left_int + zr_aot_right_int",
            "zr_aot_left_int - zr_aot_right_int",
            "zr_aot_left_int * zr_aot_right_int",
            "zr_aot_left_int / zr_aot_right_int",
            "zr_aot_left_int % zr_aot_right_int",
            "zr_aot_left_uint + zr_aot_right_uint",
            "zr_aot_left_uint - zr_aot_right_uint",
            "zr_aot_left_uint * zr_aot_right_uint",
            "zr_aot_left_uint / zr_aot_right_uint",
            "zr_aot_left_uint % zr_aot_right_uint",
            "-zr_aot_source->value.nativeObject.nativeInt64",
            "-(TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64",
            "-zr_aot_source->value.nativeObject.nativeDouble",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD):",
            "backend_aot_write_c_direct_add(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(SUB):",
            "backend_aot_write_c_direct_sub(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(MUL):",
            "backend_aot_write_c_direct_mul(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(DIV):",
            "backend_aot_write_c_direct_div(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(MOD):",
            "backend_aot_write_c_direct_mod(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(NEG):",
            "backend_aot_write_c_direct_neg(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenGenericNumericNeedles[] = {
            "ZrLibrary_AotRuntime_Add(state, &frame",
            "ZrLibrary_AotRuntime_Sub(state, &frame",
            "ZrLibrary_AotRuntime_Mul(state, &frame",
            "ZrLibrary_AotRuntime_Div(state, &frame",
            "ZrLibrary_AotRuntime_Mod(state, &frame",
            "ZrLibrary_AotRuntime_Neg(state, &frame",
            "zr_aot_use_runtime_add",
            "zr_aot_use_runtime_neg",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *genericNumericLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(genericNumericLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(genericNumericLoweringText,
                             genericNumericNeedles,
                             ARRAY_COUNT(genericNumericNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(genericNumericLoweringText,
                              forbiddenGenericNumericNeedles,
                              ARRAY_COUNT(forbiddenGenericNumericNeedles));

    free(emitterHeaderText);
    free(genericNumericLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_generic_primitive_conversions_to_direct_c_writes(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_to_bool",
            "backend_aot_write_c_direct_to_int",
            "backend_aot_write_c_direct_to_uint",
            "backend_aot_write_c_direct_to_float",
    };
    static const char *const genericConversionNeedles[] = {
            "backend_aot_c_lowering_generic_conversion.c",
            "zr_aot_convert_generic_to_bool",
            "zr_aot_convert_generic_to_int",
            "zr_aot_convert_generic_to_uint",
            "zr_aot_convert_generic_to_float",
            "backend_aot_c_write_generic_conversion_unsupported",
            "unsupported AOT generic primitive conversion",
            "ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)",
            "*zr_aot_destination = *zr_aot_source",
            "(TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64",
            "(TZrInt64)zr_aot_source->value.nativeObject.nativeDouble",
            "(TZrUInt64)zr_aot_source->value.nativeObject.nativeInt64",
            "(TZrUInt64)zr_aot_source->value.nativeObject.nativeDouble",
            "(TZrFloat64)zr_aot_source->value.nativeObject.nativeInt64",
            "(TZrFloat64)zr_aot_source->value.nativeObject.nativeUInt64",
            "zr_aot_source->value.nativeObject.nativeBool ? 1 : 0",
            "zr_aot_source->value.nativeObject.nativeBool ? (TZrUInt64)1u : (TZrUInt64)0u",
            "zr_aot_source->value.nativeObject.nativeBool ? (TZrFloat64)1.0 : (TZrFloat64)0.0",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TO_BOOL):",
            "backend_aot_write_c_direct_to_bool(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_INT):",
            "backend_aot_write_c_direct_to_int(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_UINT):",
            "backend_aot_write_c_direct_to_uint(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_FLOAT):",
            "backend_aot_write_c_direct_to_float(file, destinationSlot, operandA1);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_ToBool(state, &frame",
            "ZrLibrary_AotRuntime_ToUInt(state, &frame",
            "ZrLibrary_AotRuntime_ToFloat(state, &frame",
    };
    static const char *const forbiddenGenericConversionNeedles[] = {
            "ZrLibrary_AotRuntime_ToBool(state, &frame",
            "ZrLibrary_AotRuntime_ToInt(state, &frame",
            "ZrLibrary_AotRuntime_ToUInt(state, &frame",
            "ZrLibrary_AotRuntime_ToFloat(state, &frame",
            "ZrCore_Value_Copy(state, zr_aot_destination, zr_aot_source)",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *genericConversionLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_conversion.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(genericConversionLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(genericConversionLoweringText,
                             genericConversionNeedles,
                             ARRAY_COUNT(genericConversionNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(genericConversionLoweringText,
                              forbiddenGenericConversionNeedles,
                              ARRAY_COUNT(forbiddenGenericConversionNeedles));

    free(emitterHeaderText);
    free(genericConversionLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_typed_arithmetic_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add_signed",
            "backend_aot_write_c_direct_add_unsigned",
            "backend_aot_write_c_direct_sub_signed",
            "backend_aot_write_c_direct_sub_unsigned",
            "backend_aot_write_c_direct_mul_signed",
            "backend_aot_write_c_direct_mul_unsigned",
            "backend_aot_write_c_direct_div_signed",
            "backend_aot_write_c_direct_div_unsigned",
            "backend_aot_write_c_direct_add_float",
            "backend_aot_write_c_direct_sub_float",
            "backend_aot_write_c_direct_mul_float",
            "backend_aot_write_c_direct_div_float",
            "backend_aot_write_c_direct_add_signed_const",
            "backend_aot_write_c_direct_add_unsigned_const",
            "backend_aot_write_c_direct_sub_signed_const",
            "backend_aot_write_c_direct_sub_unsigned_const",
            "backend_aot_write_c_direct_mul_signed_const",
            "backend_aot_write_c_direct_mul_unsigned_const",
            "backend_aot_write_c_direct_div_signed_const",
            "backend_aot_write_c_direct_div_unsigned_const",
            "backend_aot_write_c_direct_neg_signed",
            "backend_aot_write_c_direct_neg_float",
            "backend_aot_write_c_direct_mod_signed",
            "backend_aot_write_c_direct_mod_unsigned",
            "backend_aot_write_c_direct_mod_signed_const",
            "backend_aot_write_c_direct_mod_unsigned_const",
            "backend_aot_write_c_direct_add_signed_mod_const",
            "backend_aot_write_c_direct_logical_equal_signed",
            "backend_aot_write_c_direct_logical_not_equal_signed",
            "backend_aot_write_c_direct_logical_equal_unsigned",
            "backend_aot_write_c_direct_logical_not_equal_unsigned",
            "backend_aot_write_c_direct_logical_equal_float",
            "backend_aot_write_c_direct_logical_not_equal_float",
            "backend_aot_write_c_direct_logical_greater_unsigned",
            "backend_aot_write_c_direct_logical_greater_float",
            "backend_aot_write_c_direct_logical_less_unsigned",
            "backend_aot_write_c_direct_logical_less_float",
            "backend_aot_write_c_direct_logical_greater_equal_unsigned",
            "backend_aot_write_c_direct_logical_greater_equal_float",
            "backend_aot_write_c_direct_logical_less_equal_unsigned",
            "backend_aot_write_c_direct_logical_less_equal_float",
    };
    static const char *const sourceNeedles[] = {
            "zr_aot_arith_exec_signed",
            "zr_aot_arith_exec_unsigned",
            "zr_aot_arith_exec_float",
            "zr_aot_arith_exec_signed_unary",
            "zr_aot_arith_exec_float_unary",
            "zr_aot_arith_exec_signed_const",
            "zr_aot_arith_exec_unsigned_const",
            "backend_aot_c_format_signed_integer_literal",
            "backend_aot_c_format_unsigned_integer_literal",
            "zr_aot_left_scalar + zr_aot_right_scalar",
            "zr_aot_left_scalar - zr_aot_right_scalar",
            "zr_aot_left_scalar * zr_aot_right_scalar",
            "zr_aot_left_scalar / zr_aot_right_scalar",
            "zr_aot_left_scalar + zr_aot_right_literal",
            "zr_aot_left_scalar - zr_aot_right_literal",
            "zr_aot_left_scalar * zr_aot_right_literal",
            "zr_aot_left_scalar / zr_aot_right_literal",
            "-zr_aot_source_scalar",
            "zr_aot_arith_exec_signed_mod",
            "zr_aot_arith_exec_unsigned_mod",
            "zr_aot_left_scalar % zr_aot_right_scalar",
            "zr_aot_left_scalar % zr_aot_right_literal",
            "(zr_aot_left_scalar + zr_aot_right_scalar) % zr_aot_mod_literal",
            "zr_aot_arith_exec_signed_add_mod_const",
            "ZrCore_Debug_RunError(state, \\\"divide by zero\\\")",
            "ZrCore_Debug_RunError(state, \\\"modulo by zero\\\")",
            "ZR_AOT_C_FAIL();",
            "zr_aot_compare_exec_signed",
            "zr_aot_compare_exec_unsigned",
            "zr_aot_compare_exec_float",
            "zr_aot_left_scalar == zr_aot_right_scalar",
            "zr_aot_left_scalar != zr_aot_right_scalar",
            "zr_aot_left_scalar > zr_aot_right_scalar",
            "zr_aot_left_scalar < zr_aot_right_scalar",
            "zr_aot_left_scalar >= zr_aot_right_scalar",
            "zr_aot_left_scalar <= zr_aot_right_scalar",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED):",
            "backend_aot_write_c_direct_add_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_add_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_sub_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_sub_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mul_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mul_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_div_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_div_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_add_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_sub_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mul_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_div_float(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(NEG_SIGNED):",
            "backend_aot_write_c_direct_neg_signed(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(NEG_FLOAT):",
            "backend_aot_write_c_direct_neg_float(file, destinationSlot, operandA1);",
            "backend_aot_write_c_direct_add_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_add_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_sub_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_sub_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mul_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mul_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_div_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_div_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mod_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mod_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mod_signed_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_mod_unsigned_const(file, entry->function, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST):",
            "backend_aot_write_c_direct_add_signed_mod_const(",
            "backend_aot_write_c_direct_logical_equal_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_not_equal_signed(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_equal_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_not_equal_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_equal_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_not_equal_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_greater_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_greater_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_less_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_less_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_greater_equal_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_greater_equal_float(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_less_equal_unsigned(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_less_equal_float(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_AddSigned(state, &frame",
            "ZrLibrary_AotRuntime_AddUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_SubSigned(state, &frame",
            "ZrLibrary_AotRuntime_SubUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_MulSigned(state, &frame",
            "ZrLibrary_AotRuntime_MulUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_DivSigned(state, &frame",
            "ZrLibrary_AotRuntime_DivUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_AddFloat(state, &frame",
            "ZrLibrary_AotRuntime_SubFloat(state, &frame",
            "ZrLibrary_AotRuntime_MulFloat(state, &frame",
            "ZrLibrary_AotRuntime_DivFloat(state, &frame",
            "ZrLibrary_AotRuntime_Neg(state, &frame",
            "ZrLibrary_AotRuntime_AddSignedConst(state, &frame",
            "ZrLibrary_AotRuntime_AddUnsignedConst(state, &frame",
            "ZrLibrary_AotRuntime_SubSignedConst(state, &frame",
            "ZrLibrary_AotRuntime_SubUnsignedConst(state, &frame",
            "ZrLibrary_AotRuntime_MulSignedConst(state, &frame",
            "ZrLibrary_AotRuntime_MulUnsignedConst(state, &frame",
            "ZrLibrary_AotRuntime_DivSignedConst(state, &frame",
            "ZrLibrary_AotRuntime_DivUnsignedConst(state, &frame",
            "ZrLibrary_AotRuntime_ModSigned(state, &frame",
            "ZrLibrary_AotRuntime_ModUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_ModSignedConst(state, &frame",
            "ZrLibrary_AotRuntime_ModUnsignedConst(state, &frame",
            "ZrLibrary_AotRuntime_LogicalEqualSigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqualSigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalEqualUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqualUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalEqualFloat(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqualFloat(state, &frame",
            "ZrLibrary_AotRuntime_LogicalGreaterSigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalGreaterUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalGreaterFloat(state, &frame",
            "ZrLibrary_AotRuntime_LogicalLessSigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalLessUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalLessFloat(state, &frame",
            "ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalGreaterEqualFloat(state, &frame",
            "ZrLibrary_AotRuntime_LogicalLessEqualSigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalLessEqualUnsigned(state, &frame",
            "ZrLibrary_AotRuntime_LogicalLessEqualFloat(state, &frame",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *typedArithmeticLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(typedArithmeticLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(typedArithmeticLoweringText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(emitterHeaderText);
    free(typedArithmeticLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_typed_signed_load_const_arithmetic_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add_signed_load_const",
            "backend_aot_write_c_direct_sub_signed_load_const",
            "backend_aot_write_c_direct_mul_signed_load_const",
            "backend_aot_write_c_direct_div_signed_load_const",
            "backend_aot_write_c_direct_mod_signed_load_const",
    };
    static const char *const loadConstSourceNeedles[] = {
            "backend_aot_c_lowering_typed_arithmetic_load_const.c",
            "zr_aot_arith_exec_signed_load_const",
            "backend_aot_c_format_signed_load_const_integer_literal",
            "backend_aot_c_signed_load_const_value_type_literal",
            "zr_aot_materialized_constant",
            "ZR_VALUE_FAST_SET(zr_aot_materialized_constant",
            "zr_aot_left_scalar + zr_aot_right_literal",
            "zr_aot_left_scalar - zr_aot_right_literal",
            "zr_aot_left_scalar * zr_aot_right_literal",
            "zr_aot_left_scalar / zr_aot_right_literal",
            "zr_aot_left_scalar % zr_aot_right_literal",
            "ZrCore_Debug_RunError(state, \\\"divide by zero\\\")",
            "ZrCore_Debug_RunError(state, \\\"modulo by zero\\\")",
            "zr_aot_load_const_result_type",
            "ZR_VALUE_TYPE_INT64",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):",
            "backend_aot_write_c_direct_add_signed_load_const(",
            "case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):",
            "backend_aot_write_c_direct_sub_signed_load_const(",
            "case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):",
            "backend_aot_write_c_direct_mul_signed_load_const(",
            "case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):",
            "backend_aot_write_c_direct_div_signed_load_const(",
            "case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):",
            "backend_aot_write_c_direct_mod_signed_load_const(",
    };
    static const char *const backendSupportNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):",
            "case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):",
            "case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):",
            "case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):",
            "case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loadConstLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *backendSupportText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(loadConstLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(backendSupportText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(loadConstLoweringText, loadConstSourceNeedles, ARRAY_COUNT(loadConstSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(backendSupportText, backendSupportNeedles, ARRAY_COUNT(backendSupportNeedles));

    free(emitterHeaderText);
    free(loadConstLoweringText);
    free(functionBodyText);
    free(backendSupportText);
}

static void test_aot_c_source_lowers_typed_signed_load_stack_const_arithmetic_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add_signed_load_stack_const",
            "backend_aot_write_c_direct_sub_signed_load_stack_const",
            "backend_aot_write_c_direct_mul_signed_load_stack_const",
            "backend_aot_write_c_direct_div_signed_load_stack_const",
            "backend_aot_write_c_direct_mod_signed_load_stack_const",
    };
    static const char *const loadConstSourceNeedles[] = {
            "zr_aot_arith_exec_signed_load_stack_const",
            "zr_aot_materialized_left",
            "*zr_aot_materialized_left = *zr_aot_source;",
            "zr_aot_left_scalar + zr_aot_right_literal",
            "zr_aot_left_scalar - zr_aot_right_literal",
            "zr_aot_left_scalar * zr_aot_right_literal",
            "zr_aot_left_scalar / zr_aot_right_literal",
            "zr_aot_left_scalar % zr_aot_right_literal",
            "ZrCore_Debug_RunError(state, \\\"divide by zero\\\")",
            "ZrCore_Debug_RunError(state, \\\"modulo by zero\\\")",
            "zr_aot_load_stack_const_result_type",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):",
            "backend_aot_write_c_direct_add_signed_load_stack_const(",
            "case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):",
            "backend_aot_write_c_direct_sub_signed_load_stack_const(",
            "case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):",
            "backend_aot_write_c_direct_mul_signed_load_stack_const(",
            "case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):",
            "backend_aot_write_c_direct_div_signed_load_stack_const(",
            "case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):",
            "backend_aot_write_c_direct_mod_signed_load_stack_const(",
    };
    static const char *const backendSupportNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):",
            "case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):",
            "case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):",
            "case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):",
            "case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loadConstLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *backendSupportText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(loadConstLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(backendSupportText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(loadConstLoweringText, loadConstSourceNeedles, ARRAY_COUNT(loadConstSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(backendSupportText, backendSupportNeedles, ARRAY_COUNT(backendSupportNeedles));

    free(emitterHeaderText);
    free(loadConstLoweringText);
    free(functionBodyText);
    free(backendSupportText);
}

static void test_aot_c_source_lowers_typed_signed_load_stack_load_const_arithmetic_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add_signed_load_stack_load_const",
    };
    static const char *const loadConstSourceNeedles[] = {
            "zr_aot_arith_exec_signed_load_stack_load_const",
            "zr_aot_materialized_left",
            "zr_aot_materialized_constant",
            "*zr_aot_materialized_left = *zr_aot_source;",
            "ZR_VALUE_FAST_SET(zr_aot_materialized_constant",
            "zr_aot_left_scalar + zr_aot_right_literal",
            "zr_aot_load_stack_load_const_result_type",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):",
            "backend_aot_write_c_direct_add_signed_load_stack_load_const(",
    };
    static const char *const backendSupportNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loadConstLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_const.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *backendSupportText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(loadConstLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(backendSupportText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(loadConstLoweringText, loadConstSourceNeedles, ARRAY_COUNT(loadConstSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(backendSupportText, backendSupportNeedles, ARRAY_COUNT(backendSupportNeedles));

    free(emitterHeaderText);
    free(loadConstLoweringText);
    free(functionBodyText);
    free(backendSupportText);
}

static void test_aot_c_source_lowers_typed_signed_load_stack_arithmetic_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_add_signed_load_stack",
            "backend_aot_write_c_direct_mul_signed_load_stack",
    };
    static const char *const loadStackSourceNeedles[] = {
            "backend_aot_c_lowering_typed_arithmetic_load_stack.c",
            "zr_aot_arith_exec_signed_load_stack",
            "zr_aot_left_scalar + zr_aot_right_scalar",
            "zr_aot_left_scalar * zr_aot_right_scalar",
            "zr_aot_load_stack_result_type",
            "ZR_VALUE_TYPE_INT64",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):",
            "backend_aot_write_c_direct_add_signed_load_stack(",
            "case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):",
            "backend_aot_write_c_direct_mul_signed_load_stack(",
    };
    static const char *const backendSupportNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):",
            "case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *loadStackLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic_load_stack.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *backendSupportText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(loadStackLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(backendSupportText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(loadStackLoweringText, loadStackSourceNeedles, ARRAY_COUNT(loadStackSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(backendSupportText, backendSupportNeedles, ARRAY_COUNT(backendSupportNeedles));

    free(emitterHeaderText);
    free(loadStackLoweringText);
    free(functionBodyText);
    free(backendSupportText);
}

static void test_aot_c_source_lowers_typed_bitwise_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_bitwise_not",
            "backend_aot_write_c_direct_bitwise_and",
            "backend_aot_write_c_direct_bitwise_or",
            "backend_aot_write_c_direct_bitwise_xor",
            "backend_aot_write_c_direct_shift_left_int",
            "backend_aot_write_c_direct_shift_right_int",
            "backend_aot_write_c_direct_bitwise_shift_left",
            "backend_aot_write_c_direct_bitwise_shift_right",
    };
    static const char *const sourceNeedles[] = {
            "zr_aot_bitwise_exec_unary",
            "zr_aot_bitwise_exec_binary",
            "zr_aot_bitwise_exec_unsigned_shift_right",
            "~zr_aot_source_scalar",
            "zr_aot_left_scalar & zr_aot_right_scalar",
            "zr_aot_left_scalar | zr_aot_right_scalar",
            "zr_aot_left_scalar ^ zr_aot_right_scalar",
            "(TZrInt64)(zr_aot_left_unsigned << zr_aot_shift_count)",
            "zr_aot_left_scalar >> zr_aot_shift_count",
            "(TZrInt64)(zr_aot_left_unsigned >> zr_aot_shift_count)",
            "shift count out of range",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_write_c_direct_bitwise_not(file, destinationSlot, operandA1);",
            "backend_aot_write_c_direct_bitwise_and(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_bitwise_or(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_bitwise_xor(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_shift_left_int(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_shift_right_int(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_bitwise_shift_left(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_bitwise_shift_right(file, destinationSlot, operandA1, operandB1);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_BitwiseNot(state, &frame",
            "ZrLibrary_AotRuntime_BitwiseAnd(state, &frame",
            "ZrLibrary_AotRuntime_BitwiseOr(state, &frame",
            "ZrLibrary_AotRuntime_BitwiseXor(state, &frame",
            "ZrLibrary_AotRuntime_BitwiseShiftLeft(state, &frame",
            "ZrLibrary_AotRuntime_BitwiseShiftRight(state, &frame",
            "ZrLibrary_AotRuntime_ShiftLeftInt(state, &frame",
            "ZrLibrary_AotRuntime_ShiftRightInt(state, &frame",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *typedBitwiseLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bitwise.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(typedBitwiseLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(typedBitwiseLoweringText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(emitterHeaderText);
    free(typedBitwiseLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_typed_bool_equality_to_c_expressions(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_equal_bool",
            "backend_aot_write_c_direct_logical_not_equal_bool",
            "backend_aot_write_c_direct_logical_not_bool",
            "backend_aot_write_c_direct_jump_if_bool_false",
    };
    static const char *const sourceNeedles[] = {
            "zr_aot_bool_compare_exec",
            "zr_aot_bool_not_exec",
            "zr_aot_left_bool == zr_aot_right_bool",
            "zr_aot_left_bool != zr_aot_right_bool",
            "!zr_aot_source_bool",
    };
    static const char *const controlNeedles[] = {
            "zr_aot_jump_if_bool_false",
            "if (!zr_aot_condition_bool)",
    };
    static const char *const functionBodyNeedles[] = {
            "backend_aot_write_c_direct_logical_equal_bool(file, destinationSlot, operandA1, operandB1);",
            "backend_aot_write_c_direct_logical_not_equal_bool(file, destinationSlot, operandA1, operandB1);",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):",
            "backend_aot_write_c_direct_logical_not_bool(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):",
            "backend_aot_write_c_direct_jump_if_bool_false(",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqualBool(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqualBool(state, &frame",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *typedLogicalLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_logical.c");
    char *controlLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(typedLogicalLoweringText);
    TEST_ASSERT_NOT_NULL(controlLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(typedLogicalLoweringText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(controlLoweringText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(emitterHeaderText);
    free(typedLogicalLoweringText);
    free(controlLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_lowers_typed_signed_branch_to_c_comparisons(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_jump_if_greater_signed",
            "backend_aot_write_c_direct_jump_if_less_equal_signed",
            "backend_aot_write_c_direct_jump_if_not_equal_signed",
            "backend_aot_write_c_direct_jump_if_not_equal_signed_const",
    };
    static const char *const controlNeedles[] = {
            "zr_aot_jump_if_signed_compare",
            "zr_aot_left_scalar > zr_aot_right_scalar",
            "zr_aot_left_scalar <= zr_aot_right_scalar",
            "zr_aot_left_scalar != zr_aot_right_scalar",
            "zr_aot_left_scalar != zr_aot_right_literal",
            "backend_aot_c_format_signed_branch_const_literal",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):",
            "backend_aot_write_c_direct_jump_if_greater_signed(",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):",
            "backend_aot_write_c_direct_jump_if_less_equal_signed(",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):",
            "backend_aot_write_c_direct_jump_if_not_equal_signed(",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):",
            "backend_aot_write_c_direct_jump_if_not_equal_signed_const(",
    };
    static const char *const backendSupportNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):",
    };
    static const char *const execIrNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):",
    };
    static const char *const forbiddenControlNeedles[] = {
            "ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *controlLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *backendSupportText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");
    char *execIrText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(controlLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(backendSupportText);
    TEST_ASSERT_NOT_NULL(execIrText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(controlLoweringText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_none(controlLoweringText, forbiddenControlNeedles, ARRAY_COUNT(forbiddenControlNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(backendSupportText, backendSupportNeedles, ARRAY_COUNT(backendSupportNeedles));
    assert_text_contains_all(execIrText, execIrNeedles, ARRAY_COUNT(execIrNeedles));

    free(emitterHeaderText);
    free(controlLoweringText);
    free(functionBodyText);
    free(backendSupportText);
    free(execIrText);
}

static void test_aot_c_source_lowers_typed_numeric_conversion_to_c_casts(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_to_float_signed",
            "backend_aot_write_c_direct_to_float_unsigned",
            "backend_aot_write_c_direct_to_int_float",
            "backend_aot_write_c_direct_to_int_unsigned",
            "backend_aot_write_c_direct_to_uint_float",
            "backend_aot_write_c_direct_to_uint_signed",
    };
    static const char *const sourceNeedles[] = {
            "zr_aot_convert_signed_to_float",
            "zr_aot_convert_unsigned_to_float",
            "zr_aot_convert_float_to_signed",
            "zr_aot_convert_unsigned_to_signed",
            "zr_aot_unsigned_to_signed_limit",
            "zr_aot_convert_float_to_unsigned",
            "zr_aot_convert_signed_to_unsigned",
            "(TZrFloat64)zr_aot_source_scalar",
            "(TZrInt64)zr_aot_source_scalar",
            "(TZrUInt64)zr_aot_source_scalar",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):",
            "backend_aot_write_c_direct_to_float_signed(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):",
            "backend_aot_write_c_direct_to_float_unsigned(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):",
            "backend_aot_write_c_direct_to_int_float(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):",
            "backend_aot_write_c_direct_to_int_unsigned(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):",
            "backend_aot_write_c_direct_to_uint_float(file, destinationSlot, operandA1);",
            "case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):",
            "backend_aot_write_c_direct_to_uint_signed(file, destinationSlot, operandA1);",
    };
    char *emitterHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *typedConversionLoweringText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_conversion.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(emitterHeaderText);
    TEST_ASSERT_NOT_NULL(typedConversionLoweringText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(emitterHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(typedConversionLoweringText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(emitterHeaderText);
    free(typedConversionLoweringText);
    free(functionBodyText);
}

static void test_aot_c_source_emits_value_frame_cleanup_exit(void) {
    static const char *const emitterNeedles[] = {
            "#include \\\"zr_vm_core/function.h\\\"",
            "#include \\\"zr_vm_core/exception.h\\\"",
            "#include \\\"zr_vm_core/execution_control.h\\\"",
            "#include \\\"zr_vm_core/type_layout.h\\\"",
            "#define ZR_AOT_C_RETURN(expr)",
            "zr_aot_return_value = (expr);",
            "goto zr_aot_function_exit;",
            "#define ZR_AOT_C_FAIL()",
            "ZrCore_Debug_RunError(state,",
            "generated AOT function failed: functionIndex=%%u instructionIndex=%%u",
            "(unsigned)frame.functionIndex",
            "frame.currentInstructionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH",
            "ZR_AOT_C_RETURN(0);",
    };
    static const char *const emitterForbiddenNeedles[] = {
            "ZrLibrary_AotRuntime_FailGeneratedFunction(state, &frame)",
    };
    static const char *const cleanupHeaderNeedles[] = {
            "backend_aot_write_c_frame_cleanup(",
            "const SZrAotExecIrFrameLayout *frameLayout",
    };
    static const char *const cleanupSourceNeedles[] = {
            "#include \"backend_aot_c_frame_cleanup.h\"",
            "zr_aot_value_frame_drop",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function",
            "zr_aot_drop_layout->dropKind != ZR_TYPE_LAYOUT_DROP_KIND_NONE",
            "ZrCore_TypeLayout_DropInline(state,",
            "(TZrByte *)frame.slotBase +",
            "layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT",
            "zr_aot_skip_drop_slot != %u",
    };
    static const char *const functionBodyNeedles[] = {
            "#include \"backend_aot_c_frame_cleanup.h\"",
            "ZrAotGeneratedFrame frame = {0};",
            "TZrInt64 zr_aot_return_value = 0;",
            "TZrBool zr_aot_frame_started = ZR_FALSE;",
            "TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;",
            "zr_aot_frame_started = ZR_TRUE;",
            "zr_aot_function_exit:",
            "if (zr_aot_frame_started) {",
            "return zr_aot_return_value;",
            "backend_aot_write_c_frame_cleanup(file, &functionIr->frameLayout);",
            "backend_aot_write_c_publish_exports(file);",
            "backend_aot_write_c_direct_return(file, operandA1);",
            "backend_aot_write_c_unsupported_instruction(file,",
    };
    static const char *const controlNeedles[] = {
            "backend_aot_write_c_begin_instruction(FILE *file,",
            "zr_aot_begin_instruction",
            "SZrCallInfo *zr_aot_call_info = frame.callInfo;",
            "frame.currentInstructionIndex = %u;",
            "zr_aot_publish_all_instructions",
            "frame.observationMask & (",
            "frame.function->instructionsList + %u;",
            "ZrCore_Exception_FindSourceLine(frame.function, (TZrMemoryOffset)%u);",
            "ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_LINE, zr_aot_source_line, 0, 0);",
            "backend_aot_write_c_unsupported_instruction(FILE *file,",
            "backend_aot_write_c_unsupported_instruction_expr(FILE *file,",
            "zr_aot_unsupported_instruction",
            "const TZrUInt32 zr_aot_function_index = %u;",
            "const TZrUInt32 zr_aot_instruction_index = %s;",
            "const TZrUInt32 zr_aot_opcode = %s;",
            "unsupported AOT instruction",
            "ZrCore_Debug_RunError(state,",
            "ZR_AOT_C_FAIL();",
            "backend_aot_write_c_unsupported_instruction_expr(file,",
            "/* zr_aot_direct_return */",
            "frame.generatedFrameSlotCount",
            "execution_discard_exception_handlers_for_callinfo(state, zr_aot_call_info);",
            "ZrCore_Function_ApplyReturnEscape(state, frame.function, %u, zr_aot_result_value);",
            "ZrCore_Closure_CloseClosure(state,",
            "ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, zr_aot_call_info);",
            "ZR_AOT_C_RETURN(1);",
            "backend_aot_write_c_pending_control_transfer(FILE *file,",
            "zr_aot_pending_return",
            "zr_aot_pending_break",
            "zr_aot_pending_continue",
            "execution_set_pending_control(state,",
            "execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)",
            "execution_jump_to_instruction_offset(state,",
            "zr_aot_next_instruction = (TZrUInt32)(zr_aot_call_info->context.context.programCounter - frame.function->instructionsList);",
            "/* zr_aot_try_direct */",
            "execution_push_exception_handler(state, zr_aot_call_info, %u)",
            "generated AOT TRY failed to push exception handler",
            "/* zr_aot_end_try_direct */",
            "execution_find_handler_state(state, zr_aot_call_info, %u)",
            "frame.function->exceptionHandlerList[%u]",
            "handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;",
            "execution_pop_exception_handler(state, handlerState);",
            "/* zr_aot_throw_direct */",
            "ZrCore_Exception_NormalizeThrownValue(state,",
            "ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)",
            "execution_unwind_exception_to_handler(state, &zr_aot_call_info)",
            "ZrCore_Exception_Throw(state, state->currentExceptionStatus);",
            "/* zr_aot_catch_direct */",
            "ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException);",
            "ZrCore_Exception_ClearCurrent(state);",
            "ZrCore_Value_ResetAsNull(zr_aot_destination);",
            "execution_clear_pending_control(state);",
    };
    static const char *const exportNeedles[] = {
            "backend_aot_write_c_publish_exports(FILE *file)",
            "/* zr_aot_publish_exports_direct */",
            "frame.module == ZR_NULL || frame.moduleExecuted == ZR_NULL",
            "TZrStackValuePointer zr_aot_exported_values_top = frame.slotBase + frame.function->stackSize;",
            "const SZrFunctionExportedVariable *zr_aot_export = &frame.function->exportedVariables[zr_aot_export_index];",
            "zr_aot_export_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_export_value);",
            "ZrCore_Value_Copy(state, &zr_aot_published_value, zr_aot_export_value);",
            "if (zr_aot_export_metadata_function->closureValueLength == 0) {",
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
            "ZrCore_Module_AddPubExport(state, frame.module, zr_aot_export->name, &zr_aot_published_value);",
            "ZrCore_Module_AddProExport(state, frame.module, zr_aot_export->name, &zr_aot_published_value);",
            "*frame.moduleExecuted = ZR_TRUE;",
    };
    static const char *const valueSemirNeedles[] = {
            "zr_aot_skip_drop_slot = %u;",
            "ZR_AOT_C_RETURN(1);",
    };
    static const char *const forbiddenEmitterNeedles[] = {
            "#define ZR_AOT_C_FAIL() return",
    };
    static const char *const forbiddenGeneratedReturnNeedles[] = {
            "\"    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction",
            "\"    return ZrLibrary_AotRuntime_Return",
            "\"        return 1;\\n\"",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "publishExports || entry->function->exceptionHandlerCount > 0",
            "ZrLibrary_AotRuntime_Return(state, &frame, %u, %s)",
            "ZrLibrary_AotRuntime_Return(state, &frame, %u, ZR_TRUE)",
    };
    static const char *const forbiddenUnsupportedNeedles[] = {
            "ZrLibrary_AotRuntime_ReportUnsupportedInstruction",
            "ZrLibrary_AotRuntime_BeginInstruction(state, &frame",
            "ZrLibrary_AotRuntime_SetPendingReturn",
            "ZrLibrary_AotRuntime_SetPendingBreak",
            "ZrLibrary_AotRuntime_SetPendingContinue",
            "ZrLibrary_AotRuntime_Try(state, &frame",
            "ZrLibrary_AotRuntime_EndTry(state, &frame",
            "ZrLibrary_AotRuntime_Throw",
            "ZrLibrary_AotRuntime_Catch(state, &frame",
            "ZrLibrary_AotRuntime_Return(state, &frame",
    };
    static const char *const forbiddenControlNeedles[] = {
            "backend_aot_write_c_publish_exports(FILE *file)",
            "ZrLibrary_AotRuntime_MaterializeModuleExportValue(state, &frame, zr_aot_export_value, &zr_aot_published_value)",
    };
    static const char *const forbiddenExportNeedles[] = {
            "ZrLibrary_AotRuntime_MaterializeModuleExportValue(state, &frame, zr_aot_export_value, &zr_aot_published_value)",
    };
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *cleanupHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.h");
    char *cleanupSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *controlText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *exportText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_exports.c");
    char *valueSemirCallText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c");

    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(cleanupHeaderText);
    TEST_ASSERT_NOT_NULL(cleanupSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(exportText);
    TEST_ASSERT_NOT_NULL(valueSemirCallText);

    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_none(emitterText, emitterForbiddenNeedles, ARRAY_COUNT(emitterForbiddenNeedles));
    assert_text_contains_all(cleanupHeaderText, cleanupHeaderNeedles, ARRAY_COUNT(cleanupHeaderNeedles));
    assert_text_contains_all(cleanupSourceText, cleanupSourceNeedles, ARRAY_COUNT(cleanupSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(controlText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_all(exportText, exportNeedles, ARRAY_COUNT(exportNeedles));
    assert_text_contains_all(valueSemirCallText, valueSemirNeedles, ARRAY_COUNT(valueSemirNeedles));
    assert_text_contains_none(emitterText, forbiddenEmitterNeedles, ARRAY_COUNT(forbiddenEmitterNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenGeneratedReturnNeedles,
                              ARRAY_COUNT(forbiddenGeneratedReturnNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenFunctionBodyNeedles,
                              ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenUnsupportedNeedles,
                              ARRAY_COUNT(forbiddenUnsupportedNeedles));
    assert_text_contains_none(controlText, forbiddenControlNeedles, ARRAY_COUNT(forbiddenControlNeedles));
    assert_text_contains_none(exportText, forbiddenExportNeedles, ARRAY_COUNT(forbiddenExportNeedles));
    assert_text_contains_none(controlText, forbiddenGeneratedReturnNeedles, ARRAY_COUNT(forbiddenGeneratedReturnNeedles));
    assert_text_contains_none(controlText, forbiddenUnsupportedNeedles, ARRAY_COUNT(forbiddenUnsupportedNeedles));
    assert_text_contains_none(valueSemirCallText,
                              forbiddenGeneratedReturnNeedles,
                              ARRAY_COUNT(forbiddenGeneratedReturnNeedles));

    free(emitterText);
    free(cleanupHeaderText);
    free(cleanupSourceText);
    free(functionBodyText);
    free(controlText);
    free(exportText);
    free(valueSemirCallText);
}

static void test_aot_c_generated_abi_header_is_public(void) {
    static const char *const abiHeaderNeedles[] = {
            "ZR_VM_COMMON_ZR_AOT_ABI_H",
            "ZR_VM_AOT_ABI_VERSION",
            "EZrAotBackendKind",
            "ZR_AOT_BACKEND_KIND_C",
            "ZR_AOT_BACKEND_KIND_LLVM",
            "EZrAotInputKind",
            "ZR_AOT_INPUT_KIND_SOURCE",
            "ZR_AOT_INPUT_KIND_BINARY",
            "FZrAotEntryThunk",
            "ZrAotCompiledModule",
            "FZrVmGetAotCompiledModule",
            "ZR_VM_AOT_EXPORT",
    };
    static const char *const emitterNeedles[] = {
            "#include \"zr_vm_common/zr_aot_abi.h\"",
            "static const ZrAotCompiledModule zr_aot_module",
            "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void)",
            "ZR_AOT_BACKEND_KIND_C",
            "ZR_VM_AOT_ABI_VERSION",
    };
    char *abiHeaderText = read_repo_text_file_owned("zr_vm_common/include/zr_vm_common/zr_aot_abi.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");

    TEST_ASSERT_NOT_NULL(abiHeaderText);
    TEST_ASSERT_NOT_NULL(emitterText);

    assert_text_contains_all(abiHeaderText, abiHeaderNeedles, ARRAY_COUNT(abiHeaderNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));

    free(abiHeaderText);
    free(emitterText);
}

static void test_aot_c_writer_options_are_public(void) {
    static const char *const writerHeaderNeedles[] = {
            "#include \"zr_vm_common/zr_aot_abi.h\"",
            "typedef struct SZrAotWriterOptions",
            "const TZrChar *moduleName;",
            "const TZrChar *sourceHash;",
            "const TZrChar *zroHash;",
            "TZrUInt32 inputKind;",
            "const TZrChar *inputHash;",
            "const TZrByte *embeddedModuleBlob;",
            "TZrSize embeddedModuleBlobLength;",
            "TZrBool requireExecutableLowering;",
            "ZrParser_Writer_WriteAotCFileWithOptions(",
            "const SZrAotWriterOptions *options",
            "ZrParser_Writer_WriteAotLlvmFileWithOptions(",
    };
    static const char *const internalHeaderNeedles[] = {
            "#include \"zr_vm_parser/writer.h\"",
            "backend_aot_option_text(const SZrAotWriterOptions *options",
            "backend_aot_option_input_kind(const SZrAotWriterOptions *options)",
            "backend_aot_option_input_hash(const SZrAotWriterOptions *options",
    };
    char *writerHeaderText = read_repo_text_file_owned("zr_vm_parser/include/zr_vm_parser/writer.h");
    char *internalHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h");

    TEST_ASSERT_NOT_NULL(writerHeaderText);
    TEST_ASSERT_NOT_NULL(internalHeaderText);

    assert_text_contains_all(writerHeaderText, writerHeaderNeedles, ARRAY_COUNT(writerHeaderNeedles));
    assert_text_contains_all(internalHeaderText, internalHeaderNeedles, ARRAY_COUNT(internalHeaderNeedles));

    free(writerHeaderText);
    free(internalHeaderText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_value_semir_with_frame_layout);
    RUN_TEST(test_aot_c_source_lowers_primitive_constants_to_direct_value_writes);
    RUN_TEST(test_aot_c_source_lowers_legacy_int_arithmetic_to_direct_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_generic_numeric_arithmetic_to_direct_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_generic_primitive_conversions_to_direct_c_writes);
    RUN_TEST(test_aot_c_source_lowers_typed_arithmetic_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_signed_load_const_arithmetic_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_signed_load_stack_const_arithmetic_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_signed_load_stack_load_const_arithmetic_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_signed_load_stack_arithmetic_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_bitwise_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_bool_equality_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_signed_branch_to_c_comparisons);
    RUN_TEST(test_aot_c_source_lowers_typed_numeric_conversion_to_c_casts);
    RUN_TEST(test_aot_c_source_emits_value_frame_cleanup_exit);
    RUN_TEST(test_aot_c_generated_abi_header_is_public);
    RUN_TEST(test_aot_c_writer_options_are_public);
    return UNITY_END();
}
