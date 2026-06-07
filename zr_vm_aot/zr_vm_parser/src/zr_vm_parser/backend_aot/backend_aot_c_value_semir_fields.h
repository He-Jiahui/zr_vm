#ifndef ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELDS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_VALUE_SEMIR_FIELDS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

void backend_aot_write_c_value_semir_field_addr(FILE *file,
                                                SZrState *state,
                                                const SZrAotExecIrFunction *functionIr,
                                                const SZrAotExecIrFrameLayout *frameLayout,
                                                const SZrAotExecIrInstruction *instruction);
void backend_aot_write_c_value_semir_load(FILE *file,
                                          SZrState *state,
                                          const SZrAotExecIrFunction *functionIr,
                                          const SZrAotExecIrFrameLayout *frameLayout,
                                          const SZrAotExecIrInstruction *instruction);
void backend_aot_write_c_value_semir_store(FILE *file,
                                           SZrState *state,
                                           const SZrAotExecIrFunction *functionIr,
                                           const SZrAotExecIrFrameLayout *frameLayout,
                                           const SZrAotExecIrInstruction *instruction);
TZrBool backend_aot_try_write_c_value_semir_field_load_exec(FILE *file,
                                                           SZrState *state,
                                                           const SZrAotExecIrFunction *functionIr,
                                                           const SZrAotExecIrFrameLayout *frameLayout,
                                                           const SZrAotExecIrInstruction *instruction);
TZrBool backend_aot_try_write_c_value_semir_field_store_exec(FILE *file,
                                                            SZrState *state,
                                                            const SZrAotExecIrFunction *functionIr,
                                                            const SZrAotExecIrFrameLayout *frameLayout,
                                                            const SZrAotExecIrInstruction *instruction);

#endif
