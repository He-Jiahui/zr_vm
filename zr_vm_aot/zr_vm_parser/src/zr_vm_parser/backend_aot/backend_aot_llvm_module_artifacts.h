#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_MODULE_ARTIFACTS_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_MODULE_ARTIFACTS_H

#include "backend_aot_llvm_module_prelude.h"

void backend_aot_llvm_write_function_thunk_table(FILE *file, const SZrAotFunctionTable *functionTable);
void backend_aot_llvm_write_module_exports(FILE *file,
                                           const TZrChar *moduleName,
                                           TZrUInt32 inputKind,
                                           const TZrChar *inputHash,
                                           const SZrAotFunctionTable *functionTable,
                                           const SZrAotWriterOptions *options);

#endif
