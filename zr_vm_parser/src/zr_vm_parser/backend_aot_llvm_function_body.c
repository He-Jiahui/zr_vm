#include "backend_aot_llvm_function_body.h"

#include "backend_aot_llvm_emitter.h"

void backend_aot_write_llvm_function_body(FILE *file,
                                          SZrState *state,
                                          const SZrAotFunctionTable *functionTable,
                                          const SZrAotFunctionEntry *entry) {
    TZrUInt32 tempCounter = 0;
    TZrBool publishExports;
    TZrUInt32 *callableSlotFunctionIndices;
    TZrChar failLabel[96];
    TZrChar endUnsupportedLabel[96];
    TZrChar startLabel[96];
    TZrUInt32 instructionCount;
    SZrAotLlvmLoweringContext loweringContext;

    if (file == ZR_NULL || entry == ZR_NULL || entry->function == ZR_NULL) {
        return;
    }

    publishExports = entry->flatIndex == ZR_AOT_FUNCTION_TREE_ROOT_INDEX &&
                     entry->function->exportedVariableLength > 0;
    instructionCount = entry->function->instructionsLength;
    callableSlotFunctionIndices = backend_aot_allocate_callable_slot_function_indices(state, entry->function);

    backend_aot_llvm_make_function_label(failLabel, sizeof(failLabel), entry->flatIndex, "fail");
    backend_aot_llvm_make_function_label(endUnsupportedLabel,
                                         sizeof(endUnsupportedLabel),
                                         entry->flatIndex,
                                         "end_unsupported");
    if (instructionCount > 0) {
        backend_aot_llvm_make_instruction_label(startLabel, sizeof(startLabel), entry->flatIndex, 0, ZR_NULL);
    } else {
        snprintf(startLabel, sizeof(startLabel), "%s", endUnsupportedLabel);
    }

    loweringContext.file = file;
    loweringContext.state = state;
    loweringContext.functionTable = functionTable;
    loweringContext.entry = entry;
    loweringContext.tempCounter = &tempCounter;
    loweringContext.callableSlotFunctionIndices = callableSlotFunctionIndices;
    loweringContext.failLabel = failLabel;
    loweringContext.instructionCount = instructionCount;
    loweringContext.publishExports = publishExports;

    fprintf(file, "define internal i64 @zr_aot_fn_%u(ptr %%state) {\n", (unsigned)entry->flatIndex);
    fprintf(file, "entry:\n");
    fprintf(file, "  %%frame = alloca %%ZrAotGeneratedFrame, align 8\n");
    fprintf(file, "  %%direct_call = alloca %%ZrAotGeneratedDirectCall, align 8\n");
    fprintf(file, "  %%resume_instruction = alloca i32, align 4\n");
    fprintf(file, "  %%truthy_value = alloca i8, align 1\n");
    {
        TZrUInt32 beginTemp = backend_aot_llvm_next_temp(&tempCounter);
        fprintf(file,
                "  %%t%u = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %%state, i32 %u, ptr %%frame)\n",
                (unsigned)beginTemp,
                (unsigned)entry->flatIndex);
        fprintf(file,
                "  br i1 %%t%u, label %%%s, label %%%s\n\n",
                (unsigned)beginTemp,
                startLabel,
                failLabel);
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < instructionCount; instructionIndex++) {
        const TZrInstruction *instruction = &entry->function->instructionsList[instructionIndex];
        SZrAotLlvmInstructionContext instructionContext;
        TZrChar instructionLabel[96];
        TZrChar bodyLabel[96];
        TZrChar nextLabel[96];

        instructionContext.instructionIndex = instructionIndex;
        instructionContext.opcode = instruction->instruction.operationCode;
        instructionContext.destinationSlot = instruction->instruction.operandExtra;
        instructionContext.operandA1 = instruction->instruction.operand.operand1[0];
        instructionContext.operandB1 = instruction->instruction.operand.operand1[1];
        instructionContext.operandA2 = instruction->instruction.operand.operand2[0];

        backend_aot_llvm_make_instruction_label(instructionLabel,
                                                sizeof(instructionLabel),
                                                entry->flatIndex,
                                                instructionIndex,
                                                ZR_NULL);
        backend_aot_llvm_make_instruction_label(bodyLabel,
                                                sizeof(bodyLabel),
                                                entry->flatIndex,
                                                instructionIndex,
                                                "body");
        if (instructionIndex + 1 < instructionCount) {
            backend_aot_llvm_make_instruction_label(nextLabel,
                                                    sizeof(nextLabel),
                                                    entry->flatIndex,
                                                    instructionIndex + 1,
                                                    ZR_NULL);
        } else {
            snprintf(nextLabel, sizeof(nextLabel), "%s", endUnsupportedLabel);
        }
        instructionContext.nextLabel = nextLabel;

        fprintf(file, "%s:\n", instructionLabel);
        backend_aot_llvm_write_begin_instruction(file,
                                                 &tempCounter,
                                                 instructionIndex,
                                                 backend_aot_c_step_flags_for_instruction(instructionContext.opcode),
                                                 bodyLabel,
                                                 failLabel);
        fprintf(file, "%s:\n", bodyLabel);

        if (!backend_aot_llvm_lower_value_instruction(&loweringContext, &instructionContext) &&
            !backend_aot_llvm_lower_call_instruction(&loweringContext, &instructionContext) &&
            !backend_aot_llvm_lower_control_instruction(&loweringContext, &instructionContext)) {
            backend_aot_llvm_write_report_unsupported_return(file,
                                                             &tempCounter,
                                                             entry->flatIndex,
                                                             instructionIndex,
                                                             instructionContext.opcode);
        }
        fprintf(file, "\n");
    }

    fprintf(file, "%s:\n", endUnsupportedLabel);
    backend_aot_llvm_write_report_unsupported_return(file,
                                                     &tempCounter,
                                                     entry->flatIndex,
                                                     instructionCount,
                                                     0);
    fprintf(file, "\n");
    fprintf(file, "%s:\n", failLabel);
    {
        TZrUInt32 failedTemp = backend_aot_llvm_next_temp(&tempCounter);
        fprintf(file,
                "  %%t%u = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %%state, ptr %%frame)\n",
                (unsigned)failedTemp);
        fprintf(file, "  ret i64 %%t%u\n", (unsigned)failedTemp);
    }
    fprintf(file, "}\n\n");
    backend_aot_release_callable_slot_function_indices(state, entry->function, callableSlotFunctionIndices);
}
