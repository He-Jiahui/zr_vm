#include "backend_aot_llvm_emitter.h"

TZrBool backend_aot_llvm_lower_logical_equal_signed_const_instruction(
        const SZrAotLlvmLoweringContext *context,
        const SZrAotLlvmInstructionContext *instruction) {
    TZrChar argsBuffer[256];
    TZrChar compareLabel[96];
    TZrChar storeLabel[96];
    TZrChar boolBitsBuffer[32];
    TZrUInt32 valueTemp;
    TZrUInt32 equalTemp;
    TZrUInt32 boolBitsTemp;
    TZrUInt32 destinationValueTemp;

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    backend_aot_llvm_make_instruction_label(compareLabel,
                                            sizeof(compareLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "equal_signed_const");
    backend_aot_llvm_make_instruction_label(storeLabel,
                                            sizeof(storeLabel),
                                            context->entry->flatIndex,
                                            instruction->instructionIndex,
                                            "equal_signed_const_store");
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, ptr %%truthy_value",
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_ShouldJumpIfNotEqualSignedConst",
                                             argsBuffer,
                                             compareLabel,
                                             context->failLabel);
    fprintf(context->file, "%s:\n", compareLabel);
    valueTemp = backend_aot_llvm_next_temp(context->tempCounter);
    equalTemp = backend_aot_llvm_next_temp(context->tempCounter);
    boolBitsTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file, "  %%t%u = load i8, ptr %%truthy_value, align 1\n", (unsigned)valueTemp);
    fprintf(context->file, "  %%t%u = icmp eq i8 %%t%u, 0\n", (unsigned)equalTemp, (unsigned)valueTemp);
    fprintf(context->file, "  %%t%u = zext i1 %%t%u to i64\n", (unsigned)boolBitsTemp, (unsigned)equalTemp);

    snprintf(argsBuffer, sizeof(argsBuffer), "ptr %%state, ptr %%frame, i32 %u",
             (unsigned)instruction->destinationSlot);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_ResetStackNull",
                                             argsBuffer,
                                             storeLabel,
                                             context->failLabel);
    fprintf(context->file, "%s:\n", storeLabel);
    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    snprintf(boolBitsBuffer, sizeof(boolBitsBuffer), "%%t%u", (unsigned)boolBitsTemp);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        boolBitsBuffer,
                                        ZR_VALUE_TYPE_BOOL);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    return ZR_TRUE;
}
