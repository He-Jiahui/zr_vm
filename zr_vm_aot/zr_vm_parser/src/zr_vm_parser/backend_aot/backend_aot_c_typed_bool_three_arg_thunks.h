#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THREE_ARG_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THREE_ARG_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

TZrBool backend_aot_c_can_emit_typed_bool_three_arg_thunk(const SZrFunction *function);
void backend_aot_c_write_bool_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex);
TZrBool backend_aot_c_try_write_bool_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry);

#endif
