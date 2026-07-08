#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFERENCE_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFERENCE_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

TZrBool backend_aot_c_reference_locals_has_locals(const SZrAotExecIrFunction *functionIr);
void backend_aot_write_c_reference_local_structs(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_reference_local_root_maps(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_reference_locals(FILE *file, const SZrAotExecIrFunction *functionIr);
void backend_aot_write_c_reference_local_root_frame_declaration(FILE *file);
void backend_aot_write_c_reference_local_root_frame_push(FILE *file, const SZrAotExecIrFunction *functionIr);
void backend_aot_write_c_reference_local_root_frame_cleanup(FILE *file);

#endif
