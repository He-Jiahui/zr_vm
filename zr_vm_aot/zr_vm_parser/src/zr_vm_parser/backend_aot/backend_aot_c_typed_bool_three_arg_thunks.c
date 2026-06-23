#include "backend_aot_c_typed_bool_three_arg_thunks.h"

#include "backend_aot_internal.h"
#include "backend_aot_c_emitter.h"

static TZrBool backend_aot_c_type_ref_is_bool(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_BOOL ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_BOOL);
}

static TZrBool backend_aot_c_try_read_bool_logical_operands(const TZrInstruction *instruction,
                                                            EZrInstructionCode logicalOperationCode,
                                                            TZrUInt32 *outLeftSlot,
                                                            TZrUInt32 *outRightSlot,
                                                            TZrUInt32 *outResultSlot) {
    if (instruction == ZR_NULL ||
        outLeftSlot == ZR_NULL ||
        outRightSlot == ZR_NULL ||
        outResultSlot == ZR_NULL ||
        instruction->instruction.operationCode != (TZrUInt16)logicalOperationCode) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    *outResultSlot = instruction->instruction.operandExtra;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_slots_are_0_and_1(TZrUInt32 leftSlot, TZrUInt32 rightSlot) {
    return (TZrBool)((leftSlot == 0u && rightSlot == 1u) ||
                     (leftSlot == 1u && rightSlot == 0u));
}

static TZrBool backend_aot_c_slots_are_temp_and_2(TZrUInt32 leftSlot, TZrUInt32 rightSlot, TZrUInt32 tempSlot) {
    return (TZrBool)((leftSlot == tempSlot && rightSlot == 2u) ||
                     (leftSlot == 2u && rightSlot == tempSlot));
}

static TZrBool backend_aot_c_bool_stack_copy_reads_slot(const TZrInstruction *instruction,
                                                        TZrUInt32 sourceSlot,
                                                        TZrUInt32 *outDestinationSlot) {
    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
         instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
        instruction->instruction.operand.operand2[0] < 0 ||
        (TZrUInt32)instruction->instruction.operand.operand2[0] != sourceSlot) {
        return ZR_FALSE;
    }

    *outDestinationSlot = instruction->instruction.operandExtra;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_and_return(
        const SZrFunction *function) {
    const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
    const TZrInstruction *initialResultInstruction = &function->instructionsList[1];
    const TZrInstruction *firstJumpIfFalseInstruction = &function->instructionsList[2];
    const TZrInstruction *middleCopyInstruction = &function->instructionsList[3];
    const TZrInstruction *middleResultInstruction = &function->instructionsList[4];
    const TZrInstruction *secondConditionCopyInstruction = &function->instructionsList[5];
    const TZrInstruction *secondJumpIfFalseInstruction = &function->instructionsList[6];
    const TZrInstruction *rightCopyInstruction = &function->instructionsList[7];
    const TZrInstruction *finalResultInstruction = &function->instructionsList[8];
    const TZrInstruction *returnInstruction = &function->instructionsList[9];
    TZrUInt32 leftTempSlot;
    TZrUInt32 resultSlot;
    TZrUInt32 middleTempSlot;
    TZrUInt32 middleResultSlot;
    TZrUInt32 secondConditionSlot;
    TZrUInt32 rightTempSlot;
    TZrUInt32 finalResultSlot;

    if (!backend_aot_c_bool_stack_copy_reads_slot(leftCopyInstruction, 0u, &leftTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(initialResultInstruction, leftTempSlot, &resultSlot) ||
        firstJumpIfFalseInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        firstJumpIfFalseInstruction->instruction.operandExtra != leftTempSlot ||
        firstJumpIfFalseInstruction->instruction.operand.operand2[0] != 2 ||
        !backend_aot_c_bool_stack_copy_reads_slot(middleCopyInstruction, 1u, &middleTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(middleResultInstruction, middleTempSlot, &middleResultSlot) ||
        middleResultSlot != resultSlot ||
        !backend_aot_c_bool_stack_copy_reads_slot(secondConditionCopyInstruction, resultSlot, &secondConditionSlot) ||
        secondJumpIfFalseInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        secondJumpIfFalseInstruction->instruction.operandExtra != resultSlot ||
        secondJumpIfFalseInstruction->instruction.operand.operand2[0] != 2 ||
        !backend_aot_c_bool_stack_copy_reads_slot(rightCopyInstruction, 2u, &rightTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(finalResultInstruction, rightTempSlot, &finalResultSlot) ||
        finalResultSlot != secondConditionSlot) {
        return ZR_FALSE;
    }

    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == finalResultSlot);
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_or_return(
        const SZrFunction *function) {
    const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
    const TZrInstruction *initialResultInstruction = &function->instructionsList[1];
    const TZrInstruction *firstJumpIfFalseInstruction = &function->instructionsList[2];
    const TZrInstruction *firstJumpEndInstruction = &function->instructionsList[3];
    const TZrInstruction *middleCopyInstruction = &function->instructionsList[4];
    const TZrInstruction *middleResultInstruction = &function->instructionsList[5];
    const TZrInstruction *secondConditionCopyInstruction = &function->instructionsList[6];
    const TZrInstruction *secondJumpIfFalseInstruction = &function->instructionsList[7];
    const TZrInstruction *secondJumpEndInstruction = &function->instructionsList[8];
    const TZrInstruction *rightCopyInstruction = &function->instructionsList[9];
    const TZrInstruction *finalResultInstruction = &function->instructionsList[10];
    const TZrInstruction *returnInstruction = &function->instructionsList[11];
    TZrUInt32 leftTempSlot;
    TZrUInt32 resultSlot;
    TZrUInt32 middleTempSlot;
    TZrUInt32 middleResultSlot;
    TZrUInt32 secondConditionSlot;
    TZrUInt32 rightTempSlot;
    TZrUInt32 finalResultSlot;

    if (!backend_aot_c_bool_stack_copy_reads_slot(leftCopyInstruction, 0u, &leftTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(initialResultInstruction, leftTempSlot, &resultSlot) ||
        firstJumpIfFalseInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        firstJumpIfFalseInstruction->instruction.operandExtra != leftTempSlot ||
        firstJumpIfFalseInstruction->instruction.operand.operand2[0] != 1 ||
        firstJumpEndInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP) ||
        firstJumpEndInstruction->instruction.operand.operand2[0] != 2 ||
        !backend_aot_c_bool_stack_copy_reads_slot(middleCopyInstruction, 1u, &middleTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(middleResultInstruction, middleTempSlot, &middleResultSlot) ||
        middleResultSlot != resultSlot ||
        !backend_aot_c_bool_stack_copy_reads_slot(secondConditionCopyInstruction, resultSlot, &secondConditionSlot) ||
        secondJumpIfFalseInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        secondJumpIfFalseInstruction->instruction.operandExtra != resultSlot ||
        secondJumpIfFalseInstruction->instruction.operand.operand2[0] != 1 ||
        secondJumpEndInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP) ||
        secondJumpEndInstruction->instruction.operand.operand2[0] != 2 ||
        !backend_aot_c_bool_stack_copy_reads_slot(rightCopyInstruction, 2u, &rightTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(finalResultInstruction, rightTempSlot, &finalResultSlot) ||
        finalResultSlot != secondConditionSlot) {
        return ZR_FALSE;
    }

    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == finalResultSlot);
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_return(
        const SZrFunction *function,
        EZrInstructionCode logicalOperationCode) {
    const TZrInstruction *firstLogicalInstruction;
    const TZrInstruction *secondLogicalInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        (function->instructionsLength != 3u &&
         function->instructionsLength != 10u &&
         function->instructionsLength != 12u) ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[2].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 10u) {
        if (logicalOperationCode != ZR_INSTRUCTION_ENUM(LOGICAL_AND)) {
            return ZR_FALSE;
        }

        return backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_and_return(function);
    }

    if (function->instructionsLength == 12u) {
        if (logicalOperationCode != ZR_INSTRUCTION_ENUM(LOGICAL_OR)) {
            return ZR_FALSE;
        }

        return backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_or_return(function);
    }

    firstLogicalInstruction = &function->instructionsList[0];
    secondLogicalInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (!backend_aot_c_try_read_bool_logical_operands(firstLogicalInstruction,
                                                      logicalOperationCode,
                                                      &firstLeftSlot,
                                                      &firstRightSlot,
                                                      &firstResultSlot) ||
        !backend_aot_c_try_read_bool_logical_operands(secondLogicalInstruction,
                                                      logicalOperationCode,
                                                      &secondLeftSlot,
                                                      &secondRightSlot,
                                                      &secondResultSlot)) {
        return ZR_FALSE;
    }

    return (TZrBool)(backend_aot_c_slots_are_0_and_1(firstLeftSlot, firstRightSlot) &&
                     backend_aot_c_slots_are_temp_and_2(secondLeftSlot, secondRightSlot, firstResultSlot) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_AND));
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_or_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_OR));
}

TZrBool backend_aot_c_can_emit_typed_bool_three_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_and_return(function) ||
                     backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_or_return(function));
}

void backend_aot_c_write_bool_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2);\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_three_arg_logical_and_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {\n"
            "    (void)state;\n"
            "    return (TZrBool)(zr_aot_arg0 && zr_aot_arg1 && zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_three_arg_logical_or_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {\n"
            "    (void)state;\n"
            "    return (TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

TZrBool backend_aot_c_try_write_bool_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry) {
    if (file == ZR_NULL || entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_and_return(entry->function)) {
        backend_aot_c_write_bool_three_arg_logical_and_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_or_return(entry->function)) {
        backend_aot_c_write_bool_three_arg_logical_or_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
