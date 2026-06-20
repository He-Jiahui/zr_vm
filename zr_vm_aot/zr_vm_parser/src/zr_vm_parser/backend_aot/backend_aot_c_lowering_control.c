#include "backend_aot_c_emitter.h"
#include "backend_aot_c_scalar_locals.h"
#include "backend_aot_internal.h"

void backend_aot_write_c_unsupported_instruction_expr(FILE *file,
                                                      TZrUInt32 functionFlatIndex,
                                                      const char *instructionIndexExpression,
                                                      const char *opcodeExpression) {
    if (file == ZR_NULL || instructionIndexExpression == ZR_NULL || opcodeExpression == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_unsupported_instruction */\n"
            "        const TZrUInt32 zr_aot_function_index = %u;\n"
            "        const TZrUInt32 zr_aot_instruction_index = %s;\n"
            "        const TZrUInt32 zr_aot_opcode = %s;\n"
            "        (void)zr_aot_function_index;\n"
            "        (void)zr_aot_instruction_index;\n"
            "        (void)zr_aot_opcode;\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT instruction\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    } while (0);\n",
            (unsigned)functionFlatIndex,
            instructionIndexExpression,
            opcodeExpression);
}

void backend_aot_write_c_unsupported_instruction(FILE *file,
                                                 TZrUInt32 functionFlatIndex,
                                                 TZrUInt32 instructionIndex,
                                                 TZrUInt32 opcode) {
    char instructionBuffer[32];
    char opcodeBuffer[32];

    if (file == ZR_NULL) {
        return;
    }

    snprintf(instructionBuffer, sizeof(instructionBuffer), "%u", (unsigned)instructionIndex);
    snprintf(opcodeBuffer, sizeof(opcodeBuffer), "%u", (unsigned)opcode);
    backend_aot_write_c_unsupported_instruction_expr(file, functionFlatIndex, instructionBuffer, opcodeBuffer);
}

void backend_aot_write_c_dispatch_loop(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 instructionCount) {
    TZrUInt32 instructionIndex;

    if (file == ZR_NULL || instructionCount == 0) {
        return;
    }

    fprintf(file, "    TZrUInt32 zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n");
    fprintf(file, "    goto zr_aot_fn_%u_ins_0;\n", (unsigned)functionFlatIndex);
    fprintf(file, "zr_aot_fn_%u_dispatch:\n", (unsigned)functionFlatIndex);
    fprintf(file, "    switch (zr_aot_next_instruction) {\n");
    for (instructionIndex = 0; instructionIndex < instructionCount; instructionIndex++) {
        fprintf(file,
                "        case %u: goto zr_aot_fn_%u_ins_%u;\n",
                (unsigned)instructionIndex,
                (unsigned)functionFlatIndex,
                (unsigned)instructionIndex);
    }
    fprintf(file, "        default:\n");
    backend_aot_write_c_unsupported_instruction_expr(file, functionFlatIndex, "zr_aot_next_instruction", "0");
    fprintf(file, "            break;\n");
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_pending_control_transfer(FILE *file,
                                                         const char *marker,
                                                         const char *pendingKind,
                                                         TZrUInt32 functionFlatIndex,
                                                         TZrUInt32 targetInstructionIndex,
                                                         TZrBool hasValue,
                                                         TZrUInt32 valueSlot) {
    if (file == ZR_NULL || marker == ZR_NULL || pendingKind == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        SZrTypeValue *zr_aot_pending_value = ZR_NULL;\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            marker);
    if (hasValue) {
        fprintf(file,
                "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        zr_aot_pending_value = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
                "        if (zr_aot_pending_value == ZR_NULL) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                (unsigned)valueSlot,
                (unsigned)valueSlot);
    }
    fprintf(file,
            "        execution_set_pending_control(state,\n"
            "                                      %s,\n"
            "                                      zr_aot_call_info,\n"
            "                                      (TZrMemoryOffset)%u,\n"
            "                                      %u,\n"
            "                                      zr_aot_pending_value);\n"
            "        if (execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)) {\n"
            "            if (zr_aot_call_info != frame.callInfo || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "        } else {\n"
            "            if (!execution_jump_to_instruction_offset(state,\n"
            "                                                      &zr_aot_call_info,\n"
            "                                                      zr_aot_call_info,\n"
            "                                                      state->pendingControl.targetInstructionOffset)) {\n"
            "                execution_clear_pending_control(state);\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            execution_clear_pending_control(state);\n"
            "            if (zr_aot_call_info != frame.callInfo || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "        }\n"
            "        frame.callInfo = zr_aot_call_info;\n"
            "        frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "        state->callInfoList = zr_aot_call_info;\n"
            "        state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        if (zr_aot_call_info->context.context.programCounter < frame.function->instructionsList ||\n"
            "            zr_aot_call_info->context.context.programCounter >= frame.function->instructionsList + frame.function->instructionsLength) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_next_instruction = (TZrUInt32)(zr_aot_call_info->context.context.programCounter - frame.function->instructionsList);\n"
            "        if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "            goto zr_aot_fn_%u_dispatch;\n"
            "        }\n"
            "    }\n",
            pendingKind,
            (unsigned)targetInstructionIndex,
            (unsigned)(hasValue ? valueSlot : 0u),
            (unsigned)functionFlatIndex);
}

