#include "backend_aot_c_typed_f64_three_arg_shapes.h"

static TZrBool backend_aot_c_type_ref_is_f64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_FLOAT ||
                     typeRef->baseType == ZR_VALUE_TYPE_DOUBLE ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_F64);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_arg2_add_return(const SZrFunction *function) {
    const TZrInstruction *firstAddInstruction;
    const TZrInstruction *secondAddInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 3u ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[2].type)) {
        return ZR_FALSE;
    }

    firstAddInstruction = &function->instructionsList[0];
    secondAddInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (firstAddInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_FLOAT) ||
        secondAddInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(ADD_FLOAT)) {
        return ZR_FALSE;
    }

    firstLeftSlot = firstAddInstruction->instruction.operand.operand1[0];
    firstRightSlot = firstAddInstruction->instruction.operand.operand1[1];
    firstResultSlot = firstAddInstruction->instruction.operandExtra;
    secondLeftSlot = secondAddInstruction->instruction.operand.operand1[0];
    secondRightSlot = secondAddInstruction->instruction.operand.operand1[1];
    secondResultSlot = secondAddInstruction->instruction.operandExtra;

    return (TZrBool)(((firstLeftSlot == 0u && firstRightSlot == 1u) ||
                      (firstLeftSlot == 1u && firstRightSlot == 0u)) &&
                     ((secondLeftSlot == firstResultSlot && secondRightSlot == 2u) ||
                      (secondLeftSlot == 2u && secondRightSlot == firstResultSlot)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_arg2_subtract_return(const SZrFunction *function) {
    const TZrInstruction *firstSubtractInstruction;
    const TZrInstruction *secondSubtractInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 3u ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[2].type)) {
        return ZR_FALSE;
    }

    firstSubtractInstruction = &function->instructionsList[0];
    secondSubtractInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (firstSubtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_FLOAT) ||
        secondSubtractInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUB_FLOAT)) {
        return ZR_FALSE;
    }

    firstLeftSlot = firstSubtractInstruction->instruction.operand.operand1[0];
    firstRightSlot = firstSubtractInstruction->instruction.operand.operand1[1];
    firstResultSlot = firstSubtractInstruction->instruction.operandExtra;
    secondLeftSlot = secondSubtractInstruction->instruction.operand.operand1[0];
    secondRightSlot = secondSubtractInstruction->instruction.operand.operand1[1];
    secondResultSlot = secondSubtractInstruction->instruction.operandExtra;

    return (TZrBool)(firstLeftSlot == 0u &&
                     firstRightSlot == 1u &&
                     secondLeftSlot == firstResultSlot &&
                     secondRightSlot == 2u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_arg2_multiply_return(const SZrFunction *function) {
    const TZrInstruction *firstMultiplyInstruction;
    const TZrInstruction *secondMultiplyInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 3u ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[2].type)) {
        return ZR_FALSE;
    }

    firstMultiplyInstruction = &function->instructionsList[0];
    secondMultiplyInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (firstMultiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_FLOAT) ||
        secondMultiplyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MUL_FLOAT)) {
        return ZR_FALSE;
    }

    firstLeftSlot = firstMultiplyInstruction->instruction.operand.operand1[0];
    firstRightSlot = firstMultiplyInstruction->instruction.operand.operand1[1];
    firstResultSlot = firstMultiplyInstruction->instruction.operandExtra;
    secondLeftSlot = secondMultiplyInstruction->instruction.operand.operand1[0];
    secondRightSlot = secondMultiplyInstruction->instruction.operand.operand1[1];
    secondResultSlot = secondMultiplyInstruction->instruction.operandExtra;

    return (TZrBool)(((firstLeftSlot == 0u && firstRightSlot == 1u) ||
                      (firstLeftSlot == 1u && firstRightSlot == 0u)) &&
                     ((secondLeftSlot == firstResultSlot && secondRightSlot == 2u) ||
                      (secondLeftSlot == 2u && secondRightSlot == firstResultSlot)) &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_arg2_divide_return(const SZrFunction *function) {
    const TZrInstruction *firstDivideInstruction;
    const TZrInstruction *secondDivideInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 3u ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[2].type)) {
        return ZR_FALSE;
    }

    firstDivideInstruction = &function->instructionsList[0];
    secondDivideInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (firstDivideInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_FLOAT) ||
        secondDivideInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(DIV_FLOAT)) {
        return ZR_FALSE;
    }

    firstLeftSlot = firstDivideInstruction->instruction.operand.operand1[0];
    firstRightSlot = firstDivideInstruction->instruction.operand.operand1[1];
    firstResultSlot = firstDivideInstruction->instruction.operandExtra;
    secondLeftSlot = secondDivideInstruction->instruction.operand.operand1[0];
    secondRightSlot = secondDivideInstruction->instruction.operand.operand1[1];
    secondResultSlot = secondDivideInstruction->instruction.operandExtra;

    return (TZrBool)(firstLeftSlot == 0u &&
                     firstRightSlot == 1u &&
                     secondLeftSlot == firstResultSlot &&
                     secondRightSlot == 2u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}

TZrBool backend_aot_c_try_get_f64_arg0_arg1_arg2_modulo_return(const SZrFunction *function) {
    const TZrInstruction *firstModuloInstruction;
    const TZrInstruction *secondModuloInstruction;
    const TZrInstruction *returnInstruction;
    TZrUInt32 firstLeftSlot;
    TZrUInt32 firstRightSlot;
    TZrUInt32 firstResultSlot;
    TZrUInt32 secondLeftSlot;
    TZrUInt32 secondRightSlot;
    TZrUInt32 secondResultSlot;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 3u ||
        function->parameterCount != 3 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 3u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_f64(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[2].type)) {
        return ZR_FALSE;
    }

    firstModuloInstruction = &function->instructionsList[0];
    secondModuloInstruction = &function->instructionsList[1];
    returnInstruction = &function->instructionsList[2];
    if (firstModuloInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_FLOAT) ||
        secondModuloInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(MOD_FLOAT)) {
        return ZR_FALSE;
    }

    firstLeftSlot = firstModuloInstruction->instruction.operand.operand1[0];
    firstRightSlot = firstModuloInstruction->instruction.operand.operand1[1];
    firstResultSlot = firstModuloInstruction->instruction.operandExtra;
    secondLeftSlot = secondModuloInstruction->instruction.operand.operand1[0];
    secondRightSlot = secondModuloInstruction->instruction.operand.operand1[1];
    secondResultSlot = secondModuloInstruction->instruction.operandExtra;

    return (TZrBool)(firstLeftSlot == 0u &&
                     firstRightSlot == 1u &&
                     secondLeftSlot == firstResultSlot &&
                     secondRightSlot == 2u &&
                     returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == secondResultSlot);
}
