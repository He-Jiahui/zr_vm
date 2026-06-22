#include "backend_aot_c_typed_i64_thunks.h"

#include "backend_aot_c_emitter.h"

static TZrBool backend_aot_c_type_ref_is_i64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_INT64 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_I64);
}

static TZrBool backend_aot_c_try_get_i64_constant_return(const SZrFunction *function, TZrInt64 *outValue) {
    const TZrInstruction *loadInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 constantSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 2u ||
        function->parameterCount != 0 ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    loadInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
        return ZR_FALSE;
    }

    constantSlot = loadInstruction->instruction.operandExtra;
    constantIndex = loadInstruction->instruction.operand.operand2[0];
    if (returnInstruction->instruction.operand.operand1[0] != constantSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_i64_identity_return(const SZrFunction *function) {
    const TZrInstruction *returnInstruction;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 1u ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    returnInstruction = &function->instructionsList[0];
    return (TZrBool)(returnInstruction->instruction.operationCode ==
                             ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                      returnInstruction->instruction.operand.operand1[0] == 0u);
}

static TZrBool backend_aot_c_try_get_i64_arg0_unary_return(const SZrFunction *function,
                                                           TZrUInt32 operationCode) {
    const TZrInstruction *unaryInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 2u ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    unaryInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (unaryInstruction->instruction.operationCode != operationCode ||
        unaryInstruction->instruction.operand.operand1[0] != 0u) {
        return ZR_FALSE;
    }

    resultSlot = unaryInstruction->instruction.operandExtra;
    return (TZrBool)(returnInstruction->instruction.operationCode ==
                             ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_i64_arg0_negate_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_unary_return(function, ZR_INSTRUCTION_ENUM(NEG_SIGNED));
}

static TZrBool backend_aot_c_try_get_i64_arg0_bitwise_not_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_unary_return(function, ZR_INSTRUCTION_ENUM(BITWISE_NOT));
}

static TZrBool backend_aot_c_try_get_i64_arg0_bitwise_constant_return(const SZrFunction *function,
                                                                      TZrUInt32 operationCode,
                                                                      TZrInt64 *outValue) {
    const TZrInstruction *loadInstruction;
    const TZrInstruction *bitwiseInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argumentSlot = 0u;
    TZrUInt32 constantSlot;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;
    TZrUInt32 instructionOffset = 0u;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 4u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];

        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u) {
            return ZR_FALSE;
        }

        argumentSlot = copyInstruction->instruction.operandExtra;
        instructionOffset = 1u;
    } else if (function->instructionsLength != 3u) {
        return ZR_FALSE;
    }

    loadInstruction = &function->instructionsList[instructionOffset];
    bitwiseInstruction = &function->instructionsList[instructionOffset + 1u];
    returnInstruction = &function->instructionsList[instructionOffset + 2u];
    if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        bitwiseInstruction->instruction.operationCode != operationCode) {
        return ZR_FALSE;
    }

    constantSlot = loadInstruction->instruction.operandExtra;
    constantIndex = loadInstruction->instruction.operand.operand2[0];
    if (!((bitwiseInstruction->instruction.operand.operand1[0] == argumentSlot &&
           bitwiseInstruction->instruction.operand.operand1[1] == constantSlot) ||
          (bitwiseInstruction->instruction.operand.operand1[0] == constantSlot &&
           bitwiseInstruction->instruction.operand.operand1[1] == argumentSlot))) {
        return ZR_FALSE;
    }

    resultSlot = bitwiseInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(const SZrFunction *function,
                                                                          TZrInt64 *outValue) {
    return backend_aot_c_try_get_i64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_AND),
                                                                 outValue);
}

static TZrBool backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(const SZrFunction *function,
                                                                         TZrInt64 *outValue) {
    return backend_aot_c_try_get_i64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_OR),
                                                                 outValue);
}

static TZrBool backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(const SZrFunction *function,
                                                                          TZrInt64 *outValue) {
    return backend_aot_c_try_get_i64_arg0_bitwise_constant_return(function,
                                                                  ZR_INSTRUCTION_ENUM(BITWISE_XOR),
                                                                  outValue);
}

