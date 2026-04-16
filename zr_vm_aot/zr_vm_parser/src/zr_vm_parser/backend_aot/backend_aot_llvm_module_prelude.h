#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_MODULE_PRELUDE_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_MODULE_PRELUDE_H

#include <stdio.h>

#include "backend_aot_internal.h"

void backend_aot_write_llvm_contracts(FILE *file, TZrUInt32 runtimeContracts);
void backend_aot_write_runtime_contract_array_llvm(FILE *file, TZrUInt32 runtimeContracts);
void backend_aot_write_runtime_contract_globals_llvm(FILE *file, TZrUInt32 runtimeContracts);
void backend_aot_write_embedded_blob_llvm(FILE *file, const TZrByte *blob, TZrSize blobLength);
void backend_aot_write_llvm_runtime_helper_decls(FILE *file);
void backend_aot_llvm_write_module_prelude(FILE *file,
                                           const SZrAotExecIrModule *module,
                                           const TZrChar *moduleName,
                                           const TZrChar *inputHash,
                                           const SZrAotWriterOptions *options);

#endif
