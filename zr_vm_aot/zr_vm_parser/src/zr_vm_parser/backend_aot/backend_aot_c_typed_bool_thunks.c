#include "backend_aot_c_typed_bool_thunks.h"

#include "backend_aot_c_emitter.h"

static TZrBool backend_aot_c_type_ref_is_bool(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_BOOL ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_BOOL);
}

static TZrBool backend_aot_c_try_get_bool_constant_return(const SZrFunction *function, TZrBool *outValue) {
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
        !backend_aot_c_type_ref_is_bool(&function->callableReturnType) ||
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

TZrBool backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function) {
    TZrBool ignored;

    return backend_aot_c_try_get_bool_constant_return(function, &ignored);
}

TZrBool backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_identity_return(function) ||
                     backend_aot_c_try_get_bool_arg0_logical_not_return(function));
}

TZrBool backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function) {
    return (TZrBool)(backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(function) ||
                     backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(function));
}

static void backend_aot_c_write_bool_no_arg_thunk_definition(FILE *file,
                                                             TZrUInt32 flatIndex,
                                                             TZrBool returnValue) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state) {\n"
            "    (void)state;\n"
            "    return %s;\n"
            "}\n",
            (unsigned)flatIndex,
            returnValue ? "ZR_TRUE" : "ZR_FALSE");
}

static void backend_aot_c_write_bool_one_arg_identity_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0) {\n"
            "    (void)state;\n"
            "    return zr_aot_arg0;\n"
            "}\n",
            (unsigned)flatIndex);
}

static void backend_aot_c_write_bool_one_arg_logical_not_thunk_definition(FILE *file, TZrUInt32 flatIndex) {
    fprintf(file,
            "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0) {\n"
            "    (void)state;\n"
            "    return (TZrBool)!zr_aot_arg0;\n"
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

void backend_aot_write_c_typed_bool_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];
        if (backend_aot_c_can_emit_typed_bool_no_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_one_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0);\n",
                    (unsigned)entry->flatIndex);
        } else if (backend_aot_c_can_emit_typed_bool_two_arg_thunk(entry->function)) {
            fprintf(file,
                    "static TZrBool zr_aot_typed_bool_fn_%u(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1);\n",
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
        } else if (backend_aot_c_try_get_bool_arg0_arg1_logical_and_return(entry->function)) {
            backend_aot_c_write_bool_two_arg_logical_and_thunk_definition(file, entry->flatIndex);
        } else if (backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(entry->function)) {
            backend_aot_c_write_bool_two_arg_logical_or_thunk_definition(file, entry->flatIndex);
        }
    }
}
