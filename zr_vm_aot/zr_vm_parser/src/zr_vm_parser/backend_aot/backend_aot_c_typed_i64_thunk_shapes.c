#include "backend_aot_c_typed_i64_thunk_shapes.h"

#include "backend_aot_c_emitter.h"

typedef TZrBool (*TZrAotReadI64BinaryOperands)(const TZrInstruction *instruction,
                                               TZrUInt32 *outLeftSlot,
                                               TZrUInt32 *outRightSlot);

static TZrBool backend_aot_c_type_ref_is_i64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_INT64 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_I64);
}

TZrBool backend_aot_c_try_get_i64_constant_return(const SZrFunction *function, TZrInt64 *outValue) {
    const TZrInstruction *loadInstruction;
    const TZrInstruction *copyInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 constantSlot;
    TZrUInt32 returnSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        (function->instructionsLength != 2u && function->instructionsLength != 3u) ||
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
        (function->instructionsLength == 2u &&
         returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN))) {
        return ZR_FALSE;
    }

    constantSlot = loadInstruction->instruction.operandExtra;
    constantIndex = loadInstruction->instruction.operand.operand2[0];
    returnSlot = constantSlot;
    if (function->instructionsLength == 3u) {
        copyInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] < 0 ||
            (TZrUInt32)copyInstruction->instruction.operand.operand2[0] != constantSlot) {
            return ZR_FALSE;
        }
        returnSlot = copyInstruction->instruction.operandExtra;
    }

    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != returnSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_i64_identity_return(const SZrFunction *function) {
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

TZrBool backend_aot_c_try_get_i64_arg0_negate_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_unary_return(function, ZR_INSTRUCTION_ENUM(NEG_SIGNED));
}

TZrBool backend_aot_c_try_get_i64_arg0_bitwise_not_return(const SZrFunction *function) {
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

TZrBool backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(const SZrFunction *function,
                                                                   TZrInt64 *outValue) {
    return backend_aot_c_try_get_i64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_AND),
                                                                 outValue);
}

TZrBool backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(const SZrFunction *function,
                                                                  TZrInt64 *outValue) {
    return backend_aot_c_try_get_i64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_OR),
                                                                 outValue);
}

TZrBool backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(const SZrFunction *function,
                                                                   TZrInt64 *outValue) {
    return backend_aot_c_try_get_i64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_XOR),
                                                                 outValue);
}

TZrBool backend_aot_c_try_get_i64_arg0_add_constant_return(const SZrFunction *function,
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

TZrBool backend_aot_c_try_get_i64_arg0_subtract_constant_return(const SZrFunction *function,
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

TZrBool backend_aot_c_try_get_i64_arg0_multiply_constant_return(const SZrFunction *function,
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

static TZrBool backend_aot_c_try_read_i64_add_operands(const TZrInstruction *instruction,
                                                       TZrUInt32 *outLeftSlot,
                                                       TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
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

static TZrBool backend_aot_c_try_read_i64_subtract_operands(const TZrInstruction *instruction,
                                                            TZrUInt32 *outLeftSlot,
                                                            TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED) ||
        instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)) {
        *outLeftSlot = instruction->instruction.operand.operand1[0];
        *outRightSlot = instruction->instruction.operand.operand1[1];
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool backend_aot_c_try_read_i64_multiply_operands(const TZrInstruction *instruction,
                                                            TZrUInt32 *outLeftSlot,
                                                            TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
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

static TZrBool backend_aot_c_try_read_i64_divide_operands(const TZrInstruction *instruction,
                                                          TZrUInt32 *outLeftSlot,
                                                          TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_SIGNED)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_read_i64_modulo_operands(const TZrInstruction *instruction,
                                                          TZrUInt32 *outLeftSlot,
                                                          TZrUInt32 *outRightSlot) {
    if (instruction == ZR_NULL || outLeftSlot == ZR_NULL || outRightSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_SIGNED)) {
        return ZR_FALSE;
    }

    *outLeftSlot = instruction->instruction.operand.operand1[0];
    *outRightSlot = instruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_read_i64_bitwise_and_operands(const TZrInstruction *instruction,
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

static TZrBool backend_aot_c_try_read_i64_bitwise_or_operands(const TZrInstruction *instruction,
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

static TZrBool backend_aot_c_try_read_i64_bitwise_xor_operands(const TZrInstruction *instruction,
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

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_binary_return(
        const SZrFunction *function,
        TZrAotReadI64BinaryOperands readOperands) {
    const TZrInstruction *binaryInstruction;
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
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type) ||
        readOperands == ZR_NULL) {
        return ZR_FALSE;
    }

    binaryInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (!readOperands(binaryInstruction, &leftSlot, &rightSlot)) {
        return ZR_FALSE;
    }

    resultSlot = binaryInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_add_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_add_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_subtract_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_subtract_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_multiply_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_multiply_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_divide_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_divide_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_modulo_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_modulo_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_bitwise_and_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_bitwise_or_operands);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_binary_return(
            function,
            backend_aot_c_try_read_i64_bitwise_xor_operands);
}

static TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
        const SZrFunction *function,
        TZrAotReadI64BinaryOperands readOperands,
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
        !backend_aot_c_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[2].type) ||
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

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_add_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_subtract_operands,
            ZR_TRUE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_multiply_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_divide_operands,
            ZR_TRUE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_modulo_operands,
            ZR_TRUE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_bitwise_and_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_bitwise_or_operands,
            ZR_FALSE);
}

TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(const SZrFunction *function) {
    return backend_aot_c_try_get_i64_arg0_arg1_arg2_binary_return(
            function,
            backend_aot_c_try_read_i64_bitwise_xor_operands,
            ZR_FALSE);
}