void backend_aot_write_c_try(FILE *file, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_try_direct */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL || %u >= frame.function->exceptionHandlerCount) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT TRY has invalid handler index\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!execution_push_exception_handler(state, zr_aot_call_info, %u)) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT TRY failed to push exception handler\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo = zr_aot_call_info;\n"
            "        if (zr_aot_call_info->functionBase.valuePointer != ZR_NULL) {\n"
            "            frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "            state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        }\n"
            "        state->callInfoList = zr_aot_call_info;\n"
            "    }\n",
            (unsigned)handlerIndex,
            (unsigned)handlerIndex);
}

void backend_aot_write_c_end_try(FILE *file, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_end_try_direct */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        SZrVmExceptionHandlerState *handlerState;\n"
            "        const SZrFunctionExceptionHandlerInfo *handlerInfo;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL || %u >= frame.function->exceptionHandlerCount) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT END_TRY has invalid handler index\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        handlerState = execution_find_handler_state(state, zr_aot_call_info, %u);\n"
            "        handlerInfo = &frame.function->exceptionHandlerList[%u];\n"
            "        if (handlerState != ZR_NULL) {\n"
            "            if (handlerInfo->hasFinally) {\n"
            "                handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;\n"
            "            } else {\n"
            "                execution_pop_exception_handler(state, handlerState);\n"
            "            }\n"
            "        }\n"
            "        frame.callInfo = zr_aot_call_info;\n"
            "        if (zr_aot_call_info->functionBase.valuePointer != ZR_NULL) {\n"
            "            frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "            state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        }\n"
            "        state->callInfoList = zr_aot_call_info;\n"
            "    }\n",
            (unsigned)handlerIndex,
            (unsigned)handlerIndex,
            (unsigned)handlerIndex);
}

void backend_aot_write_c_throw(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_throw_direct */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        SZrTypeValue *zr_aot_source_value;\n"
            "        SZrTypeValue zr_aot_payload;\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT THROW has invalid payload slot\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_value = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_source_value == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT THROW has missing payload value\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        execution_clear_pending_control(state);\n"
            "        zr_aot_payload = *zr_aot_source_value;\n"
            "        if (!ZrCore_Exception_NormalizeThrownValue(state,\n"
            "                                                   &zr_aot_payload,\n"
            "                                                   zr_aot_call_info,\n"
            "                                                   ZR_THREAD_STATUS_RUNTIME_ERROR)) {\n"
            "            if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {\n"
            "                ZrCore_Debug_RunError(state, \"generated AOT THROW failed to normalize exception\");\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "        }\n"
            "        if (!execution_unwind_exception_to_handler(state, &zr_aot_call_info)) {\n"
            "            ZrCore_Exception_Throw(state, state->currentExceptionStatus);\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info != frame.callInfo || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo = zr_aot_call_info;\n"
            "        frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "        state->callInfoList = zr_aot_call_info;\n"
            "        state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        if (zr_aot_call_info->context.context.programCounter < frame.function->instructionsList ||\n"
            "            zr_aot_call_info->context.context.programCounter >= frame.function->instructionsList + frame.function->instructionsLength) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_next_instruction = (TZrUInt32)(zr_aot_call_info->context.context.programCounter - frame.function->instructionsList);\n"
            "        goto zr_aot_fn_%u_dispatch;\n"
            "    }\n",
            (unsigned)sourceSlot,
            (unsigned)sourceSlot,
            (unsigned)functionFlatIndex);
}

