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

    marker = strstr(sourceFile, "tests/parser/test_aot_c_logical_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_logical_contracts.c");
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

static void test_aot_c_source_lowers_generic_truthiness_to_boundary_helpers(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_not(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "TZrUInt32 execInstructionIndex",
            "backend_aot_write_c_direct_jump_if(FILE *file",
            "backend_aot_c_null_constant_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_jump_if(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_bool_constant_consumed_by_local_logical_not(",
            "backend_aot_c_bool_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_reset_null_consumed_by_local_jump_if(",
            "backend_aot_c_reset_null_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_reset_null_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_write_c_null_constant_stack_copy_local_logical_not_skip(FILE *file",
            "backend_aot_write_c_null_constant_stack_copy_local_jump_if_skip(FILE *file",
            "backend_aot_write_c_reset_null_stack_copy_local_logical_not_skip(FILE *file",
            "backend_aot_write_c_reset_null_stack_copy_local_jump_if_skip(FILE *file",
            "backend_aot_write_c_string_constant_stack_copy_local_logical_not_skip(FILE *file",
            "backend_aot_write_c_string_constant_stack_copy_local_jump_if_skip(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_c_write_bool_local_sync",
            "backend_aot_write_c_direct_logical_not(",
            "backend_aot_write_c_direct_jump_if(",
            "backend_aot_c_write_generic_jump_if_scalar_local(",
            "backend_aot_c_write_generic_logical_not_scalar_local(",
            "backend_aot_c_null_constant_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_jump_if(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_bool_constant_consumed_by_local_logical_not(",
            "backend_aot_c_bool_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_to_string_slot_written_immediately_before(",
            "backend_aot_c_write_string_slot_truthiness(",
            "backend_aot_c_to_object_slot_written_immediately_before(",
            "backend_aot_c_write_object_slot_truthiness(",
            "backend_aot_c_reset_null_consumed_by_local_jump_if(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_value_written_before(functionIr, sourceSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, sourceSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, sourceSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, sourceSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_value_written_before(functionIr, conditionSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, conditionSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, conditionSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, conditionSlot, execInstructionIndex)",
            "zr_aot_generic_logical_not",
            "zr_aot_generic_logical_not_null_constant_local",
            "zr_aot_generic_logical_not_null_stack_copy_local",
            "zr_aot_generic_logical_not_bool_constant_local",
            "zr_aot_generic_logical_not_string_constant_local",
            "zr_aot_generic_logical_not_string_stack_copy_local",
            "zr_aot_generic_logical_not_string_slot_local",
            "zr_aot_generic_logical_not_object_slot_local",
            "zr_aot_generic_logical_not_reset_null_stack_copy_local",
            "zr_aot_generic_logical_not_scalar_local",
            "zr_aot_generic_logical_not_i64_scalar_local",
            "zr_aot_generic_logical_not_u64_scalar_local",
            "zr_aot_generic_logical_not_f64_scalar_local",
            "ZR_CAST_STRING(state, zr_aot_ref_locals.o%u)",
            "ZrCore_String_GetByteLength(zr_aot_string_slot_string) > 0u",
            "zr_aot_b%u = (TZrBool)(!zr_aot_string_slot_truthy);",
            "zr_aot_ref_locals.o%u != ZR_NULL",
            "zr_aot_b%u = (TZrBool)(!zr_aot_object_slot_truthy);",
            "zr_aot_b%u = ZR_TRUE;",
            "zr_aot_b%u = (TZrBool)(!zr_aot_b%u);",
            "zr_aot_b%u = (TZrBool)(zr_aot_s%u == (TZrInt64)0);",
            "zr_aot_b%u = (TZrBool)(zr_aot_u%u == (TZrUInt64)0u);",
            "zr_aot_b%u = (TZrBool)(zr_aot_f%u == (TZrFloat64)0.0);",
            "zr_aot_generic_jump_if",
            "zr_aot_generic_jump_if_bool_scalar_local",
            "zr_aot_generic_jump_if_i64_scalar_local",
            "zr_aot_generic_jump_if_u64_scalar_local",
            "zr_aot_generic_jump_if_f64_scalar_local",
            "zr_aot_generic_jump_if_null_constant_false",
            "zr_aot_generic_jump_if_null_stack_copy_false",
            "zr_aot_generic_jump_if_bool_constant_false",
            "zr_aot_generic_jump_if_bool_constant_true",
            "zr_aot_generic_jump_if_string_constant_false",
            "zr_aot_generic_jump_if_string_constant_true",
            "zr_aot_generic_jump_if_string_stack_copy_false",
            "zr_aot_generic_jump_if_string_stack_copy_true",
            "zr_aot_generic_jump_if_string_slot_local",
            "zr_aot_generic_jump_if_object_slot_local",
            "zr_aot_generic_jump_if_reset_null_false",
            "zr_aot_generic_jump_if_reset_null_stack_copy_false",
            "TZrBool zr_aot_truthy = ZR_FALSE;",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, %u, %u)",
            "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, %u, &zr_aot_truthy)",
            "backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);",
            "zr_aot_generic_logical_sync_bool_local_boundary",
            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
            "if (!zr_aot_b%u) {",
            "if (zr_aot_s%u == (TZrInt64)0) {",
            "if (zr_aot_u%u == (TZrUInt64)0u) {",
            "if (zr_aot_f%u == (TZrFloat64)0.0) {",
            "if (!zr_aot_truthy) {",
    };
    static const char *const constantConsumerNeedles[] = {
            "#include \"zr_vm_core/string.h\"",
            "backend_aot_c_function_exports_stack_slot(",
            "backend_aot_c_null_constant_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_jump_if(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_bool_constant_consumed_by_local_logical_not(",
            "backend_aot_c_bool_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_truthy(",
            "ZrCore_String_GetByteLength(stringValue) > 0u",
            "backend_aot_c_string_constant_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(",
    };
    static const char *const valuesNeedles[] = {
            "backend_aot_write_c_direct_primitive_constant(",
            "const SZrTypeValue *zr_aot_to_string_value = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_ref_locals.o%u = zr_aot_to_string_value->value.object;",
            "const SZrTypeValue *zr_aot_to_object_value = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "ZR_VALUE_IS_TYPE_NULL(zr_aot_to_object_value->type)",
            "ZR_VALUE_IS_TYPE_OBJECT(zr_aot_to_object_value->type)",
            "zr_aot_ref_locals.o%u = zr_aot_to_object_value->value.object;",
            "backend_aot_c_reset_null_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_reset_null_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(",
            "zr_aot_null_constant_stack_copy_local_logical_not_constant_skip",
            "zr_aot_null_constant_stack_copy_local_jump_if_constant_skip",
            "zr_aot_null_constant_stack_copy_local_logical_not_source_skip",
            "zr_aot_null_constant_stack_copy_local_jump_if_source_skip",
            "zr_aot_bool_constant_local_logical_not_source_skip",
            "zr_aot_string_constant_local_logical_not_source_skip",
            "zr_aot_string_constant_local_jump_if_source_skip",
            "zr_aot_string_constant_stack_copy_local_logical_not_constant_skip",
            "zr_aot_string_constant_stack_copy_local_jump_if_constant_skip",
            "zr_aot_string_constant_stack_copy_local_logical_not_source_skip",
            "zr_aot_string_constant_stack_copy_local_jump_if_source_skip",
            "zr_aot_reset_null_stack_copy_local_logical_not_reset_skip",
            "zr_aot_reset_null_stack_copy_local_jump_if_reset_skip",
            "zr_aot_reset_null_stack_copy_local_logical_not_source_skip",
            "zr_aot_reset_null_stack_copy_local_jump_if_source_skip",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(struct SZrState *state,",
            "ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "aot_runtime_generic_logical_values(",
            "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(SZrState *state,",
            "unsupported AOT generic primitive truthiness",
            "ZR_VALUE_FAST_SET(destinationValue, nativeBool, !truthy, ZR_VALUE_TYPE_BOOL);",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):",
            "backend_aot_write_c_reference_locals(file, functionIr);",
            "backend_aot_write_c_direct_logical_not(file, functionIr, destinationSlot, operandA1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF):",
            "backend_aot_write_c_direct_jump_if(file,\n"
            "                                                       functionIr,\n"
            "                                                       entry->flatIndex",
            "entry->flatIndex",
            "destinationSlot",
            "instructionIndex",
            "backend_aot_c_string_constant_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_jump_if(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_write_c_string_constant_stack_copy_local_logical_not_skip(",
            "backend_aot_write_c_string_constant_stack_copy_local_jump_if_skip(",
            "backend_aot_write_c_reset_null_stack_copy_local_logical_not_skip(",
            "backend_aot_write_c_reset_null_stack_copy_local_jump_if_skip(",
    };
    static const char *const scalarLocalNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(JUMP_IF):\n"
            "        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):\n"
            "            return (TZrBool)(instruction->instruction.operandExtra == slot);",
            "backend_aot_c_scalar_locals_truthiness_consumer_kind(",
            "kind | backend_aot_c_scalar_locals_truthiness_consumer_kind(candidateKind)",
            "if (opcode == ZR_INSTRUCTION_ENUM(LOGICAL_NOT) &&\n"
            "            instruction->instruction.operand.operand1[0] == destinationSlot) {\n"
            "            kind = (EZrAotScalarLocalKind)(\n"
            "                    kind | backend_aot_c_scalar_locals_truthiness_consumer_kind(candidateKind));\n"
            "            continue;\n"
            "        }",
            "backend_aot_c_scalar_locals_kind_from_call_result_callee(",
            "backend_aot_c_scalar_locals_resolve_callable_slot_function_before_instruction(",
            "backend_aot_c_can_emit_typed_i64_no_arg_thunk(calleeFunction)",
            "opcode == ZR_INSTRUCTION_ENUM(LOGICAL_NOT) &&",
            "instruction->instruction.operand.operand1[0] == destinationSlot",
            "EZrAotScalarLocalKind candidateKind = destinationSlot < slotCount",
            "backend_aot_c_scalar_locals_kind_from_generic_logical_not_destination_consumers(",
            "backend_aot_c_scalar_locals_record_generic_logical_not_destinations(",
            "kind = (EZrAotScalarLocalKind)(kind | ZR_AOT_SCALAR_LOCAL_KIND_BOOL);",
            "(EZrAotScalarLocalKind)(consumerKind & ZR_AOT_SCALAR_LOCAL_KIND_BOOL)",
    };
    static const char *const frameDescriptorNeedles[] = {
            "backend_aot_c_frame_descriptor_generic_jump_if_condition_can_use_local_only(",
            "case ZR_INSTRUCTION_ENUM(JUMP_IF):\n"
            "            return (TZrBool)(backend_aot_c_frame_descriptor_branch_target_is_valid(",
            "backend_aot_c_scalar_locals_bool_value_written_before(\n"
            "                             functionIr, conditionSlot, instructionIndex)",
            "backend_aot_c_scalar_locals_i64_written_before(\n"
            "                              functionIr, conditionSlot, instructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(\n"
            "                              functionIr, conditionSlot, instructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(\n"
            "                              functionIr, conditionSlot, instructionIndex)",
            "backend_aot_c_frame_descriptor_generic_jump_if_condition_can_use_local_only(",
            "functionIr, destinationSlot, instructionIndex) ||",
            "backend_aot_c_null_constant_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_jump_if(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_bool_constant_consumed_by_local_logical_not(",
            "backend_aot_c_bool_constant_consumed_by_local_jump_if(",
            "ZR_VALUE_IS_TYPE_STRING(constantValue->type)",
            "backend_aot_c_string_constant_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_jump_if(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(",
            "backend_aot_c_reset_null_consumed_by_local_jump_if(",
            "backend_aot_c_reset_null_consumed_by_local_stack_copy_logical_not(",
            "backend_aot_c_reset_null_consumed_by_local_stack_copy_jump_if(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(",
            "backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(",
            "functionIr, destinationSlot, instructionIndex - 1u)",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "backend_aot_c_write_generic_truthiness_unsupported",
            "backend_aot_c_write_primitive_truthiness",
            "ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive truthiness\")",
            "ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_truthy, ZR_VALUE_TYPE_BOOL)",
            "const SZrTypeValue *zr_aot_bool_sync = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_bool_sync->value.nativeObject.nativeBool",
            "ZrLibrary_AotRuntime_LogicalNot",
            "ZrLibrary_AotRuntime_IsTruthy",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalNot(state, &frame",
    };
    static const char *const forbiddenControlNeedles[] = {
            "ZrLibrary_AotRuntime_IsTruthy(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *valuesText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *constantConsumersText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_constant_consumers.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *controlText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");
    char *frameDescriptorText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_descriptor.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(valuesText);
    TEST_ASSERT_NOT_NULL(constantConsumersText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(controlText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);
    TEST_ASSERT_NOT_NULL(frameDescriptorText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(
            constantConsumersText, constantConsumerNeedles, ARRAY_COUNT(constantConsumerNeedles));
    assert_text_contains_all(valuesText, valuesNeedles, ARRAY_COUNT(valuesNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalNeedles, ARRAY_COUNT(scalarLocalNeedles));
    assert_text_contains_all(frameDescriptorText, frameDescriptorNeedles, ARRAY_COUNT(frameDescriptorNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(controlText, forbiddenControlNeedles, ARRAY_COUNT(forbiddenControlNeedles));

    free(headerText);
    free(moduleText);
    free(valuesText);
    free(constantConsumersText);
    free(functionBodyText);
    free(controlText);
    free(scalarLocalsText);
    free(frameDescriptorText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
}

static void test_aot_c_source_lowers_generic_primitive_equality_to_boundary_helpers(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_equal(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "TZrUInt32 execInstructionIndex",
            "backend_aot_write_c_direct_logical_not_equal(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_write_c_direct_logical_equal(",
            "backend_aot_write_c_direct_logical_not_equal(",
            "backend_aot_c_write_generic_bool_compare_scalar_local(",
            "backend_aot_c_generic_primitive_local_kind_written_before(",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_value_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_value_written_before(functionIr, rightSlot, execInstructionIndex)",
            "backend_aot_c_write_generic_mixed_primitive_compare_scalar_local(",
            "zr_aot_generic_logical_equal",
            "zr_aot_generic_logical_not_equal",
            "zr_aot_generic_bool_compare_scalar_local",
            "zr_aot_generic_mixed_primitive_compare_scalar_local",
            "zr_aot_b%u = (TZrBool)((zr_aot_b%u %s zr_aot_b%u) != 0u);",
            "zr_aot_b%u = %s;",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(state, &frame, %u, %u, %u)",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(state, &frame, %u, %u, %u)",
            "backend_aot_c_write_bool_local_sync_from_slot(file, functionIr, destinationSlot);",
            "zr_aot_generic_logical_sync_bool_local_boundary",
            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, %u, &zr_aot_b%u)",
    };
    static const char *const runtimeHeaderNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(struct SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(struct SZrState *state,",
            "ZrLibrary_AotRuntime_SyncBoolLocal(struct SZrState *state,",
    };
    static const char *const runtimeSourceNeedles[] = {
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(SZrState *state,",
            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(SZrState *state,",
            "aot_runtime_generic_primitive_equal(",
            "unsupported AOT generic primitive equality",
            "ZR_VALUE_FAST_SET(destinationValue, nativeBool, equal, ZR_VALUE_TYPE_BOOL);",
            "ZR_VALUE_FAST_SET(destinationValue, nativeBool, !equal, ZR_VALUE_TYPE_BOOL);",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):",
            "backend_aot_write_c_direct_logical_equal(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):",
            "backend_aot_write_c_direct_logical_not_equal(",
            "file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
    };
    static const char *const scalarLocalNeedles[] = {
            "backend_aot_c_scalar_locals_record_generic_bool_compare_destinations(",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):",
            "backend_aot_c_scalar_locals_bool_consumer_mentions_slot(instruction, slot)",
            "backend_aot_c_scalar_locals_bool_consumer_reads_slot(functionIr, instruction, slot)",
            "backend_aot_c_scalar_locals_kind_is_single_primitive(",
            "hasMixedPrimitiveOperands",
    };
    static const char *const frameDescriptorNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):",
            "backend_aot_c_frame_descriptor_generic_equality_operands_can_use_local_only(",
            "backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex)",
            "backend_aot_c_scalar_locals_u64_written_before(functionIr, slot, instructionIndex)",
            "backend_aot_c_scalar_locals_f64_written_before(functionIr, slot, instructionIndex)",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "backend_aot_c_write_generic_equality_unsupported",
            "backend_aot_c_write_primitive_equality",
            "ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive equality\")",
            "ZR_VALUE_IS_TYPE_NULL(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)",
            "const SZrTypeValue *zr_aot_bool_sync = ZrCore_Stack_GetValue(frame.slotBase + %u);",
            "zr_aot_bool_sync->value.nativeObject.nativeBool",
            "ZrLibrary_AotRuntime_LogicalEqual",
            "ZrCore_Value_Equal",
    };
    static const char *const forbiddenValuesNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqual(state, &frame",
            "backend_aot_write_c_direct_logical_equal(",
            "backend_aot_write_c_direct_logical_not_equal(",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqual(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqual(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *valuesText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");
    char *frameDescriptorText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_descriptor.c");
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(valuesText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);
    TEST_ASSERT_NOT_NULL(frameDescriptorText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalNeedles, ARRAY_COUNT(scalarLocalNeedles));
    assert_text_contains_all(frameDescriptorText, frameDescriptorNeedles, ARRAY_COUNT(frameDescriptorNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(valuesText, forbiddenValuesNeedles, ARRAY_COUNT(forbiddenValuesNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(moduleText);
    free(valuesText);
    free(functionBodyText);
    free(scalarLocalsText);
    free(frameDescriptorText);
    free(runtimeHeaderText);
    free(runtimeSourceText);
}

static void test_aot_c_source_lowers_bool_logical_and_or_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_and(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "backend_aot_write_c_direct_logical_or(FILE *file",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_c_write_bool_binary_logical",
            "backend_aot_c_write_bool_binary_scalar_local",
            "backend_aot_write_c_direct_logical_and(",
            "backend_aot_write_c_direct_logical_or(",
            "zr_aot_bool_logical_and",
            "zr_aot_bool_logical_or",
            "zr_aot_bool_binary_scalar_local",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, leftSlot, execInstructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, rightSlot, execInstructionIndex)",
            "zr_aot_b%u = (TZrBool)((zr_aot_b%u %s zr_aot_b%u) != 0u);",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)",
            "zr_aot_left_bool && zr_aot_right_bool",
            "zr_aot_left_bool || zr_aot_right_bool",
            "unsupported AOT bool logical binary",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL)",
            "backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, \"zr_aot_result\")",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_AND):",
            "backend_aot_write_c_direct_logical_and(file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_OR):",
            "backend_aot_write_c_direct_logical_or(file, functionIr, destinationSlot, operandA1, operandB1, instructionIndex);",
    };
    static const char *const scalarLocalNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_AND):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_OR):",
            "declaredSlotKinds[destinationSlot] & ZR_AOT_SCALAR_LOCAL_KIND_BOOL",
            "backend_aot_c_scalar_locals_record_slot(slotKinds,",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalAnd",
            "ZrLibrary_AotRuntime_LogicalOr",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalAnd(state, &frame",
            "ZrLibrary_AotRuntime_LogicalOr(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalNeedles, ARRAY_COUNT(scalarLocalNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(moduleText);
    free(functionBodyText);
    free(scalarLocalsText);
}

static void test_aot_c_source_lowers_string_equality_to_direct_c(void) {
    static const char *const headerNeedles[] = {
            "backend_aot_write_c_direct_logical_equal_string(FILE *file",
            "const SZrAotExecIrFunction *functionIr",
            "backend_aot_write_c_direct_logical_not_equal_string(FILE *file",
    };
    static const char *const emitterNeedles[] = {
            "#include <string.h>\\n",
            "#include \\\"zr_vm_core/string.h\\\"\\n",
    };
    static const char *const moduleNeedles[] = {
            "backend_aot_c_lowering_generic_logical.c",
            "backend_aot_c_write_string_logical_operand(",
            "ZrCore_Stack_GetValue(frame.slotBase + %u)",
            "backend_aot_c_write_string_equality",
            "backend_aot_c_write_string_bool_scalar_local",
            "backend_aot_write_c_direct_logical_equal_string(",
            "backend_aot_write_c_direct_logical_not_equal_string(",
            "zr_aot_string_logical_equal",
            "zr_aot_string_logical_not_equal",
            "zr_aot_string_logical_bool_scalar_local",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, execInstructionIndex)",
            "zr_aot_b%u = (TZrBool)((%s) != 0u);",
            "ZR_VALUE_IS_TYPE_STRING(zr_aot_left->type)",
            "ZR_VALUE_IS_TYPE_STRING(zr_aot_right->type)",
            "ZR_CAST_STRING(state, zr_aot_left->value.object)",
            "ZrCore_String_GetByteLength(zr_aot_left_string)",
            "ZrCore_String_GetNativeString(zr_aot_left_string)",
            "memcmp(zr_aot_left_bytes, zr_aot_right_bytes, zr_aot_left_length) == 0",
            "unsupported AOT string equality",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_equal, ZR_VALUE_TYPE_BOOL)",
            "ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_equal, ZR_VALUE_TYPE_BOOL)",
            "backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, \"zr_aot_equal\")",
            "backend_aot_c_write_bool_local_sync(file, functionIr, destinationSlot, \"!zr_aot_equal\")",
    };
    static const char *const functionBodyNeedles[] = {
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):",
            "backend_aot_write_c_direct_logical_equal_string(file,",
            "functionIr,",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):",
            "backend_aot_write_c_direct_logical_not_equal_string(file,",
            "instructionIndex);",
    };
    static const char *const scalarLocalNeedles[] = {
            "backend_aot_c_scalar_locals_record_semir(",
            "backend_aot_c_scalar_locals_kind_for_semir_instruction(function, instruction)",
            "backend_aot_c_scalar_locals_record_slot(slotKinds, slotCount, instruction->destinationSlot, kind)",
    };
    static const char *const forbiddenModuleNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqualString",
            "ZrLibrary_AotRuntime_LogicalNotEqualString",
            "ZrCore_String_Equal",
            "((const TZrByte *)frame.slotBase + %u)",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_LogicalEqualString(state, &frame",
            "ZrLibrary_AotRuntime_LogicalNotEqualString(state, &frame",
    };
    char *headerText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h");
    char *emitterText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *moduleText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");

    TEST_ASSERT_NOT_NULL(headerText);
    TEST_ASSERT_NOT_NULL(emitterText);
    TEST_ASSERT_NOT_NULL(moduleText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);

    assert_text_contains_all(headerText, headerNeedles, ARRAY_COUNT(headerNeedles));
    assert_text_contains_all(emitterText, emitterNeedles, ARRAY_COUNT(emitterNeedles));
    assert_text_contains_all(moduleText, moduleNeedles, ARRAY_COUNT(moduleNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalNeedles, ARRAY_COUNT(scalarLocalNeedles));
    assert_text_contains_none(moduleText, forbiddenModuleNeedles, ARRAY_COUNT(forbiddenModuleNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));

    free(headerText);
    free(emitterText);
    free(moduleText);
    free(functionBodyText);
    free(scalarLocalsText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_generic_truthiness_to_boundary_helpers);
    RUN_TEST(test_aot_c_source_lowers_generic_primitive_equality_to_boundary_helpers);
    RUN_TEST(test_aot_c_source_lowers_bool_logical_and_or_to_direct_c);
    RUN_TEST(test_aot_c_source_lowers_string_equality_to_direct_c);
    return UNITY_END();
}
