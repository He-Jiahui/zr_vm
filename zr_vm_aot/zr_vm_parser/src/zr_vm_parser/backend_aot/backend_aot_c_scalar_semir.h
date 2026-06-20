#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_SEMIR_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_SEMIR_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

TZrBool backend_aot_try_write_c_scalar_semir_for_exec_instruction(FILE *file,
                                                                  const SZrAotExecIrModule *module,
                                                                  const SZrAotExecIrFunction *functionIr,
                                                                  const TZrInstruction *execInstruction,
                                                                  TZrUInt32 execInstructionIndex);

#endif
