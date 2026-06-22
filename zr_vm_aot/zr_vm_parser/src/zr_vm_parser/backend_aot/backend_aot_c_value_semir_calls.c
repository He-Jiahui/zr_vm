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
    if (instruction->operand0 >= frameLayout->generatedFrameSlotCount ||
        argumentCount > frameLayout->generatedFrameSlotCount - instruction->operand0 - 1u) {
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

    fprintf(file,
            "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
            "    {\n"
            "        /* PostCall routes the callee inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CallInlineStruct(state,\n"
            "                                                               &frame,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               %u,\n"
            "                                                               zr_aot_fn_%u));\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)argumentCount,
            (unsigned)calleeFunctionIndex,
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)argumentCount,
            (unsigned)calleeFunctionIndex,
            (unsigned)destinationLayout->typeLayoutId,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)destinationLayout->byteSize,
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
