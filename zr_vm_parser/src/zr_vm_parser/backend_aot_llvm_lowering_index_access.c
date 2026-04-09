#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_triple_slot_index_call(const SZrAotLlvmLoweringContext *context,
                                                             const SZrAotLlvmInstructionContext *instruction,
                                                             const TZrChar *helperName,
                                                             TZrBool clearCallableProvenance) {
    TZrChar argsBuffer[256];

    if (clearCallableProvenance) {
        backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                     context->entry->function,
                                                     instruction->destinationSlot,
                                                     ZR_AOT_INVALID_FUNCTION_INDEX);
    }

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

static TZrBool backend_aot_llvm_lower_super_array_add_int4(const SZrAotLlvmLoweringContext *context,
                                                           const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_SuperArrayAddInt4",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_super_array_add_int4_const(const SZrAotLlvmLoweringContext *context,
                                                                 const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_SuperArrayAddInt4Const",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_super_array_fill_int4_const(const SZrAotLlvmLoweringContext *context,
                                                                  const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u",
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1,
             (unsigned)instruction->destinationSlot);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_SuperArrayFillInt4Const",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_index_value_family(const SZrAotLlvmLoweringContext *context,
                                                  const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            return backend_aot_llvm_lower_triple_slot_index_call(context,
                                                                 instruction,
                                                                 "ZrLibrary_AotRuntime_GetByIndex",
                                                                 ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            return backend_aot_llvm_lower_triple_slot_index_call(context,
                                                                 instruction,
                                                                 "ZrLibrary_AotRuntime_SuperArrayGetInt",
                                                                 ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
            return backend_aot_llvm_lower_triple_slot_index_call(context,
                                                                 instruction,
                                                                 "ZrLibrary_AotRuntime_SetByIndex",
                                                                 ZR_FALSE);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
            return backend_aot_llvm_lower_triple_slot_index_call(context,
                                                                 instruction,
                                                                 "ZrLibrary_AotRuntime_SuperArraySetInt",
                                                                 ZR_FALSE);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            return backend_aot_llvm_lower_triple_slot_index_call(context,
                                                                 instruction,
                                                                 "ZrLibrary_AotRuntime_SuperArrayAddInt",
                                                                 ZR_TRUE);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            return backend_aot_llvm_lower_super_array_add_int4(context, instruction);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            return backend_aot_llvm_lower_super_array_add_int4_const(context, instruction);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            return backend_aot_llvm_lower_super_array_fill_int4_const(context, instruction);
        default:
            return ZR_FALSE;
    }
}
