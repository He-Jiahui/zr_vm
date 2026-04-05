#include "backend_aot_c_emitter.h"
#include "backend_aot_internal.h"

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
    fprintf(file,
            "        default: return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, zr_aot_next_instruction, 0);\n",
            (unsigned)functionFlatIndex);
    fprintf(file, "    }\n");
}

static void backend_aot_write_c_control_transfer_call(FILE *file,
                                                      const char *helperName,
                                                      TZrUInt32 functionFlatIndex,
                                                      const char *argumentsText) {
    if (file == ZR_NULL || helperName == ZR_NULL || argumentsText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "    ZR_AOT_C_GUARD(%s(state, &frame, %s, &zr_aot_next_instruction));\n"
            "    if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "        goto zr_aot_fn_%u_dispatch;\n"
            "    }\n",
            helperName,
            argumentsText,
            (unsigned)functionFlatIndex);
}

void backend_aot_write_c_try(FILE *file, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Try(state, &frame, %u));\n",
            (unsigned)handlerIndex);
}

void backend_aot_write_c_end_try(FILE *file, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_EndTry(state, &frame, %u));\n",
            (unsigned)handlerIndex);
}

void backend_aot_write_c_throw(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 sourceSlot) {
    char argumentsBuffer[64];

    if (file == ZR_NULL) {
        return;
    }

    snprintf(argumentsBuffer, sizeof(argumentsBuffer), "%u", (unsigned)sourceSlot);
    backend_aot_write_c_control_transfer_call(file,
                                              "ZrLibrary_AotRuntime_Throw",
                                              functionFlatIndex,
                                              argumentsBuffer);
}

void backend_aot_write_c_catch(FILE *file, TZrUInt32 destinationSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Catch(state, &frame, %u));\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_end_finally(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 handlerIndex) {
    char argumentsBuffer[64];

    if (file == ZR_NULL) {
        return;
    }

    snprintf(argumentsBuffer, sizeof(argumentsBuffer), "%u", (unsigned)handlerIndex);
    backend_aot_write_c_control_transfer_call(file,
                                              "ZrLibrary_AotRuntime_EndFinally",
                                              functionFlatIndex,
                                              argumentsBuffer);
}

void backend_aot_write_c_set_pending_return(FILE *file,
                                            TZrUInt32 functionFlatIndex,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 targetInstructionIndex) {
    char argumentsBuffer[96];

    if (file == ZR_NULL) {
        return;
    }

    snprintf(argumentsBuffer,
             sizeof(argumentsBuffer),
             "%u, %u",
             (unsigned)sourceSlot,
             (unsigned)targetInstructionIndex);
    backend_aot_write_c_control_transfer_call(file,
                                              "ZrLibrary_AotRuntime_SetPendingReturn",
                                              functionFlatIndex,
                                              argumentsBuffer);
}

void backend_aot_write_c_set_pending_break(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 targetInstructionIndex) {
    char argumentsBuffer[64];

    if (file == ZR_NULL) {
        return;
    }

    snprintf(argumentsBuffer, sizeof(argumentsBuffer), "%u", (unsigned)targetInstructionIndex);
    backend_aot_write_c_control_transfer_call(file,
                                              "ZrLibrary_AotRuntime_SetPendingBreak",
                                              functionFlatIndex,
                                              argumentsBuffer);
}

void backend_aot_write_c_set_pending_continue(FILE *file,
                                              TZrUInt32 functionFlatIndex,
                                              TZrUInt32 targetInstructionIndex) {
    char argumentsBuffer[64];

    if (file == ZR_NULL) {
        return;
    }

    snprintf(argumentsBuffer, sizeof(argumentsBuffer), "%u", (unsigned)targetInstructionIndex);
    backend_aot_write_c_control_transfer_call(file,
                                              "ZrLibrary_AotRuntime_SetPendingContinue",
                                              functionFlatIndex,
                                              argumentsBuffer);
}

void backend_aot_write_c_direct_jump(FILE *file, TZrUInt32 functionIndex, TZrUInt32 targetInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "    goto zr_aot_fn_%u_ins_%u;\n", (unsigned)functionIndex, (unsigned)targetInstructionIndex);
}

void backend_aot_write_c_direct_jump_if(FILE *file,
                                        TZrUInt32 functionIndex,
                                        TZrUInt32 conditionSlot,
                                        TZrUInt32 targetInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        TZrBool zr_aot_condition = ZR_FALSE;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_IsTruthy(state, &frame, %u, &zr_aot_condition));\n"
            "        if (!zr_aot_condition) {\n"
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    }\n",
            (unsigned)conditionSlot,
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}

void backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        TZrStackValuePointer zr_aot_result_slot = frame.slotBase + %u;\n"
            "        if (zr_aot_call_info == ZR_NULL || zr_aot_call_info->functionBase.valuePointer == ZR_NULL ||\n"
            "            zr_aot_result_slot == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_Copy(state,\n"
            "                          ZrCore_Stack_GetValue(zr_aot_call_info->functionBase.valuePointer),\n"
            "                          ZrCore_Stack_GetValue(zr_aot_result_slot));\n"
            "        state->stackTop.valuePointer = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "        return 1;\n"
            "    }\n",
            (unsigned)sourceSlot);
}

void backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports) {
    if (file == ZR_NULL) {
        return;
    }

    if (publishExports) {
        fprintf(file,
                "    return ZrLibrary_AotRuntime_Return(state, &frame, %u, ZR_TRUE);\n",
                (unsigned)sourceSlot);
        return;
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

    fprintf(file, "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BeginInstruction(state, &frame, %u, ",
            (unsigned)instructionIndex);
    backend_aot_write_c_step_flag_expr(file, stepFlags);
    fprintf(file, "));\n");
}
