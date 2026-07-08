#include "backend_aot_c_frame_descriptor.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_c_scalar_semir.h"
#include "backend_aot_c_scalar_stack_copy.h"

static TZrBool backend_aot_c_frame_descriptor_branch_target_is_valid(const SZrFunction *function,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrInt64 relativeOffset) {
    TZrInt64 targetInstructionIndex;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)instructionIndex + relativeOffset + 1;
    if (targetInstructionIndex < 0 ||
        (TZrUInt32)targetInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_frame_descriptor_signed_constant_is_i64(const SZrFunction *function,
                                                                     TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);

    return (TZrBool)(constantValue != ZR_NULL && ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type));
}

static TZrBool backend_aot_c_frame_descriptor_constant_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 destinationSlot,
        TZrInt32 constantIndex,
        TZrUInt32 instructionIndex) {
    const SZrTypeValue *constantValue;

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_STRING(constantValue->type)) {
        return (TZrBool)(backend_aot_c_string_constant_consumed_by_local_logical_not(
                                 functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                         backend_aot_c_string_constant_consumed_by_local_jump_if(
                                 functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                         backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(
                                 functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                         backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(
                                 functionIr, destinationSlot, instructionIndex, ZR_NULL));
    }

    if (!backend_aot_c_constant_can_emit_immediate(function, constantIndex)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        return (TZrBool)(backend_aot_c_null_constant_consumed_by_local_logical_not(
                                 functionIr, destinationSlot, instructionIndex) ||
                         backend_aot_c_null_constant_consumed_by_local_jump_if(
                                 functionIr, destinationSlot, instructionIndex) ||
                         backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(
                                 functionIr, destinationSlot, instructionIndex) ||
                         backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(
                                 functionIr, destinationSlot, instructionIndex));
    }

    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        return (TZrBool)(backend_aot_c_bool_constant_consumed_by_local_logical_not(
                                 functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                         backend_aot_c_bool_constant_consumed_by_local_jump_if(
                                 functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                         (backend_aot_c_scalar_locals_has_bool_slot(functionIr, destinationSlot) &&
                          backend_aot_c_scalar_locals_bool_constant_can_skip_value_slot(
                                  functionIr, destinationSlot, instructionIndex)));
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return (TZrBool)((backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) &&
                          backend_aot_c_scalar_locals_i64_constant_can_skip_value_slot(
                                  functionIr, destinationSlot, instructionIndex)) ||
                         (backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) &&
                          backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(
                                  functionIr, destinationSlot, instructionIndex)));
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        return (TZrBool)(backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) &&
                         backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(
                                 functionIr, destinationSlot, instructionIndex));
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return (TZrBool)(backend_aot_c_scalar_locals_has_f64_slot(functionIr, destinationSlot) &&
                         backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot(
                                 functionIr, destinationSlot, instructionIndex));
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_frame_descriptor_signed_branch_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrInt64 relativeOffset) {
    return (TZrBool)(backend_aot_c_frame_descriptor_branch_target_is_valid(
                             function, instructionIndex, relativeOffset) &&
                     backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
                     backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot) &&
                     backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, instructionIndex));
}

static TZrBool backend_aot_c_frame_descriptor_signed_const_branch_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 leftSlot,
        TZrUInt32 constantIndex,
        TZrInt64 relativeOffset) {
    return (TZrBool)(backend_aot_c_frame_descriptor_branch_target_is_valid(
                             function, instructionIndex, relativeOffset) &&
                     backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
                     backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, instructionIndex) &&
                     backend_aot_c_frame_descriptor_signed_constant_is_i64(function, constantIndex));
}

