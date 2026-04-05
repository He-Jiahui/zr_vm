#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_pair_slot_type_conversion_call(const SZrAotLlvmLoweringContext *context,
                                                                     const SZrAotLlvmInstructionContext *instruction,
                                                                     const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_triple_slot_type_conversion_call(const SZrAotLlvmLoweringContext *context,
                                                                       const SZrAotLlvmInstructionContext *instruction,
                                                                       const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_type_conversion_value_family(const SZrAotLlvmLoweringContext *context,
                                                            const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            return backend_aot_llvm_lower_pair_slot_type_conversion_call(context,
                                                                         instruction,
                                                                         "ZrLibrary_AotRuntime_TypeOf");
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
            return backend_aot_llvm_lower_triple_slot_type_conversion_call(context,
                                                                           instruction,
                                                                           "ZrLibrary_AotRuntime_ToObject");
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
            return backend_aot_llvm_lower_triple_slot_type_conversion_call(context,
                                                                           instruction,
                                                                           "ZrLibrary_AotRuntime_ToStruct");
        default:
            return ZR_FALSE;
    }
}
