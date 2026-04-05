#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_closure_index_call(const SZrAotLlvmLoweringContext *context,
                                                         const SZrAotLlvmInstructionContext *instruction,
                                                         const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA2);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_create_closure_instruction(const SZrAotLlvmLoweringContext *context,
                                                                 const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 callableFlatIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    TZrChar argsBuffer[256];

    backend_aot_resolve_callable_constant_function_index(context->functionTable,
                                                         context->state,
                                                         context->entry->function,
                                                         (TZrInt32)instruction->operandA1,
                                                         &callableFlatIndex);
    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 callableFlatIndex);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_CreateClosure",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_closure_slot_instruction(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction,
                                                               const TZrChar *helperName,
                                                               TZrUInt32 closureIndex) {
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(GETUPVAL)) {
        backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                     context->entry->function,
                                                     instruction->destinationSlot,
                                                     ZR_AOT_INVALID_FUNCTION_INDEX);
    }

    {
        TZrChar argsBuffer[256];

        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)closureIndex);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 helperName,
                                                 argsBuffer,
                                                 instruction->nextLabel,
                                                 context->failLabel);
    }
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_closure_value_subfamily(const SZrAotLlvmLoweringContext *context,
                                                       const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return backend_aot_llvm_lower_create_closure_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                         context->entry->function,
                                                         instruction->destinationSlot,
                                                         ZR_AOT_INVALID_FUNCTION_INDEX);
            return backend_aot_llvm_lower_closure_index_call(context,
                                                             instruction,
                                                             "ZrLibrary_AotRuntime_GetClosureValue");
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            return backend_aot_llvm_lower_closure_index_call(context,
                                                             instruction,
                                                             "ZrLibrary_AotRuntime_SetClosureValue");
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
            return backend_aot_llvm_lower_closure_slot_instruction(context,
                                                                   instruction,
                                                                   "ZrLibrary_AotRuntime_GetClosureValue",
                                                                   instruction->operandA1);
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
            return backend_aot_llvm_lower_closure_slot_instruction(context,
                                                                   instruction,
                                                                   "ZrLibrary_AotRuntime_SetClosureValue",
                                                                   instruction->operandA1);
        default:
            return ZR_FALSE;
    }
}
