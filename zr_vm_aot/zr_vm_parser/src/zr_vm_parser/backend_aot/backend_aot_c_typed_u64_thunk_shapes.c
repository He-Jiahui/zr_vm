#include "backend_aot_c_typed_u64_thunk_shapes.h"

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
