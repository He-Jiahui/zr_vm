#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_resume_control_instruction(const SZrAotLlvmLoweringContext *context,
                                                                 const SZrAotLlvmInstructionContext *instruction) {
    TZrChar resumeOkLabel[96];
    TZrChar argsBuffer[256];

    backend_aot_llvm_make_instruction_label(resumeOkLabel,
                                            sizeof(resumeOkLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "resume_ok");
    if (instruction->opcode == ZR_INSTRUCTION_ENUM(THROW)) {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, ptr %%resume_instruction",
                 (unsigned)instruction->destinationSlot);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_Throw",
                                                 argsBuffer,
                                                 resumeOkLabel,
                                                 context->failLabel);
    } else if (instruction->opcode == ZR_INSTRUCTION_ENUM(END_FINALLY)) {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, ptr %%resume_instruction",
                 (unsigned)instruction->destinationSlot);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_EndFinally",
                                                 argsBuffer,
                                                 resumeOkLabel,
                                                 context->failLabel);
    } else if (instruction->opcode == ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN)) {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u, ptr %%resume_instruction",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)instruction->operandA2);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_SetPendingReturn",
                                                 argsBuffer,
                                                 resumeOkLabel,
                                                 context->failLabel);
    } else if (instruction->opcode == ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK)) {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, ptr %%resume_instruction",
                 (unsigned)instruction->operandA2);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_SetPendingBreak",
                                                 argsBuffer,
                                                 resumeOkLabel,
                                                 context->failLabel);
    } else {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, ptr %%resume_instruction",
                 (unsigned)instruction->operandA2);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_SetPendingContinue",
                                                 argsBuffer,
                                                 resumeOkLabel,
                                                 context->failLabel);
    }

    fprintf(context->file, "%s:\n", resumeOkLabel);
    backend_aot_llvm_write_resume_dispatch(context->file,
                                           context->tempCounter,
                                           context->entry->flatIndex,
                                           instruction->instructionIndex,
                                           context->instructionCount,
                                           "%resume_instruction",
                                           instruction->nextLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_exception_control_family(const SZrAotLlvmLoweringContext *context,
                                                        const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(TRY):
            snprintf(argsBuffer,
                     sizeof(argsBuffer),
                     "ptr %%state, ptr %%frame, i32 %u",
                     (unsigned)instruction->destinationSlot);
            backend_aot_llvm_write_guarded_call_text(context->file,
                                                     context->tempCounter,
                                                     "ZrLibrary_AotRuntime_Try",
                                                     argsBuffer,
                                                     instruction->nextLabel,
                                                     context->failLabel);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(END_TRY):
            snprintf(argsBuffer,
                     sizeof(argsBuffer),
                     "ptr %%state, ptr %%frame, i32 %u",
                     (unsigned)instruction->destinationSlot);
            backend_aot_llvm_write_guarded_call_text(context->file,
                                                     context->tempCounter,
                                                     "ZrLibrary_AotRuntime_EndTry",
                                                     argsBuffer,
                                                     instruction->nextLabel,
                                                     context->failLabel);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
            return backend_aot_llvm_lower_resume_control_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(CATCH):
            snprintf(argsBuffer,
                     sizeof(argsBuffer),
                     "ptr %%state, ptr %%frame, i32 %u",
                     (unsigned)instruction->destinationSlot);
            backend_aot_llvm_write_guarded_call_text(context->file,
                                                     context->tempCounter,
                                                     "ZrLibrary_AotRuntime_Catch",
                                                     argsBuffer,
                                                     instruction->nextLabel,
                                                     context->failLabel);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}
