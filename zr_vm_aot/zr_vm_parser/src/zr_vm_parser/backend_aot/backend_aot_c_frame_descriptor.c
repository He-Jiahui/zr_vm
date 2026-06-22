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

    if (!backend_aot_c_constant_can_emit_immediate(function, constantIndex)) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return (TZrBool)((backend_aot_c_scalar_locals_has_i64_slot(functionIr, destinationSlot) &&
                          backend_aot_c_scalar_locals_i64_constant_can_skip_value_slot(
                                  functionIr, destinationSlot, instructionIndex)) ||
                         (backend_aot_c_scalar_locals_has_u64_slot(functionIr, destinationSlot) &&
                          backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(
                                  functionIr, destinationSlot, instructionIndex)));
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
            return backend_aot_c_scalar_locals_reset_can_skip_value_slot(
                    functionIr, destinationSlot, instructionIndex);

        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2):
            return backend_aot_c_scalar_locals_reset2_can_skip_value_slots(
                    functionIr, destinationSlot, operandA1, instructionIndex);

        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return backend_aot_c_scalar_stack_copy_can_use_local_only(
                    functionIr, destinationSlot, (TZrUInt32)operandA2, instructionIndex);

        case ZR_INSTRUCTION_ENUM(JUMP):
            return backend_aot_c_frame_descriptor_branch_target_is_valid(
                    function, instructionIndex, (TZrInt64)operandA2);

        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return (TZrBool)(backend_aot_c_frame_descriptor_branch_target_is_valid(
                                     function, instructionIndex, (TZrInt64)operandA2) &&
                             backend_aot_c_scalar_locals_bool_written_before(
                                     functionIr, destinationSlot, instructionIndex));

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
            return (TZrBool)(!publishExports &&
                             backend_aot_c_scalar_locals_can_direct_return_i64_local(
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
