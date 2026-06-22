#include "backend_aot_c_typed_u64_thunks.h"

#include "backend_aot_c_emitter.h"
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

static TZrBool backend_aot_c_try_get_u64_constant_return(const SZrFunction *function, TZrUInt64 *outValue) {
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
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
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        *outValue = constantValue->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) ||
        constantValue->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_u64_identity_return(const SZrFunction *function) {
    const TZrInstruction *returnInstruction;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 1u ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    returnInstruction = &function->instructionsList[0];
    return (TZrBool)(returnInstruction->instruction.operationCode ==
                             ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == 0u);
}

static TZrBool backend_aot_c_try_get_u64_arg0_add_constant_return(const SZrFunction *function,
                                                                  TZrUInt64 *outValue) {
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        addInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST) &&
            addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST)) {
            return ZR_FALSE;
        }
        if (addInstruction->instruction.operand.operand1[0] != 0u) {
            return ZR_FALSE;
        }
        constantIndex = addInstruction->instruction.operand.operand1[1];
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];
        TZrUInt32 constantSlot;

        addInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            (addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED) &&
             addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        if (!((addInstruction->instruction.operand.operand1[0] == 0u &&
               addInstruction->instruction.operand.operand1[1] == constantSlot) ||
              (addInstruction->instruction.operand.operand1[0] == constantSlot &&
               addInstruction->instruction.operand.operand1[1] == 0u))) {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 5u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];
        const TZrInstruction *convertInstruction = &function->instructionsList[2];
        TZrUInt32 copiedArgSlot;
        TZrUInt32 convertedArgSlot;
        TZrUInt32 constantSlot;

        addInstruction = &function->instructionsList[3];
        returnInstruction = &function->instructionsList[4];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            convertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            (addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_SIGNED) &&
             addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        copiedArgSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        convertedArgSlot = convertInstruction->instruction.operandExtra;
        if (convertInstruction->instruction.operand.operand1[0] != copiedArgSlot ||
            !((addInstruction->instruction.operand.operand1[0] == convertedArgSlot &&
               addInstruction->instruction.operand.operand1[1] == constantSlot) ||
              (addInstruction->instruction.operand.operand1[0] == constantSlot &&
               addInstruction->instruction.operand.operand1[1] == convertedArgSlot))) {
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
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        *outValue = constantValue->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) ||
        constantValue->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_u64_arg0_subtract_constant_return(const SZrFunction *function,
                                                                       TZrUInt64 *outValue) {
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        subtractInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST) &&
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST) &&
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST) &&
            subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST)) {
            return ZR_FALSE;
        }
        if (subtractInstruction->instruction.operand.operand1[0] != 0u) {
            return ZR_FALSE;
        }
        constantIndex = subtractInstruction->instruction.operand.operand1[1];
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];
        TZrUInt32 constantSlot;

        subtractInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED) &&
             subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST) &&
             subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED) &&
             subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        if (subtractInstruction->instruction.operand.operand1[0] != 0u ||
            subtractInstruction->instruction.operand.operand1[1] != constantSlot) {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 5u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];
        const TZrInstruction *convertInstruction = &function->instructionsList[2];
        TZrUInt32 copiedArgSlot;
        TZrUInt32 convertedArgSlot;
        TZrUInt32 constantSlot;

        subtractInstruction = &function->instructionsList[3];
        returnInstruction = &function->instructionsList[4];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            convertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            (subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED) &&
             subtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        copiedArgSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        convertedArgSlot = convertInstruction->instruction.operandExtra;
        if (convertInstruction->instruction.operand.operand1[0] != copiedArgSlot ||
            subtractInstruction->instruction.operand.operand1[0] != convertedArgSlot ||
            subtractInstruction->instruction.operand.operand1[1] != constantSlot) {
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
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        *outValue = constantValue->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) ||
        constantValue->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_u64_arg0_multiply_constant_return(const SZrFunction *function,
                                                                       TZrUInt64 *outValue) {
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 2u) {
        multiplyInstruction = &function->instructionsList[0];
        returnInstruction = &function->instructionsList[1];
        if (multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST) &&
            multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST) &&
            multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST) &&
            multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)) {
            return ZR_FALSE;
        }
        if (multiplyInstruction->instruction.operand.operand1[0] != 0u) {
            return ZR_FALSE;
        }
        constantIndex = multiplyInstruction->instruction.operand.operand1[1];
    } else if (function->instructionsLength == 3u) {
        const TZrInstruction *loadInstruction = &function->instructionsList[0];
        TZrUInt32 constantSlot;

        multiplyInstruction = &function->instructionsList[1];
        returnInstruction = &function->instructionsList[2];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            (multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_UNSIGNED) &&
             multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST) &&
             multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED) &&
             multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        if (!((multiplyInstruction->instruction.operand.operand1[0] == 0u &&
               multiplyInstruction->instruction.operand.operand1[1] == constantSlot) ||
              (multiplyInstruction->instruction.operand.operand1[0] == constantSlot &&
               multiplyInstruction->instruction.operand.operand1[1] == 0u))) {
            return ZR_FALSE;
        }
    } else if (function->instructionsLength == 5u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *loadInstruction = &function->instructionsList[1];
        const TZrInstruction *convertInstruction = &function->instructionsList[2];
        TZrUInt32 copiedArgSlot;
        TZrUInt32 convertedArgSlot;
        TZrUInt32 constantSlot;

        multiplyInstruction = &function->instructionsList[3];
        returnInstruction = &function->instructionsList[4];
        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u ||
            loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            convertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_INT) ||
            (multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED) &&
             multiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST))) {
            return ZR_FALSE;
        }

        copiedArgSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        convertedArgSlot = convertInstruction->instruction.operandExtra;
        if (convertInstruction->instruction.operand.operand1[0] != copiedArgSlot ||
            !((multiplyInstruction->instruction.operand.operand1[0] == convertedArgSlot &&
               multiplyInstruction->instruction.operand.operand1[1] == constantSlot) ||
              (multiplyInstruction->instruction.operand.operand1[0] == constantSlot &&
               multiplyInstruction->instruction.operand.operand1[1] == convertedArgSlot))) {
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
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        *outValue = constantValue->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) ||
        constantValue->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_u64_arg0_bitwise_constant_return(const SZrFunction *function,
                                                                      TZrUInt32 operationCode,
                                                                      TZrUInt64 *outValue) {
    const TZrInstruction *loadInstruction;
    const TZrInstruction *bitwiseInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 argumentSlot = 0u;
    TZrUInt32 bitwiseRightSlot;
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
        !backend_aot_c_type_ref_is_u64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
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
    } else if (function->instructionsLength == 5u) {
        const TZrInstruction *copyInstruction = &function->instructionsList[0];
        const TZrInstruction *convertInstruction = &function->instructionsList[2];
        TZrUInt32 convertedConstantSlot;

        if ((copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
             copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
            copyInstruction->instruction.operand.operand2[0] != 0u) {
            return ZR_FALSE;
        }

        loadInstruction = &function->instructionsList[1];
        bitwiseInstruction = &function->instructionsList[3];
        returnInstruction = &function->instructionsList[4];
        if (loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            convertInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_UINT) ||
            bitwiseInstruction->instruction.operationCode != operationCode) {
            return ZR_FALSE;
        }

        argumentSlot = copyInstruction->instruction.operandExtra;
        constantSlot = loadInstruction->instruction.operandExtra;
        constantIndex = loadInstruction->instruction.operand.operand2[0];
        convertedConstantSlot = convertInstruction->instruction.operandExtra;
        if (convertInstruction->instruction.operand.operand1[0] != constantSlot ||
            !((bitwiseInstruction->instruction.operand.operand1[0] == argumentSlot &&
               bitwiseInstruction->instruction.operand.operand1[1] == convertedConstantSlot) ||
              (bitwiseInstruction->instruction.operand.operand1[0] == convertedConstantSlot &&
               bitwiseInstruction->instruction.operand.operand1[1] == argumentSlot))) {
            return ZR_FALSE;
        }

        resultSlot = bitwiseInstruction->instruction.operandExtra;
        if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
            returnInstruction->instruction.operand.operand1[0] != resultSlot) {
            return ZR_FALSE;
        }

        constantValue = backend_aot_c_get_constant_value(function, constantIndex);
        if (constantValue == ZR_NULL) {
            return ZR_FALSE;
        }

        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
            *outValue = constantValue->value.nativeObject.nativeUInt64;
            return ZR_TRUE;
        }
        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) ||
            constantValue->value.nativeObject.nativeInt64 < 0) {
            return ZR_FALSE;
        }

        *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
        return ZR_TRUE;
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
    bitwiseRightSlot = constantSlot;
    constantIndex = loadInstruction->instruction.operand.operand2[0];
    if (!((bitwiseInstruction->instruction.operand.operand1[0] == argumentSlot &&
           bitwiseInstruction->instruction.operand.operand1[1] == bitwiseRightSlot) ||
          (bitwiseInstruction->instruction.operand.operand1[0] == bitwiseRightSlot &&
           bitwiseInstruction->instruction.operand.operand1[1] == argumentSlot))) {
        return ZR_FALSE;
    }

    resultSlot = bitwiseInstruction->instruction.operandExtra;
    if (returnInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        returnInstruction->instruction.operand.operand1[0] != resultSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(function, constantIndex);
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        *outValue = constantValue->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (!ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) ||
        constantValue->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_u64_arg0_bitwise_and_constant_return(const SZrFunction *function,
                                                                          TZrUInt64 *outValue) {
    return backend_aot_c_try_get_u64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_AND),
                                                                 outValue);
}

