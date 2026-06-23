#include "backend_aot_c_typed_u64_thunk_shapes.h"

typedef TZrBool (*TZrAotReadU64BinaryOperands)(const TZrInstruction *instruction,
                                               TZrUInt32 *outLeftSlot,
                                               TZrUInt32 *outRightSlot);

static TZrBool backend_aot_c_type_ref_is_u64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_UINT8 ||
                     typeRef->baseType == ZR_VALUE_TYPE_UINT16 ||
                     typeRef->baseType == ZR_VALUE_TYPE_UINT32 ||
                     typeRef->baseType == ZR_VALUE_TYPE_UINT64 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_U8 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_U16 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_U32 ||
                      typeRef->staticCType == ZR_STATIC_C_TYPE_U64);
}

static TZrBool backend_aot_c_try_read_u64_add_operands(const TZrInstruction *instruction,
                                                       TZrUInt32 *outLeftSlot,
                                                       TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_UNSIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)) {
        *outLeftSlot = instruction->instruction.operand.operand1[0];
        *outRightSlot = instruction->instruction.operand.operand1[1];
        return ZR_TRUE;
    }
    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)) {
        *outLeftSlot = instruction->instruction.operand.operand0[0];
        *outRightSlot = instruction->instruction.operand.operand0[1];
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_try_read_u64_multiply_operands(const TZrInstruction *instruction,
                                                            TZrUInt32 *outLeftSlot,
                                                            TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_UNSIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) {
        *outLeftSlot = instruction->instruction.operand.operand1[0];
        *outRightSlot = instruction->instruction.operand.operand1[1];
        return ZR_TRUE;
    }
    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)) {
        *outLeftSlot = instruction->instruction.operand.operand0[0];
        *outRightSlot = instruction->instruction.operand.operand0[1];
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_try_read_u64_subtract_operands(const TZrInstruction *instruction,
                                                            TZrUInt32 *outLeftSlot,
                                                            TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_UNSIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)) {
        *outLeftSlot = instruction->instruction.operand.operand1[0];
        *outRightSlot = instruction->instruction.operand.operand1[1];
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_try_read_u64_divide_operands(const TZrInstruction *instruction,
                                                          TZrUInt32 *outLeftSlot,
                                                          TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_UNSIGNED)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_read_u64_modulo_operands(const TZrInstruction *instruction,
                                                          TZrUInt32 *outLeftSlot,
                                                          TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_UNSIGNED)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_read_u64_bitwise_and_operands(const TZrInstruction *instruction,
                                                               TZrUInt32 *outLeftSlot,
                                                               TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(BITWISE_AND)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_read_u64_bitwise_or_operands(const TZrInstruction *instruction,
                                                              TZrUInt32 *outLeftSlot,
                                                              TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(BITWISE_OR)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_read_u64_bitwise_xor_operands(const TZrInstruction *instruction,
                                                               TZrUInt32 *outLeftSlot,
                                                               TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(BITWISE_XOR)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(
        const SZrFunction *function,
        TZrAotReadU64BinaryOperands readOperands,
        TZrBool preserveOperandOrder) {
    const TZrInstruction *firstBinaryInstruction;
    const TZrInstruction *secondBinaryInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 3u ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[2].type) ||
        readOperands == ZR_NULL) {
        return ZR_FALSE;
    }

    firstBinaryInstruction = &function->instructionsList[0];
    secondBinaryInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (!readOperands(firstBinaryInstruction, &firstLeftSlot, &firstRightSlot) ||
        !readOperands(secondBinaryInstruction, &secondLeftSlot, &secondRightSlot)) {
        return ZR_FALSE;
    }

    firstResultSlot = firstBinaryInstruction->instruction.operandExtra;
    secondResultSlot = secondBinaryInstruction->instruction.operandExtra;
    if (preserveOperandOrder) {
        return (TZrBool)(firstLeftSlot == 0u &&
                         firstRightSlot == 1u &&
                         secondLeftSlot == firstResultSlot &&
                         secondRightSlot == 2u &&
                         returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                         returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
    }

    return (TZrBool)(((firstLeftSlot == 0u && firstRightSlot == 1u) ||
                      (firstLeftSlot == 1u && firstRightSlot == 0u)) &&
                     ((secondLeftSlot == firstResultSlot && secondRightSlot == 2u) ||
                      (secondLeftSlot == 2u && secondRightSlot == firstResultSlot)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_add_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(function,
                                                                 backend_aot_c_try_read_u64_add_operands,
                                                                 ZR_FALSE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_multiply_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(function,
                                                                 backend_aot_c_try_read_u64_multiply_operands,
                                                                 ZR_FALSE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_subtract_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(function,
                                                                 backend_aot_c_try_read_u64_subtract_operands,
                                                                 ZR_TRUE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_divide_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(function,
                                                                 backend_aot_c_try_read_u64_divide_operands,
                                                                 ZR_TRUE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(function,
                                                                 backend_aot_c_try_read_u64_modulo_operands,
                                                                 ZR_TRUE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_u64_bitwise_and_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_or_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_u64_bitwise_or_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_arg2_bitwise_xor_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_u64_bitwise_xor_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_add_return(const SZrFunction *function) {
    const TZrInstruction *addInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        addInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_UNSIGNED) ||
            addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST) ||
            addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
            addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)) {
            leftSlot = addInstruction->instruction.operand.operand1[0];
            rightSlot = addInstruction->instruction.operand.operand1[1];
        } else if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)) {
            leftSlot = addInstruction->instruction.operand.operand0[0];
            rightSlot = addInstruction->instruction.operand.operand0[1];
        } else {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *leftConvertInstruction = &function->instructionsList[0];
        const TZrInstruction *rightConvertInstruction = &function->instructionsList[1];
        TZrUInt32 convertedLeftSlot;
        TZrUInt32 convertedRightSlot;

        addInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if (leftConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            rightConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            leftConvertInstruction->instruction.operand.operand1[0] != 0u ||
            rightConvertInstruction->instruction.operand.operand1[0] != 1u ||
            (addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_SIGNED) &&
             addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        convertedLeftSlot = leftConvertInstruction->instruction.operandExtra;
        convertedRightSlot = rightConvertInstruction->instruction.operandExtra;
        leftSlot = addInstruction->instruction.operand.operand1[0];
        rightSlot = addInstruction->instruction.operand.operand1[1];
        if (!((leftSlot == convertedLeftSlot && rightSlot == convertedRightSlot) ||
              (leftSlot == convertedRightSlot && rightSlot == convertedLeftSlot))) {
            return ZR_FALSE;
        }
        leftSlot = 0u;
        rightSlot = 1u;
    } else if (function->instructionsLength == 6u) {
        const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
        const TZrInstruction *rightCopyInstruction = &function->instructionsList[1];
        const TZrInstruction *leftConvertInstruction = &function->instructionsList[2];
        const TZrInstruction *rightConvertInstruction = &function->instructionsList[3];
        TZrUInt32 copiedLeftSlot;
        TZrUInt32 copiedRightSlot;
        TZrUInt32 convertedLeftSlot;
        TZrUInt32 convertedRightSlot;

        addInstruction = &function->instructionsList[4];
        returnInstruction = &function->instructionsList[5];
        if ((leftCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             leftCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            (rightCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             rightCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            leftCopyInstruction->instruction.operand.operand2[0] != 0u ||
            rightCopyInstruction->instruction.operand.operand2[0] != 1u ||
            leftConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            rightConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            (addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_SIGNED) &&
             addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        copiedLeftSlot = leftCopyInstruction->instruction.operandExtra;
        copiedRightSlot = rightCopyInstruction->instruction.operandExtra;
        convertedLeftSlot = leftConvertInstruction->instruction.operandExtra;
        convertedRightSlot = rightConvertInstruction->instruction.operandExtra;
        if (leftConvertInstruction->instruction.operand.operand1[0] != copiedLeftSlot ||
            rightConvertInstruction->instruction.operand.operand1[0] != copiedRightSlot) {
            return ZR_FALSE;
        }

        leftSlot = addInstruction->instruction.operand.operand1[0];
        rightSlot = addInstruction->instruction.operand.operand1[1];
        if (!((leftSlot == convertedLeftSlot && rightSlot == convertedRightSlot) ||
              (leftSlot == convertedRightSlot && rightSlot == convertedLeftSlot))) {
            return ZR_FALSE;
        }
        leftSlot = 0u;
        rightSlot = 1u;
    } else {
        return ZR_FALSE;
    }

    resultSlot = addInstruction->instruction.operandExtra;
    return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                      (leftSlot == 1u && rightSlot == 0u)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_multiply_return(const SZrFunction *function) {
    const TZrInstruction *multiplyInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        multiplyInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_UNSIGNED) ||
            multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST) ||
            multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
            multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) {
            leftSlot = multiplyInstruction->instruction.operand.operand1[0];
            rightSlot = multiplyInstruction->instruction.operand.operand1[1];
        } else if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)) {
            leftSlot = multiplyInstruction->instruction.operand.operand0[0];
            rightSlot = multiplyInstruction->instruction.operand.operand0[1];
        } else {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *leftConvertInstruction = &function->instructionsList[0];
        const TZrInstruction *rightConvertInstruction = &function->instructionsList[1];
        TZrUInt32 convertedLeftSlot;
        TZrUInt32 convertedRightSlot;

        multiplyInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if (leftConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            rightConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            leftConvertInstruction->instruction.operand.operand1[0] != 0u ||
            rightConvertInstruction->instruction.operand.operand1[0] != 1u ||
            (multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED) &&
             multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        convertedLeftSlot = leftConvertInstruction->instruction.operandExtra;
        convertedRightSlot = rightConvertInstruction->instruction.operandExtra;
        leftSlot = multiplyInstruction->instruction.operand.operand1[0];
        rightSlot = multiplyInstruction->instruction.operand.operand1[1];
        if (!((leftSlot == convertedLeftSlot && rightSlot == convertedRightSlot) ||
              (leftSlot == convertedRightSlot && rightSlot == convertedLeftSlot))) {
            return ZR_FALSE;
        }
        leftSlot = 0u;
        rightSlot = 1u;
    } else if (function->instructionsLength == 6u) {
        const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
        const TZrInstruction *rightCopyInstruction = &function->instructionsList[1];
        const TZrInstruction *leftConvertInstruction = &function->instructionsList[2];
        const TZrInstruction *rightConvertInstruction = &function->instructionsList[3];
        TZrUInt32 copiedLeftSlot;
        TZrUInt32 copiedRightSlot;
        TZrUInt32 convertedLeftSlot;
        TZrUInt32 convertedRightSlot;

        multiplyInstruction = &function->instructionsList[4];
        returnInstruction = &function->instructionsList[5];
        if ((leftCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             leftCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            (rightCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             rightCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            leftCopyInstruction->instruction.operand.operand2[0] != 0u ||
            rightCopyInstruction->instruction.operand.operand2[0] != 1u ||
            leftConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            rightConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            (multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED) &&
             multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        copiedLeftSlot = leftCopyInstruction->instruction.operandExtra;
        copiedRightSlot = rightCopyInstruction->instruction.operandExtra;
        convertedLeftSlot = leftConvertInstruction->instruction.operandExtra;
        convertedRightSlot = rightConvertInstruction->instruction.operandExtra;
        if (leftConvertInstruction->instruction.operand.operand1[0] != copiedLeftSlot ||
            rightConvertInstruction->instruction.operand.operand1[0] != copiedRightSlot) {
            return ZR_FALSE;
        }

        leftSlot = multiplyInstruction->instruction.operand.operand1[0];
        rightSlot = multiplyInstruction->instruction.operand.operand1[1];
        if (!((leftSlot == convertedLeftSlot && rightSlot == convertedRightSlot) ||
              (leftSlot == convertedRightSlot && rightSlot == convertedLeftSlot))) {
            return ZR_FALSE;
        }
        leftSlot = 0u;
        rightSlot = 1u;
    } else {
        return ZR_FALSE;
    }

    resultSlot = multiplyInstruction->instruction.operandExtra;
    return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                      (leftSlot == 1u && rightSlot == 0u)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_u64_arg0_arg1_bitwise_return(const SZrFunction *function,
                                                                  TZrUInt32 operationCode) {
    const TZrInstruction *bitwiseInstruction;
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    bitwiseInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (bitwiseInstruction->instruction.operationCode != operationCode) {
        return ZR_FALSE;
    }

    leftSlot = bitwiseInstruction->instruction.operand.operand1[0];
    rightSlot = bitwiseInstruction->instruction.operand.operand1[1];
    resultSlot = bitwiseInstruction->instruction.operandExtra;
    return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                      (leftSlot == 1u && rightSlot == 0u)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_bitwise_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_bitwise_return(function, ZR_INSTRUCTION_ENUM(BITWISE_AND));
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_bitwise_or_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_bitwise_return(function, ZR_INSTRUCTION_ENUM(BITWISE_OR));
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_bitwise_xor_return(const SZrFunction *function) {
    return backend_aot_c_try_get_u64_arg0_arg1_bitwise_return(function, ZR_INSTRUCTION_ENUM(BITWISE_XOR));
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_divide_return(const SZrFunction *function) {
    const TZrInstruction *divideInstruction;
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    divideInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (divideInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_UNSIGNED)) {
        return ZR_FALSE;
    }

    leftSlot = divideInstruction->instruction.operand.operand1[0];
    rightSlot = divideInstruction->instruction.operand.operand1[1];
    resultSlot = divideInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_modulo_return(const SZrFunction *function) {
    const TZrInstruction *moduloInstruction;
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    moduloInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (moduloInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_UNSIGNED)) {
        return ZR_FALSE;
    }

    leftSlot = moduloInstruction->instruction.operand.operand1[0];
    rightSlot = moduloInstruction->instruction.operand.operand1[1];
    resultSlot = moduloInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_u64_arg0_arg1_subtract_return(const SZrFunction *function) {
    const TZrInstruction *subtractInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        subtractInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED) &&
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST) &&
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED) &&
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)) {
            return ZR_FALSE;
        }

        leftSlot = subtractInstruction->instruction.operand.operand1[0];
        rightSlot = subtractInstruction->instruction.operand.operand1[1];
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *leftConvertInstruction = &function->instructionsList[0];
        const TZrInstruction *rightConvertInstruction = &function->instructionsList[1];
        TZrUInt32 convertedLeftSlot;
        TZrUInt32 convertedRightSlot;

        subtractInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if (leftConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            rightConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            leftConvertInstruction->instruction.operand.operand1[0] != 0u ||
            rightConvertInstruction->instruction.operand.operand1[0] != 1u ||
            (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED) &&
             subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        convertedLeftSlot = leftConvertInstruction->instruction.operandExtra;
        convertedRightSlot = rightConvertInstruction->instruction.operandExtra;
        leftSlot = subtractInstruction->instruction.operand.operand1[0];
        rightSlot = subtractInstruction->instruction.operand.operand1[1];
        if (leftSlot != convertedLeftSlot || rightSlot != convertedRightSlot) {
            return ZR_FALSE;
        }
        leftSlot = 0u;
        rightSlot = 1u;
    } else if (function->instructionsLength == 6u) {
        const TZrInstruction *leftCopyInstruction = &function->instructionsList[0];
        const TZrInstruction *rightCopyInstruction = &function->instructionsList[1];
        const TZrInstruction *leftConvertInstruction = &function->instructionsList[2];
        const TZrInstruction *rightConvertInstruction = &function->instructionsList[3];
        TZrUInt32 copiedLeftSlot;
        TZrUInt32 copiedRightSlot;
        TZrUInt32 convertedLeftSlot;
        TZrUInt32 convertedRightSlot;

        subtractInstruction = &function->instructionsList[4];
        returnInstruction = &function->instructionsList[5];
        if ((leftCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             leftCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            (rightCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             rightCopyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            leftCopyInstruction->instruction.operand.operand2[0] != 0u ||
            rightCopyInstruction->instruction.operand.operand2[0] != 1u ||
            leftConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            rightConvertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED) &&
             subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        copiedLeftSlot = leftCopyInstruction->instruction.operandExtra;
        copiedRightSlot = rightCopyInstruction->instruction.operandExtra;
        convertedLeftSlot = leftConvertInstruction->instruction.operandExtra;
        convertedRightSlot = rightConvertInstruction->instruction.operandExtra;
        if (leftConvertInstruction->instruction.operand.operand1[0] != copiedLeftSlot ||
            rightConvertInstruction->instruction.operand.operand1[0] != copiedRightSlot) {
            return ZR_FALSE;
        }

        leftSlot = subtractInstruction->instruction.operand.operand1[0];
        rightSlot = subtractInstruction->instruction.operand.operand1[1];
        if (leftSlot != convertedLeftSlot || rightSlot != convertedRightSlot) {
            return ZR_FALSE;
        }
        leftSlot = 0u;
        rightSlot = 1u;
    } else {
        return ZR_FALSE;
    }

    resultSlot = subtractInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}
