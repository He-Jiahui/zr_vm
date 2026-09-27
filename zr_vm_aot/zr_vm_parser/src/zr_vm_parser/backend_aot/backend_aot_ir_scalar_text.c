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
        function->blockCount != 1u || function->instructionCount != 2u ||
        function->operandCount != 1u || function->resultCount != 1u ||
        module->constantCount != 1u || module->layoutCount != 0u ||
        module->flags != 0u || module->target.requiredCapabilities != 0u ||
        module->contract.requiredCapabilities != 0u ||
        module->contract.declaredEffects != 0u ||
        function->contract.requiredCapabilities != 0u ||
        function->contract.declaredEffects != 0u ||
        function->frameSlotCount != 0u || function->valueSlotCount != 0u ||
        function->phiIncomingCount != 0u || function->successorCount != 0u ||
        function->memoryTokenCount != 0u || function->gcMap != ZR_NULL ||
        function->gcRootCount != 0u || function->deoptStateCount != 0u ||
        function->deoptValueCount != 0u ||
        function->deoptAggregateCount != 0u ||
        function->deoptAggregateFieldCount != 0u ||
        function->logicalStateMap != ZR_NULL) {
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
        block->predecessors.count != 0u || block->successors.count != 0u ||
        constantInstruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT ||
        returnInstruction->opcode != ZR_EXEC_IR_OPCODE_RETURN ||
        constantInstruction->flags != 0u || returnInstruction->flags != 0u ||
        constantInstruction->effectIn != 0u ||
        constantInstruction->effectOut != 0u ||
        returnInstruction->effectIn != 0u ||
        returnInstruction->effectOut != 0u ||
        constantInstruction->results.count != 1u ||
        constantInstruction->operands.count != 0u ||
        returnInstruction->results.count != 0u ||
        returnInstruction->operands.count != 1u ||
        constantInstruction->successors.count != 0u ||
        returnInstruction->successors.count != 0u ||
        constantInstruction->phiIncoming.count != 0u ||
        returnInstruction->phiIncoming.count != 0u ||
        constantInstruction->memoryIn.count != 0u ||
        constantInstruction->memoryOut.count != 0u ||
        returnInstruction->memoryIn.count != 0u ||
        returnInstruction->memoryOut.count != 0u ||
        constantInstruction->bindingRow != 0u ||
        returnInstruction->bindingRow != 0u ||
        constantInstruction->deoptId != 0u ||
        returnInstruction->deoptId != 0u ||
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
