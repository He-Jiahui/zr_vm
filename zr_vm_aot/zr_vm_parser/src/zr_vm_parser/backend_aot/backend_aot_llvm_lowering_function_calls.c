#include "backend_aot_llvm_emitter.h"

static TZrUInt32 backend_aot_llvm_function_call_argument_count(const SZrFunction *function,
                                                               const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 argumentCount = instruction->operandB1;

    if (instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS)) {
        return 0;
    }
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED)) {
        return backend_aot_get_callsite_cache_argument_count(function,
                                                             instruction->operandB1,
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);
    }
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED)) {
        return backend_aot_get_callsite_cache_argument_count(function,
                                                             instruction->operandB1,
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL);
    }

    return argumentCount;
}

TZrBool backend_aot_llvm_lower_function_call_family(const SZrAotLlvmLoweringContext *context,
                                                    const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 argumentCount;
    TZrUInt32 functionSlot;
    TZrUInt32 calleeFlatIndex;
    TZrBool isTailCall;
    TZrChar prepareOkLabel[96];
    TZrChar invokeOkLabel[96];
    TZrChar finishOkLabel[96];
    TZrChar calleeName[64];
    TZrChar argsBuffer[256];

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instruction->opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD)) {
        TZrChar spreadOkLabel[96];

        backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                     context->entry->function,
                                                     instruction->destinationSlot,
                                                     ZR_AOT_INVALID_FUNCTION_INDEX);
        backend_aot_llvm_make_instruction_label(spreadOkLabel,
                                                sizeof(spreadOkLabel),
                                                context->entry->flatIndex,
                                                instruction->instructionIndex,
                                                "spread_ok");
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u, ptr null",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)instruction->operandA1,
                 (unsigned)instruction->operandB1);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_CallSpread",
                                                 argsBuffer,
                                                 spreadOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", spreadOkLabel);
        fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
        return ZR_TRUE;
    }

    if (instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL) ||
        instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)) {
        TZrChar memberOkLabel[96];
        TZrUInt32 cacheIndex = instruction->operandA1;

        if (instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) {
            argumentCount = backend_aot_get_callsite_cache_argument_count(
                    context->entry->function,
                    cacheIndex,
                    ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
        } else if (instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)) {
            TZrChar receiverCopyOkLabel[96];
            TZrChar argumentCopyOkLabel[96];

            cacheIndex = instruction->operandU8A;
            argumentCount = backend_aot_get_callsite_cache_argument_count(
                    context->entry->function,
                    cacheIndex,
                    ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
            backend_aot_llvm_make_instruction_label(receiverCopyOkLabel,
                                                    sizeof(receiverCopyOkLabel),
                                                    context->entry->flatIndex,
                                                    instruction->instructionIndex,
                                                    "receiver_copy_ok");
            backend_aot_llvm_make_instruction_label(argumentCopyOkLabel,
                                                    sizeof(argumentCopyOkLabel),
                                                    context->entry->flatIndex,
                                                    instruction->instructionIndex,
                                                    "argument_copy_ok");
            snprintf(argsBuffer,
                     sizeof(argsBuffer),
                     "ptr %%state, ptr %%frame, i32 %u, i32 %u",
                     (unsigned)(instruction->destinationSlot + 1u),
                     (unsigned)instruction->operandU8B);
            backend_aot_llvm_write_guarded_call_text(context->file,
                                                     context->tempCounter,
                                                     "ZrLibrary_AotRuntime_CopyStack",
                                                     argsBuffer,
                                                     receiverCopyOkLabel,
                                                     context->failLabel);
            fprintf(context->file, "%s:\n", receiverCopyOkLabel);
            snprintf(argsBuffer,
                     sizeof(argsBuffer),
                     "ptr %%state, ptr %%frame, i32 %u, i32 %u",
                     (unsigned)(instruction->destinationSlot + 2u),
                     (unsigned)instruction->operandU8C);
            backend_aot_llvm_write_guarded_call_text(context->file,
                                                     context->tempCounter,
                                                     "ZrLibrary_AotRuntime_CopyStack",
                                                     argsBuffer,
                                                     argumentCopyOkLabel,
                                                     context->failLabel);
            fprintf(context->file, "%s:\n", argumentCopyOkLabel);
        } else {
            argumentCount = instruction->operandB1;
        }

        backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                     context->entry->function,
                                                     instruction->destinationSlot,
                                                     ZR_AOT_INVALID_FUNCTION_INDEX);
        backend_aot_llvm_make_instruction_label(memberOkLabel,
                                                sizeof(memberOkLabel),
                                                context->entry->flatIndex,
                                                instruction->instructionIndex,
                                                "member_ok");
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
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)(instruction->destinationSlot + 1u),
                 (unsigned)cacheIndex);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_GetMemberSlot",
                                                 argsBuffer,
                                                 memberOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", memberOkLabel);
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u, ptr %%direct_call",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)instruction->destinationSlot,
                 (unsigned)argumentCount);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_PrepareDirectCall",
                                                 argsBuffer,
                                                 prepareOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", prepareOkLabel);
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, ptr %%direct_call, i32 %u, i32 %u, i32 %u, i32 1, ptr %%resume_instruction",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)instruction->destinationSlot,
                 (unsigned)argumentCount);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume",
                                                 argsBuffer,
                                                 finishOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", finishOkLabel);
        backend_aot_llvm_write_resume_dispatch(context->file,
                                               context->tempCounter,
                                               context->entry->flatIndex,
                                               instruction->instructionIndex,
                                               context->instructionCount,
                                               "%resume_instruction",
                                               instruction->nextLabel);
        return ZR_TRUE;
    }

    argumentCount = backend_aot_llvm_function_call_argument_count(context->entry->function, instruction);
    functionSlot = instruction->operandA1;
    isTailCall = instruction->opcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS) ||
                 instruction->opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED);

    calleeFlatIndex = backend_aot_get_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                                   context->entry->function,
                                                                   functionSlot);
    if (calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        calleeFlatIndex = backend_aot_resolve_callable_slot_function_index_before_instruction(context->functionTable,
                                                                                              context->state,
                                                                                              context->entry->function,
                                                                                              instruction->instructionIndex,
                                                                                              functionSlot,
                                                                                              0);
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    backend_aot_llvm_make_instruction_label(prepareOkLabel,
                                            sizeof(prepareOkLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "prepare_ok");
    backend_aot_llvm_make_instruction_label(invokeOkLabel,
                                            sizeof(invokeOkLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "invoke_ok");
    backend_aot_llvm_make_instruction_label(finishOkLabel,
                                            sizeof(finishOkLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "finish_ok");

    if (calleeFlatIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u, i32 %u, ptr %%direct_call",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)functionSlot,
                 (unsigned)argumentCount,
                 (unsigned)calleeFlatIndex);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_PrepareStaticDirectCall",
                                                 argsBuffer,
                                                 prepareOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", prepareOkLabel);
        backend_aot_llvm_format_function_symbol(calleeName,
                                                sizeof(calleeName),
                                                calleeFlatIndex,
                                                context->stripGeneratedSymbols);
        backend_aot_llvm_write_nonzero_call_text(context->file,
                                                 context->tempCounter,
                                                 calleeName,
                                                 "ptr %state",
                                                 invokeOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", invokeOkLabel);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_FinishDirectCall",
                                                 "ptr %state, ptr %frame, ptr %direct_call, i32 1",
                                                 finishOkLabel,
                                                 context->failLabel);
    } else {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u, ptr %%direct_call",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)functionSlot,
                 (unsigned)argumentCount);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_PrepareDirectCall",
                                                 argsBuffer,
                                                 prepareOkLabel,
                                                 context->failLabel);
        fprintf(context->file, "%s:\n", prepareOkLabel);
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, ptr %%direct_call, i32 %u, i32 %u, i32 %u, i32 1",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)functionSlot,
                 (unsigned)argumentCount);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_CallPreparedOrGeneric",
                                                 argsBuffer,
                                                 finishOkLabel,
                                                 context->failLabel);
    }

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
