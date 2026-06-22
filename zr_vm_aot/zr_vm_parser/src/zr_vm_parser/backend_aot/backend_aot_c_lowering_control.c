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
            "    {\n"
            "        /* zr_aot_unsupported_instruction */\n"
            "        ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,\n"
            "                                                                         %u,\n"
            "                                                                         %s,\n"
            "                                                                         %s));\n"
            "    }\n",
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
                                                         const char *helperCallFormat,
                                                         TZrUInt32 functionFlatIndex,
                                                         TZrUInt32 firstArgument,
                                                         TZrUInt32 secondArgument,
                                                         TZrBool hasSecondArgument) {
    if (file == ZR_NULL || marker == ZR_NULL || helperCallFormat == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        ZR_AOT_C_GUARD(",
            marker);
    if (hasSecondArgument) {
        fprintf(file,
                helperCallFormat,
                (unsigned)firstArgument,
                (unsigned)secondArgument);
    } else {
        fprintf(file, helperCallFormat, (unsigned)firstArgument);
    }
    fprintf(file,
            ");\n"
            "        if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "            goto zr_aot_fn_%u_dispatch;\n"
            "        }\n"
            "    }\n",
            (unsigned)functionFlatIndex);
}

void backend_aot_write_c_try(FILE *file, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_try_direct */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Try(state, &frame, %u));\n"
            "    }\n",
            (unsigned)handlerIndex);
}

void backend_aot_write_c_end_try(FILE *file, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_end_try_direct */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_EndTry(state, &frame, %u));\n"
            "    }\n",
            (unsigned)handlerIndex);
}

void backend_aot_write_c_throw(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_throw_direct */\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Throw(state, &frame, %u, &zr_aot_next_instruction));\n"
            "        if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "            goto zr_aot_fn_%u_dispatch;\n"
            "        }\n"
            "    }\n",
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
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Catch(state, &frame, %u));\n"
            "    }\n",
            (unsigned)destinationSlot);
}

void backend_aot_write_c_end_finally(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 handlerIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_end_finally_direct */\n"
            "        zr_aot_next_instruction = ZR_AOT_RUNTIME_RESUME_FALLTHROUGH;\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_EndFinally(state, &frame, %u, &zr_aot_next_instruction));\n"
            "        if (zr_aot_next_instruction != ZR_AOT_RUNTIME_RESUME_FALLTHROUGH) {\n"
            "            goto zr_aot_fn_%u_dispatch;\n"
            "        }\n"
            "    }\n",
            (unsigned)handlerIndex,
            (unsigned)functionFlatIndex);
}

void backend_aot_write_c_set_pending_return(FILE *file,
                                            TZrUInt32 functionFlatIndex,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_pending_control_transfer(file,
                                                 "zr_aot_pending_return",
                                                 "ZrLibrary_AotRuntime_SetPendingReturn(state, &frame, %u, %u, &zr_aot_next_instruction)",
                                                 functionFlatIndex,
                                                 sourceSlot,
                                                 targetInstructionIndex,
                                                 ZR_TRUE);
}

void backend_aot_write_c_set_pending_break(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_pending_control_transfer(file,
                                                 "zr_aot_pending_break",
                                                 "ZrLibrary_AotRuntime_SetPendingBreak(state, &frame, %u, &zr_aot_next_instruction)",
                                                 functionFlatIndex,
                                                 targetInstructionIndex,
                                                 0,
                                                 ZR_FALSE);
}

