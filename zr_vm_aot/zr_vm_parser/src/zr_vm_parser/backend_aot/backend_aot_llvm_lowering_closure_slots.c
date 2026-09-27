/* 把闭包创建/访问和栈槽读写路由到各自的值 lowering 子模块。 */
#include "backend_aot_llvm_emitter.h"

TZrBool backend_aot_llvm_lower_closure_slot_value_family(const SZrAotLlvmLoweringContext *context,
                                                         const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_llvm_lower_closure_value_subfamily(context, instruction)) {
        return ZR_TRUE;
    }
    if (backend_aot_llvm_lower_stack_slot_value_subfamily(context, instruction)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
