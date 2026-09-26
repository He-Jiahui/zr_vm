#ifndef ZR_VM_PARSER_AOT_IR_PROJECTION_DESCRIPTOR_H
#define ZR_VM_PARSER_AOT_IR_PROJECTION_DESCRIPTOR_H

#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_core/aot_ir.h"

typedef struct SZrAotIrProjectionDescriptor {
    SZrAotIrModule module;
    SZrAotIrFunction function;
    SZrAotIrInstruction *instructions;
    SZrAotIrBlock *blocks;
    SZrAotIrPhiIncoming *phiIncoming;
    SZrAotIrFrameSlot *frameSlots;
    SZrAotIrSourceMap *sourceMaps;
    SZrExecIrConstant *constants;
    const SZrAotIrProjection *owner;
} SZrAotIrProjectionDescriptor;

ZR_PARSER_API TZrBool ZrParser_AotIrProjection_BuildDescriptor(
        const SZrAotIrProjection *projection,
        const SZrAotIrTargetContract *target,
        const SZrExecutionContract *moduleContract,
        SZrAotIrProjectionDescriptor *descriptor,
        SZrAotIrDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_AotIrProjection_FreeDescriptor(
        SZrAotIrProjectionDescriptor *descriptor);

#endif /* ZR_VM_PARSER_AOT_IR_PROJECTION_DESCRIPTOR_H */
