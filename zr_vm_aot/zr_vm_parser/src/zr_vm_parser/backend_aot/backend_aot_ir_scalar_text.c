#include "backend_aot_ir_scalar_text_internal.h"

#include <string.h>

EZrAotIrStatus backend_aot_ir_scalar_text_fail(
        SZrAotIrDiagnostic *diagnostic, EZrAotIrStatus status,
        TZrUInt32 functionId, TZrUInt32 instructionId,
        TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->functionId = functionId;
        diagnostic->instructionId = instructionId;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return status;
}

EZrAotIrStatus backend_aot_ir_scalar_text_check_output(
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic) {
    if (outLength != ZR_NULL) *outLength = 0u;
    if (output != ZR_NULL && capacity != 0u) output[0] = '\0';
    if (output == ZR_NULL || capacity == 0u || outLength == ZR_NULL) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, 0u, 0u, 1u, 0u);
    }
    return ZR_AOT_IR_OK;
}

static TZrBool scalar_text_instruction_plain(
        const SZrAotIrInstruction *instruction) {
    return (TZrBool)(instruction->flags == 0u &&
                    instruction->effectIn == 0u &&
                    instruction->effectOut == 0u &&
                    instruction->phiIncoming.count == 0u &&
                    instruction->memoryIn.count == 0u &&
                    instruction->memoryOut.count == 0u &&
                    instruction->bindingRow == 0u &&
                    instruction->deoptId == 0u);
}

EZrAotIrStatus backend_aot_ir_scalar_text_prepare(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        SZrBackendAotIrScalarTextPlan *plan,
        SZrAotIrDiagnostic *diagnostic) {
    SZrAotIrCallableAbi abi;
    const SZrAotIrFunction *function;
    const SZrAotIrBlock *block;
    const SZrAotIrInstruction *constantInstruction;
    const SZrAotIrInstruction *returnInstruction;
    const SZrExecIrConstant *constant;
    EZrAotIrStatus status;

    if (plan == ZR_NULL) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, functionId, 0u,
                1u, 0u);
    }
    memset(plan, 0, sizeof(*plan));
    status = ZrCore_AotIr_RequireExecutableAbi(module, functionId, &abi,
                                               diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    if (module->functionCount != 1u) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u,
                1u, module->functionCount);
    }
    function = &module->functions[0];
    if (function->id != functionId ||
        abi.kind != ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64 ||
        (function->blockCount != 1u && function->blockCount != 2u) ||
        function->operandCount != 1u || function->resultCount != 1u ||
        module->constantCount != 1u || module->layoutCount != 0u ||
        module->flags != 0u || module->target.requiredCapabilities != 0u ||
        module->contract.requiredCapabilities != 0u ||
        module->contract.declaredEffects != 0u ||
        function->contract.requiredCapabilities != 0u ||
        function->contract.declaredEffects != 0u ||
        function->frameSlotCount != 0u || function->valueSlotCount != 0u ||
        function->phiIncomingCount != 0u ||
        function->memoryTokenCount != 0u || function->gcMap != ZR_NULL ||
        function->gcRootCount != 0u || function->deoptStateCount != 0u ||
        function->deoptValueCount != 0u ||
        function->deoptAggregateCount != 0u ||
        function->deoptAggregateFieldCount != 0u ||
        function->logicalStateMap != ZR_NULL) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
    }
    if (function->blockCount == 1u) {
        if (function->instructionCount != 2u || function->successorCount != 0u) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
        }
        block = &function->blocks[0];
        constantInstruction = &function->instructions[0];
        returnInstruction = &function->instructions[1];
        if (block->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
            block->terminatorInstructionId != returnInstruction->id ||
            block->instructions.offset != 0u ||
            block->instructions.count != 2u ||
            block->predecessors.count != 0u ||
            block->successors.count != 0u ||
            !scalar_text_instruction_plain(constantInstruction) ||
            !scalar_text_instruction_plain(returnInstruction)) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                    constantInstruction->id, 0u, 1u);
        }
    } else {
        const SZrAotIrBlock *entry = &function->blocks[0];
        const SZrAotIrBlock *exit = &function->blocks[1];
        const SZrAotIrInstruction *branch = &function->instructions[0];
        if (function->instructionCount != 3u || function->successorCount != 2u) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId, 0u, 0u, 1u);
        }
        constantInstruction = &function->instructions[1];
        returnInstruction = &function->instructions[2];
        if (entry->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY || exit->flags != 0u ||
            entry->instructions.offset != 0u ||
            entry->instructions.count != 1u ||
            exit->instructions.offset != 1u ||
            exit->instructions.count != 2u ||
            entry->terminatorInstructionId != branch->id ||
            exit->terminatorInstructionId != returnInstruction->id ||
            entry->predecessors.count != 0u ||
            entry->successors.offset != 0u ||
            entry->successors.count != 1u ||
            exit->predecessors.offset != 1u ||
            exit->predecessors.count != 1u ||
            exit->successors.count != 0u ||
            function->successorPool[0] != exit->id ||
            function->successorPool[1] != entry->id ||
            branch->opcode != ZR_EXEC_IR_OPCODE_BRANCH ||
            branch->results.count != 0u || branch->operands.count != 0u ||
            branch->successors.offset != 0u ||
            branch->successors.count != 1u ||
            branch->layoutId != 0u || branch->typeToken != 0u ||
            !scalar_text_instruction_plain(branch) ||
            !scalar_text_instruction_plain(constantInstruction) ||
            !scalar_text_instruction_plain(returnInstruction)) {
            return backend_aot_ir_scalar_text_fail(
                    diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                    branch->id, 0u, 1u);
        }
        plan->branchTargetBlockId = exit->id;
    }
    if (constantInstruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT ||
        returnInstruction->opcode != ZR_EXEC_IR_OPCODE_RETURN ||
        constantInstruction->results.count != 1u ||
        constantInstruction->operands.count != 0u ||
        returnInstruction->results.count != 0u ||
        returnInstruction->operands.count != 1u ||
        constantInstruction->successors.count != 0u ||
        returnInstruction->successors.count != 0u ||
        constantInstruction->layoutId != 0u ||
        returnInstruction->layoutId != 0u ||
        function->resultPool[constantInstruction->results.offset] !=
                function->operandPool[returnInstruction->operands.offset]) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                constantInstruction->id, 0u, 1u);
    }
    constant = &module->constantPool[0];
    if (constant->flags != 0u ||
        constant->typeToken != abi.returnTypeToken) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_UNSUPPORTED, functionId,
                constantInstruction->id, abi.returnTypeToken,
                constant->typeToken);
    }
    plan->functionId = functionId;
    plan->bits = constant->bits;
    return ZR_AOT_IR_OK;
}
