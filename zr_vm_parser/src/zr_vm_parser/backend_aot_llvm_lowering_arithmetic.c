#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_binary_value_instruction(const SZrAotLlvmLoweringContext *context,
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

static TZrBool backend_aot_llvm_lower_unary_value_instruction(const SZrAotLlvmLoweringContext *context,
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

static const TZrChar *backend_aot_llvm_binary_value_helper_name(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            return "ZrLibrary_AotRuntime_LogicalEqual";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            return "ZrLibrary_AotRuntime_LogicalNotEqual";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalLessSigned";
        case ZR_INSTRUCTION_ENUM(ADD):
            return "ZrLibrary_AotRuntime_Add";
        case ZR_INSTRUCTION_ENUM(ADD_INT):
            return "ZrLibrary_AotRuntime_AddInt";
        case ZR_INSTRUCTION_ENUM(SUB_INT):
            return "ZrLibrary_AotRuntime_SubInt";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            return "ZrLibrary_AotRuntime_MulSigned";
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            return "ZrLibrary_AotRuntime_DivSigned";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *backend_aot_llvm_unary_value_helper_name(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NEG):
            return "ZrLibrary_AotRuntime_Neg";
        case ZR_INSTRUCTION_ENUM(TO_INT):
            return "ZrLibrary_AotRuntime_ToInt";
        default:
            return ZR_NULL;
    }
}

TZrBool backend_aot_llvm_lower_arithmetic_value_family(const SZrAotLlvmLoweringContext *context,
                                                       const SZrAotLlvmInstructionContext *instruction) {
    const TZrChar *helperName;

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    helperName = backend_aot_llvm_binary_value_helper_name(instruction->opcode);
    if (helperName != ZR_NULL) {
        return backend_aot_llvm_lower_binary_value_instruction(context, instruction, helperName);
    }

    helperName = backend_aot_llvm_unary_value_helper_name(instruction->opcode);
    if (helperName != ZR_NULL) {
        return backend_aot_llvm_lower_unary_value_instruction(context, instruction, helperName);
    }

    return ZR_FALSE;
}
