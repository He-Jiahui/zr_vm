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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_generic_numeric_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_generic_numeric_contracts.c");
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

static void test_aot_c_source_lowers_generic_numeric_float_binary_local_before_boundary_helper(void) {
    static const char *const moduleNeedles[] = {
            "backend_aot_write_c_direct_add(",
            "backend_aot_write_c_direct_sub(",
            "backend_aot_write_c_direct_mul(",
            "backend_aot_write_c_direct_div(",
            "backend_aot_write_c_direct_mod(",
            "backend_aot_write_c_direct_neg(",
            "const SZrAotExecIrFunction *functionIr,",
            "backend_aot_c_write_generic_numeric_f64_binary_scalar_local(",
            "backend_aot_c_write_generic_numeric_i64_binary_scalar_local(",
            "backend_aot_c_write_generic_numeric_u64_binary_scalar_local(",
            "backend_aot_c_write_generic_numeric_mixed_i64_u64_binary_scalar_local(",
            "backend_aot_c_write_generic_numeric_mixed_i64_u64_div_scalar_local(",
            "backend_aot_c_write_generic_numeric_mixed_i64_u64_mod_scalar_local(",
            "backend_aot_c_write_generic_numeric_mixed_f64_binary_scalar_local(",
            "backend_aot_c_write_generic_numeric_mixed_f64_div_scalar_local(",
            "backend_aot_c_write_generic_numeric_mixed_f64_mod_scalar_local(",
            "backend_aot_c_generic_numeric_scalar_kinds_form_mixed_i64_u64(",
            "backend_aot_c_generic_numeric_i64_expression_prefix(",
            "const char *operatorToken",
            "zr_aot_generic_numeric_i64_add_scalar_local",
            "zr_aot_generic_numeric_i64_sub_scalar_local",
            "zr_aot_generic_numeric_i64_mul_scalar_local",
            "zr_aot_generic_numeric_u64_add_scalar_local",
            "zr_aot_generic_numeric_u64_sub_scalar_local",
            "zr_aot_generic_numeric_u64_mul_scalar_local",
            "backend_aot_c_write_generic_numeric_u64_div_scalar_local(",
            "zr_aot_generic_numeric_u64_div_scalar_local",
            "backend_aot_c_write_generic_numeric_u64_mod_scalar_local(",
            "zr_aot_generic_numeric_u64_mod_scalar_local",
            "backend_aot_c_write_generic_numeric_i64_div_scalar_local(",
            "zr_aot_generic_numeric_i64_div_scalar_local",
            "backend_aot_c_write_generic_numeric_i64_mod_scalar_local(",
            "zr_aot_generic_numeric_i64_mod_scalar_local",
            "backend_aot_c_write_generic_numeric_i64_neg_scalar_local(",
            "zr_aot_generic_numeric_i64_neg_scalar_local",
            "backend_aot_c_write_generic_numeric_u64_neg_to_i64_scalar_local(",
            "zr_aot_generic_numeric_u64_neg_to_i64_scalar_local",
            "zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local",
            "zr_aot_generic_numeric_mixed_i64_u64_sub_scalar_local",
            "zr_aot_generic_numeric_mixed_i64_u64_mul_scalar_local",
            "zr_aot_generic_numeric_mixed_i64_u64_div_scalar_local",
            "zr_aot_generic_numeric_mixed_i64_u64_mod_scalar_local",
            "zr_aot_generic_numeric_mixed_f64_add_scalar_local",
            "zr_aot_generic_numeric_mixed_f64_sub_scalar_local",
            "zr_aot_generic_numeric_mixed_f64_mul_scalar_local",
            "zr_aot_generic_numeric_mixed_f64_div_scalar_local",
            "zr_aot_generic_numeric_mixed_f64_mod_scalar_local",
            "zr_aot_generic_numeric_f64_add_scalar_local",
            "zr_aot_generic_numeric_f64_sub_scalar_local",
            "zr_aot_generic_numeric_f64_mul_scalar_local",
            "backend_aot_c_write_generic_numeric_f64_div_scalar_local(",
            "zr_aot_generic_numeric_f64_div_scalar_local",
            "backend_aot_c_write_generic_numeric_f64_mod_scalar_local(",
            "zr_aot_generic_numeric_f64_mod_scalar_local",
            "backend_aot_c_write_generic_numeric_f64_neg_scalar_local(",
            "zr_aot_generic_numeric_f64_neg_scalar_local",
            "backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex)",
            "zr_aot_f%u = zr_aot_f%u %s zr_aot_f%u;",
            "zr_aot_s%u = zr_aot_s%u %s zr_aot_s%u;",
            "zr_aot_u%u = zr_aot_u%u %s zr_aot_u%u;",
            "zr_aot_s%u = -zr_aot_s%u;",
            "if (zr_aot_s%u == (TZrInt64)0)",
            "if (zr_aot_u%u == (TZrUInt64)0u)",
            "zr_aot_s%u = zr_aot_s%u / zr_aot_s%u;",
            "zr_aot_s%u = zr_aot_s%u %% zr_aot_s%u;",
            "zr_aot_u%u = zr_aot_u%u / zr_aot_u%u;",
            "zr_aot_u%u = zr_aot_u%u %% zr_aot_u%u;",
            "zr_aot_s%u = -(TZrInt64)zr_aot_u%u;",
            "if (zr_aot_f%u == (TZrFloat64)0.0)",
            "ZrCore_Debug_RunError(state, \\\"divide by zero\\\")",
            "zr_aot_f%u = zr_aot_f%u / zr_aot_f%u;",
            "ZrCore_Debug_RunError(state, \\\"modulo by zero\\\")",
            "zr_aot_f%u = fmod(zr_aot_f%u, zr_aot_f%u);",
            "zr_aot_f%u = -zr_aot_f%u;",
            "zr_aot_f%u = %s%u %s %s%u;",
            "if (%s%u == (TZrFloat64)0.0)",
            "zr_aot_f%u = %s%u / %s%u;",
            "zr_aot_f%u = fmod(%s%u, %s%u);",
            "(TZrFloat64)zr_aot_s",
            "(TZrInt64)zr_aot_u",
            "zr_aot_s%u = %s%u %s %s%u;",
            "zr_aot_arith_exec_generic_numeric_binary_boundary",
            "zr_aot_arith_exec_generic_numeric_unary_boundary",
            "ZrLibrary_AotRuntime_GenericNumericAdd",
            "ZrLibrary_AotRuntime_GenericNumericSub",
            "ZrLibrary_AotRuntime_GenericNumericMul",
            "ZrLibrary_AotRuntime_GenericNumericDiv",
            "ZrLibrary_AotRuntime_GenericNumericMod",
            "ZrLibrary_AotRuntime_GenericNumericNeg",
            "backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot)",
            "backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot)",
            "zr_aot_generic_numeric_sync_i64_local_boundary",
            "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, %u, &zr_aot_s%u)",
            "zr_aot_generic_numeric_sync_u64_local_boundary",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, %u, &zr_aot_u%u)",
            "zr_aot_generic_numeric_sync_f64_local_boundary",
            "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, %u, &zr_aot_f%u)",
    };
    static const char *const scalarLocalsNeedles[] = {
            "backend_aot_c_scalar_locals_record_generic_numeric_destinations(",
            "backend_aot_c_scalar_locals_stack_copy_source_constant_kind_before_instruction(",
            "backend_aot_c_scalar_locals_f64_value_kind_before_instruction(",
            "backend_aot_c_scalar_locals_slot_is_generic_numeric_i64_binary_operand(",
            "backend_aot_c_scalar_locals_slot_is_generic_numeric_u64_binary_operand(",
            "backend_aot_c_scalar_locals_slot_is_generic_numeric_mixed_i64_u64_binary_operand(",
            "backend_aot_c_scalar_locals_slot_is_generic_numeric_mixed_f64_binary_operand(",
            "backend_aot_c_scalar_locals_kinds_form_mixed_i64_u64_numeric(",
            "backend_aot_c_scalar_locals_kinds_form_mixed_f64_numeric(",
            "backend_aot_c_scalar_locals_instruction_is_generic_numeric_mixed_i64_u64_binary_opcode(",
            "backend_aot_c_scalar_locals_instruction_is_generic_numeric_mixed_f64_binary_opcode(",
            "backend_aot_c_scalar_locals_slot_is_generic_numeric_i64_unary_operand(",
            "backend_aot_c_scalar_locals_instruction_is_generic_numeric_i64_binary_opcode(",
            "backend_aot_c_scalar_locals_instruction_is_generic_numeric_u64_binary_opcode(",
            "return (TZrBool)(opcode == ZR_INSTRUCTION_ENUM(ADD) ||",
            "opcode == ZR_INSTRUCTION_ENUM(SUB) ||",
            "opcode == ZR_INSTRUCTION_ENUM(MUL) ||",
            "opcode == ZR_INSTRUCTION_ENUM(DIV) ||",
            "opcode == ZR_INSTRUCTION_ENUM(MOD));",
            "return (TZrBool)(opcode == ZR_INSTRUCTION_ENUM(ADD) ||",
            "opcode == ZR_INSTRUCTION_ENUM(SUB) ||",
            "opcode == ZR_INSTRUCTION_ENUM(MUL) ||",
            "opcode == ZR_INSTRUCTION_ENUM(DIV) ||",
            "opcode == ZR_INSTRUCTION_ENUM(MOD));",
            "backend_aot_c_scalar_locals_record_stack_copy_destinations(slotKinds, slotCount, function);",
            "opcode != ZR_INSTRUCTION_ENUM(ADD) &&",
            "opcode != ZR_INSTRUCTION_ENUM(SUB) &&",
            "opcode != ZR_INSTRUCTION_ENUM(MUL) &&",
            "opcode != ZR_INSTRUCTION_ENUM(DIV) &&",
            "opcode == ZR_INSTRUCTION_ENUM(MOD)",
            "opcode == ZR_INSTRUCTION_ENUM(NEG)",
            "ZR_AOT_SCALAR_LOCAL_KIND_F64",
            "ZR_AOT_SCALAR_LOCAL_KIND_I64",
            "ZR_AOT_SCALAR_LOCAL_KIND_U64",
            "backend_aot_c_scalar_locals_record_generic_numeric_i64_binary_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_u64_binary_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_mixed_i64_u64_binary_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_mixed_f64_binary_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_i64_unary_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_u64_neg_to_i64_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_f64_binary_exec_write(",
            "backend_aot_c_scalar_locals_record_generic_numeric_f64_unary_exec_write(",
            "(slotKinds[leftSlot] & ZR_AOT_SCALAR_LOCAL_KIND_F64) == 0",
            "(slotKinds[leftSlot] & ZR_AOT_SCALAR_LOCAL_KIND_I64) == 0",
            "(slotKinds[leftSlot] & ZR_AOT_SCALAR_LOCAL_KIND_U64) == 0",
            "(slotKinds[rightSlot] & ZR_AOT_SCALAR_LOCAL_KIND_U64) == 0",
            "(slotKinds[sourceSlot] & ZR_AOT_SCALAR_LOCAL_KIND_F64) == 0",
            "(slotKinds[sourceSlot] & ZR_AOT_SCALAR_LOCAL_KIND_I64) == 0",
            "(slotKinds[sourceSlot] & ZR_AOT_SCALAR_LOCAL_KIND_U64) == 0",
            "backend_aot_c_scalar_locals_generic_numeric_i64_binary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_u64_binary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_mixed_i64_u64_binary_has_operand_slots(",
            "backend_aot_c_scalar_locals_generic_numeric_mixed_i64_u64_binary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_mixed_f64_binary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_i64_unary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_u64_neg_to_i64_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_f64_binary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_f64_unary_reads_slot(",
            "case ZR_INSTRUCTION_ENUM(ADD):",
            "case ZR_INSTRUCTION_ENUM(SUB):",
            "case ZR_INSTRUCTION_ENUM(MUL):",
            "case ZR_INSTRUCTION_ENUM(DIV):",
            "case ZR_INSTRUCTION_ENUM(MOD):",
            "case ZR_INSTRUCTION_ENUM(NEG):",
            "backend_aot_c_scalar_locals_generic_numeric_i64_binary_reads_slot(",
            "functionIr, instruction, slot) ||",
            "functionIr, instruction, slot, ZR_AOT_SCALAR_LOCAL_KIND_I64",
            "functionIr, instruction, slot, ZR_AOT_SCALAR_LOCAL_KIND_U64",
            ("case ZR_INSTRUCTION_ENUM(DIV):\n"
             "        case ZR_INSTRUCTION_ENUM(MOD):\n"
             "        case ZR_INSTRUCTION_OP_ADD_UNSIGNED:"),
            ("case ZR_INSTRUCTION_OP_MOD_UNSIGNED:\n"
             "            return (TZrBool)(backend_aot_c_scalar_locals_generic_numeric_u64_binary_reads_slot("),
            "backend_aot_c_scalar_locals_generic_numeric_mixed_i64_u64_binary_reads_slot(",
            "backend_aot_c_scalar_locals_generic_numeric_mixed_f64_binary_reads_slot(",
            "return backend_aot_c_scalar_locals_generic_numeric_i64_unary_reads_slot(functionIr, instruction, slot);",
            "backend_aot_c_scalar_locals_generic_numeric_f64_binary_reads_slot(",
            "functionIr, instruction, slot, ZR_AOT_SCALAR_LOCAL_KIND_F64",
            "return backend_aot_c_scalar_locals_generic_numeric_f64_unary_reads_slot(functionIr, instruction, slot);",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GenericNumericAdd(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericNumericSub(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericNumericMul(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericNumericDiv(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericNumericMod(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericNumericNeg(struct SZrState *state,",
    };
    static const char *const runtimeValuesNeedles[] = {
            "#include <math.h>",
            "ZrLibrary_AotRuntime_GenericNumericMod(SZrState *state,",
            "modulo by zero",
            "fmod(leftFloat, rightFloat)",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(ADD):",
            "backend_aot_write_c_direct_add(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(SUB):",
            "backend_aot_write_c_direct_sub(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(MUL):",
            "backend_aot_write_c_direct_mul(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(DIV):",
            "backend_aot_write_c_direct_div(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(MOD):",
            "backend_aot_write_c_direct_mod(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(NEG):",
            "backend_aot_write_c_direct_neg(file, functionIr, destinationSlot, operandA1, instructionIndex);",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "backend_aot_c_write_generic_numeric_zero_guard",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type) || ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)",
            "TZrFloat64 zr_aot_left_float;",
            "TZrFloat64 zr_aot_right_float;",
            "fmod(zr_aot_left_float, zr_aot_right_float)",
            "ZR_VALUE_TYPE_DOUBLE",
            "zr_aot_left_int % zr_aot_right_int",
            "zr_aot_left_uint % zr_aot_right_uint",
    };
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeValuesText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");

    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeValuesText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);

    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalsNeedles, ARRAY_COUNT(scalarLocalsNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeValuesText, runtimeValuesNeedles, ARRAY_COUNT(runtimeValuesNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));

    free(moduleText);
    free(runtimeHeaderText);
    free(runtimeValuesText);
    free(functionBodyText);
    free(scalarLocalsText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_generic_numeric_float_binary_local_before_boundary_helper);
    return UNITY_END();
}
