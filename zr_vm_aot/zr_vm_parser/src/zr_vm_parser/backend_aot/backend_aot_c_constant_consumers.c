#include "backend_aot_c_emitter.h"

#include "backend_aot_c_scalar_locals.h"

#include "zr_vm_core/string.h"

static TZrBool backend_aot_c_function_exports_stack_slot(const SZrFunction *function, TZrUInt32 stackSlot) {
    TZrUInt32 exportIndex;

    if (function == ZR_NULL || function->exportedVariables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (exportIndex = 0u; exportIndex < function->exportedVariableLength; exportIndex++) {
        if (function->exportedVariables[exportIndex].stackSlot == stackSlot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_string_constant_truthy(const SZrTypeValue *constantValue, TZrBool *outTruthy) {
    const SZrString *stringValue;

    if (constantValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_STRING(constantValue->type) ||
        constantValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    stringValue = ZR_CAST(SZrString *, constantValue->value.object);
    if (outTruthy != ZR_NULL) {
        *outTruthy = (TZrBool)(ZrCore_String_GetByteLength(stringValue) > 0u);
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_stack_copy_instruction_is_copy(const TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) ||
                     instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SET_STACK));
}

static TZrBool backend_aot_c_branch_target_is_valid(const SZrFunction *function,
                                                    TZrUInt32 instructionIndex,
                                                    TZrInt64 relativeOffset) {
    TZrInt64 targetInstructionIndex;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)instructionIndex + relativeOffset + 1;
    if (targetInstructionIndex < 0 ||
        targetInstructionIndex >= (TZrInt64)function->instructionsLength) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool backend_aot_c_null_constant_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *logicalNotInstruction;
    const SZrTypeValue *constantValue;
    TZrUInt32 logicalNotInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    logicalNotInstructionIndex = getConstantInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != sourceSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
            functionIr,
            logicalNotInstruction->instruction.operandExtra,
            logicalNotInstructionIndex);
}

TZrBool backend_aot_c_null_constant_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *jumpInstruction;
    const SZrTypeValue *constantValue;
    TZrUInt32 jumpInstructionIndex;
    TZrInt64 targetInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    jumpInstructionIndex = getConstantInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)jumpInstructionIndex +
                             (TZrInt64)jumpInstruction->instruction.operand.operand2[0] +
                             1;
    return (TZrBool)(targetInstructionIndex >= 0 &&
                       targetInstructionIndex < (TZrInt64)function->instructionsLength);
}

TZrBool backend_aot_c_bool_constant_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *logicalNotInstruction;
    const SZrTypeValue *constantValue;
    TZrUInt32 logicalNotInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    logicalNotInstructionIndex = getConstantInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != sourceSlot) {
        return ZR_FALSE;
    }

    if (!backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
                functionIr,
                logicalNotInstruction->instruction.operandExtra,
                logicalNotInstructionIndex)) {
        return ZR_FALSE;
    }

    if (outTruthy != ZR_NULL) {
        *outTruthy = (TZrBool)(constantValue->value.nativeObject.nativeBool != 0u);
    }
    return ZR_TRUE;
}

TZrBool backend_aot_c_bool_constant_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *jumpInstruction;
    const SZrTypeValue *constantValue;
    TZrUInt32 jumpInstructionIndex;
    TZrInt64 targetInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    jumpInstructionIndex = getConstantInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)jumpInstructionIndex +
                             (TZrInt64)jumpInstruction->instruction.operand.operand2[0] +
                             1;
    if (targetInstructionIndex < 0 || targetInstructionIndex >= (TZrInt64)function->instructionsLength) {
        return ZR_FALSE;
    }

    if (outTruthy != ZR_NULL) {
        *outTruthy = (TZrBool)(constantValue->value.nativeObject.nativeBool != 0u);
    }
    return ZR_TRUE;
}

TZrBool backend_aot_c_string_constant_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *logicalNotInstruction;
    const SZrTypeValue *constantValue;
    TZrUInt32 logicalNotInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    logicalNotInstructionIndex = getConstantInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (!backend_aot_c_string_constant_truthy(constantValue, outTruthy)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != sourceSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
            functionIr,
            logicalNotInstruction->instruction.operandExtra,
            logicalNotInstructionIndex);
}

