#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_stack_copy_instruction(const SZrAotLlvmLoweringContext *context,
                                                             const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(
            context->callableSlotFunctionIndices,
            context->entry->function,
            instruction->destinationSlot,
            backend_aot_get_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                         context->entry->function,
                                                         (TZrUInt32)instruction->operandA2));
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA2);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_CopyStack",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_stack_slot_value_subfamily(const SZrAotLlvmLoweringContext *context,
                                                          const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return backend_aot_llvm_lower_stack_copy_instruction(context, instruction);
        default:
            return ZR_FALSE;
    }
}
