/* 迭代器初始化、推进和当前值读取由运行时 helper 按槽位执行。
 * 融合跳转保留下一条指令，假分支目标与解释器 DONE_SKIP(2)、校验器 step 2 一致：pc + offset + 2。 */
#include "backend_aot/backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_pair_slot_iterator_call(const SZrAotLlvmLoweringContext *context,
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

static TZrBool backend_aot_llvm_lower_fused_iter_move_next_jump(const SZrAotLlvmLoweringContext *context,
                                                                const SZrAotLlvmInstructionContext *instruction) {
    TZrInt64 targetIndex =
            (TZrInt64)instruction->instructionIndex + (TZrInt64)(TZrInt16)instruction->operandB1 + 2;
    TZrChar checkedLabel[96];
    TZrChar targetLabel[96];
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    if (targetIndex < 0 || (TZrUInt32)targetIndex >= context->instructionCount) {
        backend_aot_llvm_write_report_unsupported_return(context->file,
                                                         context->tempCounter,
                                                         context->entry->flatIndex,
                                                         instruction->instructionIndex,
                                                         instruction->opcode);
        return ZR_TRUE;
    }

    backend_aot_llvm_make_instruction_label(checkedLabel,
                                            sizeof(checkedLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "iter_branch");
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
                                             "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse",
                                             argsBuffer,
                                             checkedLabel,
                                             context->failLabel);
    fprintf(context->file, "%s:\n", checkedLabel);
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

TZrBool backend_aot_llvm_lower_iterator_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
            return backend_aot_llvm_lower_pair_slot_iterator_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_IterInit");
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
            return backend_aot_llvm_lower_pair_slot_iterator_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_IterMoveNext");
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            return backend_aot_llvm_lower_pair_slot_iterator_call(context,
                                                                  instruction,
                                                                  "ZrLibrary_AotRuntime_IterCurrent");
        case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            return backend_aot_llvm_lower_fused_iter_move_next_jump(context, instruction);
        default:
            return ZR_FALSE;
    }
}