static TZrBool backend_aot_c_try_get_u64_arg0_bitwise_or_constant_return(const SZrFunction *function,
                                                                         TZrUInt64 *outValue) {
    return backend_aot_c_try_get_u64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_OR),
                                                                 outValue);
}

static TZrBool backend_aot_c_try_get_u64_arg0_bitwise_xor_constant_return(const SZrFunction *function,
                                                                          TZrUInt64 *outValue) {
    return backend_aot_c_try_get_u64_arg0_bitwise_constant_return(function,
                                                                 ZR_INSTRUCTION_ENUM(BITWISE_XOR),
                                                                 outValue);
}

TZrBool backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function) {
    TZrUInt64 ignored;

    return backend_aot_c_try_get_u64_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function) {
    TZrUInt64 ignored;

    return (TZrBool)(backend_aot_c_try_get_u64_identity_return(function) ||
                     backend_aot_c_try_get_u64_arg0_add_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_u64_arg0_subtract_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_u64_arg0_multiply_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_u64_arg0_bitwise_and_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_u64_arg0_bitwise_or_constant_return(function, &ignored) ||
                     backend_aot_c_try_get_u64_arg0_bitwise_xor_constant_return(function, &ignored));
}

TZrBool backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_u64_arg0_arg1_add_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_multiply_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_subtract_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_bitwise_and_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_bitwise_or_return(function) ||
                     backend_aot_c_try_get_u64_arg0_arg1_bitwise_xor_return(function));
}