TZrBool backend_aot_c_string_constant_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *jumpInstruction;
    const SZrTypeValue *constantValue;
    TZrUInt32 jumpInstructionIndex;
    TZrInt64 targetInstructionIndex;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    jumpInstructionIndex = getConstantInstructionIndex + 1u;
    if (function->exceptionHandlerCount > 0 ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (!backend_aot_c_string_constant_truthy(constantValue, outTruthy)) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    targetInstructionIndex = (TZrInt64)jumpInstructionIndex +
                             (TZrInt64)jumpInstruction->instruction.operand.operand2[0] +
                             1;
    if (targetInstructionIndex < 0 || targetInstructionIndex >= (TZrInt64)function->instructionsLength) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_null_constant_stack_copy_candidate(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 copiedSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrUInt32 stackCopyInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *stackCopyInstruction;
    const SZrTypeValue *constantValue;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        getConstantInstructionIndex + 1u != stackCopyInstructionIndex ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        backend_aot_c_function_exports_stack_slot(function, copiedSlot)) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_NULL(constantValue->type)) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0 ||
        (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0] != sourceSlot) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;
    const TZrInstruction *logicalNotInstruction;
    TZrUInt32 stackCopyInstructionIndex;
    TZrUInt32 logicalNotInstructionIndex;
    TZrUInt32 copiedSlot;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    stackCopyInstructionIndex = getConstantInstructionIndex + 1u;
    logicalNotInstructionIndex = stackCopyInstructionIndex + 1u;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction)) {
        return ZR_FALSE;
    }

    copiedSlot = stackCopyInstruction->instruction.operandExtra;
    if (!backend_aot_c_null_constant_stack_copy_candidate(
                functionIr,
                sourceSlot,
                copiedSlot,
                getConstantInstructionIndex,
                stackCopyInstructionIndex)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != copiedSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
            functionIr,
            logicalNotInstruction->instruction.operandExtra,
            logicalNotInstructionIndex);
}

TZrBool backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;
    const TZrInstruction *jumpInstruction;
    TZrUInt32 stackCopyInstructionIndex;
    TZrUInt32 jumpInstructionIndex;
    TZrUInt32 copiedSlot;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    stackCopyInstructionIndex = getConstantInstructionIndex + 1u;
    jumpInstructionIndex = stackCopyInstructionIndex + 1u;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction)) {
        return ZR_FALSE;
    }

    copiedSlot = stackCopyInstruction->instruction.operandExtra;
    if (!backend_aot_c_null_constant_stack_copy_candidate(
                functionIr,
                sourceSlot,
                copiedSlot,
                getConstantInstructionIndex,
                stackCopyInstructionIndex)) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != copiedSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_branch_target_is_valid(
            function,
            jumpInstructionIndex,
            (TZrInt64)jumpInstruction->instruction.operand.operand2[0]);
}

TZrBool backend_aot_c_null_constant_stack_copy_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 copiedSlot,
        TZrUInt32 stackCopyInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        stackCopyInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0) {
        return ZR_FALSE;
    }

    return backend_aot_c_null_constant_consumed_by_local_stack_copy_logical_not(
            functionIr,
            (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0],
            stackCopyInstructionIndex - 1u);
}

TZrBool backend_aot_c_null_constant_stack_copy_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 copiedSlot,
        TZrUInt32 stackCopyInstructionIndex) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        stackCopyInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0) {
        return ZR_FALSE;
    }

    return backend_aot_c_null_constant_consumed_by_local_stack_copy_jump_if(
            functionIr,
            (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0],
            stackCopyInstructionIndex - 1u);
}

