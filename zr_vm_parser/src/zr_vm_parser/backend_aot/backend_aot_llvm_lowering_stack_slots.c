#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_stack_copy_instruction(const SZrAotLlvmLoweringContext *context,
                                                             const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 destinationValueTemp;
    TZrUInt32 sourceValueTemp;
    TZrUInt32 sourceOwnershipTemp;
    TZrUInt32 destinationOwnershipTemp;
    TZrUInt32 isSharedTemp;
    TZrUInt32 isUniqueTemp;
    TZrUInt32 isLoanedTemp;
    TZrUInt32 transferPairTemp;
    TZrUInt32 transferTemp;
    TZrUInt32 isWeakTemp;
    TZrUInt32 sourceTypeTemp;
    TZrUInt32 sourceGcTemp;
    TZrUInt32 sourceGcNonZeroTemp;
    TZrUInt32 sourceIsObjectTemp;
    TZrUInt32 sourceGcObjectTemp;
    TZrUInt32 sourceOwnershipNoneTemp;
    TZrUInt32 destinationOwnershipNoneTemp;
    TZrUInt32 ownershipSafeTemp;
    TZrUInt32 notGcObjectTemp;
    TZrUInt32 fastPlainCopyTemp;
    TZrUInt32 copiedValueTemp;
    TZrUInt32 labelSeed;
    TZrChar transferLabel[96];
    TZrChar weakLabel[96];
    TZrChar fastCopyLabel[96];
    TZrChar slowCopyLabel[96];
    TZrChar weakCheckLabel[96];
    TZrChar fastCheckLabel[96];
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(
            context->callableSlotFunctionIndices,
            context->entry->function,
            instruction->destinationSlot,
            backend_aot_get_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                         context->entry->function,
                                                         (TZrUInt32)instruction->operandA2));
    if (instruction->operandA2 < 0) {
        snprintf(argsBuffer,
                 sizeof(argsBuffer),
                 "ptr %%state, ptr %%frame, i32 %u, i32 %u",
                 (unsigned)instruction->destinationSlot,
                 (unsigned)instruction->operandA2);
        backend_aot_llvm_write_guarded_call_text(context->file,
                                                 context->tempCounter,
                                                 "ZrLibrary_AotRuntime_CopyStack",
                                                 argsBuffer,
                                                 instruction->nextLabel,
                                                 context->failLabel);
        return ZR_TRUE;
    }
    if (instruction->destinationSlot == (TZrUInt32)instruction->operandA2) {
        fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
        return ZR_TRUE;
    }

    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    sourceValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                context->tempCounter,
                                                                (TZrUInt32)instruction->operandA2);
    sourceOwnershipTemp = backend_aot_llvm_emit_load_value_ownership_kind(context->file,
                                                                          context->tempCounter,
                                                                          sourceValueTemp);
    destinationOwnershipTemp = backend_aot_llvm_emit_load_value_ownership_kind(context->file,
                                                                               context->tempCounter,
                                                                               destinationValueTemp);
    isSharedTemp = backend_aot_llvm_next_temp(context->tempCounter);
    isUniqueTemp = backend_aot_llvm_next_temp(context->tempCounter);
    isLoanedTemp = backend_aot_llvm_next_temp(context->tempCounter);
    transferPairTemp = backend_aot_llvm_next_temp(context->tempCounter);
    transferTemp = backend_aot_llvm_next_temp(context->tempCounter);
    isWeakTemp = backend_aot_llvm_next_temp(context->tempCounter);
    sourceTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, sourceValueTemp);
    sourceGcTemp = backend_aot_llvm_emit_load_value_i8_field(context->file, context->tempCounter, sourceValueTemp, 2);
    sourceGcNonZeroTemp = backend_aot_llvm_next_temp(context->tempCounter);
    sourceIsObjectTemp = backend_aot_llvm_next_temp(context->tempCounter);
    sourceGcObjectTemp = backend_aot_llvm_next_temp(context->tempCounter);
    sourceOwnershipNoneTemp = backend_aot_llvm_next_temp(context->tempCounter);
    destinationOwnershipNoneTemp = backend_aot_llvm_next_temp(context->tempCounter);
    ownershipSafeTemp = backend_aot_llvm_next_temp(context->tempCounter);
    notGcObjectTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fastPlainCopyTemp = backend_aot_llvm_next_temp(context->tempCounter);

    labelSeed = backend_aot_llvm_next_temp(context->tempCounter);
    snprintf(transferLabel, sizeof(transferLabel), "zr_aot_stack_copy_transfer_%u", (unsigned)labelSeed);
    snprintf(weakCheckLabel, sizeof(weakCheckLabel), "zr_aot_stack_copy_weak_check_%u", (unsigned)labelSeed);
    snprintf(weakLabel, sizeof(weakLabel), "zr_aot_stack_copy_weak_%u", (unsigned)labelSeed);
    snprintf(fastCheckLabel, sizeof(fastCheckLabel), "zr_aot_stack_copy_fast_check_%u", (unsigned)labelSeed);
    snprintf(fastCopyLabel, sizeof(fastCopyLabel), "zr_aot_stack_copy_fast_%u", (unsigned)labelSeed);
    snprintf(slowCopyLabel, sizeof(slowCopyLabel), "zr_aot_stack_copy_slow_%u", (unsigned)labelSeed);

    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)isSharedTemp,
            (unsigned)sourceOwnershipTemp,
            (unsigned)ZR_OWNERSHIP_VALUE_KIND_SHARED);
    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)isUniqueTemp,
            (unsigned)sourceOwnershipTemp,
            (unsigned)ZR_OWNERSHIP_VALUE_KIND_UNIQUE);
    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)isLoanedTemp,
            (unsigned)sourceOwnershipTemp,
            (unsigned)ZR_OWNERSHIP_VALUE_KIND_LOANED);
    fprintf(context->file,
            "  %%t%u = or i1 %%t%u, %%t%u\n",
            (unsigned)transferPairTemp,
            (unsigned)isUniqueTemp,
            (unsigned)isLoanedTemp);
    fprintf(context->file,
            "  %%t%u = or i1 %%t%u, %%t%u\n",
            (unsigned)transferTemp,
            (unsigned)transferPairTemp,
            (unsigned)isSharedTemp);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)transferTemp,
            transferLabel,
            weakCheckLabel);

    fprintf(context->file, "%s:\n", transferLabel);
    fprintf(context->file,
            "  call void @ZrCore_Ownership_ReleaseValue(ptr %%state, ptr %%t%u)\n",
            (unsigned)destinationValueTemp);
    copiedValueTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file,
            "  %%t%u = load %%SZrTypeValue, ptr %%t%u, align %u\n",
            (unsigned)copiedValueTemp,
            (unsigned)sourceValueTemp,
            (unsigned)backend_aot_llvm_type_value_alignment());
    fprintf(context->file,
            "  store %%SZrTypeValue %%t%u, ptr %%t%u, align %u\n",
            (unsigned)copiedValueTemp,
            (unsigned)destinationValueTemp,
            (unsigned)backend_aot_llvm_type_value_alignment());
    backend_aot_llvm_emit_fast_set_null(context->file, context->tempCounter, sourceValueTemp);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);

    fprintf(context->file, "%s:\n", weakCheckLabel);
    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)isWeakTemp,
            (unsigned)sourceOwnershipTemp,
            (unsigned)ZR_OWNERSHIP_VALUE_KIND_WEAK);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)isWeakTemp,
            weakLabel,
            fastCheckLabel);

    fprintf(context->file, "%s:\n", weakLabel);
    fprintf(context->file,
            "  call void @ZrCore_Value_CopySlow(ptr %%state, ptr %%t%u, ptr %%t%u)\n",
            (unsigned)destinationValueTemp,
            (unsigned)sourceValueTemp);
    fprintf(context->file,
            "  call void @ZrCore_Ownership_ReleaseValue(ptr %%state, ptr %%t%u)\n",
            (unsigned)sourceValueTemp);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);

    fprintf(context->file, "%s:\n", fastCheckLabel);
    fprintf(context->file,
            "  %%t%u = icmp ne i8 %%t%u, 0\n",
            (unsigned)sourceGcNonZeroTemp,
            (unsigned)sourceGcTemp);
    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)sourceIsObjectTemp,
            (unsigned)sourceTypeTemp,
            (unsigned)ZR_VALUE_TYPE_OBJECT);
    fprintf(context->file,
            "  %%t%u = and i1 %%t%u, %%t%u\n",
            (unsigned)sourceGcObjectTemp,
            (unsigned)sourceGcNonZeroTemp,
            (unsigned)sourceIsObjectTemp);
    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)sourceOwnershipNoneTemp,
            (unsigned)sourceOwnershipTemp,
            (unsigned)ZR_OWNERSHIP_VALUE_KIND_NONE);
    fprintf(context->file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)destinationOwnershipNoneTemp,
            (unsigned)destinationOwnershipTemp,
            (unsigned)ZR_OWNERSHIP_VALUE_KIND_NONE);
    fprintf(context->file,
            "  %%t%u = and i1 %%t%u, %%t%u\n",
            (unsigned)ownershipSafeTemp,
            (unsigned)sourceOwnershipNoneTemp,
            (unsigned)destinationOwnershipNoneTemp);
    fprintf(context->file,
            "  %%t%u = xor i1 %%t%u, true\n",
            (unsigned)notGcObjectTemp,
            (unsigned)sourceGcObjectTemp);
    fprintf(context->file,
            "  %%t%u = and i1 %%t%u, %%t%u\n",
            (unsigned)fastPlainCopyTemp,
            (unsigned)ownershipSafeTemp,
            (unsigned)notGcObjectTemp);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)fastPlainCopyTemp,
            fastCopyLabel,
            slowCopyLabel);

    fprintf(context->file, "%s:\n", fastCopyLabel);
    copiedValueTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file,
            "  %%t%u = load %%SZrTypeValue, ptr %%t%u, align %u\n",
            (unsigned)copiedValueTemp,
            (unsigned)sourceValueTemp,
            (unsigned)backend_aot_llvm_type_value_alignment());
    fprintf(context->file,
            "  store %%SZrTypeValue %%t%u, ptr %%t%u, align %u\n",
            (unsigned)copiedValueTemp,
            (unsigned)destinationValueTemp,
            (unsigned)backend_aot_llvm_type_value_alignment());
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);

    fprintf(context->file, "%s:\n", slowCopyLabel);
    fprintf(context->file,
            "  call void @ZrCore_Value_CopySlow(ptr %%state, ptr %%t%u, ptr %%t%u)\n",
            (unsigned)destinationValueTemp,
            (unsigned)sourceValueTemp);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_stack_slot_value_subfamily(const SZrAotLlvmLoweringContext *context,
                                                          const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return backend_aot_llvm_lower_stack_copy_instruction(context, instruction);
        default:
            return ZR_FALSE;
    }
}
