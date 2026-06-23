#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_RETURN_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_RETURN_H

#include <stdio.h>

#include "backend_aot_internal.h"

TZrBool backend_aot_try_write_c_typed_return(FILE *file,
                                             const SZrAotExecIrFunction *functionIr,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 execInstructionIndex,
                                             TZrBool publishExports);

#endif
