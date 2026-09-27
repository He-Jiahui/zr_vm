/* 声明单函数 LLVM 文本发射入口，函数表负责提供稳定的扁平索引。 */
#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_FUNCTION_BODY_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_FUNCTION_BODY_H

#include <stdio.h>

#include "backend_aot_internal.h"

void backend_aot_write_llvm_function_body(FILE *file,
                                          SZrState *state,
                                          const SZrAotFunctionTable *functionTable,
                                          const SZrAotFunctionEntry *entry,
                                          TZrBool stripGeneratedSymbols);

#endif
