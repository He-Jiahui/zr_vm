#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

void backend_aot_write_c_value_semir_for_function(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotExecIrModule *module,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const SZrAotExecIrFrameLayout *frameLayout);
TZrBool backend_aot_try_write_c_value_semir_for_exec_instruction(FILE *file,
                                                                 SZrState *state,
                                                                 const SZrAotExecIrModule *module,
                                                                 const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 execInstructionIndex,
                                                                 TZrUInt32 calleeFunctionIndex,
                                                                 TZrBool allowTypedReturn);

#endif
