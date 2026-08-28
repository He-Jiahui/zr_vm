#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_STACK_COPY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_STACK_COPY_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

typedef enum EZrAotRuntimeScalarKindMask {
    ZR_AOT_RUNTIME_SCALAR_KIND_NONE = 0,
    ZR_AOT_RUNTIME_SCALAR_KIND_BOOL = 1u << 0,
    ZR_AOT_RUNTIME_SCALAR_KIND_I64 = 1u << 1,
    ZR_AOT_RUNTIME_SCALAR_KIND_U64 = 1u << 2,
    ZR_AOT_RUNTIME_SCALAR_KIND_F64 = 1u << 3
} EZrAotRuntimeScalarKindMask;

TZrBool backend_aot_try_write_c_scalar_stack_copy(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot,
                                                  TZrUInt32 execInstructionIndex,
                                                  TZrBool forceValueSlotWrite);
TZrBool backend_aot_c_scalar_stack_copy_can_use_local_only(const SZrAotExecIrFunction *functionIr,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 sourceSlot,
                                                           TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_stack_copy_source_can_use_local(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex,
        EZrStaticCType staticCType);
TZrBool backend_aot_c_scalar_stack_copy_has_scalar_provenance_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 slot,
        TZrUInt32 execInstructionIndex);
TZrUInt32 backend_aot_c_scalar_stack_copy_source_reaching_runtime_scalar_kind_mask_before(
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 sourceSlot,
        TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_stack_copy_destination_is_next_ownership_source(
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 destinationSlot);

#endif
