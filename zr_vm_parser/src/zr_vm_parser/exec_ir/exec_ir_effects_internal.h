#ifndef ZR_VM_PARSER_EXEC_IR_EFFECTS_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_EFFECTS_INTERNAL_H

#include "zr_vm_parser/exec_ir_builder.h"
#include <string.h>

TZrBool zr_parser_exec_ir_collect_loop_effects(
        const SZrExecIrFunction *function, TZrUInt32 *loopWrites,
        TZrBool *loopEffects, TZrExecIrBlockId *blockOrder,
        TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic);

static void zr_parser_exec_ir_effect_diag(SZrExecIrDiagnostic *diagnostic,
                                           const SZrExecIrFunction *function,
                                           EZrExecutionDiagnosticCode code,
                                           TZrUInt32 instructionId,
                                           TZrUInt32 expected,
                                           TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->blockId = ZR_EXEC_IR_BLOCK_ID_ENTRY;
    diagnostic->instructionId = instructionId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrUInt16 zr_parser_exec_ir_required_flags(
        const SZrExecIrOpcodeInfo *info) {
    TZrUInt16 flags = 0u;
    if (info == ZR_NULL) return flags;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_THROW);
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_GC);
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u)
        flags = (TZrUInt16)(flags | ZR_EXEC_IR_FLAG_MAY_SUSPEND);
    return flags;
}

static TZrBool zr_parser_exec_ir_is_observable(
        const SZrExecIrOpcodeInfo *info) {
    TZrUInt16 required = zr_parser_exec_ir_required_flags(info);
    return (TZrBool)(info != ZR_NULL &&
                     (info->memoryWrites != 0u || required != 0u ||
                      (info->effects & ZR_EXEC_IR_EFFECT_DROP) != 0u));
}

#endif
