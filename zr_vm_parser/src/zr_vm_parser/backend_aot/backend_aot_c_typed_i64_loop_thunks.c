#include "backend_aot/backend_aot_c_typed_i64_loop_thunks.h"

#include "backend_aot/backend_aot_c_emitter.h"

/* Accept the original and compare-branch-fused counting loops only. */
static TZrBool backend_aot_c_loop_type_ref_is_i64(const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(typeRef->baseType == ZR_VALUE_TYPE_INT64 ||
                     typeRef->staticCType == ZR_STATIC_C_TYPE_I64);
}

static TZrBool backend_aot_c_loop_constant_is_i64(const SZrFunction *function,
                                                   TZrInt32 constantIndex,
                                                   TZrInt64 expectedValue) {
    const SZrTypeValue *constantValue = backend_aot_c_get_constant_value(function, constantIndex);

    return (TZrBool)(constantValue != ZR_NULL &&
                     ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type) &&
                     constantValue->value.nativeObject.nativeInt64 == expectedValue);
}

static TZrBool backend_aot_c_loop_is_signed_add(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
                     instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST));
}

static TZrBool backend_aot_c_loop_is_signed_add_const(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST) ||
                     instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST));
}

TZrBool backend_aot_c_can_emit_typed_i64_counting_sum_loop_thunk(const SZrFunction *function) {
    const TZrInstruction *instructions;
    TZrUInt32 indexSlot;
    TZrUInt32 accumulatorSlot;
    TZrUInt32 conditionSlot;
    TZrUInt32 bodyStart;
    TZrUInt32 sumResultSlot;
    TZrInt32 zeroIndex;
    TZrInt32 oneIndex;

    if (function == ZR_NULL ||
        function->instructionsList == ZR_NULL ||
        (function->instructionsLength != 10u && function->instructionsLength != 9u) ||
        function->parameterCount != 1u ||
        function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount < 1u ||
        function->hasVariableArguments ||
        !function->hasCallableReturnType ||
        !backend_aot_c_loop_type_ref_is_i64(&function->callableReturnType) ||
        !backend_aot_c_loop_type_ref_is_i64(&function->parameterMetadata[0].type)) {
        return ZR_FALSE;
    }

    instructions = function->instructionsList;
    if (instructions[0].instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
        return ZR_FALSE;
    }
    indexSlot = instructions[0].instruction.operandExtra;
    zeroIndex = instructions[0].instruction.operand.operand2[0];
    if (indexSlot == 0u || !backend_aot_c_loop_constant_is_i64(function, zeroIndex, 0)) {
        return ZR_FALSE;
    }

    if (instructions[1].instruction.operationCode != ZR_INSTRUCTION_ENUM(RESET_STACK_NULL)) {
        return ZR_FALSE;
    }
    accumulatorSlot = instructions[1].instruction.operandExtra;
    if (accumulatorSlot == 0u || accumulatorSlot == indexSlot ||
        instructions[2].instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        instructions[2].instruction.operandExtra != accumulatorSlot ||
        !backend_aot_c_loop_constant_is_i64(
                function,
                instructions[2].instruction.operand.operand2[0],
                0)) {
        return ZR_FALSE;
    }

    bodyStart = function->instructionsLength == 9u ? 4u : 5u;
    conditionSlot = instructions[3].instruction.operandExtra;
    if (bodyStart == 4u) {
        /* !(index < limit) is limit <= index, targeting the return. */
        if (instructions[3].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED) ||
            instructions[3].instruction.operandExtra != 0u ||
            instructions[3].instruction.operand.operand1[0] != indexSlot ||
            (TZrInt16)instructions[3].instruction.operand.operand1[1] != 4) {
            return ZR_FALSE;
        }
    } else if (instructions[3].instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED) ||
        instructions[3].instruction.operand.operand1[0] != indexSlot ||
        instructions[3].instruction.operand.operand1[1] != 0u ||
        instructions[4].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE) ||
        instructions[4].instruction.operandExtra != conditionSlot ||
        instructions[4].instruction.operand.operand2[0] != 4) {
        return ZR_FALSE;
    }

    sumResultSlot = instructions[bodyStart].instruction.operandExtra;
    if (!backend_aot_c_loop_is_signed_add(&instructions[bodyStart]) ||
        instructions[bodyStart].instruction.operand.operand1[0] != accumulatorSlot ||
        instructions[bodyStart].instruction.operand.operand1[1] != indexSlot ||
        (instructions[bodyStart + 1u].instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
         instructions[bodyStart + 1u].instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
        instructions[bodyStart + 1u].instruction.operandExtra != accumulatorSlot ||
        instructions[bodyStart + 1u].instruction.operand.operand2[0] < 0 ||
        (TZrUInt32)instructions[bodyStart + 1u].instruction.operand.operand2[0] != sumResultSlot) {
        return ZR_FALSE;
    }

    oneIndex = (TZrInt32)instructions[bodyStart + 2u].instruction.operand.operand1[1];
    if (!backend_aot_c_loop_is_signed_add_const(&instructions[bodyStart + 2u]) ||
        instructions[bodyStart + 2u].instruction.operandExtra != indexSlot ||
        instructions[bodyStart + 2u].instruction.operand.operand1[0] != indexSlot ||
        !backend_aot_c_loop_constant_is_i64(function, oneIndex, 1) ||
        instructions[bodyStart + 3u].instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP) ||
        instructions[bodyStart + 3u].instruction.operand.operand2[0] != -(TZrInt32)(bodyStart + 1u) ||
        instructions[bodyStart + 4u].instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN) ||
        instructions[bodyStart + 4u].instruction.operand.operand1[0] != accumulatorSlot) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* Preserve the checked intermediate sums of the original counting loop. */
void backend_aot_c_write_typed_i64_counting_sum_loop_thunk(FILE *file, TZrUInt32 flatIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "static TZrInt64 zr_aot_typed_i64_fn_%u(struct SZrState *state, TZrInt64 zr_aot_arg0) {\n"
            "    TZrInt64 zr_aot_loop_index = 0;\n"
            "    TZrInt64 zr_aot_loop_accumulator = 0;\n"
            "    while (zr_aot_loop_index < zr_aot_arg0) {\n"
            "        zr_aot_loop_accumulator = ZrCore_Integer_Add(state, zr_aot_loop_accumulator, zr_aot_loop_index);\n"
            "        zr_aot_loop_index = ZrCore_Integer_Add(state, zr_aot_loop_index, 1);\n"
            "    }\n"
            "    return zr_aot_loop_accumulator;\n"
            "}\n",
            (unsigned)flatIndex);
}
