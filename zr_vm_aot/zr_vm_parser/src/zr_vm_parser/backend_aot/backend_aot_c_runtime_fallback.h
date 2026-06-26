#ifndef ZR_VM_PARSER_BACKEND_AOT_C_RUNTIME_FALLBACK_H
#define ZR_VM_PARSER_BACKEND_AOT_C_RUNTIME_FALLBACK_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

TZrBool backend_aot_c_validate_full_aot_runtime_closure(SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        const SZrAotExecIrModule *module);
TZrUInt32 backend_aot_c_count_runtime_fallback_warnings(SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        const SZrAotExecIrModule *module,
                                                        TZrUInt32 suppressedReasonMask);
TZrUInt32 backend_aot_c_count_suppressed_runtime_fallback_warnings(SZrState *state,
                                                                   const SZrAotFunctionTable *functionTable,
                                                                   const SZrAotExecIrModule *module,
                                                                   TZrUInt32 suppressedReasonMask);
void backend_aot_write_c_trim_warnings(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotExecIrModule *module,
                                       TZrUInt32 suppressedReasonMask);

#endif
