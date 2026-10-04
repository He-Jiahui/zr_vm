#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_ARITHMETIC_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_ARITHMETIC_H

#include "backend_aot_ir_scalar_text.h"

/* Private finite two-constant ADD/SUB template. The public emitter has already
 * established empty output and zero length before entering this helper. */
EZrAotIrStatus backend_aot_ir_scalar_arithmetic_emit(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic, TZrBool llvm);

#endif