static void backend_aot_c_write_u64_no_arg_thunk_definition(FILE *file,
                                                            TZrUInt32 flatIndex,
                                                            TZrUInt64 returnValue) {
    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state) {\n"
            "    (void)state;\n"
            "    return (TZrUInt64)%llu;\n"
            "}\n",
            (unsigned)flatIndex,
            (unsigned long long)returnValue);
}

static void backend_aot_c_write_u64_one_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpressionFormat,
                                                             TZrBool hasReturnValue,
                                                             TZrUInt64 returnValue) {
    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0) {\n"
            "    (void)state;\n",
            (unsigned)flatIndex);
    if (hasReturnValue) {
        fprintf(file, returnExpressionFormat, (unsigned long long)returnValue);
    } else {
        fputs(returnExpressionFormat, file);
    }
    fprintf(file, "}\n");
}

static void backend_aot_c_write_u64_two_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             const char *returnExpression) {
    fprintf(file,
            "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    (void)state;\n"
            "%s"
            "}\n",
            (unsigned)flatIndex,
            returnExpression);
}

void backend_aot_write_c_typed_u64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_u64_no_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_u64_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_u64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrUInt64 zr_aot_typed_u64_fn_%u(struct SZrState *state, TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        }
    }
}

void backend_aot_write_c_typed_u64_thunks(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrUInt64 returnValue;
        if (backend_aot_c_try_get_u64_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_no_arg_thunk_definition(file, entry->flatIndex, returnValue);
        } else if (backend_aot_c_try_get_u64_identity_return(entry->function)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return zr_aot_arg0;\n",
                                                             ZR_FALSE,
                                                             0u);
        } else if (backend_aot_c_try_get_u64_arg0_add_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrUInt64)(zr_aot_arg0 + (TZrUInt64)%llu);\n",
                    ZR_TRUE,
                    returnValue);
        } else if (backend_aot_c_try_get_u64_arg0_subtract_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrUInt64)(zr_aot_arg0 - (TZrUInt64)%llu);\n",
                    ZR_TRUE,
                    returnValue);
        } else if (backend_aot_c_try_get_u64_arg0_multiply_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrUInt64)(zr_aot_arg0 * (TZrUInt64)%llu);\n",
                    ZR_TRUE,
                    returnValue);
        } else if (backend_aot_c_try_get_u64_arg0_bitwise_and_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrUInt64)(zr_aot_arg0 & (TZrUInt64)%llu);\n",
                    ZR_TRUE,
                    returnValue);
        } else if (backend_aot_c_try_get_u64_arg0_bitwise_or_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrUInt64)(zr_aot_arg0 | (TZrUInt64)%llu);\n",
                    ZR_TRUE,
                    returnValue);
        } else if (backend_aot_c_try_get_u64_arg0_bitwise_xor_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_u64_one_arg_thunk_definition(
                    file,
                    entry->flatIndex,
                    "    return (TZrUInt64)(zr_aot_arg0 ^ (TZrUInt64)%llu);\n",
                    ZR_TRUE,
                    returnValue);
        } else if (backend_aot_c_try_get_u64_arg0_arg1_add_return(entry->function)) {
            backend_aot_c_write_u64_two_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return (TZrUInt64)(zr_aot_arg0 + zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_u64_arg0_arg1_multiply_return(entry->function)) {
            backend_aot_c_write_u64_two_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return (TZrUInt64)(zr_aot_arg0 * zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_u64_arg0_arg1_subtract_return(entry->function)) {
            backend_aot_c_write_u64_two_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return (TZrUInt64)(zr_aot_arg0 - zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_u64_arg0_arg1_bitwise_and_return(entry->function)) {
            backend_aot_c_write_u64_two_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return (TZrUInt64)(zr_aot_arg0 & zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_u64_arg0_arg1_bitwise_or_return(entry->function)) {
            backend_aot_c_write_u64_two_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return (TZrUInt64)(zr_aot_arg0 | zr_aot_arg1);\n");
        } else if (backend_aot_c_try_get_u64_arg0_arg1_bitwise_xor_return(entry->function)) {
            backend_aot_c_write_u64_two_arg_thunk_definition(file,
                                                             entry->flatIndex,
                                                             "    return (TZrUInt64)(zr_aot_arg0 ^ zr_aot_arg1);\n");
        }
    }
}
