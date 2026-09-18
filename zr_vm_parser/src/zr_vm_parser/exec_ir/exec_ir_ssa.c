#include "exec_ir_internal.h"

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

static TZrBool ssa_value_is_phi_result(const SZrExecIrFunction *function,
                                       TZrExecIrValueId valueId) {
    TZrUInt32 phiIndex;
    if (function->phiCount > function->phiCapacity ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (phiIndex = 0u; phiIndex < function->phiCount; ++phiIndex) {
        if (function->phiPool[phiIndex].result == valueId) return ZR_TRUE;
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
    /* Values emitted by SemIR already carry stable definitions.  Phi results
       deliberately have no ordinary instruction definition. */
    for (i = 0u; i < f->valueCount; ++i) {
        if ((f->values[i].flags & ~ZR_EXEC_IR_VALUE_FLAG_MASK) != 0u ||
            ((f->values[i].flags &
              ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u &&
             (f->values[i].flags &
              ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) == 0u) ||
            ((f->values[i].flags &
              ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u &&
             f->values[i].definition !=
                     ZR_EXEC_IR_INSTRUCTION_ID_INVALID)) {
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0u);
        }
    }
    for (i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *in = &f->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)in->opcode);
        if (info == ZR_NULL)
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE, i + 1u);
        if (in->operands.count < info->minimumOperands ||
            in->operands.count > info->maximumOperands)
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, i + 1u);
        if (in->operands.start > f->operandCount ||
            in->operands.count > f->operandCount - in->operands.start)
            return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, i + 1u);
        for (j = 0u; j < in->operands.count; ++j) {
            TZrExecIrValueId id = f->operands[in->operands.start + j];
            TZrBool external;
            if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > f->valueCount) {
                return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, i + 1u);
            }
            external = (TZrBool)(
                    (f->values[id - 1u].flags &
                     ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u);
            if (!external && f->values[id - 1u].definition ==
                                     ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                !ssa_value_is_phi_result(f, id)) {
                return ssa_fail(f, d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                i + 1u);
            }
        }
    }
    return zr_parser_exec_ir_promote_places(f, d);
}
