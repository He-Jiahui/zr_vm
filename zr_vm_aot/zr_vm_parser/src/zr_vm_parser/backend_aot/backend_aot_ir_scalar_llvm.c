#include "backend_aot_ir_scalar_text_internal.h"

#include <stdint.h>
#include <stdio.h>

EZrAotIrStatus backend_aot_ir_llvm_emit_const_i64(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic) {
    SZrBackendAotIrScalarTextPlan plan;
    char literal[64];
    int count;
    EZrAotIrStatus status = backend_aot_ir_scalar_text_check_output(
            output, capacity, outLength, diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    status = backend_aot_ir_scalar_text_prepare(module, functionId, &plan,
                                                diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    if (plan.bits <= INT64_MAX) {
        count = snprintf(literal, sizeof(literal), "%llu",
                         (unsigned long long)plan.bits);
    } else {
        count = snprintf(literal, sizeof(literal), "-%llu",
                         (unsigned long long)(~plan.bits + 1u));
    }
    if (count < 0 || (size_t)count >= sizeof(literal)) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_RANGE, functionId, 0u,
                sizeof(literal), count < 0 ? 0u : (TZrUInt64)count + 1u);
    }
    if (plan.branchTargetBlockId == 0u) {
        count = snprintf(output, capacity,
                         "define i64 @zr_aot_scalar_fn_%u() {\n"
                         "entry:\n  ret i64 %s\n}\n", plan.functionId, literal);
    } else {
        count = snprintf(output, capacity,
                         "define i64 @zr_aot_scalar_fn_%u() {\n"
                         "entry:\n  br label %%block_%u\n"
                         "block_%u:\n  ret i64 %s\n}\n", plan.functionId,
                         plan.branchTargetBlockId,
                         plan.branchTargetBlockId, literal);
    }
    if (count < 0 || (size_t)count >= capacity) {
        output[0] = '\0';
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_RANGE, functionId, 0u,
                count < 0 ? 0u : (TZrUInt64)count + 1u, capacity);
    }
    *outLength = (size_t)count;
    return ZR_AOT_IR_OK;
}