static TZrBool backend_aot_c_string_constant_stack_copy_candidate(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 copiedSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrUInt32 stackCopyInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *constantInstruction;
    const TZrInstruction *stackCopyInstruction;
    const SZrTypeValue *constantValue;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->exceptionHandlerCount > 0 ||
        function->instructionsList == ZR_NULL ||
        getConstantInstructionIndex >= function->instructionsLength ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        getConstantInstructionIndex + 1u != stackCopyInstructionIndex ||
        backend_aot_c_function_exports_stack_slot(function, sourceSlot) ||
        backend_aot_c_function_exports_stack_slot(function, copiedSlot)) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[getConstantInstructionIndex];
    if (constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != sourceSlot) {
        return ZR_FALSE;
    }

    constantValue = backend_aot_c_get_constant_value(
            function,
            constantInstruction->instruction.operand.operand2[0]);
    if (!backend_aot_c_string_constant_truthy(constantValue, outTruthy)) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0 ||
        (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0] != sourceSlot) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;
    const TZrInstruction *logicalNotInstruction;
    TZrUInt32 stackCopyInstructionIndex;
    TZrUInt32 logicalNotInstructionIndex;
    TZrUInt32 copiedSlot;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    stackCopyInstructionIndex = getConstantInstructionIndex + 1u;
    logicalNotInstructionIndex = stackCopyInstructionIndex + 1u;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        logicalNotInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction)) {
        return ZR_FALSE;
    }

    copiedSlot = stackCopyInstruction->instruction.operandExtra;
    if (!backend_aot_c_string_constant_stack_copy_candidate(
                functionIr,
                sourceSlot,
                copiedSlot,
                getConstantInstructionIndex,
                stackCopyInstructionIndex,
                outTruthy)) {
        return ZR_FALSE;
    }

    logicalNotInstruction = &function->instructionsList[logicalNotInstructionIndex];
    if (logicalNotInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT) ||
        logicalNotInstruction->instruction.operand.operand1[0] != copiedSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(
            functionIr,
            logicalNotInstruction->instruction.operandExtra,
            logicalNotInstructionIndex);
}

TZrBool backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 getConstantInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;
    const TZrInstruction *jumpInstruction;
    TZrUInt32 stackCopyInstructionIndex;
    TZrUInt32 jumpInstructionIndex;
    TZrUInt32 copiedSlot;

    if (functionIr == ZR_NULL || functionIr->function == ZR_NULL) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    stackCopyInstructionIndex = getConstantInstructionIndex + 1u;
    jumpInstructionIndex = stackCopyInstructionIndex + 1u;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength ||
        jumpInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction)) {
        return ZR_FALSE;
    }

    copiedSlot = stackCopyInstruction->instruction.operandExtra;
    if (!backend_aot_c_string_constant_stack_copy_candidate(
                functionIr,
                sourceSlot,
                copiedSlot,
                getConstantInstructionIndex,
                stackCopyInstructionIndex,
                outTruthy)) {
        return ZR_FALSE;
    }

    jumpInstruction = &function->instructionsList[jumpInstructionIndex];
    if (jumpInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        jumpInstruction->instruction.operandExtra != copiedSlot) {
        return ZR_FALSE;
    }

    return backend_aot_c_branch_target_is_valid(
            function,
            jumpInstructionIndex,
            (TZrInt64)jumpInstruction->instruction.operand.operand2[0]);
}

TZrBool backend_aot_c_string_constant_stack_copy_consumed_by_local_logical_not(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 copiedSlot,
        TZrUInt32 stackCopyInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        stackCopyInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0) {
        return ZR_FALSE;
    }

    return backend_aot_c_string_constant_consumed_by_local_stack_copy_logical_not(
            functionIr,
            (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0],
            stackCopyInstructionIndex - 1u,
            outTruthy);
}

TZrBool backend_aot_c_string_constant_stack_copy_consumed_by_local_jump_if(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 copiedSlot,
        TZrUInt32 stackCopyInstructionIndex,
        TZrBool *outTruthy) {
    const SZrFunction *function;
    const TZrInstruction *stackCopyInstruction;

    if (functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        stackCopyInstructionIndex == 0u) {
        return ZR_FALSE;
    }

    function = functionIr->function;
    if (function->instructionsList == ZR_NULL ||
        stackCopyInstructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    stackCopyInstruction = &function->instructionsList[stackCopyInstructionIndex];
    if (!backend_aot_c_stack_copy_instruction_is_copy(stackCopyInstruction) ||
        stackCopyInstruction->instruction.operandExtra != copiedSlot ||
        stackCopyInstruction->instruction.operand.operand2[0] < 0) {
        return ZR_FALSE;
    }

    return backend_aot_c_string_constant_consumed_by_local_stack_copy_jump_if(
            functionIr,
            (TZrUInt32)stackCopyInstruction->instruction.operand.operand2[0],
            stackCopyInstructionIndex - 1u,
            outTruthy);
}