void backend_aot_write_c_catch(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_catch_direct */\n"
            "        SZrTypeValue *zr_aot_destination;\n"
            "        if (state == ZR_NULL || frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT CATCH has invalid destination slot\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT CATCH is missing destination value\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (state->hasCurrentException) {\n"
            "            ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException);\n"
            "            ZrCore_Exception_ClearCurrent(state);\n"
            "        } else {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        }\n"
            "        execution_clear_pending_control(state);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_end_finally(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_end_finally_direct */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        SZrCallInfo *resumeCallInfo;\n"
            "        SZrVmExceptionHandlerState *handlerState;\n"
            "        TZrStackValuePointer targetSlot;\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT END_FINALLY is missing call frame\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        handlerState = execution_find_handler_state(state, zr_aot_call_info, %u);\n"
            "        if (handlerState != ZR_NULL) {\n"
            "            execution_pop_exception_handler(state, handlerState);\n"
            "        }\n"
            "        switch (state->pendingControl.kind) {\n"
            "            case ZR_VM_PENDING_CONTROL_NONE:\n"
            "                break;\n"
            "            case ZR_VM_PENDING_CONTROL_EXCEPTION:\n"
            "                resumeCallInfo = state->pendingControl.callInfo != ZR_NULL ? state->pendingControl.callInfo : zr_aot_call_info;\n"
            "                if (resumeCallInfo == ZR_NULL || resumeCallInfo->functionBase.valuePointer == ZR_NULL) {\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n"
            "                zr_aot_call_info = resumeCallInfo;\n"
            "                frame.callInfo = zr_aot_call_info;\n"
            "                frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "                state->callInfoList = zr_aot_call_info;\n"
            "                state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "                if (!execution_unwind_exception_to_handler(state, &zr_aot_call_info)) {\n"
            "                    ZrCore_Exception_Throw(state, state->currentExceptionStatus);\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n",
            (unsigned)handlerIndex);
    fprintf(file,
            "                if (zr_aot_call_info != frame.callInfo || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n"
            "                frame.callInfo = zr_aot_call_info;\n"
            "                frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "                state->callInfoList = zr_aot_call_info;\n"
            "                state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "                if (zr_aot_call_info->context.context.programCounter < frame.function->instructionsList ||\n"
            "                    zr_aot_call_info->context.context.programCounter >= frame.function->instructionsList + frame.function->instructionsLength) {\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n"
            "                zr_aot_next_instruction = (TZrUInt32)(zr_aot_call_info->context.context.programCounter - frame.function->instructionsList);\n"
            "                goto zr_aot_fn_%u_dispatch;\n"
            "            case ZR_VM_PENDING_CONTROL_RETURN:\n"
            "            case ZR_VM_PENDING_CONTROL_BREAK:\n"
            "            case ZR_VM_PENDING_CONTROL_CONTINUE:\n"
            "                resumeCallInfo = state->pendingControl.callInfo != ZR_NULL ? state->pendingControl.callInfo : zr_aot_call_info;\n"
            "                if (resumeCallInfo == ZR_NULL || resumeCallInfo->functionBase.valuePointer == ZR_NULL) {\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n"
            "                if (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_RETURN && state->pendingControl.hasValue) {\n"
            "                    targetSlot = resumeCallInfo->functionBase.valuePointer + 1 + state->pendingControl.valueSlot;\n"
            "                    ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);\n"
            "                }\n"
            "                zr_aot_call_info = resumeCallInfo;\n"
            "                frame.callInfo = zr_aot_call_info;\n"
            "                frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "                state->callInfoList = zr_aot_call_info;\n"
            "                state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "                if (execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)) {\n"
            "                    if (zr_aot_call_info != frame.callInfo || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "                        ZR_AOT_C_FAIL();\n"
            "                    }\n"
            "                } else {\n"
            "                    if (!execution_jump_to_instruction_offset(state,\n"
            "                                                          &zr_aot_call_info,\n",
            (unsigned)functionFlatIndex);
    fprintf(file,
            "                                                          zr_aot_call_info,\n"
            "                                                          state->pendingControl.targetInstructionOffset)) {\n"
            "                        execution_clear_pending_control(state);\n"
            "                        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {\n"
            "                            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);\n"
            "                        }\n"
            "                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);\n"
            "                        ZR_AOT_C_FAIL();\n"
            "                    }\n"
            "                    execution_clear_pending_control(state);\n"
            "                    if (zr_aot_call_info != frame.callInfo || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "                        ZR_AOT_C_FAIL();\n"
            "                    }\n"
            "                }\n"
            "                frame.callInfo = zr_aot_call_info;\n"
            "                frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "                state->callInfoList = zr_aot_call_info;\n"
            "                state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "                if (zr_aot_call_info->context.context.programCounter < frame.function->instructionsList ||\n"
            "                    zr_aot_call_info->context.context.programCounter >= frame.function->instructionsList + frame.function->instructionsLength) {\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n"
            "                zr_aot_next_instruction = (TZrUInt32)(zr_aot_call_info->context.context.programCounter - frame.function->instructionsList);\n"
            "                goto zr_aot_fn_%u_dispatch;\n"
            "            default:\n"
            "                execution_clear_pending_control(state);\n"
            "                break;\n"
            "        }\n"
            "    }\n",
            (unsigned)functionFlatIndex);
}

void backend_aot_write_c_set_pending_return(FILE *file,
                                            TZrUInt32 functionFlatIndex,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_pending_control_transfer(file,
                                                 "zr_aot_pending_return",
                                                 "ZR_VM_PENDING_CONTROL_RETURN",
                                                 functionFlatIndex,
                                                 targetInstructionIndex,
                                                 ZR_TRUE,
                                                 sourceSlot);
}

void backend_aot_write_c_set_pending_break(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_pending_control_transfer(file,
                                                 "zr_aot_pending_break",
                                                 "ZR_VM_PENDING_CONTROL_BREAK",
                                                 functionFlatIndex,
                                                 targetInstructionIndex,
                                                 ZR_FALSE,
                                                 0);
}

void backend_aot_write_c_set_pending_continue(FILE *file,
                                              TZrUInt32 functionFlatIndex,
                                              TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_pending_control_transfer(file,
                                                 "zr_aot_pending_continue",
                                                 "ZR_VM_PENDING_CONTROL_CONTINUE",
                                                 functionFlatIndex,
                                                 targetInstructionIndex,
                                                 ZR_FALSE,
                                                 0);
}

void backend_aot_write_c_direct_jump(FILE *file, TZrUInt32 functionIndex, TZrUInt32 targetInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "    goto zr_aot_fn_%u_ins_%u;\n", (unsigned)functionIndex, (unsigned)targetInstructionIndex);
}

void backend_aot_write_c_direct_jump_if_bool_false(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 functionIndex,
                                                   TZrUInt32 conditionSlot,
                                                   TZrUInt32 targetInstructionIndex) {
    TZrBool useScalarCondition;

    if (file == ZR_NULL) {
        return;
    }

    useScalarCondition = backend_aot_c_scalar_locals_has_bool_slot(functionIr, conditionSlot);

    if (useScalarCondition) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_jump_if_bool_false */\n"
                "        /* zr_aot_jump_if_bool_false_scalar_local */\n"
                "        if (!zr_aot_b%u) {\n"
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n"
                "    }\n",
                (unsigned)conditionSlot,
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_jump_if_bool_false */\n"
            "        const SZrTypeValue *zr_aot_condition = ZR_NULL;\n"
            "        TZrBool zr_aot_condition_bool;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_condition = &frame.slotBase[%u].value;\n"
            "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_condition->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_condition_bool = (TZrBool)(zr_aot_condition->value.nativeObject.nativeBool != 0u);\n"
            "        if (!zr_aot_condition_bool) {\n"
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    }\n",
            (unsigned)conditionSlot,
            (unsigned)conditionSlot,
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}