static TZrBool backend_aot_c_frame_descriptor_generic_jump_if_condition_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 conditionSlot,
        TZrUInt32 instructionIndex) {
    return (TZrBool)(backend_aot_c_scalar_locals_bool_value_written_before(
                             functionIr, conditionSlot, instructionIndex) ||
                     (backend_aot_c_scalar_locals_has_i64_slot(functionIr, conditionSlot) &&
                      backend_aot_c_scalar_locals_i64_written_before(
                              functionIr, conditionSlot, instructionIndex)) ||
                     (backend_aot_c_scalar_locals_has_u64_slot(functionIr, conditionSlot) &&
                      backend_aot_c_scalar_locals_u64_written_before(
                              functionIr, conditionSlot, instructionIndex)) ||
                     (backend_aot_c_scalar_locals_has_f64_slot(functionIr, conditionSlot) &&
                      backend_aot_c_scalar_locals_f64_written_before(
                              functionIr, conditionSlot, instructionIndex)));
}

typedef enum EZrAotFrameDescriptorGenericNumericScalarLocalKind {
    ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_NONE,
    ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64,
    ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64,
    ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64
} EZrAotFrameDescriptorGenericNumericScalarLocalKind;

static EZrAotFrameDescriptorGenericNumericScalarLocalKind
backend_aot_c_frame_descriptor_generic_numeric_written_scalar_kind(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 instructionIndex) {
    if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_f64_written_before(functionIr, slot, instructionIndex)) {
        return ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64;
    }
    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex)) {
        return ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64;
    }
    if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_u64_written_before(functionIr, slot, instructionIndex)) {
        return ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64;
    }
    return ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_NONE;
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_scalar_kind_is_integer(
        EZrAotFrameDescriptorGenericNumericScalarLocalKind kind) {
    return (TZrBool)(kind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64 ||
                     kind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64);
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_scalar_kinds_form_mixed_i64_u64(
        EZrAotFrameDescriptorGenericNumericScalarLocalKind leftKind,
        EZrAotFrameDescriptorGenericNumericScalarLocalKind rightKind) {
    return (TZrBool)((leftKind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64 &&
                      rightKind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64) ||
                     (leftKind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64 &&
                      rightKind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64));
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_binary_opcode_can_use_local_only(
        EZrInstructionCode operationCode) {
    switch (operationCode) {
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_same_i64_binary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    return (TZrBool)(backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                             functionIr, destinationSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
                     backend_aot_c_scalar_locals_has_i64_slot(functionIr, rightSlot) &&
                     backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, instructionIndex));
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_same_u64_binary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    return (TZrBool)(backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                             functionIr, destinationSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_has_u64_slot(functionIr, leftSlot) &&
                     backend_aot_c_scalar_locals_has_u64_slot(functionIr, rightSlot) &&
                     backend_aot_c_scalar_locals_u64_written_before(functionIr, leftSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_u64_written_before(functionIr, rightSlot, instructionIndex));
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_same_f64_binary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    return (TZrBool)(backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                             functionIr, destinationSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_has_f64_slot(functionIr, leftSlot) &&
                     backend_aot_c_scalar_locals_has_f64_slot(functionIr, rightSlot) &&
                     backend_aot_c_scalar_locals_f64_written_before(functionIr, leftSlot, instructionIndex) &&
                     backend_aot_c_scalar_locals_f64_written_before(functionIr, rightSlot, instructionIndex));
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_mixed_i64_u64_binary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    EZrAotFrameDescriptorGenericNumericScalarLocalKind leftKind;
    EZrAotFrameDescriptorGenericNumericScalarLocalKind rightKind;

    if (!backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                functionIr, destinationSlot, instructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_frame_descriptor_generic_numeric_written_scalar_kind(
            functionIr, leftSlot, instructionIndex);
    rightKind = backend_aot_c_frame_descriptor_generic_numeric_written_scalar_kind(
            functionIr, rightSlot, instructionIndex);
    return backend_aot_c_frame_descriptor_generic_numeric_scalar_kinds_form_mixed_i64_u64(
            leftKind, rightKind);
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_mixed_f64_binary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    EZrAotFrameDescriptorGenericNumericScalarLocalKind leftKind;
    EZrAotFrameDescriptorGenericNumericScalarLocalKind rightKind;

    if (!backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                functionIr, destinationSlot, instructionIndex)) {
        return ZR_FALSE;
    }

    leftKind = backend_aot_c_frame_descriptor_generic_numeric_written_scalar_kind(
            functionIr, leftSlot, instructionIndex);
    rightKind = backend_aot_c_frame_descriptor_generic_numeric_written_scalar_kind(
            functionIr, rightSlot, instructionIndex);
    return (TZrBool)((leftKind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
                      backend_aot_c_frame_descriptor_generic_numeric_scalar_kind_is_integer(rightKind)) ||
                     (rightKind == ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64 &&
                      backend_aot_c_frame_descriptor_generic_numeric_scalar_kind_is_integer(leftKind)));
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_binary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        EZrInstructionCode operationCode,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    if (!backend_aot_c_frame_descriptor_generic_numeric_binary_opcode_can_use_local_only(operationCode)) {
        return ZR_FALSE;
    }

    return (TZrBool)(backend_aot_c_frame_descriptor_generic_numeric_same_i64_binary_can_use_local_only(
                             functionIr, instructionIndex, destinationSlot, leftSlot, rightSlot) ||
                     backend_aot_c_frame_descriptor_generic_numeric_same_u64_binary_can_use_local_only(
                             functionIr, instructionIndex, destinationSlot, leftSlot, rightSlot) ||
                     backend_aot_c_frame_descriptor_generic_numeric_mixed_i64_u64_binary_can_use_local_only(
                             functionIr, instructionIndex, destinationSlot, leftSlot, rightSlot) ||
                     backend_aot_c_frame_descriptor_generic_numeric_same_f64_binary_can_use_local_only(
                             functionIr, instructionIndex, destinationSlot, leftSlot, rightSlot) ||
                     backend_aot_c_frame_descriptor_generic_numeric_mixed_f64_binary_can_use_local_only(
                             functionIr, instructionIndex, destinationSlot, leftSlot, rightSlot));
}

static TZrBool backend_aot_c_frame_descriptor_generic_numeric_unary_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        EZrInstructionCode operationCode,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    EZrAotFrameDescriptorGenericNumericScalarLocalKind sourceKind;

    if (operationCode != ZR_INSTRUCTION_ENUM(NEG)) {
        return ZR_FALSE;
    }

    sourceKind = backend_aot_c_frame_descriptor_generic_numeric_written_scalar_kind(
            functionIr, sourceSlot, instructionIndex);
    switch (sourceKind) {
        case ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_I64:
            return backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                    functionIr, destinationSlot, instructionIndex);
        case ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_U64:
            return backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                    functionIr, destinationSlot, instructionIndex);
        case ZR_AOT_FRAME_DESCRIPTOR_GENERIC_NUMERIC_SCALAR_LOCAL_KIND_F64:
            return backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                    functionIr, destinationSlot, instructionIndex);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_frame_descriptor_primitive_slot_kind_written_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 instructionIndex,
        char *outKindCode) {
    TZrUInt32 kindCount;
    char kindCode;

    if (outKindCode == ZR_NULL) {
        return ZR_FALSE;
    }
    *outKindCode = '\0';
    if (functionIr == ZR_NULL) {
        return ZR_FALSE;
    }

    kindCount = 0u;
    kindCode = '\0';
    if (backend_aot_c_scalar_locals_has_bool_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_bool_value_written_before(functionIr, slot, instructionIndex)) {
        kindCount++;
        kindCode = 'b';
    }
    if (backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, instructionIndex)) {
        kindCount++;
        kindCode = 's';
    }
    if (backend_aot_c_scalar_locals_has_u64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_u64_written_before(functionIr, slot, instructionIndex)) {
        kindCount++;
        kindCode = 'u';
    }
    if (backend_aot_c_scalar_locals_has_f64_slot(functionIr, slot) &&
        backend_aot_c_scalar_locals_f64_written_before(functionIr, slot, instructionIndex)) {
        kindCount++;
        kindCode = 'f';
    }
    if (kindCount != 1u) {
        return ZR_FALSE;
    }

    *outKindCode = kindCode;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_frame_descriptor_generic_equality_operands_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 instructionIndex,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot) {
    char leftKindCode;
    char rightKindCode;

    return (TZrBool)(backend_aot_c_frame_descriptor_primitive_slot_kind_written_before(
                             functionIr, leftSlot, instructionIndex, &leftKindCode) &&
                     backend_aot_c_frame_descriptor_primitive_slot_kind_written_before(
                             functionIr, rightSlot, instructionIndex, &rightKindCode));
}

