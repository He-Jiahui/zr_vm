#include "backend_aot_c_typed_bool_thunks.h"

#include "backend_aot_c_emitter.h"
#include "backend_aot_c_typed_bool_two_arg_thunks.h"
#include "backend_aot_c_typed_bool_three_arg_thunks.h"

static TZrBool backend_aot_c_type_ref_is_bool(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_BOOL ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_BOOL);
}

static TZrBool backend_aot_c_type_ref_is_i64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_INT64 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_I64);
}

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

static TZrBool backend_aot_c_type_ref_is_f64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_FLOAT ||
                     typeRef->baseType == ZR_VALUE_TYPE_DOUBLE ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_F64);
}

static TZrBool backend_aot_c_try_get_bool_constant_return(const SZrFunction *function, TZrBool *outValue) {
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
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
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
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = (TZrBool)(constantValue->value.nativeObject.nativeBool != 0u);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_try_get_bool_identity_return(const SZrFunction *function) {
    const TZrInstruction *returnInstruction;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        function->instructionsLength != 1u ||
        function->parameterCount != 1 ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    returnInstruction = &function->instructionsList[0];
    return (TZrBool)(returnInstruction->instruction.operationCode ==
                             ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == 0u);
}

static TZrBool backend_aot_c_try_get_bool_arg0_logical_not_return(const SZrFunction *function) {
    const TZrInstruction *logicalNotInstruction;
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
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_bool(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL) ||
        logicalNotInstruction->instruction.operand.operand1[0] != 0u) {
        return ZR_FALSE;
    }

    resultSlot = logicalNotInstruction->instruction.operandExtra;
    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(const SZrFunction *function,
                                                                       EZrInstructionCode compareOperationCode) {
    const TZrInstruction *compareInstruction;
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
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_i64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    compareInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (compareInstruction->instruction.operationCode != (TZrUInt16)compareOperationCode ||
        compareInstruction->instruction.operand.operand1[0] != 0u ||
        compareInstruction->instruction.operand.operand1[1] != 1u) {
        return ZR_FALSE;
    }

    resultSlot = compareInstruction->instruction.operandExtra;
    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_less_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED));
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED));
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_not_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED));
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_greater_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED));
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED));
}

static TZrBool backend_aot_c_try_get_bool_i64_arg0_arg1_greater_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_i64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED));
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(const SZrFunction *function,
                                                                       EZrInstructionCode compareOperationCode) {
    const TZrInstruction *compareInstruction;
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
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_u64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    compareInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (compareInstruction->instruction.operationCode != (TZrUInt16)compareOperationCode ||
        compareInstruction->instruction.operand.operand1[0] != 0u ||
        compareInstruction->instruction.operand.operand1[1] != 1u) {
        return ZR_FALSE;
    }

    resultSlot = compareInstruction->instruction.operandExtra;
    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_less_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED));
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED));
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_not_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED));
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED));
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_less_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED));
}

static TZrBool backend_aot_c_try_get_bool_u64_arg0_arg1_greater_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_u64_arg0_arg1_compare_return(function,
                                                                   ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED));
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(const SZrFunction *function,
                                                                       EZrInstructionCode compareOperationCode) {
    const TZrInstruction *compareInstruction;
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
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[0].type) ||
        !backend_aot_c_type_ref_is_f64(&function->parameterMetadata[1].type)) {
        return ZR_FALSE;
    }

    compareInstruction = &function->instructionsList[0];
    returnInstruction = &function->instructionsList[1];
    if (compareInstruction->instruction.operationCode != (TZrUInt16)compareOperationCode ||
        compareInstruction->instruction.operand.operand1[0] != 0u ||
        compareInstruction->instruction.operand.operand1[1] != 1u) {
        return ZR_FALSE;
    }

    resultSlot = compareInstruction->instruction.operandExtra;
    return (TZrBool)(returnInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) &&
                     returnInstruction->instruction.operand.operand1[0] == resultSlot);
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_less_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT));
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT));
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_not_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT));
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_greater_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT));
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_less_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT));
}

static TZrBool backend_aot_c_try_get_bool_f64_arg0_arg1_greater_equal_return(const SZrFunction *function) {
    return backend_aot_c_try_get_bool_f64_arg0_arg1_compare_return(function,
                                                                  ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT));
}

TZrBool backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function) {
    TZrBool ignored;

    return backend_aot_c_try_get_bool_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_identity_return(function) ||
                     backend_aot_c_try_get_bool_arg0_logical_not_return(function));
}

TZrBool backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_i64_arg0_arg1_less_return(function) ||
                     backend_aot_c_try_get_bool_i64_arg0_arg1_equal_return(function) ||
                     backend_aot_c_try_get_bool_i64_arg0_arg1_not_equal_return(function) ||
                     backend_aot_c_try_get_bool_i64_arg0_arg1_greater_return(function) ||
                     backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(function) ||
                     backend_aot_c_try_get_bool_i64_arg0_arg1_greater_equal_return(function));
}

TZrBool backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_u64_arg0_arg1_less_return(function) ||
                     backend_aot_c_try_get_bool_u64_arg0_arg1_equal_return(function) ||
                     backend_aot_c_try_get_bool_u64_arg0_arg1_not_equal_return(function) ||
                     backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(function) ||
                     backend_aot_c_try_get_bool_u64_arg0_arg1_less_equal_return(function) ||
                     backend_aot_c_try_get_bool_u64_arg0_arg1_greater_equal_return(function));
}

