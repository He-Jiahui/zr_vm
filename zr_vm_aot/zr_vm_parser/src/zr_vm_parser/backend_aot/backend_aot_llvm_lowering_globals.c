#include "backend_aot_llvm_emitter.h"

TZrBool backend_aot_llvm_lower_global_value_family(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    if (context == ZR_NULL || instruction == ZR_NULL ||
        instruction->opcode != ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
        return ZR_FALSE;
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u",
             (unsigned)instruction->destinationSlot);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_GetGlobal",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}
