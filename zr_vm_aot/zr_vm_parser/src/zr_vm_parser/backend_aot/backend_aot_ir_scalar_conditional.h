#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_CONDITIONAL_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_CONDITIONAL_H

#include "backend_aot_ir_scalar_text.h"

/* Private descriptor-only conditional template; the public entry has already
 * established empty output and zero length. No callable bool ABI is inferred. */
EZrAotIrStatus backend_aot_ir_scalar_conditional_emit(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic, TZrBool llvm);

#endif
