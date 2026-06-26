#ifndef ZR_VM_PARSER_BACKEND_AOT_C_METHOD_METADATA_H
#define ZR_VM_PARSER_BACKEND_AOT_C_METHOD_METADATA_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

unsigned long long backend_aot_write_c_method_infos(FILE *file,
                                                    SZrState *state,
                                                    const SZrAotFunctionTable *table,
                                                    const SZrAotExecIrModule *module,
                                                    TZrUInt8 reflectionMetadataLevel);
unsigned long long backend_aot_c_method_metadata_generated_bytes_referenced(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const SZrAotExecIrModule *module,
        TZrUInt8 reflectionMetadataLevel);
void backend_aot_write_c_reflection_invokers(FILE *file);
TZrUInt32 backend_aot_c_method_metadata_count_gc_roots(SZrState *state,
                                                       const SZrAotExecIrFunction *functionIr);
void backend_aot_write_c_method_info_table(FILE *file,
                                           const SZrAotFunctionTable *table,
                                           TZrUInt32 functionIndexSpace);

#endif
