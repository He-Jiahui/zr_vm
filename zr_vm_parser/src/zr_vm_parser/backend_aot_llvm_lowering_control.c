#include "backend_aot_llvm_emitter.h"

TZrBool backend_aot_llvm_lower_control_instruction(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_llvm_lower_exception_control_family(context, instruction)) {
        return ZR_TRUE;
    }
    if (backend_aot_llvm_lower_branch_control_family(context, instruction)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