static TZrBool backend_aot_c_frame_descriptor_bool_logical_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        EZrInstructionCode operationCode,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 operandA1,
        TZrUInt32 operandB1) {
    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, instructionIndex)) {
        return ZR_FALSE;
    }

    switch (operationCode) {
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            return backend_aot_c_frame_descriptor_generic_equality_operands_can_use_local_only(
                    functionIr, instructionIndex, operandA1, operandB1);

        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            return (TZrBool)(backend_aot_c_scalar_locals_bool_written_before(functionIr, operandA1, instructionIndex) &&
                             backend_aot_c_scalar_locals_bool_written_before(functionIr, operandB1, instructionIndex));

        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
            return (TZrBool)(backend_aot_c_scalar_locals_bool_value_written_before(
                                     functionIr, operandA1, instructionIndex) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_null_constant_consumed_by_local_logical_not(
                                      functionIr, operandA1, instructionIndex - 1u)) ||
                            (instructionIndex > 0u &&
                             backend_aot_c_reset_null_consumed_by_local_logical_not(
                                     functionIr, operandA1, instructionIndex - 1u)) ||
                            (instructionIndex > 0u &&
                             backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(
                                     functionIr, operandA1, instructionIndex - 1u)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(
                                      functionIr, operandA1, instructionIndex - 1u)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_bool_constant_consumed_by_local_logical_not(
                                      functionIr, operandA1, instructionIndex - 1u, ZR_NULL)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_string_constant_consumed_by_local_logical_not(
                                      functionIr, operandA1, instructionIndex - 1u, ZR_NULL)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(
                                      functionIr, operandA1, instructionIndex - 1u, ZR_NULL)));

        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
            return backend_aot_c_scalar_locals_bool_written_before(functionIr, operandA1, instructionIndex);

        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_frame_descriptor_conversion_can_use_local_only(
        const SZrAotExecIrFunction *functionIr,
        EZrInstructionCode operationCode,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot) {
    switch (operationCode) {
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
            return (TZrBool)(backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             (backend_aot_c_scalar_locals_i64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_u64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_f64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_bool_written_before(
                                      functionIr, sourceSlot, instructionIndex)));

        case ZR_INSTRUCTION_ENUM(TO_INT):
            return (TZrBool)(backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             (backend_aot_c_scalar_locals_i64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_u64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_f64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_bool_written_before(
                                      functionIr, sourceSlot, instructionIndex)));

        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
            return (TZrBool)(backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             backend_aot_c_scalar_locals_f64_written_before(
                                     functionIr, sourceSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
            return (TZrBool)(backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             backend_aot_c_scalar_locals_u64_written_before(
                                     functionIr, sourceSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(TO_UINT):
            return (TZrBool)(backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             (backend_aot_c_scalar_locals_i64_written_before(
                                       functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_u64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_bool_written_before(
                                      functionIr, sourceSlot, instructionIndex)));

        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
            return (TZrBool)(backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             backend_aot_c_scalar_locals_f64_written_before(
                                     functionIr, sourceSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
            return (TZrBool)(backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             backend_aot_c_scalar_locals_i64_written_before(
                                     functionIr, sourceSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            return (TZrBool)(backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             (backend_aot_c_scalar_locals_i64_written_before(
                                       functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_u64_written_before(
                                       functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_f64_written_before(
                                      functionIr, sourceSlot, instructionIndex) ||
                              backend_aot_c_scalar_locals_bool_written_before(
                                      functionIr, sourceSlot, instructionIndex)));

        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
            return (TZrBool)(backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             backend_aot_c_scalar_locals_i64_written_before(
                                     functionIr, sourceSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
            return (TZrBool)(backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex) &&
                             backend_aot_c_scalar_locals_u64_written_before(
                                     functionIr, sourceSlot, instructionIndex));

        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_frame_descriptor_instruction_can_use_local_only(
        const SZrAotExecIrModule *module,
        const SZrAotExecIrFunction *functionIr,
        const SZrFunction *function,
        const TZrInstruction *instruction,
        TZrUInt32 instructionIndex,
        TZrBool publishExports) {
    TZrUInt32 destinationSlot;
    TZrUInt32 operandA1;
    TZrUInt32 operandB1;
    TZrInt32 operandA2;

    if (functionIr == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_scalar_semir_can_write_frame_free_for_exec_instruction(
                module, functionIr, instruction, instructionIndex)) {
        return ZR_TRUE;
    }

    destinationSlot = instruction->instruction.operandExtra;
    operandA1 = instruction->instruction.operand.operand1[0];
    operandB1 = instruction->instruction.operand.operand1[1];
    operandA2 = instruction->instruction.operand.operand2[0];

    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(NOP):
            return ZR_TRUE;

        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return backend_aot_c_frame_descriptor_constant_can_use_local_only(
                    functionIr, function, destinationSlot, operandA2, instructionIndex);

        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
            return (TZrBool)(backend_aot_c_reset_null_consumed_by_local_logical_not(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_reset_null_consumed_by_local_stack_copy_logical_not(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_reset_null_consumed_by_local_stack_copy_jump_if(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_reset_null_consumed_by_local_jump_if(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_scalar_locals_reset_can_skip_value_slot(
                                     functionIr, destinationSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2):
            return backend_aot_c_scalar_locals_reset2_can_skip_value_slots(
                    functionIr, destinationSlot, operandA1, instructionIndex);

        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return (TZrBool)(backend_aot_c_reset_null_stack_copy_consumed_by_local_logical_not(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(
                                     functionIr, destinationSlot, instructionIndex) ||
                             backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(
                                     functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                             backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(
                                     functionIr, destinationSlot, instructionIndex, ZR_NULL) ||
                             backend_aot_c_scalar_stack_copy_can_use_local_only(
                                     functionIr, destinationSlot, (TZrUInt32)operandA2, instructionIndex));

        case ZR_INSTRUCTION_ENUM(JUMP):
            return backend_aot_c_frame_descriptor_branch_target_is_valid(
                    function, instructionIndex, (TZrInt64)operandA2);

        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            return (TZrBool)(backend_aot_c_frame_descriptor_branch_target_is_valid(
                                     function, instructionIndex, (TZrInt64)operandA2) &&
                             (backend_aot_c_frame_descriptor_generic_jump_if_condition_can_use_local_only(
                                      functionIr, destinationSlot, instructionIndex) ||
                             (instructionIndex > 0u &&
                             backend_aot_c_null_constant_consumed_by_local_jump_if(
                                     functionIr, destinationSlot, instructionIndex - 1u)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(
                                      functionIr, destinationSlot, instructionIndex - 1u)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_reset_null_stack_copy_consumed_by_local_jump_if(
                                      functionIr, destinationSlot, instructionIndex - 1u)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_bool_constant_consumed_by_local_jump_if(
                                      functionIr, destinationSlot, instructionIndex - 1u, ZR_NULL)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_string_constant_consumed_by_local_jump_if(
                                      functionIr, destinationSlot, instructionIndex - 1u, ZR_NULL)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(
                                      functionIr, destinationSlot, instructionIndex - 1u, ZR_NULL)) ||
                             (instructionIndex > 0u &&
                              backend_aot_c_reset_null_consumed_by_local_jump_if(
                                      functionIr, destinationSlot, instructionIndex - 1u))));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return (TZrBool)(backend_aot_c_frame_descriptor_branch_target_is_valid(
                                     function, instructionIndex, (TZrInt64)operandA2) &&
                             backend_aot_c_scalar_locals_bool_written_before(
                                     functionIr, destinationSlot, instructionIndex));

        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
            return backend_aot_c_frame_descriptor_bool_logical_can_use_local_only(
                    functionIr,
                    (EZrInstructionCode)instruction->instruction.operationCode,
                    instructionIndex,
                    destinationSlot,
                    operandA1,
                    operandB1);

        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return backend_aot_c_frame_descriptor_generic_numeric_binary_can_use_local_only(
                    functionIr,
                    (EZrInstructionCode)instruction->instruction.operationCode,
                    instructionIndex,
                    destinationSlot,
                    operandA1,
                    operandB1);

        case ZR_INSTRUCTION_ENUM(NEG):
            return backend_aot_c_frame_descriptor_generic_numeric_unary_can_use_local_only(
                    functionIr,
                    (EZrInstructionCode)instruction->instruction.operationCode,
                    instructionIndex,
                    destinationSlot,
                    operandA1);

        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
            return backend_aot_c_frame_descriptor_conversion_can_use_local_only(
                    functionIr,
                    (EZrInstructionCode)instruction->instruction.operationCode,
                    instructionIndex,
                    destinationSlot,
                    operandA1);

        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return backend_aot_c_frame_descriptor_signed_branch_can_use_local_only(
                    functionIr,
                    function,
                    instructionIndex,
                    destinationSlot,
                    operandA1,
                    (TZrInt64)(TZrInt16)operandB1);

        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return backend_aot_c_frame_descriptor_signed_const_branch_can_use_local_only(
                    functionIr,
                    function,
                    instructionIndex,
                    destinationSlot,
                    operandA1,
                    (TZrInt64)(TZrInt16)operandB1);

        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            if (publishExports) {
                return ZR_FALSE;
            }
            return (TZrBool)(backend_aot_c_scalar_locals_can_direct_return_i64_local(
                                     functionIr, operandA1, instructionIndex) ||
                             backend_aot_c_scalar_locals_can_direct_return_bool_local(
                                     functionIr, operandA1, instructionIndex) ||
                             backend_aot_c_scalar_locals_can_infer_return_bool_local(
                                     functionIr, operandA1, instructionIndex) ||
                             backend_aot_c_scalar_locals_can_direct_return_u64_local(
                                     functionIr, operandA1, instructionIndex) ||
                             backend_aot_c_scalar_locals_can_infer_return_u64_local(
                                     functionIr, operandA1, instructionIndex) ||
                             backend_aot_c_scalar_locals_can_direct_return_f64_local(
                                     functionIr, operandA1, instructionIndex) ||
                             backend_aot_c_scalar_locals_can_infer_return_f64_local(
                                     functionIr, operandA1, instructionIndex));

        default:
            return ZR_FALSE;
    }
}

TZrBool backend_aot_c_function_body_needs_frame_descriptor(const SZrAotExecIrModule *module,
                                                           const SZrAotExecIrFunction *functionIr,
                                                           const SZrFunction *function,
                                                           TZrBool publishExports,
                                                           TZrBool needsFrameCleanup) {
    TZrUInt32 instructionIndex;

    if (publishExports || needsFrameCleanup ||
        module == ZR_NULL || functionIr == ZR_NULL || function == ZR_NULL ||
        functionIr->function != function ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0 ||
        function->exceptionHandlerCount > 0) {
        return ZR_TRUE;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        if (!backend_aot_c_frame_descriptor_instruction_can_use_local_only(
                    module,
                    functionIr,
                    function,
                    &function->instructionsList[instructionIndex],
                    instructionIndex,
                    publishExports)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}
