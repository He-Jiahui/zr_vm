#include "backend_aot_c_value_semir_calls.h"

#include "backend_aot_internal.h"

static const SZrAotExecIrFrameSlotLayout *backend_aot_c_value_call_find_frame_slot_layout(
        const SZrAotExecIrFrameLayout *frameLayout,
        TZrUInt32 stackSlot) {
    TZrUInt32 layoutIndex;

    if (frameLayout == ZR_NULL || frameLayout->slotLayouts == ZR_NULL) {
        return ZR_NULL;
    }

    for (layoutIndex = 0; layoutIndex < frameLayout->slotLayoutCount; layoutIndex++) {
        const SZrAotExecIrFrameSlotLayout *layout = &frameLayout->slotLayouts[layoutIndex];

        if (layout->stackSlot == stackSlot) {
            return layout;
        }
    }

    return ZR_NULL;
}

static void backend_aot_write_c_value_call_slot_layout(FILE *file,
                                                       const char *label,
                                                       const SZrAotExecIrFrameSlotLayout *layout) {
    if (layout == ZR_NULL) {
        fprintf(file, " %s.layout=missing", label);
        return;
    }

    fprintf(file,
            " %s.offset=%u %s.size=%u %s.align=%u %s.typeLayoutId=%u",
            label,
            (unsigned)layout->byteOffset,
            label,
            (unsigned)layout->byteSize,
            label,
            (unsigned)layout->byteAlign,
            label,
            (unsigned)layout->typeLayoutId);
}

static TZrBool backend_aot_c_value_call_layout_can_inline_struct(
        const SZrAotExecIrFrameSlotLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     layout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     layout->byteSize > 0u);
}

static TZrBool backend_aot_c_value_call_type_ref_is_reference(
        const SZrFunctionTypedTypeRef *typeRef) {
    return (TZrBool)(typeRef != ZR_NULL &&
                     (typeRef->baseType == ZR_VALUE_TYPE_OBJECT ||
                      typeRef->baseType == ZR_VALUE_TYPE_ARRAY));
}

