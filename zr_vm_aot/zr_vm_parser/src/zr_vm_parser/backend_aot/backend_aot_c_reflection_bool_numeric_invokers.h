#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_BOOL_NUMERIC_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_BOOL_NUMERIC_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_reflection_bool_i64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_reflection_bool_u64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_reflection_bool_f64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table);

#endif
