#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_CALL_RESULT_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_CALL_RESULT_H

#include "backend_aot_exec_ir.h"

TZrBool backend_aot_c_scalar_call_result_has_nonprimitive_type(
        const SZrFunction *function,
        const SZrFunction *calleeFunction,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 destinationSlot);

#endif
