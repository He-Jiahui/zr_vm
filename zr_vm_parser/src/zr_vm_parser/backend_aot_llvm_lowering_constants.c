#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_constant_instruction(const SZrAotLlvmLoweringContext *context,
                                                           const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 callableFlatIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    TZrChar argsBuffer[256];

    backend_aot_resolve_callable_constant_function_index(context->functionTable,
                                                         context->state,
                                                         context->entry->function,
                                                         instruction->operandA2,
                                                         &callableFlatIndex);
    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 callableFlatIndex);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA2);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_CopyConstant",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_set_constant_instruction(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA2);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_SetConstant",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_constant_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return backend_aot_llvm_lower_constant_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            return backend_aot_llvm_lower_set_constant_instruction(context, instruction);
        default:
            return ZR_FALSE;
    }
}
