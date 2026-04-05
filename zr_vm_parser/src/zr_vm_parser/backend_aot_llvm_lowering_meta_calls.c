#include "backend_aot_llvm_emitter.h"

static TZrUInt32 backend_aot_llvm_meta_call_argument_count(const SZrFunction *function,
                                                           const SZrAotLlvmInstructionContext *instruction) {
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS)) {
        return 0;
    }
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)) {
        return backend_aot_get_callsite_cache_argument_count(function,
                                                             instruction->operandB1,
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL);
    }
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED)) {
        return backend_aot_get_callsite_cache_argument_count(function,
                                                             instruction->operandB1,
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL);
    }

    return instruction->operandB1;
}

TZrBool backend_aot_llvm_lower_meta_call_family(const SZrAotLlvmLoweringContext *context,
                                                const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 argumentCount;
    TZrUInt32 receiverSlot;
    TZrUInt32 callableArgumentCount;
    TZrBool isTailCall;
    TZrChar prepareOkLabel[96];
    TZrChar finishOkLabel[96];
    TZrChar argsBuffer[256];

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    argumentCount = backend_aot_llvm_meta_call_argument_count(context->entry->function, instruction);
    receiverSlot = instruction->operandA1;
    callableArgumentCount = argumentCount + 1;
    isTailCall = instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL);

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    backend_aot_llvm_make_instruction_label(prepareOkLabel,
                                            sizeof(prepareOkLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "prepare_ok");
    backend_aot_llvm_make_instruction_label(finishOkLabel,
                                            sizeof(finishOkLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "finish_ok");

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u, ptr %%direct_call",
             (unsigned)instruction->destinationSlot,
             (unsigned)receiverSlot,
             (unsigned)argumentCount);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_PrepareMetaCall",
                                             argsBuffer,
                                             prepareOkLabel,
                                             context->failLabel);
    fprintf(context->file, "%s:\n", prepareOkLabel);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, ptr %%direct_call, i32 %u, i32 %u, i32 %u, i32 1",
             (unsigned)instruction->destinationSlot,
             (unsigned)receiverSlot,
             (unsigned)callableArgumentCount);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_CallPreparedOrGeneric",
                                             argsBuffer,
                                             finishOkLabel,
                                             context->failLabel);

    fprintf(context->file, "%s:\n", finishOkLabel);
    if (isTailCall) {
        backend_aot_llvm_write_return_call(context->file,
                                           context->tempCounter,
                                           instruction->destinationSlot,
                                           context->publishExports);
    } else {
        fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    }
    return ZR_TRUE;
}
