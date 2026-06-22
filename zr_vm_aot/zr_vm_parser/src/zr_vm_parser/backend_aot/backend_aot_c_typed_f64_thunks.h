#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_F64_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_F64_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_typed_f64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_typed_f64_thunks(FILE *file, const SZrAotFunctionTable *table);

#endif
