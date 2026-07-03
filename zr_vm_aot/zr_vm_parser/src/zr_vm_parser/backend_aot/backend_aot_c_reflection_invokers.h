#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_reflection_invokers(FILE *file, const SZrAotFunctionTable *table);

#endif
