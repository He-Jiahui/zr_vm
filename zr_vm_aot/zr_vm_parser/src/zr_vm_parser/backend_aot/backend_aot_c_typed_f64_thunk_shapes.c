#include "backend_aot_c_typed_f64_thunk_shapes.h"

#include "backend_aot_c_emitter.h"

static TZrBool backend_aot_c_type_ref_is_f64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_FLOAT ||
                     typeRef->baseType == ZR_VALUE_TYPE_DOUBLE ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_F64);
}

TZrBool backend_aot_c_try_get_f64_constant_return(const SZrFunction *function,
                                                          TZrFloat64 *outValue) {
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
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    loadInstruction = &function->instructionsList[0];
    if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
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
    } else {
        returnInstruction = &function->instructionsList[1];
    }

    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != returnSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = (TZrFloat64)constantValue->value.nativeObject.nativeDouble;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_f64_identity_return(const SZrFunction *function) {
    const TZrInstruction *returnInstruction;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 1u ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    returnInstruction = &function->instructionsList[0];
    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == 0u);
}

TZrBool backend_aot_c_try_get_f64_arg0_negate_return(const SZrFunction *function) {
    const TZrInstruction *negateInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argSlot = 0u;
    TZrUInt32 resultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        negateInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];

        negateInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u) {
            return ZR_FALSE;
        }

        argSlot = copyInstruction->instruction.operandExtra;
    } else {
        return ZR_FALSE;
    }

    if (negateInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(NEG_FLOAT) ||
        negateInstruction->instruction.operand.operand1[0] != argSlot) {
        return ZR_FALSE;
    }

    resultSlot = negateInstruction->instruction.operandExtra;
    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_add_constant_return(const SZrFunction *function,
                                                                  TZrFloat64 *outValue) {
    const TZrInstruction *addInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argSlot = 0u;
    TZrUInt32 constantSlot;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];

        addInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_FLOAT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];

        addInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_FLOAT)) {
            return ZR_FALSE;
        }

        argSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else {
        return ZR_FALSE;
    }

    if (!((addInstruction->instruction.operand.operand1[0] == argSlot &&
           addInstruction->instruction.operand.operand1[1] == constantSlot) ||
          (addInstruction->instruction.operand.operand1[0] == constantSlot &&
           addInstruction->instruction.operand.operand1[1] == argSlot))) {
        return ZR_FALSE;
    }

    resultSlot = addInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = (TZrFloat64)constantValue->value.nativeObject.nativeDouble;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_f64_arg0_subtract_constant_return(const SZrFunction *function,
                                                                       TZrFloat64 *outValue) {
    const TZrInstruction *subtractInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argSlot = 0u;
    TZrUInt32 constantSlot;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];

        subtractInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_FLOAT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];

        subtractInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_FLOAT)) {
            return ZR_FALSE;
        }

        argSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else {
        return ZR_FALSE;
    }

    if (subtractInstruction->instruction.operand.operand1[0] != argSlot ||
        subtractInstruction->instruction.operand.operand1[1] != constantSlot) {
        return ZR_FALSE;
    }

    resultSlot = subtractInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = (TZrFloat64)constantValue->value.nativeObject.nativeDouble;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_f64_arg0_multiply_constant_return(const SZrFunction *function,
                                                                       TZrFloat64 *outValue) {
    const TZrInstruction *multiplyInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argSlot = 0u;
    TZrUInt32 constantSlot;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];

        multiplyInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_FLOAT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];

        multiplyInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_FLOAT)) {
            return ZR_FALSE;
        }

        argSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else {
        return ZR_FALSE;
    }

    if (!((multiplyInstruction->instruction.operand.operand1[0] == argSlot &&
           multiplyInstruction->instruction.operand.operand1[1] == constantSlot) ||
          (multiplyInstruction->instruction.operand.operand1[0] == constantSlot &&
           multiplyInstruction->instruction.operand.operand1[1] == argSlot))) {
        return ZR_FALSE;
    }

    resultSlot = multiplyInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = (TZrFloat64)constantValue->value.nativeObject.nativeDouble;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_f64_arg0_divide_constant_return(const SZrFunction *function,
                                                                     TZrFloat64 *outValue) {
    const TZrInstruction *divideInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argSlot = 0u;
    TZrUInt32 constantSlot;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;
    TZrFloat64 returnValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];

        divideInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            divideInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_FLOAT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];

        divideInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            divideInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_FLOAT)) {
            return ZR_FALSE;
        }

        argSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else {
        return ZR_FALSE;
    }

    if (divideInstruction->instruction.operand.operand1[0] != argSlot ||
        divideInstruction->instruction.operand.operand1[1] != constantSlot) {
        return ZR_FALSE;
    }

    resultSlot = divideInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_FALSE;
    }

    returnValue = (TZrFloat64)constantValue->value.nativeObject.nativeDouble;
    if (returnValue == 0.0) {
        return ZR_FALSE;
    }

    *outValue = returnValue;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_f64_arg0_modulo_constant_return(const SZrFunction *function,
                                                                     TZrFloat64 *outValue) {
    const TZrInstruction *moduloInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argSlot = 0u;
    TZrUInt32 constantSlot;
    TZrUInt32 resultSlot;
    TZrInt32 constantIndex;
    const SZrTypeValue *constantValue;
    TZrFloat64 returnValue;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];

        moduloInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            moduloInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_FLOAT)) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else if (function->instructionsLength == 4u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];

        moduloInstruction = &function->instructionsList[2];
        returnInstruction = &function->instructionsList[3];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            moduloInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_FLOAT)) {
            return ZR_FALSE;
        }

        argSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
    } else {
        return ZR_FALSE;
    }

    if (moduloInstruction->instruction.operand.operand1[0] != argSlot ||
        moduloInstruction->instruction.operand.operand1[1] != constantSlot) {
        return ZR_FALSE;
    }

    resultSlot = moduloInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_FLOAT(constantValue->type)) {
        return ZR_FALSE;
    }

    returnValue = (TZrFloat64)constantValue->value.nativeObject.nativeDouble;
    if (returnValue == 0.0) {
        return ZR_FALSE;
    }

    *outValue = returnValue;
    return ZR_TRUE;
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_add_return(const SZrFunction *function) {
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
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    addInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_FLOAT)) {
        return ZR_FALSE;
    }

    leftSlot = addInstruction->instruction.operand.operand1[0];
    rightSlot = addInstruction->instruction.operand.operand1[1];
    resultSlot = addInstruction->instruction.operandExtra;
    return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                      (leftSlot == 1u && rightSlot == 0u)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_subtract_return(const SZrFunction *function) {
    const TZrInstruction *subtractInstruction;
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
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    subtractInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_FLOAT)) {
        return ZR_FALSE;
    }

    leftSlot = subtractInstruction->instruction.operand.operand1[0];
    rightSlot = subtractInstruction->instruction.operand.operand1[1];
    resultSlot = subtractInstruction->instruction.operandExtra;
    return (TZrBool)(leftSlot == 0u &&
                     rightSlot == 1u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_multiply_return(const SZrFunction *function) {
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
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    multiplyInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_FLOAT)) {
        return ZR_FALSE;
    }

    leftSlot = multiplyInstruction->instruction.operand.operand1[0];
    rightSlot = multiplyInstruction->instruction.operand.operand1[1];
    resultSlot = multiplyInstruction->instruction.operandExtra;
    return (TZrBool)(((leftSlot == 0u && rightSlot == 1u) ||
                      (leftSlot == 1u && rightSlot == 0u)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_divide_return(const SZrFunction *function) {
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
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    divideInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (divideInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_FLOAT)) {
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

TZrBool backend_aot_c_try_get_f64_arg0_arg1_modulo_return(const SZrFunction *function) {
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
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    moduloInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (moduloInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_FLOAT)) {
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
