#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_jump_instruction(const SZrAotLlvmLoweringContext *context,
                                                       const SZrAotLlvmInstructionContext *instruction) {
    TZrInt64 targetIndex = (TZrInt64)instruction->instructionIndex + (TZrInt64)instruction->operandA2 + 1;
    TZrChar targetLabel[96];

    if (targetIndex < 0 || (TZrUInt32)targetIndex >= context->instructionCount) {
        backend_aot_llvm_write_report_unsupported_return(context->file,
                                                         context->tempCounter,
                                                         context->entry->flatIndex,
                                                         instruction->instructionIndex,
                                                         instruction->opcode);
        return ZR_TRUE;
    }

    backend_aot_llvm_make_instruction_label(targetLabel,
                                            sizeof(targetLabel),
                                            context->entry->flatIndex,
                                            (TZrUInt32)targetIndex,
                                            ZR_NULL);
    fprintf(context->file, "  br label %%%s\n", targetLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_jump_if_instruction(const SZrAotLlvmLoweringContext *context,
                                                          const SZrAotLlvmInstructionContext *instruction) {
    TZrInt64 targetIndex = (TZrInt64)instruction->instructionIndex + (TZrInt64)instruction->operandA2 + 1;
    TZrChar truthyLabel[96];
    TZrChar falseLabel[96];
    TZrChar argsBuffer[256];

    if (targetIndex < 0 || (TZrUInt32)targetIndex >= context->instructionCount) {
        backend_aot_llvm_write_report_unsupported_return(context->file,
                                                         context->tempCounter,
                                                         context->entry->flatIndex,
                                                         instruction->instructionIndex,
                                                         instruction->opcode);
        return ZR_TRUE;
    }

    backend_aot_llvm_make_instruction_label(truthyLabel,
                                            sizeof(truthyLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "truthy");
    backend_aot_llvm_make_instruction_label(falseLabel,
                                            sizeof(falseLabel),
                                            context->entry->flatIndex,
                                            (TZrUInt32)targetIndex,
                                            ZR_NULL);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, ptr %%truthy_value",
             (unsigned)instruction->destinationSlot);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_IsTruthy",
                                             argsBuffer,
                                             truthyLabel,
                                             context->failLabel);
    fprintf(context->file, "%s:\n", truthyLabel);
    {
        TZrUInt32 valueTemp = backend_aot_llvm_next_temp(context->tempCounter);
        TZrUInt32 cmpTemp = backend_aot_llvm_next_temp(context->tempCounter);
        fprintf(context->file, "  %%t%u = load i8, ptr %%truthy_value, align 1\n", (unsigned)valueTemp);
        fprintf(context->file,
                "  %%t%u = icmp eq i8 %%t%u, 0\n",
                (unsigned)cmpTemp,
                (unsigned)valueTemp);
        fprintf(context->file,
                "  br i1 %%t%u, label %%%s, label %%%s\n",
                (unsigned)cmpTemp,
                falseLabel,
                instruction->nextLabel);
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_jump_if_greater_signed_instruction(
        const SZrAotLlvmLoweringContext *context,
        const SZrAotLlvmInstructionContext *instruction) {
    TZrInt64 targetIndex = (TZrInt64)instruction->instructionIndex + (TZrInt64)(TZrInt16)instruction->operandB1 + 1;
    TZrChar compareLabel[96];
    TZrChar targetLabel[96];
    TZrChar argsBuffer[256];

    if (targetIndex < 0 || (TZrUInt32)targetIndex >= context->instructionCount) {
        backend_aot_llvm_write_report_unsupported_return(context->file,
                                                         context->tempCounter,
                                                         context->entry->flatIndex,
                                                         instruction->instructionIndex,
                                                         instruction->opcode);
        return ZR_TRUE;
    }

    backend_aot_llvm_make_instruction_label(compareLabel,
                                            sizeof(compareLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "compare");
    backend_aot_llvm_make_instruction_label(targetLabel,
                                            sizeof(targetLabel),
                                            context->entry->flatIndex,
                                            (TZrUInt32)targetIndex,
                                            ZR_NULL);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, ptr %%truthy_value",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_ShouldJumpIfGreaterSigned",
                                             argsBuffer,
                                             compareLabel,
                                             context->failLabel);
    fprintf(context->file, "%s:\n", compareLabel);
    {
        TZrUInt32 valueTemp = backend_aot_llvm_next_temp(context->tempCounter);
        TZrUInt32 cmpTemp = backend_aot_llvm_next_temp(context->tempCounter);
        fprintf(context->file, "  %%t%u = load i8, ptr %%truthy_value, align 1\n", (unsigned)valueTemp);
        fprintf(context->file,
                "  %%t%u = icmp ne i8 %%t%u, 0\n",
                (unsigned)cmpTemp,
                (unsigned)valueTemp);
        fprintf(context->file,
                "  br i1 %%t%u, label %%%s, label %%%s\n",
                (unsigned)cmpTemp,
                targetLabel,
                instruction->nextLabel);
    }
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_branch_control_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
            return backend_aot_llvm_lower_jump_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            return backend_aot_llvm_lower_jump_if_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            return backend_aot_llvm_lower_jump_if_greater_signed_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            backend_aot_llvm_write_return_call(context->file,
                                               context->tempCounter,
                                               instruction->operandA1,
                                               context->publishExports);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}
