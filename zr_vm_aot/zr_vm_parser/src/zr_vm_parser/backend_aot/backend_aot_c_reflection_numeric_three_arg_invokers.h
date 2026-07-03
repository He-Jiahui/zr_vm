#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_NUMERIC_THREE_ARG_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_NUMERIC_THREE_ARG_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_reflection_i64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_reflection_u64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
void backend_aot_write_c_reflection_f64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);

#endif