static TZrBool backend_aot_c_try_get_i64_arg0_add_constant_return(const SZrFunction *function,
                                                                  TZrInt64 *outValue) {
    const TZrInstruction *addInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex = -1;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        addInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST) ||
            addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST)) {
            if (addInstruction->instruction.operand.operand1[0] != 0u) {
                return ZR_FALSE;
            }
            constantIndex = addInstruction->instruction.operand.operand1[1];
        } else if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST)) {
            if (addInstruction->instruction.operand.operand0[0] != 0u) {
                return ZR_FALSE;
            }
            constantIndex = addInstruction->instruction.operand.operand1[1];
        } else {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];
        TZrUInt32 constantSlot;

        addInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST)) {
            if (addInstruction->instruction.operand.operand0[0] != 0u ||
                addInstruction->instruction.operand.operand0[1] != constantSlot ||
                addInstruction->instruction.operand.operand1[1] != (TZrUInt32)constantIndex) {
                return ZR_FALSE;
            }
        } else if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
                   addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)) {
            if (addInstruction->instruction.operand.operand1[0] != 0u ||
                addInstruction->instruction.operand.operand1[1] != constantSlot) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }

    resultSlot = addInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_i64_arg0_subtract_constant_return(const SZrFunction *function,
                                                                       TZrInt64 *outValue) {
    const TZrInstruction *subtractInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex = -1;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        subtractInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (subtractInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST) ||
            subtractInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST)) {
            if (subtractInstruction->instruction.operand.operand1[0] != 0u) {
                return ZR_FALSE;
            }
            constantIndex = subtractInstruction->instruction.operand.operand1[1];
        } else if (subtractInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST)) {
            if (subtractInstruction->instruction.operand.operand0[0] != 0u) {
                return ZR_FALSE;
            }
            constantIndex = subtractInstruction->instruction.operand.operand1[1];
        } else {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];
        TZrUInt32 constantSlot;

        subtractInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        if (subtractInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST)) {
            if (subtractInstruction->instruction.operand.operand0[0] != 0u ||
                subtractInstruction->instruction.operand.operand0[1] != constantSlot ||
                subtractInstruction->instruction.operand.operand1[1] != (TZrUInt32)constantIndex) {
                return ZR_FALSE;
            }
        } else if (subtractInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED) ||
                   subtractInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)) {
            if (subtractInstruction->instruction.operand.operand1[0] != 0u ||
                subtractInstruction->instruction.operand.operand1[1] != constantSlot) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }

    resultSlot = subtractInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_i64_arg0_multiply_constant_return(const SZrFunction *function,
                                                                       TZrInt64 *outValue) {
    const TZrInstruction *multiplyInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex = -1;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        multiplyInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST) ||
            multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)) {
            if (multiplyInstruction->instruction.operand.operand1[0] != 0u) {
                return ZR_FALSE;
            }
            constantIndex = multiplyInstruction->instruction.operand.operand1[1];
        } else if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST)) {
            if (multiplyInstruction->instruction.operand.operand0[0] != 0u) {
                return ZR_FALSE;
            }
            constantIndex = multiplyInstruction->instruction.operand.operand1[1];
        } else {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];
        TZrUInt32 constantSlot;

        multiplyInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST)) {
            if (multiplyInstruction->instruction.operand.operand0[0] != 0u ||
                multiplyInstruction->instruction.operand.operand0[1] != constantSlot ||
                multiplyInstruction->instruction.operand.operand1[1] != (TZrUInt32)constantIndex) {
                return ZR_FALSE;
            }
        } else if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
                   multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) {
            if (multiplyInstruction->instruction.operand.operand1[0] != 0u ||
                multiplyInstruction->instruction.operand.operand1[1] != constantSlot) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }

    resultSlot = multiplyInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_add_return(const SZrFunction *function) {
    const TZrInstruction *addInstruction;
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
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    addInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
        addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)) {
        leftSlot = addInstruction->instruction.operand.operand1[0];
        rightSlot = addInstruction->instruction.operand.operand1[1];
    } else if (addInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)) {
        leftSlot = addInstruction->instruction.operand.operand0[0];
        rightSlot = addInstruction->instruction.operand.operand0[1];
    } else {
        return ZR_FALSE;
    }

    resultSlot = addInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_binary_return(const SZrFunction *function,
                                                                 TZrUInt32 operationCode,
                                                                 TZrUInt32 plainDestOperationCode) {
    const TZrInstruction *binaryInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 2u ||
        function->parameterCount != 2 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 2u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    binaryInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (binaryInstruction->instruction.operationCode != operationCode &&
        binaryInstruction->instruction.operationCode != plainDestOperationCode) {
        return ZR_FALSE;
    }

    resultSlot = binaryInstruction->instruction.operandExtra;
    return (TZrBool)(binaryInstruction->instruction.operand.operand1[0] == 0u &&
                     binaryInstruction->instruction.operand.operand1[1] == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_subtract_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(function,
                                                            ZR_INSTRUCTION_ENUM(SUB_SIGNED),
                                                            ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST));
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_multiply_return(const SZrFunction *function) {
    const TZrInstruction *multiplyInstruction;
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
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    multiplyInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
        multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) {
        leftSlot = multiplyInstruction->instruction.operand.operand1[0];
        rightSlot = multiplyInstruction->instruction.operand.operand1[1];
    } else if (multiplyInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK)) {
        leftSlot = multiplyInstruction->instruction.operand.operand0[0];
        rightSlot = multiplyInstruction->instruction.operand.operand0[1];
    } else {
        return ZR_FALSE;
    }

    resultSlot = multiplyInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(function,
                                                            ZR_INSTRUCTION_ENUM(BITWISE_AND),
                                                            ZR_INSTRUCTION_ENUM(BITWISE_AND));
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(function,
                                                            ZR_INSTRUCTION_ENUM(BITWISE_OR),
                                                            ZR_INSTRUCTION_ENUM(BITWISE_OR));
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(function,
                                                            ZR_INSTRUCTION_ENUM(BITWISE_XOR),
                                                            ZR_INSTRUCTION_ENUM(BITWISE_XOR));
}