void backend_aot_write_c_set_pending_continue(FILE *file,
                                              TZrUInt32 functionFlatIndex,
                                              TZrUInt32 targetInstructionIndex) {
    backend_aot_write_c_pending_control_transfer(file,
                                                 "zr_aot_pending_continue",
                                                 "ZrLibrary_AotRuntime_SetPendingContinue(state, &frame, %u, &zr_aot_next_instruction)",
                                                 functionFlatIndex,
                                                 targetInstructionIndex,
                                                 0,
                                                 ZR_FALSE);
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
                                                   TZrUInt32 execInstructionIndex,
                                                   TZrUInt32 targetInstructionIndex) {
    TZrBool useScalarCondition;

    if (file == ZR_NULL) {
        return;
    }

    useScalarCondition =
            backend_aot_c_scalar_locals_bool_written_before(functionIr, conditionSlot, execInstructionIndex);

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

static TZrBool backend_aot_c_signed_branch_operand_has_i64_local(const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 slot,
                                                                 TZrUInt32 execInstructionIndex) {
    return (TZrBool)(backend_aot_c_scalar_locals_has_i64_slot(functionIr, slot) &&
                     backend_aot_c_scalar_locals_i64_written_before(functionIr, slot, execInstructionIndex));
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
    TZrBool leftUseScalar;
    TZrBool rightUseScalar;
    TZrBool useScalarOperands;

    if (file == ZR_NULL || expressionText == ZR_NULL || operatorText == ZR_NULL) {
        return;
    }

    leftUseScalar = backend_aot_c_signed_branch_operand_has_i64_local(functionIr, leftSlot, execInstructionIndex);
    rightUseScalar = backend_aot_c_signed_branch_operand_has_i64_local(functionIr, rightSlot, execInstructionIndex);
    useScalarOperands = (TZrBool)(leftUseScalar && rightUseScalar);

    if (useScalarOperands) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_jump_if_signed_compare */\n"
                "        if (zr_aot_s%u %s zr_aot_s%u) {\n"
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n"
                "    }\n",
                (unsigned)leftSlot,
                operatorText,
                (unsigned)rightSlot,
                (unsigned)functionIndex,
                (unsigned)targetInstructionIndex);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_jump_if_signed_compare */\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n");
    if (!leftUseScalar || !rightUseScalar) {
        fprintf(file, "        if (frame.slotBase == ZR_NULL");
        if (!leftUseScalar) {
            fprintf(file, " || %u >= frame.generatedFrameSlotCount", (unsigned)leftSlot);
        }
        if (!rightUseScalar) {
            fprintf(file, " || %u >= frame.generatedFrameSlotCount", (unsigned)rightSlot);
        }
        fprintf(file,
                ") {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    if (leftUseScalar) {
        fprintf(file, "        zr_aot_left_scalar = zr_aot_s%u;\n", (unsigned)leftSlot);
    } else {
        fprintf(file,
                "        const SZrTypeValue *zr_aot_left = &frame.slotBase[%u].value;\n"
                "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n",
                (unsigned)leftSlot);
    }
    if (rightUseScalar) {
        fprintf(file, "        zr_aot_right_scalar = zr_aot_s%u;\n", (unsigned)rightSlot);
    } else {
        fprintf(file,
                "        const SZrTypeValue *zr_aot_right = &frame.slotBase[%u].value;\n"
                "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n",
                (unsigned)rightSlot);
    }
    fprintf(file,
            "        if (%s) {\n"
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n",
            expressionText,
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
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
        backend_aot_c_scalar_locals_has_i64_slot(functionIr, leftSlot) &&
        backend_aot_c_scalar_locals_i64_written_before(functionIr, leftSlot, execInstructionIndex)) {
        fprintf(file,
                "    {\n"
                "        /* zr_aot_jump_if_signed_compare */\n"
                "        TZrInt64 zr_aot_right_literal = %s;\n"
                "        if (zr_aot_s%u %s zr_aot_right_literal) {\n"
                "            goto zr_aot_fn_%u_ins_%u;\n"
                "        }\n"
                "    }\n",
                rightLiteral,
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
            "        ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_Return(state, &frame, %u, ZR_FALSE));\n"
            "    }\n",
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_return_i64_local(FILE *file, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_direct_return_i64_local */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s%u));\n"
            "        ZR_AOT_C_RETURN(1);\n"
            "    }\n",
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
