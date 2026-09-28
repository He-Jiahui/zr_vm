/* 异常、分支与作用域控制共享末级分派，未识别操作码继续向 unsupported 路径传递。 */
#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_scope_control_instruction(const SZrAotLlvmLoweringContext *context,
                                                                const SZrAotLlvmInstructionContext *instruction,
                                                                const TZrChar *helperName) {
    TZrChar argsBuffer[256];

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

static TZrBool backend_aot_llvm_lower_mark_close_proxy(
        const SZrAotLlvmLoweringContext *context,
        const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];

    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_MarkCloseProxy",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_control_instruction(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_llvm_lower_exception_control_family(context, instruction)) {
        return ZR_TRUE;
    }
    if (backend_aot_llvm_lower_branch_control_family(context, instruction)) {
        return ZR_TRUE;
    }
    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
            return backend_aot_llvm_lower_scope_control_instruction(context,
                                                                    instruction,
                                                                    "ZrLibrary_AotRuntime_MarkToBeClosed");
        case ZR_INSTRUCTION_ENUM(MARK_CLOSE_PROXY):
            return backend_aot_llvm_lower_mark_close_proxy(context, instruction);
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
            return backend_aot_llvm_lower_scope_control_instruction(context,
                                                                    instruction,
                                                                    "ZrLibrary_AotRuntime_CloseScope");
        default:
            break;
    }

    return ZR_FALSE;
}
