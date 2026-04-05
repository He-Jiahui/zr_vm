#include "backend_aot_llvm_emitter.h"

TZrBool backend_aot_llvm_lower_call_instruction(const SZrAotLlvmLoweringContext *context,
                                                const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            return backend_aot_llvm_lower_function_call_family(context, instruction);
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            return backend_aot_llvm_lower_meta_call_family(context, instruction);
        default:
            return ZR_FALSE;
    }
}
