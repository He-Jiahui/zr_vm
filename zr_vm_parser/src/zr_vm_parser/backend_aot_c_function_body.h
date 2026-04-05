#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FUNCTION_BODY_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FUNCTION_BODY_H

#include <stdio.h>

#include "backend_aot_internal.h"

void backend_aot_write_c_function_body(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotFunctionEntry *entry);

#endif
