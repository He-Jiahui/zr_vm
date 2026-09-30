#include "exec_ir_verify_constants.h"

#include <string.h>

static TZrBool zr_exec_ir_constant_failure(
        const SZrExecIrFunction *function, TZrUInt32 instructionIndex,
        EZrExecutionDiagnosticCode code, TZrUInt32 expected, TZrUInt32 actual,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    if (diagnostic == ZR_NULL) return ZR_FALSE;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function->functionToken;
    diagnostic->instructionId = instructionIndex + 1u;
    diagnostic->sourceId = function->instructions[instructionIndex].sourceId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (instructionIndex >= block->instructionRange.start &&
            instructionIndex - block->instructionRange.start <
                block->instructionRange.count) {
            diagnostic->blockId = block->id;
            break;
        }
    }
    return ZR_FALSE;
}

TZrBool zr_exec_ir_verify_owned_constants(
        const SZrExecIrModule *module, const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 instructionIndex;
    if (module->constantCount == 0u) return ZR_TRUE;
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount; ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        const SZrExecIrConstant *constant;
        const SZrExecIrValue *result;
        TZrExecIrValueId resultId;
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT) continue;
        if (instruction->layoutId >= module->constantCount) {
            return zr_exec_ir_constant_failure(function, instructionIndex,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, module->constantCount,
                    instruction->layoutId, diagnostic);
        }
        /* Full verification established one result and valid pool/value IDs. */
        resultId = function->results[instruction->resultRange.start];
        result = &function->values[resultId - 1u];
        constant = &module->constants[instruction->layoutId];
        if (constant->typeToken != result->typeToken) {
            return zr_exec_ir_constant_failure(function, instructionIndex,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, result->typeToken,
                    constant->typeToken, diagnostic);
        }
        if (instruction->typeToken != 0u &&
            instruction->typeToken != result->typeToken) {
            return zr_exec_ir_constant_failure(function, instructionIndex,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, result->typeToken,
                    instruction->typeToken, diagnostic);
        }
    }
    return ZR_TRUE;
}