static TZrBool backend_aot_c_format_signed_branch_const_literal(char *buffer,
                                                                TZrSize bufferSize,
                                                                const SZrTypeValue *constantValue) {
    if (buffer == ZR_NULL || bufferSize == 0 || constantValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    snprintf(buffer,
             (size_t)bufferSize,
             "(TZrInt64)%lld",
             (long long)constantValue->value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

static void backend_aot_write_c_direct_signed_branch(FILE *file,
                                                     const SZrAotExecIrFunction *functionIr,
                                                     const char *expressionText,
                                                     const char *operatorText,
                                                     TZrUInt32 functionIndex,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot,
                                                     TZrUInt32 execInstructionIndex,
                                                     TZrUInt32 targetInstructionIndex) {
    TZrBool useScalarOperands;

    if (file == ZR_NULL || expressionText == ZR_NULL || operatorText == ZR_NULL) {
        return;
    }

    useScalarOperands =
            (TZrBool)(backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex) &&
                      backend_aot_c_scalar_locals_i64_written_before(functionIr, rightSlot, execInstructionIndex));

    fprintf(file,
            "    {\n"
            "        /* zr_aot_jump_if_signed_compare */\n"
            "        const SZrTypeValue *zr_aot_left = ZR_NULL;\n"
            "        const SZrTypeValue *zr_aot_right = ZR_NULL;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left = &frame.slotBase[%u].value;\n"
            "        zr_aot_right = &frame.slotBase[%u].value;\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    if (useScalarOperands) {
        fprintf(file,
                "        if (zr_aot_s%u %s zr_aot_s%u) {\n"
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n",
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot,
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
    } else {
        fprintf(file,
                "        TZrInt64 zr_aot_left_scalar;\n"
                "        TZrInt64 zr_aot_right_scalar;\n"
                "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
                "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n"
                "        if (%s) {\n"
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n",
                expressionText,
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
    }
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_direct_signed_branch_const(FILE *file,
                                                           const SZrAotExecIrFunction *functionIr,
                                                           const SZrFunction *function,
                                                           const char *expressionText,
                                                           const char *operatorText,
                                                           TZrUInt32 functionIndex,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex,
                                                           TZrUInt32 execInstructionIndex,
                                                           TZrUInt32 targetInstructionIndex) {
    const SZrTypeValue *constantValue;
    char rightLiteral[64];

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_signed_branch_const_literal(rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        fprintf(file,
                "    {\n"
                "        ZrCore_Debug_RunError(state, \"unsupported typed signed branch constant\");\n"
                "        ZR_AOT_C_FAIL();\n"
                "    }\n");
        return;
    }

    if (operatorText != ZR_NULL &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_jump_if_signed_compare */\n"
                "        const SZrTypeValue *zr_aot_left = ZR_NULL;\n"
                "        TZrInt64 zr_aot_right_literal = %s;\n"
                "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        zr_aot_left = &frame.slotBase[%u].value;\n"
                "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        if (zr_aot_s%u %s zr_aot_right_literal) {\n"
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n"
                "    }\n",
                rightLiteral,
                (unsigned)leftSlot,
                (unsigned)leftSlot,
                (unsigned)leftSlot,
                operatorText,
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_jump_if_signed_compare */\n"
            "        const SZrTypeValue *zr_aot_left = ZR_NULL;\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        if (frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left = &frame.slotBase[%u].value;\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        if (%s) {\n"
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    }\n",
            rightLiteral,
            (unsigned)leftSlot,
            (unsigned)leftSlot,
            expressionText,
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}

void backend_aot_write_c_direct_jump_if_greater_signed(FILE *file,
                                                       const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 functionIndex,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrUInt32 execInstructionIndex,
                                                       TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_direct_signed_branch(file,
                                             functionIr,
                                             "zr_aot_left_scalar > zr_aot_right_scalar",
                                             ">",
                                             functionIndex,
                                             leftSlot,
                                             rightSlot,
                                             execInstructionIndex,
                                             targetInstructionIndex);
}

void backend_aot_write_c_direct_jump_if_less_equal_signed(FILE *file,
                                                          const SZrAotExecIrFunction *functionIr,
                                                          TZrUInt32 functionIndex,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot,
                                                          TZrUInt32 execInstructionIndex,
                                                          TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_direct_signed_branch(file,
                                             functionIr,
                                             "zr_aot_left_scalar <= zr_aot_right_scalar",
                                             "<=",
                                             functionIndex,
                                             leftSlot,
                                             rightSlot,
                                             execInstructionIndex,
                                             targetInstructionIndex);
}

void backend_aot_write_c_direct_jump_if_not_equal_signed(FILE *file,
                                                         const SZrAotExecIrFunction *functionIr,
                                                         TZrUInt32 functionIndex,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         TZrUInt32 execInstructionIndex,
                                                         TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_direct_signed_branch(file,
                                             functionIr,
                                             "zr_aot_left_scalar != zr_aot_right_scalar",
                                             "!=",
                                             functionIndex,
                                             leftSlot,
                                             rightSlot,
                                             execInstructionIndex,
                                             targetInstructionIndex);
}

void backend_aot_write_c_direct_jump_if_not_equal_signed_const(FILE *file,
                                                               const SZrAotExecIrFunction *functionIr,
                                                               const SZrFunction *function,
                                                               TZrUInt32 functionIndex,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 constantIndex,
                                                               TZrUInt32 execInstructionIndex,
                                                               TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_direct_signed_branch_const(file,
                                                   functionIr,
                                                   function,
                                                   "zr_aot_left_scalar != zr_aot_right_literal",
                                                   "!=",
                                                   functionIndex,
                                                   leftSlot,
                                                   constantIndex,
                                                   execInstructionIndex,
                                                   targetInstructionIndex);
}

void backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_direct_return */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        TZrStackValuePointer zr_aot_result_slot;\n"
            "        SZrTypeValue *zr_aot_result_value;\n"
            "        SZrTypeValue *zr_aot_caller_result_value;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_result_slot = frame.slotBase + %u;\n"
            "        zr_aot_result_value = &zr_aot_result_slot->value;\n"
            "        zr_aot_caller_result_value = &zr_aot_call_info->functionBase.valuePointer->value;\n"
            "        if (zr_aot_result_value == ZR_NULL || zr_aot_caller_result_value == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        execution_discard_exception_handlers_for_callinfo(state, zr_aot_call_info);\n"
            "        if (zr_aot_call_info->functionTop.valuePointer != ZR_NULL &&\n"
            "            (state->stackTop.valuePointer == ZR_NULL ||\n"
            "             state->stackTop.valuePointer < zr_aot_call_info->functionTop.valuePointer)) {\n"
            "            state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        }\n"
            "        ZrCore_Function_ApplyReturnEscape(state, frame.function, %u, zr_aot_result_value);\n"
            "        ZrCore_Closure_CloseClosure(state,\n"
            "                                    zr_aot_call_info->functionBase.valuePointer + 1,\n"
            "                                    ZR_THREAD_STATUS_INVALID,\n"
            "                                    ZR_FALSE);\n"
            "        ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, zr_aot_call_info);\n"
            "        if (frame.function->functionName == ZR_NULL ||\n"
            "            ZrCore_NativeString_Compare(ZrCore_String_GetNativeString(frame.function->functionName), \"constructor\") != 0) {\n"
            "            ZrCore_Value_Copy(state,\n"
            "                              zr_aot_caller_result_value,\n"
            "                              zr_aot_result_value);\n"
            "        }\n"
            "        state->stackTop.valuePointer = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "        ZR_AOT_C_RETURN(1);\n"
            "    }\n",
            (unsigned)sourceSlot,
            (unsigned)sourceSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports) {
    if (file == ZR_NULL) {
        return;
    }

    if (publishExports) {
        backend_aot_write_c_publish_exports(file);
    }

    backend_aot_write_c_direct_return(file, sourceSlot);
}

static void backend_aot_write_c_step_flag_expr(FILE *file, TZrUInt32 stepFlags) {
    TZrBool wroteFlag = ZR_FALSE;

    if (file == ZR_NULL) {
        return;
    }

    if (stepFlags == ZR_AOT_EMITTER_STEP_FLAG_NONE) {
        fprintf(file, "ZR_AOT_GENERATED_STEP_FLAG_NONE");
        return;
    }

    if (stepFlags & ZR_AOT_EMITTER_STEP_FLAG_MAY_THROW) {
        fprintf(file, "ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW");
        wroteFlag = ZR_TRUE;
    }
    if (stepFlags & ZR_AOT_EMITTER_STEP_FLAG_CONTROL_FLOW) {
        fprintf(file, "%sZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW", wroteFlag ? " | " : "");
        wroteFlag = ZR_TRUE;
    }
    if (stepFlags & ZR_AOT_EMITTER_STEP_FLAG_CALL) {
        fprintf(file, "%sZR_AOT_GENERATED_STEP_FLAG_CALL", wroteFlag ? " | " : "");
        wroteFlag = ZR_TRUE;
    }
    if (stepFlags & ZR_AOT_EMITTER_STEP_FLAG_RETURN) {
        fprintf(file, "%sZR_AOT_GENERATED_STEP_FLAG_RETURN", wroteFlag ? " | " : "");
    }
}

void backend_aot_write_c_begin_instruction(FILE *file, TZrUInt32 instructionIndex, TZrUInt32 stepFlags) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_begin_instruction */\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        TZrBool zr_aot_line_debug_enabled;\n"
            "        TZrBool zr_aot_publish_all_instructions;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            zr_aot_call_info = state->callInfoList;\n"
            "        }\n"
            "        if (zr_aot_call_info == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_call_info->functionBase.valuePointer != ZR_NULL) {\n"
            "            frame.callInfo = zr_aot_call_info;\n"
            "            frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "            state->callInfoList = zr_aot_call_info;\n"
            "            state->stackTop.valuePointer = zr_aot_call_info->functionTop.valuePointer;\n"
            "        } else {\n"
            "            frame.callInfo = zr_aot_call_info;\n"
            "            state->callInfoList = zr_aot_call_info;\n"
            "        }\n"
            "        frame.currentInstructionIndex = %u;\n"
            "        zr_aot_line_debug_enabled = (TZrBool)((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0u);\n"
            "        zr_aot_publish_all_instructions = (TZrBool)(frame.publishAllInstructions || zr_aot_line_debug_enabled);\n"
            "        if (zr_aot_publish_all_instructions || (frame.observationMask & (",
            (unsigned)instructionIndex);
    backend_aot_write_c_step_flag_expr(file, stepFlags);
    fprintf(file,
            ")) != 0u) {\n"
            "            zr_aot_call_info->context.context.programCounter = frame.function->instructionsList + %u;\n"
            "            frame.lastObservedInstructionIndex = %u;\n"
            "            if (zr_aot_line_debug_enabled) {\n"
            "                TZrUInt32 zr_aot_source_line = ZrCore_Exception_FindSourceLine(frame.function, (TZrMemoryOffset)%u);\n"
            "                if (zr_aot_source_line != ZR_RUNTIME_DEBUG_HOOK_LINE_NONE && zr_aot_source_line != frame.lastObservedLine) {\n"
            "                    frame.lastObservedLine = zr_aot_source_line;\n"
            "                    ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_LINE, zr_aot_source_line, 0, 0);\n"
            "                }\n"
            "            }\n"
            "        }\n"
            "    }\n",
            (unsigned)instructionIndex,
            (unsigned)instructionIndex,
            (unsigned)instructionIndex);
}
