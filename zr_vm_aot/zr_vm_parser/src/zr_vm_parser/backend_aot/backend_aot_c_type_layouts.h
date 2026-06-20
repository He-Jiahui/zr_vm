#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUTS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

void backend_aot_write_c_type_layout_declarations(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotFunctionTable *table);

#endif
