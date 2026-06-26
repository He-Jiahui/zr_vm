#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_CALLS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_CALLS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

void backend_aot_write_c_value_semir_call_typed(FILE *file,
                                                const SZrAotExecIrFrameLayout *frameLayout,
                                                const SZrAotExecIrInstruction *instruction);
void backend_aot_write_c_value_semir_return_typed(FILE *file,
                                                  const SZrAotExecIrFrameLayout *frameLayout,
                                                  const SZrAotExecIrInstruction *instruction);
TZrBool backend_aot_try_write_c_value_semir_call_typed_exec(FILE *file,
                                                           const SZrAotExecIrFrameLayout *frameLayout,
                                                           const SZrAotExecIrInstruction *instruction,
                                                           const SZrFunction *calleeFunction,
                                                           TZrUInt32 callerFunctionIndex,
                                                           TZrUInt32 execInstructionIndex,
                                                           TZrUInt32 calleeFunctionIndex,
                                                           TZrBool requireFullAot);
TZrBool backend_aot_try_write_c_value_semir_return_typed_exec(FILE *file,
                                                             const SZrAotExecIrFrameLayout *frameLayout,
                                                             const SZrAotExecIrInstruction *instruction,
                                                             TZrBool allowTypedReturn);

#endif
