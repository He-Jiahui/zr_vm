#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

TZrBool backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(const SZrFunction *function);
void backend_aot_write_c_typed_bool_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_typed_bool_thunks(FILE *file, const SZrAotFunctionTable *table);

#endif