static TZrBool backend_aot_c_value_call_parameters_are_value_passing(
        const SZrAotExecIrFunction *calleeFunctionIr,
        TZrUInt32 argumentCount) {
    const SZrAotExecIrFrameLayout *calleeFrameLayout;

    if (calleeFunctionIr == ZR_NULL) {
        return ZR_FALSE;
    }
    calleeFrameLayout = &calleeFunctionIr->frameLayout;
    if (calleeFrameLayout->parameterCount != argumentCount ||
        calleeFrameLayout->parameterLayoutCount != argumentCount) {
        return ZR_FALSE;
    }
    if (argumentCount == 0u) {
        return ZR_TRUE;
    }
    if (calleeFrameLayout->parameterLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 argumentIndex = 0u;
         argumentIndex < argumentCount;
         argumentIndex++) {
        const SZrAotExecIrParameterLayout *parameterLayout =
                &calleeFrameLayout->parameterLayouts[argumentIndex];

        if (!backend_aot_exec_ir_parameter_passing_form_is_valid(
                    parameterLayout)) {
            return ZR_FALSE;
        }
        if (parameterLayout->roleFlags ==
            ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) {
            if (argumentIndex != 0u ||
                parameterLayout->passingFormKnown != ZR_FALSE) {
                return ZR_FALSE;
            }
            continue;
        }
        if (parameterLayout->roleFlags != 0u ||
            !backend_aot_exec_ir_parameter_is_value_passing(
                    parameterLayout)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_value_call_should_use_shared_method_slot(
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrAotExecIrFunction *calleeFunctionIr,
        TZrBool *outHasDefaultableParameter) {
    const SZrAotExecIrFrameLayout *calleeFrameLayout;
    TZrUInt32 argumentCount;
    TZrUInt32 argumentIndex;
    TZrBool hasReferenceArgument = ZR_FALSE;

    if (outHasDefaultableParameter != ZR_NULL) {
        *outHasDefaultableParameter = ZR_FALSE;
    }
    if (frameLayout == ZR_NULL ||
        instruction == ZR_NULL ||
        calleeFunctionIr == ZR_NULL ||
        outHasDefaultableParameter == ZR_NULL) {
        return ZR_FALSE;
    }

    argumentCount = instruction->operand1;
    calleeFrameLayout = &calleeFunctionIr->frameLayout;
    if (argumentCount == 0u ||
        calleeFrameLayout->parameterCount != argumentCount ||
        calleeFrameLayout->parameterLayoutCount != argumentCount ||
        calleeFrameLayout->parameterLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = instruction->operand0 + 1u + argumentIndex;
        const SZrAotExecIrParameterLayout *parameterLayout =
                &calleeFrameLayout->parameterLayouts[argumentIndex];
        const SZrFunctionTypedTypeRef *parameterType =
                &parameterLayout->type;
        const SZrAotExecIrFrameSlotLayout *sourceLayout =
                backend_aot_c_value_call_find_frame_slot_layout(frameLayout, sourceSlot);
        const TZrBool isReferenceParameter =
                backend_aot_c_value_call_type_ref_is_reference(parameterType);

        if (!backend_aot_exec_ir_parameter_default_declaration_is_valid(
                    parameterLayout)) {
            return ZR_FALSE;
        }
        if (parameterLayout->defaultDeclarationKnown &&
            parameterLayout->hasDeclaredDefault) {
            *outHasDefaultableParameter = ZR_TRUE;
        }
        if (parameterLayout->roleFlags != 0u &&
            (parameterLayout->roleFlags !=
                     ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER ||
             argumentIndex != 0u ||
             !isReferenceParameter)) {
            return ZR_FALSE;
        }
        if (!isReferenceParameter) {
            continue;
        }

        if (sourceLayout == ZR_NULL ||
            sourceLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            sourceLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }

        hasReferenceArgument = ZR_TRUE;
    }

    return hasReferenceArgument;
}

static void backend_aot_write_c_value_call_typed_metadata_guard(FILE *file,
                                                                const char *indent,
                                                                TZrUInt32 calleeFunctionIndex) {
    fprintf(file,
            "%s/* zr_aot_value_exec_call_typed_metadata_guard */\n"
            "%sif (ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, %u)) {\n",
            indent,
            indent,
            (unsigned)calleeFunctionIndex);
}

static void backend_aot_write_c_value_call_typed_inline_struct_direct(
        FILE *file,
        const char *indent,
        const SZrAotExecIrInstruction *instruction,
        TZrUInt32 argumentCount,
        TZrUInt32 calleeFunctionIndex,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const char *calleeThunkExpression) {
    fprintf(file,
            "%sZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallInlineStruct(state,\n"
            "%s                                                       &frame,\n"
            "%s                                                       %u,\n"
            "%s                                                       %u,\n"
            "%s                                                       %u,\n"
            "%s                                                       %u,\n"
            "%s                                                       %u,\n"
            "%s                                                       %u,\n"
            "%s                                                       %u,\n"
            "%s                                                       %s));\n",
            indent,
            indent,
            indent,
            (unsigned)instruction->destinationSlot,
            indent,
            (unsigned)instruction->operand0,
            indent,
            (unsigned)argumentCount,
            indent,
            (unsigned)calleeFunctionIndex,
            indent,
            (unsigned)destinationLayout->typeLayoutId,
            indent,
            (unsigned)destinationLayout->byteOffset,
            indent,
            (unsigned)destinationLayout->byteSize,
            indent,
            calleeThunkExpression);
}

static void backend_aot_write_c_value_call_typed_inline_struct_metadata_deopt(
        FILE *file,
        const char *indent,
        const SZrAotExecIrInstruction *instruction,
        TZrUInt32 argumentCount,
        const SZrAotExecIrFrameSlotLayout *destinationLayout) {
    fprintf(file,
            "%s/* zr_aot_value_exec_call_typed_metadata_deopt deopt=%u */\n"
            "%sZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,\n"
            "%s                                                                                 &frame,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 %u,\n"
            "%s                                                                                 \"typed inline struct direct call metadata drift\"));\n",
            indent,
            (unsigned)instruction->deoptId,
            indent,
            indent,
            indent,
            (unsigned)instruction->destinationSlot,
            indent,
            (unsigned)instruction->operand0,
            indent,
            (unsigned)argumentCount,
            indent,
            (unsigned)destinationLayout->typeLayoutId,
            indent,
            (unsigned)destinationLayout->byteOffset,
            indent,
            (unsigned)destinationLayout->byteSize,
            indent,
            (unsigned)instruction->deoptId,
            indent);
}

void backend_aot_write_c_value_semir_call_typed(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_value_call_find_frame_slot_layout(frameLayout, instruction->destinationSlot);

    fprintf(file,
            "    /* zr_aot_value_call_typed dstSlot=%u calleeSlot=%u argCount=%u type=%u",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)instruction->operand1,
            (unsigned)instruction->typeTableIndex);
    backend_aot_write_c_value_call_slot_layout(file, "dst", destinationLayout);
    fprintf(file, " */\n");
}

void backend_aot_write_c_value_semir_return_typed(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_value_call_find_frame_slot_layout(frameLayout, instruction->operand0);

    fprintf(file,
            "    /* zr_aot_value_return_typed sourceSlot=%u type=%u",
            (unsigned)instruction->operand0,
            (unsigned)instruction->typeTableIndex);
    backend_aot_write_c_value_call_slot_layout(file, "src", sourceLayout);
    fprintf(file, " */\n");
}

TZrBool backend_aot_try_write_c_value_semir_call_typed_exec(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrAotExecIrFunction *calleeFunctionIr,
        TZrUInt32 callerFunctionIndex,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 calleeFunctionIndex,
        TZrBool requireFullAot) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout;
    TZrUInt32 directInlineReturnTypeLayoutId;
    TZrUInt32 argumentCount;
    TZrUInt32 argumentIndex;
    TZrBool hasDefaultableParameter = ZR_FALSE;

    if (file == ZR_NULL || frameLayout == ZR_NULL || instruction == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    directInlineReturnTypeLayoutId =
            backend_aot_exec_ir_direct_inline_return_type_layout_id(calleeFunctionIr);
    destinationLayout = backend_aot_c_value_call_find_frame_slot_layout(
            frameLayout, instruction->destinationSlot);
    if (!backend_aot_c_value_call_layout_can_inline_struct(destinationLayout) ||
        (destinationLayout->reserved0 &
         ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS) != 0u ||
        directInlineReturnTypeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        destinationLayout->typeLayoutId != directInlineReturnTypeLayoutId) {
        return ZR_FALSE;
    }
    argumentCount = instruction->operand1;
    if (instruction->operand0 >= frameLayout->generatedFrameSlotCount ||
        argumentCount > frameLayout->generatedFrameSlotCount - instruction->operand0 - 1u) {
        return ZR_FALSE;
    }
    if (!backend_aot_c_value_call_parameters_are_value_passing(
                calleeFunctionIr, argumentCount)) {
        return ZR_FALSE;
    }
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = instruction->operand0 + 1u + argumentIndex;
        const SZrAotExecIrFrameSlotLayout *sourceLayout =
                backend_aot_c_value_call_find_frame_slot_layout(frameLayout, sourceSlot);

        if (sourceLayout == ZR_NULL ||
            sourceLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            sourceLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }
    }

    if (backend_aot_c_value_call_should_use_shared_method_slot(
                frameLayout,
                instruction,
                calleeFunctionIr,
                &hasDefaultableParameter)) {
        if (hasDefaultableParameter) {
            fprintf(file,
                    "    /* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */\n");
        }
        if (requireFullAot) {
            char calleeThunkExpression[32];

            snprintf(calleeThunkExpression,
                     sizeof(calleeThunkExpression),
                     "zr_aot_fn_%u",
                     (unsigned)calleeFunctionIndex);
            fprintf(file,
                    "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
                    "    /* zr_aot_generic_call_typed_shared_callsite */\n"
                    "    /* zr_aot_generic_call_typed_full_aot_no_deopt */\n"
                    "    /* zr_aot_value_exec_call_typed_inline_struct_full_aot_direct */\n",
                    (unsigned)instruction->destinationSlot,
                    (unsigned)instruction->operand0,
                    (unsigned)argumentCount,
                    (unsigned)calleeFunctionIndex);
            backend_aot_write_c_value_call_typed_inline_struct_direct(file,
                                                                      "    ",
                                                                      instruction,
                                                                      argumentCount,
                                                                      calleeFunctionIndex,
                                                                      destinationLayout,
                                                                      calleeThunkExpression);
            return ZR_TRUE;
        }

        fprintf(file,
                "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
                "    /* zr_aot_generic_call_typed_shared_callsite */\n"
                "    {\n"
                "        static const SZrAotGenericSlot zr_aot_generic_call_typed_%u_%u_slots[] = {\n"
                "            { .kind = ZR_AOT_GENERIC_SLOT_METHOD, .typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, .metadataToken = 0u, .methodIndex = %uu, .flags = 0u, .debugName = \"CALL_TYPED\", .staticTypeLayout = ZR_NULL, .staticMethod = zr_aot_fn_%u },\n"
                "        };\n"
                "        static SZrAotGenericResolvedSlot zr_aot_generic_call_typed_%u_%u_cache[1];\n"
                "        static SZrAotGenericDictionary zr_aot_generic_call_typed_%u_%u = {\n"
                "            .slotCount = 1u,\n"
                "            .slots = zr_aot_generic_call_typed_%u_%u_slots,\n"
                "            .resolvedSlots = zr_aot_generic_call_typed_%u_%u_cache,\n"
                "        };\n"
                "        FZrAotEntryThunk zr_aot_generic_call_typed_method =\n"
                "                ZrAot_GenericSlot_Method(&zr_aot_generic_call_typed_%u_%u, 0u);\n"
                "        if (zr_aot_generic_call_typed_method != ZR_NULL) {\n",
                (unsigned)instruction->destinationSlot,
                (unsigned)instruction->operand0,
                (unsigned)argumentCount,
                (unsigned)calleeFunctionIndex,
                (unsigned)callerFunctionIndex,
                (unsigned)execInstructionIndex,
                (unsigned)calleeFunctionIndex,
                (unsigned)calleeFunctionIndex,
                (unsigned)callerFunctionIndex,
                (unsigned)execInstructionIndex,
                (unsigned)callerFunctionIndex,
                (unsigned)execInstructionIndex,
                (unsigned)callerFunctionIndex,
                (unsigned)execInstructionIndex,
                (unsigned)callerFunctionIndex,
                (unsigned)execInstructionIndex,
                (unsigned)callerFunctionIndex,
                (unsigned)execInstructionIndex);
        backend_aot_write_c_value_call_typed_metadata_guard(file, "            ", calleeFunctionIndex);
        backend_aot_write_c_value_call_typed_inline_struct_direct(file,
                                                                  "                ",
                                                                  instruction,
                                                                  argumentCount,
                                                                  calleeFunctionIndex,
                                                                  destinationLayout,
                                                                  "zr_aot_generic_call_typed_method");
        fprintf(file, "            } else {\n");
        backend_aot_write_c_value_call_typed_inline_struct_metadata_deopt(file,
                                                                          "                ",
                                                                          instruction,
                                                                          argumentCount,
                                                                          destinationLayout);
        fprintf(file, "            }\n");
        fprintf(file,
                "        } else {\n"
                "            /* zr_aot_generic_call_typed_missing_instance_deopt deopt=%u */\n"
                "            ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,\n"
                "                                                                                     &frame,\n"
                "                                                                                     %u,\n"
                "                                                                                     %u,\n"
                "                                                                                     %u,\n"
                "                                                                                     %u,\n"
                "                                                                                     %u,\n"
                "                                                                                     %u,\n"
                "                                                                                     %u,\n"
                "                                                                                     \"generic call typed missing AOT instance\"));\n"
                "        }\n"
                "    }\n",
                (unsigned)instruction->deoptId,
                (unsigned)instruction->destinationSlot,
                (unsigned)instruction->operand0,
                (unsigned)argumentCount,
                (unsigned)destinationLayout->typeLayoutId,
                (unsigned)destinationLayout->byteOffset,
                (unsigned)destinationLayout->byteSize,
                (unsigned)instruction->deoptId);
        return ZR_TRUE;
    }

    {
        char calleeThunkExpression[32];

        snprintf(calleeThunkExpression,
                 sizeof(calleeThunkExpression),
                 "zr_aot_fn_%u",
                 (unsigned)calleeFunctionIndex);
        if (requireFullAot) {
            fprintf(file,
                    "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
                    "    /* zr_aot_value_exec_call_typed_inline_struct_full_aot_direct */\n",
                    (unsigned)instruction->destinationSlot,
                    (unsigned)instruction->operand0,
                    (unsigned)argumentCount,
                    (unsigned)calleeFunctionIndex);
            backend_aot_write_c_value_call_typed_inline_struct_direct(file,
                                                                      "    ",
                                                                      instruction,
                                                                      argumentCount,
                                                                      calleeFunctionIndex,
                                                                      destinationLayout,
                                                                      calleeThunkExpression);
            return ZR_TRUE;
        }

        fprintf(file,
                "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
                "    {\n"
                "        /* PostCall routes the callee inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n",
                (unsigned)instruction->destinationSlot,
                (unsigned)instruction->operand0,
                (unsigned)argumentCount,
                (unsigned)calleeFunctionIndex);
        backend_aot_write_c_value_call_typed_metadata_guard(file, "        ", calleeFunctionIndex);
        backend_aot_write_c_value_call_typed_inline_struct_direct(file,
                                                                  "            ",
                                                                  instruction,
                                                                  argumentCount,
                                                                  calleeFunctionIndex,
                                                                  destinationLayout,
                                                                  calleeThunkExpression);
        fprintf(file, "        } else {\n");
        backend_aot_write_c_value_call_typed_inline_struct_metadata_deopt(file,
                                                                          "            ",
                                                                          instruction,
                                                                          argumentCount,
                                                                          destinationLayout);
        fprintf(file, "        }\n"
                      "    }\n");
    }
    return ZR_TRUE;
}

TZrBool backend_aot_try_write_c_value_semir_return_typed_exec(
        FILE *file,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrInstruction *instruction,
        TZrBool allowTypedReturn) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout;
    TZrUInt32 directInlineReturnTypeLayoutId;

    if (file == ZR_NULL || functionIr == ZR_NULL || instruction == ZR_NULL ||
        !allowTypedReturn) {
        return ZR_FALSE;
    }

    directInlineReturnTypeLayoutId =
            backend_aot_exec_ir_direct_inline_return_type_layout_id(functionIr);
    sourceLayout = backend_aot_c_value_call_find_frame_slot_layout(
            &functionIr->frameLayout, instruction->operand0);
    if (!backend_aot_c_value_call_layout_can_inline_struct(sourceLayout) ||
        (sourceLayout->reserved0 &
         ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS) != 0u ||
        directInlineReturnTypeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        sourceLayout->typeLayoutId != directInlineReturnTypeLayoutId) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_return_typed sourceSlot=%u source.offset=%u source.size=%u source.typeLayoutId=%u */\n"
            "    {\n"
            "        /* PostCall routes this inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ReturnInlineStruct(state,\n"
            "                                                                 &frame,\n"
            "                                                                 %u,\n"
            "                                                                 %u,\n"
            "                                                                 %u,\n"
            "                                                                 %u,\n"
            "                                                                 &zr_aot_skip_drop_slot));\n"
            "        ZR_AOT_C_RETURN(1);\n"
            "    }\n",
            (unsigned)instruction->operand0,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)sourceLayout->byteSize,
            (unsigned)sourceLayout->typeLayoutId,
            (unsigned)instruction->operand0,
            (unsigned)sourceLayout->typeLayoutId,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)sourceLayout->byteSize);
    return ZR_TRUE;
}
