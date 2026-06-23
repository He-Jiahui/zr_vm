#include "backend_aot_c_typed_bool_two_arg_thunks.h"

#include "backend_aot_internal.h"

static TZrBool backend_aot_c_type_ref_is_bool(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_BOOL ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_BOOL);
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_compare_return(const SZrFunction *function,
                                                                   EZrInstructionCode compareOperationCode) {
    const TZrInstruction *logicalInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 2u ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    logicalInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (logicalInstruction->instruction.operationCode != (TZrUInt16)compareOperationCode) {
        return ZR_FALSE;
    }

    leftSlot = logicalInstruction->instruction.operand.operand1[0];
    rightSlot = logicalInstruction->instruction.operand.operand1[1];
    resultSlot = logicalInstruction->instruction.operandExtra;
    return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                      (leftSlot == 1u && rightSlot == 0u)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_arg0_arg1_compare_return(function,
                                                              ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL));
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_not_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_arg0_arg1_compare_return(function,
                                                              ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL));
}

static TZrBool backend_aot_c_bool_stack_copy_reads_slot(const TZrInstruction *instruction,
                                                        TZrUInt32 sourceSlot,
                                                        TZrUInt32 *outDestinationSlot) {
    if (instruction == ZR_NULL ||
        outDestinationSlot == ZR_NULL ||
        (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
         instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
        (TZrUInt32)instruction->instruction.operand.operand2[0] != sourceSlot) {
        return ZR_FALSE;
    }

    *outDestinationSlot = instruction->instruction.operandExtra;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_logical_return(const SZrFunction *function,
                                                                   EZrInstructionCode logicalOperationCode,
                                                                   EZrInstructionCode shortCircuitJumpCode) {
    const TZrInstruction *logicalInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        (function->instructionsLength != 2u && function->instructionsLength != 6u) ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        logicalInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (logicalInstruction->instruction.operationCode != (TZrUInt16)logicalOperationCode) {
            return ZR_FALSE;
        }

        leftSlot = logicalInstruction->instruction.operand.operand1[0];
        rightSlot = logicalInstruction->instruction.operand.operand1[1];
        resultSlot = logicalInstruction->instruction.operandExtra;
        return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                          (leftSlot == 1u && rightSlot == 0u)) &&
                         returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                         returnInstruction->instruction.operand.operand1[0] == resultSlot);
    }

    if (function->instructionsLength == 6u) {
        const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
        const TZrInstruction *initialResultInstruction = &function->instructionsList[1];
        const TZrInstruction *jumpIfFalseInstruction = &function->instructionsList[2];
        const TZrInstruction *rightCopyInstruction = &function->instructionsList[3];
        const TZrInstruction *finalResultInstruction = &function->instructionsList[4];
        TZrUInt32 leftTempSlot;
        TZrUInt32 rightTempSlot;
        TZrUInt32 finalResultSlot;

        returnInstruction = &function->instructionsList[5];
        if (!backend_aot_c_bool_stack_copy_reads_slot(leftCopyInstruction, 0u, &leftTempSlot) ||
            !backend_aot_c_bool_stack_copy_reads_slot(initialResultInstruction, leftTempSlot, &resultSlot) ||
            jumpIfFalseInstruction->instruction.operationCode != (TZrUInt16)shortCircuitJumpCode ||
            jumpIfFalseInstruction->instruction.operandExtra != leftTempSlot ||
            jumpIfFalseInstruction->instruction.operand.operand2[0] != 2u ||
            !backend_aot_c_bool_stack_copy_reads_slot(rightCopyInstruction, 1u, &rightTempSlot) ||
            !backend_aot_c_bool_stack_copy_reads_slot(finalResultInstruction, rightTempSlot, &finalResultSlot) ||
            finalResultSlot != resultSlot) {
            return ZR_FALSE;
        }

        return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                         returnInstruction->instruction.operand.operand1[0] == resultSlot);
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_arg0_arg1_logical_return(function,
                                                              ZR_INSTRUCTION_ENUM(LOGICAL_AND),
                                                              ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE));
}

static TZrBool backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(const SZrFunction *function) {
    const TZrInstruction *logicalInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        (function->instructionsLength != 2u && function->instructionsLength != 7u) ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        logicalInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (logicalInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_OR)) {
            return ZR_FALSE;
        }

        leftSlot = logicalInstruction->instruction.operand.operand1[0];
        rightSlot = logicalInstruction->instruction.operand.operand1[1];
        resultSlot = logicalInstruction->instruction.operandExtra;
        return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                          (leftSlot == 1u && rightSlot == 0u)) &&
                         returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                         returnInstruction->instruction.operand.operand1[0] == resultSlot);
    }

    if (function->instructionsLength == 7u) {
        const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
        const TZrInstruction *initialResultInstruction = &function->instructionsList[1];
        const TZrInstruction *jumpIfFalseInstruction = &function->instructionsList[2];
        const TZrInstruction *jumpEndInstruction = &function->instructionsList[3];
        const TZrInstruction *rightCopyInstruction = &function->instructionsList[4];
        const TZrInstruction *finalResultInstruction = &function->instructionsList[5];
        TZrUInt32 leftTempSlot;
        TZrUInt32 rightTempSlot;
        TZrUInt32 finalResultSlot;

        returnInstruction = &function->instructionsList[6];
        if (!backend_aot_c_bool_stack_copy_reads_slot(leftCopyInstruction, 0u, &leftTempSlot) ||
            !backend_aot_c_bool_stack_copy_reads_slot(initialResultInstruction, leftTempSlot, &resultSlot) ||
            jumpIfFalseInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
            jumpIfFalseInstruction->instruction.operandExtra != leftTempSlot ||
            jumpIfFalseInstruction->instruction.operand.operand2[0] != 1u ||
            jumpEndInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP) ||
            jumpEndInstruction->instruction.operand.operand2[0] != 2u ||
            !backend_aot_c_bool_stack_copy_reads_slot(rightCopyInstruction, 1u, &rightTempSlot) ||
            !backend_aot_c_bool_stack_copy_reads_slot(finalResultInstruction, rightTempSlot, &finalResultSlot) ||
            finalResultSlot != resultSlot) {
            return ZR_FALSE;
        }

        return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                         returnInstruction->instruction.operand.operand1[0] == resultSlot);
    }

    return ZR_FALSE;
}

TZrBool backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_arg0_arg1_equal_return(function) ||
                     backend_aot_c_try_get_bool_arg0_arg1_not_equal_return(function) ||
                     backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(function) ||
                     backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(function));
}

void backend_aot_c_write_bool_two_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_two_arg_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {\n"
            "    (void)state;\n"
            "    return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_two_arg_not_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {\n"
            "    (void)state;\n"
            "    return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_two_arg_logical_and_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {\n"
            "    (void)state;\n"
            "    return (TZrBool)(zr_aot_arg0 && zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_two_arg_logical_or_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1) {\n"
            "    (void)state;\n"
            "    return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

TZrBool backend_aot_c_try_write_bool_two_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry) {
    if (file == ZR_NULL || entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_c_try_get_bool_arg0_arg1_equal_return(entry->function)) {
        backend_aot_c_write_bool_two_arg_equal_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_bool_arg0_arg1_not_equal_return(entry->function)) {
        backend_aot_c_write_bool_two_arg_not_equal_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(entry->function)) {
        backend_aot_c_write_bool_two_arg_logical_and_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    if (backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(entry->function)) {
        backend_aot_c_write_bool_two_arg_logical_or_thunk_definition(file, entry->flatIndex);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
