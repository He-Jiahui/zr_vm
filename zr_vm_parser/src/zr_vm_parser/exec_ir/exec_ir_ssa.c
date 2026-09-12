#include "zr_vm_parser/exec_ir_builder.h"

#include <string.h>

TZrBool ZrParser_ExecIr_BuildSsa(SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 i, j;
    if (d != ZR_NULL) memset(d, 0, sizeof(*d));
    if (f == ZR_NULL) return ZR_FALSE;
    /* Values emitted by SemIR already carry stable definitions.  This pass
       performs the conservative verifier-side SSA checks and leaves memory
       places unpromoted when no canonical fact is available. */
    for (i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *in = &f->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)in->opcode);
        if (info == ZR_NULL) return ZR_FALSE;
        if (info->operandArity != ZR_EXEC_IR_VARIADIC && in->operands.count != info->operandArity) {
            if (d != ZR_NULL) { d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE; d->instructionId = i + 1u; }
            return ZR_FALSE;
        }
        for (j = 0u; j < in->operands.count; ++j) {
            TZrExecIrValueId id = f->operands[in->operands.start + j];
            if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > f->valueCount || f->values[id - 1u].definition == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                if (d != ZR_NULL) { d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE; d->instructionId = i + 1u; }
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}
