#ifndef ZR_VM_PARSER_EXEC_IR_EXECBC_VM_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_EXECBC_VM_INTERNAL_H

#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_parser/exec_ir_execbc_vm.h"

typedef struct SZrExecBcVmPlannedInstruction {
    TZrInstruction instruction;
    TZrExecIrBlockId targetBlockId;
    TZrBool hasRelativeTarget;
    SZrExecBcVmPcMapEntry pcMap;
} SZrExecBcVmPlannedInstruction;

typedef struct SZrExecBcVmConstant {
    TZrExecIrTypeToken typeToken;
    TZrUInt64 bits;
} SZrExecBcVmConstant;

enum {
    ZR_EXEC_IR_COMPARE_KIND_LESS = 1u,
    ZR_EXEC_IR_COMPARE_KIND_GREATER = 3u
};

void execbc_vm_set_diagnostic(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId blockId,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId,
        TZrUInt32 expected,
        TZrUInt32 actual);
TZrBool execbc_vm_fail(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId blockId,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId,
        TZrUInt32 expected,
        TZrUInt32 actual);
TZrBool execbc_vm_value_slot(
        const SZrExecBcProjection *projection,
        TZrExecIrValueId valueId,
        TZrUInt32 *slot,
        TZrExecIrTypeToken *typeToken);
const SZrExecBcBlock *execbc_vm_block_at(
        const SZrExecBcProjection *projection,
        TZrExecIrBlockId blockId);
TZrBool execbc_vm_is_synthetic_block(
        const SZrExecBcProjection *projection,
        const SZrExecBcBlock *block);
TZrBool execbc_vm_validate_projection(
        const SZrExecBcProjection *projection,
        TZrUInt32 **outInstructionOwners,
        TZrUInt32 *outStackSize,
        TZrExecIrTypeToken **outSlotTypes,
        SZrExecIrDiagnostic *diagnostic);
TZrBool execbc_vm_validate_dead_place_uses(
        const SZrExecBcProjection *projection,
        SZrExecIrDiagnostic *diagnostic);
TZrBool execbc_vm_validate_phi_data(
        const SZrExecBcProjection *projection,
        const TZrUInt32 *instructionOwners,
        TZrUInt32 stackSize,
        SZrExecIrDiagnostic *diagnostic);

#endif
