#include "backend_aot_ir_scalar_text_internal.h"
#include "backend_aot_ir_scalar_arithmetic.h"
#include "backend_aot_ir_scalar_conditional.h"

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
    if (module != ZR_NULL && module->constantCount == 4u) {
        return backend_aot_ir_scalar_conditional_emit(
                module, functionId, output, capacity, outLength, diagnostic, 1u);
    }
    if (module != ZR_NULL && module->constantCount == 2u) {
        return backend_aot_ir_scalar_arithmetic_emit(
                module, functionId, output, capacity, outLength, diagnostic, 1u);
    }
    status = backend_aot_ir_scalar_text_prepare(module, functionId, &plan,
                                                diagnostic);
    if (status != ZR_AOT_IR_OK) return status;
    /* LLVM 接受最小负十进制 i64；在无符号域求幅值，避免宿主有符号转换溢出。 */
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
    /* snprintf 的计数不含 NUL；截断的 IR 必须清空，长度保持入口设置的零。 */
    if (count < 0 || (size_t)count >= capacity) {
        output[0] = '\0';
        return backend_aot_ir_scalar_text_fail(
                diagnostic, ZR_AOT_IR_INVALID_RANGE, functionId, 0u,
                count < 0 ? 0u : (TZrUInt64)count + 1u, capacity);
    }
    *outLength = (size_t)count;
    return ZR_AOT_IR_OK;
}