TZrBool backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_f64_arg0_arg1_less_return(function) ||
                     backend_aot_c_try_get_bool_f64_arg0_arg1_equal_return(function) ||
                     backend_aot_c_try_get_bool_f64_arg0_arg1_not_equal_return(function) ||
                     backend_aot_c_try_get_bool_f64_arg0_arg1_greater_return(function) ||
                     backend_aot_c_try_get_bool_f64_arg0_arg1_less_equal_return(function) ||
                     backend_aot_c_try_get_bool_f64_arg0_arg1_greater_equal_return(function));
}

static void backend_aot_c_write_bool_no_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             TZrBool returnValue) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(void) {\n"
            "    return %s;\n"
            "}\n",
            (unsigned)flatIndex,
            returnValue ? "ZR_TRUE" : "ZR_FALSE");
}

static void backend_aot_c_write_bool_one_arg_identity_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrBool zr_aot_arg0) {\n"
            "    return zr_aot_arg0;\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_one_arg_logical_not_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrBool zr_aot_arg0) {\n"
            "    return (TZrBool)!zr_aot_arg0;\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_i64_two_arg_less_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_i64_two_arg_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_i64_two_arg_not_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_i64_two_arg_greater_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_i64_two_arg_less_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_i64_two_arg_greater_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_u64_two_arg_less_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_u64_two_arg_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_u64_two_arg_not_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_u64_two_arg_greater_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_u64_two_arg_less_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_u64_two_arg_greater_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_f64_two_arg_less_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_f64_two_arg_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_f64_two_arg_not_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_f64_two_arg_greater_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_f64_two_arg_less_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_f64_two_arg_greater_equal_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1) {\n"
            "    return (TZrBool)(zr_aot_arg0 >= zr_aot_arg1);\n"
            "}\n",
            (unsigned)flatIndex);
}

void backend_aot_write_c_typed_bool_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_bool_no_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(void);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(TZrBool zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_two_arg_thunk(entry->function)) {
            backend_aot_c_write_bool_two_arg_thunk_forward_decl(file, entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_three_arg_thunk(entry->function)) {
            backend_aot_c_write_bool_three_arg_thunk_forward_decl(file, entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(TZrUInt64 zr_aot_arg0, TZrUInt64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(TZrFloat64 zr_aot_arg0, TZrFloat64 zr_aot_arg1);\n",
                    (unsigned)entry->flatIndex);
        }
    }
}

void backend_aot_write_c_typed_bool_thunks(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        TZrBool returnValue;
        if (backend_aot_c_try_get_bool_constant_return(entry->function, &returnValue)) {
            backend_aot_c_write_bool_no_arg_thunk_definition(file, entry->flatIndex, returnValue);
        } else if (backend_aot_c_try_get_bool_identity_return(entry->function)) {
            backend_aot_c_write_bool_one_arg_identity_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_arg0_logical_not_return(entry->function)) {
            backend_aot_c_write_bool_one_arg_logical_not_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_write_bool_two_arg_thunk_definition(file, entry)) {
            continue;
        } else if (backend_aot_c_try_write_bool_three_arg_thunk_definition(file, entry)) {
            continue;
        } else if (backend_aot_c_try_get_bool_i64_arg0_arg1_less_return(entry->function)) {
            backend_aot_c_write_bool_i64_two_arg_less_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_i64_arg0_arg1_equal_return(entry->function)) {
            backend_aot_c_write_bool_i64_two_arg_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_i64_arg0_arg1_not_equal_return(entry->function)) {
            backend_aot_c_write_bool_i64_two_arg_not_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_i64_arg0_arg1_greater_return(entry->function)) {
            backend_aot_c_write_bool_i64_two_arg_greater_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(entry->function)) {
            backend_aot_c_write_bool_i64_two_arg_less_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_i64_arg0_arg1_greater_equal_return(entry->function)) {
            backend_aot_c_write_bool_i64_two_arg_greater_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_u64_arg0_arg1_less_return(entry->function)) {
            backend_aot_c_write_bool_u64_two_arg_less_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_u64_arg0_arg1_equal_return(entry->function)) {
            backend_aot_c_write_bool_u64_two_arg_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_u64_arg0_arg1_not_equal_return(entry->function)) {
            backend_aot_c_write_bool_u64_two_arg_not_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(entry->function)) {
            backend_aot_c_write_bool_u64_two_arg_greater_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_u64_arg0_arg1_less_equal_return(entry->function)) {
            backend_aot_c_write_bool_u64_two_arg_less_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_u64_arg0_arg1_greater_equal_return(entry->function)) {
            backend_aot_c_write_bool_u64_two_arg_greater_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_f64_arg0_arg1_less_return(entry->function)) {
            backend_aot_c_write_bool_f64_two_arg_less_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_f64_arg0_arg1_equal_return(entry->function)) {
            backend_aot_c_write_bool_f64_two_arg_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_f64_arg0_arg1_not_equal_return(entry->function)) {
            backend_aot_c_write_bool_f64_two_arg_not_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_f64_arg0_arg1_greater_return(entry->function)) {
            backend_aot_c_write_bool_f64_two_arg_greater_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_f64_arg0_arg1_less_equal_return(entry->function)) {
            backend_aot_c_write_bool_f64_two_arg_less_equal_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_f64_arg0_arg1_greater_equal_return(entry->function)) {
            backend_aot_c_write_bool_f64_two_arg_greater_equal_thunk_definition(file, entry->flatIndex);
        }
    }
}
