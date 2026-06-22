#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_DIRECT_CALLS_H

#include <stdio.h>

#include "backend_aot_internal.h"

TZrBool backend_aot_try_write_c_static_direct_typed_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex);
TZrBool backend_aot_try_write_c_static_direct_typed_no_arg_function_call(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        const SZrAotExecIrFunction *functionIr,
        TZrUInt32 destinationSlot,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex);

#endif
