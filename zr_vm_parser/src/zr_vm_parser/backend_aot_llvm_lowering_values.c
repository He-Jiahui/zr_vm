#include "backend_aot_llvm_emitter.h"

TZrBool backend_aot_llvm_lower_value_instruction(const SZrAotLlvmLoweringContext *context,
                                                 const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->opcode == ZR_INSTRUCTION_ENUM(NOP)) {
        return ZR_TRUE;
    }

    if (backend_aot_llvm_lower_constant_value_family(context, instruction)) {
        return ZR_TRUE;
    }
    if (backend_aot_llvm_lower_closure_slot_value_family(context, instruction)) {
        return ZR_TRUE;
    }
    if (backend_aot_llvm_lower_object_meta_owning_value_family(context, instruction)) {
        return ZR_TRUE;
    }
    if (backend_aot_llvm_lower_arithmetic_value_family(context, instruction)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
