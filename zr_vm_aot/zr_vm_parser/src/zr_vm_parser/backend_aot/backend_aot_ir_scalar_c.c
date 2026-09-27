#include "backend_aot_ir_scalar_text_internal.h"

#include <stdint.h>
#include <stdio.h>

EZrAotIrStatus backend_aot_ir_c_emit_const_i64(
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
    if (plan.bits == UINT64_C(0x8000000000000000)) {
        count = snprintf(literal, sizeof(literal), "INT64_MIN");
    } else if (plan.bits <= INT64_MAX) {
        count = snprintf(literal, sizeof(literal), "INT64_C(%llu)",
                         (unsigned long long)plan.bits);
    } else {
        count = snprintf(literal, sizeof(literal), "-INT64_C(%llu)",
                         (unsigned long long)(~plan.bits + 1u));
    }
    if (count < 0 || (size_t)count >= sizeof(literal)) {
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_RANGE, functionId, 0u,
                sizeof(literal), count < 0 ? 0u : (TZrUInt64)count + 1u);
    }
    count = snprintf(output, capacity,
                     "#include <stdint.h>\n"
                     "int64_t zr_aot_scalar_fn_%u(void) {\n"
                     "    return %s;\n}\n", plan.functionId, literal);
    if (count < 0 || (size_t)count >= capacity) {
        output[0] = '\0';
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_RANGE, functionId, 0u,
                count < 0 ? 0u : (TZrUInt64)count + 1u, capacity);
    }
    *outLength = (size_t)count;
    return ZR_AOT_IR_OK;
}
