#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_BINARY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_BINARY_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

TZrBool backend_aot_try_write_c_scalar_binary(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              const SZrAotExecIrInstruction *semIrInstruction,
                                              const TZrInstruction *execInstruction,
                                              EZrStaticCType staticCType);

#endif
