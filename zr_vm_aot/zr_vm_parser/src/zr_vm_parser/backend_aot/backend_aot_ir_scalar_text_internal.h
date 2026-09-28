#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_INTERNAL_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_INTERNAL_H

#include "backend_aot_ir_scalar_text.h"

typedef struct SZrBackendAotIrScalarTextPlan {
    TZrUInt32 functionId;
    TZrUInt64 bits;
    TZrUInt32 branchTargetBlockId; /* zero means the single-block form */
} SZrBackendAotIrScalarTextPlan;

EZrAotIrStatus backend_aot_ir_scalar_text_prepare(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        SZrBackendAotIrScalarTextPlan *plan,
        SZrAotIrDiagnostic *diagnostic);

EZrAotIrStatus backend_aot_ir_scalar_text_fail(
        SZrAotIrDiagnostic *diagnostic, EZrAotIrStatus status,
        TZrUInt32 functionId, TZrUInt32 instructionId,
        TZrUInt64 expected, TZrUInt64 actual);

EZrAotIrStatus backend_aot_ir_scalar_text_check_output(
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_INTERNAL_H */
