#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELD_SCALAR_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELD_SCALAR_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "zr_vm_core/function.h"

TZrBool backend_aot_try_write_c_value_field_scalar_local_store_exec(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout,
        const char *fieldTypeName);

#endif
