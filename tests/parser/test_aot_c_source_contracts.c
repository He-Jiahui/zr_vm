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
            "backend_aot_c_find_frame_slot_layout(",
            "backend_aot_c_resolve_field_layout(",
            "frameLayout->slotLayouts",
            "ZrCore_Function_ResolvePrototypeFrameFieldLayout(state",
            "ZR_SEMIR_OPCODE_FIELD_ADDR",
            "ZR_SEMIR_OPCODE_LOAD_VALUE",
            "ZR_SEMIR_OPCODE_STORE_VALUE",
            "ZR_SEMIR_OPCODE_COPY_VALUE",
            "ZR_SEMIR_OPCODE_CALL_TYPED",
            "ZR_SEMIR_OPCODE_RETURN_TYPED",
            "zr_aot_value_field_addr",
            "zr_aot_value_load",
            "zr_aot_value_store",
            "zr_aot_value_copy",
            "zr_aot_value_call_typed",
            "zr_aot_value_return_typed",
            "zr_aot_value_expr_inline_copy",
            "zr_aot_value_expr_field_load",
            "zr_aot_value_expr_field_store",
            "memmove((TZrByte *)frame.slotBase +",
            "fieldLayout.byteOffset",
            "zr_aot_value_exec_inline_copy",
            "zr_aot_value_exec_inline_field_copy",
            "const SZrTypeLayout *zr_aot_copy_layout =",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function",
            "ZrCore_TypeLayout_CanRawCopy(zr_aot_copy_layout)",
            "ZrCore_TypeLayout_CopyInline(state,",
            "case ZR_SEMIR_OPCODE_COPY_VALUE",
            "zr_aot_value_exec_field_load",
            "zr_aot_value_exec_field_store",
            "backend_aot_c_value_field_layout_can_value_slot_exec",
            "zr_aot_value_exec_field_value_slot_load",
            "zr_aot_value_exec_field_value_slot_store",
            "ZrCore_Value_Copy(state, zr_aot_destination, (const SZrTypeValue *)zr_aot_field);",
            "ZrCore_Value_Copy(state, (SZrTypeValue *)zr_aot_field, zr_aot_source);",
            "backend_aot_c_value_field_layout_can_inline_struct_exec",
            "zr_aot_value_exec_field_inline_struct_load",
            "zr_aot_value_exec_field_inline_struct_store",
            "memmove((TZrByte *)frame.slotBase + %u, (const TZrByte *)frame.slotBase + %u + %u, %u);",
            "memmove((TZrByte *)frame.slotBase + %u + %u, (const TZrByte *)frame.slotBase + %u, %u);",
            "zr_aot_value_unsupported_field_load",
            "zr_aot_value_unsupported_field_store",
            "unsupported AOT value SemIR field load",
            "unsupported AOT value SemIR field store",
            "fieldLayout.isPrimitivePod",
            "fieldLayout.isValueSlot",
            "fieldLayout.typeLayoutId",
            "zr_aot_value_exec_call_typed",
            "zr_aot_value_exec_return_typed",
            "ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,",
            "case ZR_SEMIR_OPCODE_LOAD_VALUE",
            "case ZR_SEMIR_OPCODE_STORE_VALUE",
            "case ZR_SEMIR_OPCODE_CALL_TYPED",
            "case ZR_SEMIR_OPCODE_RETURN_TYPED",
            "const TZrByte *zr_aot_field = (const TZrByte *)frame.slotBase + %u + %u;",
            "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "memcpy(&zr_aot_field_value, zr_aot_field, sizeof(zr_aot_field_value));",
            "ZR_VALUE_FAST_SET(zr_aot_destination,",
            "TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;",
            "const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "memcpy(zr_aot_field, &zr_aot_stored_value, sizeof(zr_aot_stored_value));",
            "ZrCore_Function_TryCopyInlineFrameReturnValue(state,",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout",
            "ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1)",
            "state->stackTop.valuePointer = zr_aot_return_source + 1;",
    };
    static const char *const functionBodyNeedles[] = {
            "#include \"backend_aot_c_value_semir.h\"",
            "backend_aot_write_c_value_semir_for_function(",
            "backend_aot_write_c_value_semir_for_function(file, state, module",
            "&functionIr->frameLayout",
            "backend_aot_try_write_c_value_semir_for_exec_instruction(",
            "break;\n                }\n                backend_aot_write_c_direct_stack_copy",
            "case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):\n                if (backend_aot_try_write_c_value_semir_for_exec_instruction(",
            "case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):\n                if (backend_aot_try_write_c_value_semir_for_exec_instruction(",
            "case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):",
            "case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):",
            "case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):",
            "case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):",
            "case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):\n                if (backend_aot_try_write_c_value_semir_for_exec_instruction(",
    };
    char *valueLoweringHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.h");
    char *valueLoweringSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");

    TEST_ASSERT_NOT_NULL(valueLoweringHeaderText);
    TEST_ASSERT_NOT_NULL(valueLoweringSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);

    assert_text_contains_all(valueLoweringHeaderText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(valueLoweringSourceText, sourceNeedles, ARRAY_COUNT(sourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));

    free(valueLoweringHeaderText);
    free(valueLoweringSourceText);
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
            "#include \\\"zr_vm_core/type_layout.h\\\"",
            "#define ZR_AOT_C_RETURN(expr)",
            "zr_aot_return_value = (expr);",
            "goto zr_aot_function_exit;",
            "#define ZR_AOT_C_FAIL() ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_FailGeneratedFunction(state, &frame))",
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
            "TZrInt64 zr_aot_return_value = 0;",
            "TZrBool zr_aot_frame_started = ZR_FALSE;",
            "TZrUInt32 zr_aot_skip_drop_slot = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;",
            "zr_aot_frame_started = ZR_TRUE;",
            "zr_aot_function_exit:",
            "if (zr_aot_frame_started) {",
            "return zr_aot_return_value;",
            "backend_aot_write_c_frame_cleanup(file, &functionIr->frameLayout);",
    };
    static const char *const controlNeedles[] = {
            "ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state",
            "ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_Return(state, &frame",
            "ZR_AOT_C_RETURN(1);",
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
    char *valueSemirText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c");

    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(cleanupHeaderText);
    TEST_ASSERT_NOT_NULL(cleanupSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(valueSemirText);

    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(cleanupHeaderText, cleanupHeaderNeedles, ARRAY_COUNT(cleanupHeaderNeedles));
    assert_text_contains_all(cleanupSourceText, cleanupSourceNeedles, ARRAY_COUNT(cleanupSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(controlText, controlNeedles, ARRAY_COUNT(controlNeedles));
    assert_text_contains_all(valueSemirText, valueSemirNeedles, ARRAY_COUNT(valueSemirNeedles));
    assert_text_contains_none(emitterText, forbiddenEmitterNeedles, ARRAY_COUNT(forbiddenEmitterNeedles));
    assert_text_contains_none(functionBodyText,
                              forbiddenGeneratedReturnNeedles,
                              ARRAY_COUNT(forbiddenGeneratedReturnNeedles));
    assert_text_contains_none(controlText, forbiddenGeneratedReturnNeedles, ARRAY_COUNT(forbiddenGeneratedReturnNeedles));
    assert_text_contains_none(valueSemirText,
                              forbiddenGeneratedReturnNeedles,
                              ARRAY_COUNT(forbiddenGeneratedReturnNeedles));

    free(emitterText);
    free(cleanupHeaderText);
    free(cleanupSourceText);
    free(functionBodyText);
    free(controlText);
    free(valueSemirText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_value_semir_with_frame_layout);
    RUN_TEST(test_aot_c_source_lowers_typed_arithmetic_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_bitwise_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_bool_equality_to_c_expressions);
    RUN_TEST(test_aot_c_source_lowers_typed_numeric_conversion_to_c_casts);
    RUN_TEST(test_aot_c_source_emits_value_frame_cleanup_exit);
    return UNITY_END();
}
