#include "zr_vm_parser/exec_ir_builder.h"

#include <string.h>

static TZrBool ssa_fail(const SZrExecIrFunction *function,
                       SZrExecIrDiagnostic *diagnostic,
                       EZrExecutionDiagnosticCode code,
                       TZrExecIrInstructionId instructionId) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->functionToken = function->functionToken;
        diagnostic->instructionId = instructionId;
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_BuildSsa(SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 i, j;
    if (d != ZR_NULL) memset(d, 0, sizeof(*d));
    if (f == ZR_NULL) return ZR_FALSE;
    if (f->instructionCount > f->instructionCapacity ||
        (f->instructionCount != 0u && f->instructions == ZR_NULL) ||
        f->operandCount > f->operandCapacity ||
        (f->operandCount != 0u && f->operands == ZR_NULL) ||
        f->valueCount > f->valueCapacity ||
        (f->valueCount != 0u && f->values == ZR_NULL))
        return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
    /* Values emitted by SemIR already carry stable definitions.  This pass
       performs the conservative verifier-side SSA checks and leaves memory
       places unpromoted when no canonical fact is available. */
    for (i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *in = &f->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)in->opcode);
        if (info == ZR_NULL)
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE, i + 1u);
        if (info->operandArity != ZR_EXEC_IR_VARIADIC &&
            in->operands.count != info->operandArity)
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, i + 1u);
        if (in->operands.start > f->operandCount ||
            in->operands.count > f->operandCount - in->operands.start)
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, i + 1u);
        for (j = 0u; j < in->operands.count; ++j) {
            TZrExecIrValueId id = f->operands[in->operands.start + j];
            if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > f->valueCount || f->values[id - 1u].definition == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, i + 1u);
            }
        }
    }
    return ZR_TRUE;
}