TZrBool backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function) {
    TZrInt64 ignored;

    return backend_aot_c_try_get_i64_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function) {
    TZrInt64 ignored;

    return (TZrBool)(backend_aot_c_try_get_i64_identity_return(function) ||
                     backend_aot_c_try_get_i64_arg0_negate_return(function) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_not_return(function) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_add_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_subtract_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_i64_arg0_multiply_constant_return(function, &ignored));
}

TZrBool backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_i64_arg0_arg1_add_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_subtract_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_multiply_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(function) ||
                     backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(function));
}

static void backend_aot_c_write_i64_no_arg_thunk_definition(FILE *file,
                                                            TZrUInt32 flatIndex,
                                                            TZrInt64 returnValue) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state) {\n"
            "    (void)state;\n"
            "    return (TZrInt64)%lld;\n"
            "}\n",
            (unsigned)flatIndex,
            (long long)returnValue);
}

static void backend_aot_c_write_i64_one_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpressionFormat,
                                                             TZrBool hasReturnValue,
                                                             TZrInt64 returnValue) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0) {\n"
            "    (void)state;\n",
            (unsigned)flatIndex);
    if (hasReturnValue) {
        fprintf(file, returnExpressionFormat, (long long)returnValue);
    } else {
        fputs(returnExpressionFormat, file);
    }
    fprintf(file, "}\n");
}

static void backend_aot_c_write_i64_two_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpression) {
    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    (void)state;\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

void backend_aot_write_c_typed_i64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_i64_no_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_i64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        }
    }
}

void backend_aot_write_c_typed_i64_thunks(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrInt64 returnValue;
        if (!backend_aot_c_try_get_i64_constant_return(entry->function, &returnValue)) {
            if (backend_aot_c_try_get_i64_identity_return(entry->function)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return zr_aot_arg0;\n",
                                                                 ZR_FALSE,
                                                                 0);
            } else if (backend_aot_c_try_get_i64_arg0_negate_return(entry->function)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(-zr_aot_arg0);\n",
                                                                 ZR_FALSE,
                                                                 0);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_not_return(entry->function)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(~zr_aot_arg0);\n",
                                                                 ZR_FALSE,
                                                                 0);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 & (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 | (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 ^ (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_add_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 + (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_subtract_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 - (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_multiply_constant_return(entry->function, &returnValue)) {
                backend_aot_c_write_i64_one_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 * (TZrInt64)%lld);\n",
                                                                 ZR_TRUE,
                                                                 returnValue);
            } else if (backend_aot_c_try_get_i64_arg0_arg1_add_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_subtract_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 - zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_multiply_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 * zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 & zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 | zr_aot_arg1);\n");
            } else if (backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(entry->function)) {
                backend_aot_c_write_i64_two_arg_thunk_definition(file,
                                                                 entry->flatIndex,
                                                                 "    return (TZrInt64)(zr_aot_arg0 ^ zr_aot_arg1);\n");
            }
            continue;
        }

        backend_aot_c_write_i64_no_arg_thunk_definition(file, entry->flatIndex, returnValue);
    }
}
