#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_STACK_COPY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_STACK_COPY_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

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

#endif
