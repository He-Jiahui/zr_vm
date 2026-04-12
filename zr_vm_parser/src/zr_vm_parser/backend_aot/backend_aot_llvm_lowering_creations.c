#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_single_slot_creation_call(const SZrAotLlvmLoweringContext *context,
                                                                const SZrAotLlvmInstructionContext *instruction,
                                                                const TZrChar *helperName) {
    TZrChar argsBuffer[256];

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
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_creation_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            return backend_aot_llvm_lower_single_slot_creation_call(context,
                                                                    instruction,
                                                                    "ZrLibrary_AotRuntime_CreateObject");
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            return backend_aot_llvm_lower_single_slot_creation_call(context,
                                                                    instruction,
                                                                    "ZrLibrary_AotRuntime_CreateArray");
        default:
            return ZR_FALSE;
    }
}
