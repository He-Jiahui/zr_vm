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
        TZrUInt32 calleeFunctionIndex) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout;
    TZrUInt32 argumentCount;
    TZrUInt32 argumentIndex;

    if (file == ZR_NULL || frameLayout == ZR_NULL || instruction == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    destinationLayout = backend_aot_c_value_call_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    if (!backend_aot_c_value_call_layout_can_inline_struct(destinationLayout)) {
        return ZR_FALSE;
    }
    argumentCount = instruction->operand1;

    fprintf(file,
            "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
            "    {\n"
            "        const SZrTypeLayout *zr_aot_return_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        TZrStackValuePointer zr_aot_call_base;\n"
            "        TZrStackValuePointer zr_aot_destination_pointer;\n"
            "        SZrCallInfo *zr_aot_call_info;\n"
            "        SZrFunction *zr_aot_metadata_function;\n"
            "        SZrTypeValue *zr_aot_callable_value;\n"
            "        TZrUInt32 zr_aot_call_window_index;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            frame.callInfo == ZR_NULL || frame.callInfo != state->callInfoList ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u > frame.generatedFrameSlotCount - %u - 1u ||\n"
            "            zr_aot_return_layout == ZR_NULL ||\n"
            "            zr_aot_return_layout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||\n"
            "            zr_aot_return_layout->byteSize != %u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);\n"
            "        if (zr_aot_call_base == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT typed call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_call_base = ZrCore_Function_CheckStackAndGc(state, 1u + %u, zr_aot_call_base);\n"
            "        if (zr_aot_call_base == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||\n"
            "            state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT typed call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo = state->callInfoList;\n"
            "        frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;\n"
            "        for (zr_aot_call_window_index = 0u; zr_aot_call_window_index < 1u + %u; zr_aot_call_window_index++) {\n"
            "            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(zr_aot_call_base + zr_aot_call_window_index));\n"
            "        }\n"
            "        state->stackTop.valuePointer = zr_aot_call_base + 1 + %u;\n"
            "        if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {\n"
            "            state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;\n"
            "        }\n"
            "        zr_aot_callable_value = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        zr_aot_destination_pointer = (TZrStackValuePointer)((TZrByte *)frame.slotBase + %u);\n"
            "        zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_callable_value);\n"
            "        if (zr_aot_callable_value == ZR_NULL || zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT typed call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(zr_aot_call_base), zr_aot_callable_value);\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)argumentCount,
            (unsigned)calleeFunctionIndex,
            (unsigned)destinationLayout->typeLayoutId,
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)argumentCount,
            (unsigned)instruction->operand0,
            (unsigned)destinationLayout->byteSize,
            (unsigned)argumentCount,
            (unsigned)argumentCount,
            (unsigned)argumentCount,
            (unsigned)instruction->operand0,
            (unsigned)destinationLayout->byteOffset);
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = instruction->operand0 + 1u + argumentIndex;
        const SZrAotExecIrFrameSlotLayout *sourceLayout =
                backend_aot_c_value_call_find_frame_slot_layout(frameLayout, sourceSlot);

        if (sourceLayout == ZR_NULL ||
            sourceLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            sourceLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }

        fprintf(file,
                "        ZrCore_Value_Copy(state,\n"
                "                          ZrCore_Stack_GetValue(zr_aot_call_base + 1u + %u),\n"
                "                          (const SZrTypeValue *)((const TZrByte *)frame.slotBase + %u));\n",
                (unsigned)argumentIndex,
                (unsigned)sourceLayout->byteOffset);
    }
    fprintf(file,
            "        zr_aot_call_info = ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(\n"
            "                state,\n"
            "                zr_aot_call_base,\n"
            "                zr_aot_metadata_function,\n"
            "                %u,\n"
            "                1,\n"
            "                zr_aot_destination_pointer,\n"
            "                frame.slotBase,\n"
            "                %u);\n"
            "        if (zr_aot_call_info == ZR_NULL || state->callInfoList != zr_aot_call_info) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT typed call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        /* PostCall routes the callee inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n"
            "        ZR_AOT_C_GUARD(zr_aot_fn_%u(state));\n"
            "        ZrCore_Function_PostCall(state, zr_aot_call_info, 1);\n"
            "        if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL ||\n"
            "            state->callInfoList->functionBase.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT typed call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo = state->callInfoList;\n"
            "        frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;\n"
            "        zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);\n"
            "        if (zr_aot_call_base == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT typed call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo->functionTop.valuePointer = zr_aot_call_base;\n"
            "        state->stackTop.valuePointer = zr_aot_call_base;\n"
            "    }\n",
            (unsigned)argumentCount,
            (unsigned)(instruction->operand0 + 1u),
            (unsigned)calleeFunctionIndex);
    return ZR_TRUE;
}

TZrBool backend_aot_try_write_c_value_semir_return_typed_exec(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction,
        TZrBool allowTypedReturn) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout;

    if (file == ZR_NULL || frameLayout == ZR_NULL || instruction == ZR_NULL || !allowTypedReturn) {
        return ZR_FALSE;
    }

    sourceLayout = backend_aot_c_value_call_find_frame_slot_layout(frameLayout, instruction->operand0);
    if (!backend_aot_c_value_call_layout_can_inline_struct(sourceLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_return_typed sourceSlot=%u source.offset=%u source.size=%u source.typeLayoutId=%u */\n"
            "    {\n"
            "        const SZrTypeLayout *zr_aot_return_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        TZrStackValuePointer zr_aot_return_source =\n"
            "                (TZrStackValuePointer)((TZrByte *)frame.slotBase + %u);\n"
            "        if (zr_aot_return_layout == ZR_NULL ||\n"
            "            zr_aot_return_layout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||\n"
            "            zr_aot_return_layout->byteSize != %u ||\n"
            "            zr_aot_call_info == ZR_NULL ||\n"
            "            zr_aot_call_info->functionBase.valuePointer == ZR_NULL ||\n"
            "            zr_aot_return_source == ZR_NULL ||\n"
            "            (zr_aot_call_info->functionTop.valuePointer != ZR_NULL &&\n"
            "             zr_aot_return_source >= zr_aot_call_info->functionTop.valuePointer)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        /* PostCall routes this inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n"
            "        state->stackTop.valuePointer = zr_aot_return_source + 1;\n"
            "        zr_aot_skip_drop_slot = %u;\n"
            "        ZR_AOT_C_RETURN(1);\n"
            "    }\n",
            (unsigned)instruction->operand0,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)sourceLayout->byteSize,
            (unsigned)sourceLayout->typeLayoutId,
            (unsigned)sourceLayout->typeLayoutId,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)sourceLayout->byteSize,
            (unsigned)instruction->operand0);
    return ZR_TRUE;
}
