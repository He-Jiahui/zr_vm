#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ANNOTATION_WARNINGS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ANNOTATION_WARNINGS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

TZrUInt32 backend_aot_c_count_annotation_warnings(SZrState *state,
                                                  const SZrAotFunctionTable *functionTable);
TZrUInt32 backend_aot_c_count_suppressed_annotation_warnings(SZrState *state,
                                                             const SZrAotFunctionTable *functionTable);
void backend_aot_write_c_annotation_warnings(FILE *file,
                                             SZrState *state,
                                             const SZrAotFunctionTable *functionTable);

#endif
