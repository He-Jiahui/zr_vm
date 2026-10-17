#include "backend_aot/backend_aot_c_typed_bool_three_arg_thunks.h"

#include "backend_aot/backend_aot_internal.h"
#include "backend_aot/backend_aot_c_emitter.h"

/* 识别精确短路控制流后才把解释器栈逻辑替换为 C 的 && 或 || 表达式。 */
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

static TZrBool backend_aot_c_bool_reset_stack_null_targets_slot(const TZrInstruction *instruction,
                                                                TZrUInt32 slot) {
    if (instruction == ZR_NULL ||
        instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(RESET_STACK_NULL)) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->instruction.operandExtra == slot);
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

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_and_with_reset_return(
        const SZrFunction *function) {
    const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
    const TZrInstruction *initialResultInstruction = &function->instructionsList[1];
    const TZrInstruction *firstJumpIfFalseInstruction = &function->instructionsList[2];
    const TZrInstruction *middleCopyInstruction = &function->instructionsList[3];
    const TZrInstruction *middleResultInstruction = &function->instructionsList[4];
    const TZrInstruction *middleResetInstruction = &function->instructionsList[5];
    const TZrInstruction *secondConditionCopyInstruction = &function->instructionsList[6];
    const TZrInstruction *secondJumpIfFalseInstruction = &function->instructionsList[7];
    const TZrInstruction *rightCopyInstruction = &function->instructionsList[8];
    const TZrInstruction *finalResultInstruction = &function->instructionsList[9];
    const TZrInstruction *rightResetInstruction = &function->instructionsList[10];
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
        firstJumpIfFalseInstruction->instruction.operand.operand2[0] != 3 ||
        !backend_aot_c_bool_stack_copy_reads_slot(middleCopyInstruction, 1u, &middleTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(middleResultInstruction, middleTempSlot, &middleResultSlot) ||
        middleResultSlot != resultSlot ||
        !backend_aot_c_bool_reset_stack_null_targets_slot(middleResetInstruction, middleTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(secondConditionCopyInstruction, resultSlot, &secondConditionSlot) ||
        secondJumpIfFalseInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        secondJumpIfFalseInstruction->instruction.operandExtra != resultSlot ||
        secondJumpIfFalseInstruction->instruction.operand.operand2[0] != 3 ||
        !backend_aot_c_bool_stack_copy_reads_slot(rightCopyInstruction, 2u, &rightTempSlot) ||
        !backend_aot_c_bool_stack_copy_reads_slot(finalResultInstruction, rightTempSlot, &finalResultSlot) ||
        finalResultSlot != secondConditionSlot ||
        !backend_aot_c_bool_reset_stack_null_targets_slot(rightResetInstruction, rightTempSlot)) {
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

/* 长度与跳转偏移共同限定短路模板，防止仅按 opcode 误认其他控制流。 */
/* Each nested short-circuit expression owns its result slot. Recognize the
 * extra copy that initializes the outer result, including optional resets. */
static TZrBool backend_aot_c_try_get_bool_three_arg_nested_short_circuit(
        const SZrFunction *function,
        EZrInstructionCode logicalOperationCode) {
    const TZrInstruction *instructions = function->instructionsList;
    TZrBool isOr = (TZrBool)(logicalOperationCode == ZR_INSTRUCTION_ENUM(LOGICAL_OR));
    TZrUInt32 baseLength = isOr ? 13u : 11u;
    TZrBool hasReset = (TZrBool)(function->instructionsLength == baseLength + 2u);
    TZrUInt32 firstRightIndex = isOr ? 4u : 3u;
    TZrUInt32 secondCopyIndex = firstRightIndex + 2u + (hasReset ? 1u : 0u);
    TZrUInt32 secondJumpIndex = secondCopyIndex + 2u;
    TZrUInt32 lastRightIndex = secondJumpIndex + (isOr ? 2u : 1u);
    TZrUInt32 leftTemp, middleTemp, firstResult, secondCondition, finalResult, rightTemp, destination;

    if (function->instructionsLength != baseLength && !hasReset) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_bool_stack_copy_reads_slot(&instructions[0], 0u, &leftTemp) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[1], leftTemp, &firstResult) ||
        instructions[2].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        instructions[2].instruction.operandExtra != leftTemp ||
        instructions[2].instruction.operand.operand2[0] !=
                (isOr ? 1 : (hasReset ? 3 : 2)) ||
        (isOr && (instructions[3].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP) ||
                  instructions[3].instruction.operand.operand2[0] != (hasReset ? 3 : 2))) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[firstRightIndex], 1u, &middleTemp) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[firstRightIndex + 1u], middleTemp, &destination) ||
        destination != firstResult ||
        (hasReset && !backend_aot_c_bool_reset_stack_null_targets_slot(
                             &instructions[firstRightIndex + 2u], middleTemp)) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[secondCopyIndex], firstResult, &secondCondition) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[secondCopyIndex + 1u], secondCondition, &finalResult) ||
        instructions[secondJumpIndex].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        instructions[secondJumpIndex].instruction.operandExtra != secondCondition ||
        instructions[secondJumpIndex].instruction.operand.operand2[0] !=
                (isOr ? 1 : (hasReset ? 3 : 2)) ||
        (isOr && (instructions[secondJumpIndex + 1u].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP) ||
                  instructions[secondJumpIndex + 1u].instruction.operand.operand2[0] != (hasReset ? 3 : 2))) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[lastRightIndex], 2u, &rightTemp) ||
        !backend_aot_c_bool_stack_copy_reads_slot(&instructions[lastRightIndex + 1u], rightTemp, &destination) ||
        destination != finalResult ||
        (hasReset && !backend_aot_c_bool_reset_stack_null_targets_slot(
                             &instructions[lastRightIndex + 2u], rightTemp))) {
        return ZR_FALSE;
    }
    /* Temporary stores/resets must not overwrite an unread argument or the
     * result that a short-circuit branch will return. */
    if (leftTemp < 3u || middleTemp < 3u || rightTemp < 3u ||
        firstResult < 3u || secondCondition < 3u || finalResult < 3u ||
        (hasReset && (middleTemp == firstResult || rightTemp == finalResult))) {
        return ZR_FALSE;
    }
    return (TZrBool)(
            instructions[function->instructionsLength - 1u].instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
            instructions[function->instructionsLength - 1u].instruction.operandExtra == 1u &&
            instructions[function->instructionsLength - 1u].instruction.operand.operand1[0] == finalResult);
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
         function->instructionsLength != 11u &&
         function->instructionsLength != 12u &&
         function->instructionsLength != 13u &&
         function->instructionsLength != 15u) ||
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

    if (function->instructionsLength == 11u ||
        function->instructionsLength == 13u ||
        function->instructionsLength == 15u) {
        return backend_aot_c_try_get_bool_three_arg_nested_short_circuit(
                function, logicalOperationCode);
    }

    /* 两种短路展开拥有不同长度和跳转位移，必须先分流再验证各自拓扑。 */
    if (function->instructionsLength == 10u) {
        if (logicalOperationCode != ZR_INSTRUCTION_ENUM(LOGICAL_AND)) {
            return ZR_FALSE;
        }

        return backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_and_return(function);
    }

    if (function->instructionsLength == 12u) {
        if (logicalOperationCode == ZR_INSTRUCTION_ENUM(LOGICAL_AND)) {
            return backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_and_with_reset_return(function);
        }

        if (logicalOperationCode == ZR_INSTRUCTION_ENUM(LOGICAL_OR)) {
            return backend_aot_c_try_get_bool_arg0_arg1_arg2_short_circuit_or_return(function);
        }

        return ZR_FALSE;
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
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2);\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_three_arg_logical_and_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {\n"
            "    return (TZrBool)(zr_aot_arg0 && zr_aot_arg1 && zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_three_arg_logical_or_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrBool zr_aot_arg0, TZrBool zr_aot_arg1, TZrBool zr_aot_arg2) {\n"
            "    return (TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2);\n"
            "}\n",
            (unsigned)flatIndex);
}

/* 命中次序与 can_emit 保持一致，拒绝未识别函数而不写半个定义。 */
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
