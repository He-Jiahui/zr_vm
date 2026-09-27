#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H

#include <stddef.h>

#include "zr_vm_core/aot_ir.h"

/* Emit one standalone C11 or textual LLVM IR function. These calls do not
 * compile, register, or advertise a loadable ZR artifact. The caller owns the
 * buffer; failure clears its first byte and resets outLength when possible. */
EZrAotIrStatus backend_aot_ir_c_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

EZrAotIrStatus backend_aot_ir_llvm_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_H */
