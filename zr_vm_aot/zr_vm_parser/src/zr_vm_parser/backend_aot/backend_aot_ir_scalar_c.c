#include "backend_aot_ir_scalar_text_internal.h"
#include "backend_aot_ir_scalar_arithmetic.h"
#include "backend_aot_ir_scalar_conditional.h"

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
    if (module != ZR_NULL && module->constantCount == 4u) {
        return backend_aot_ir_scalar_conditional_emit(
                module, functionId, output, capacity, outLength, diagnostic, 0u);
    }
    if (module != ZR_NULL && module->constantCount == 2u) {
        return backend_aot_ir_scalar_arithmetic_emit(
                module, functionId, output, capacity, outLength, diagnostic, 0u);
    }
    status = backend_aot_ir_scalar_text_prepare(module, functionId, &plan,
                                                diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    /* 最小 i64 的正幅值不能写成有符号 INT64_C；其他负数先在无符号域取幅值。 */
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
    if (plan.branchTargetBlockId == 0u) {
        count = snprintf(output, capacity,
                         "#include <stdint.h>\n"
                         "int64_t zr_aot_scalar_fn_%u(void) {\n"
                         "    return %s;\n}\n", plan.functionId, literal);
    } else {
        count = snprintf(output, capacity,
                         "#include <stdint.h>\n"
                         "int64_t zr_aot_scalar_fn_%u(void) {\n"
                         "    goto zr_aot_block_%u;\n"
                         "zr_aot_block_%u:\n"
                         "    return %s;\n}\n", plan.functionId,
                         plan.branchTargetBlockId,
                         plan.branchTargetBlockId, literal);
    }
    /* snprintf 的计数不含 NUL；截断后的部分源码不能作为成功输出交给调用方。 */
    if (count < 0 || (size_t)count >= capacity) {
        output[0] = '\0';
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_RANGE, functionId, 0u,
                count < 0 ? 0u : (TZrUInt64)count + 1u, capacity);
    }
    *outLength = (size_t)count;
    return ZR_AOT_IR_OK;
}
