#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_CALL_BINDINGS_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_CALL_BINDINGS_H

#include "backend_aot_c_call_bindings.h"

TZrBool backend_aot_llvm_write_call_bindings(
        FILE *file, SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 count);
void backend_aot_llvm_write_call_binding_registration(FILE *file, TZrUInt32 count);
void backend_aot_llvm_write_bound_method_infos(FILE *file, const SZrAotFunctionTable *table);

#endif
