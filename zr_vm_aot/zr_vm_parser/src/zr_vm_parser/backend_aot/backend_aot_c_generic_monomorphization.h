#ifndef ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_MONOMORPHIZATION_H
#define ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_MONOMORPHIZATION_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_generic_monomorphization_layouts(FILE *file,
                                                          SZrState *state,
                                                          const SZrAotFunctionTable *table);

void backend_aot_write_c_generic_monomorphization_entries(FILE *file,
                                                          const SZrAotFunctionTable *table,
                                                          TZrBool stripGeneratedSymbols);

#endif
