#include "backend_aot_c_value_semir.h"

#include "backend_aot_c_value_semir_calls.h"
#include "backend_aot_c_value_semir_fields.h"
#include "backend_aot_internal.h"

static const SZrAotExecIrFrameSlotLayout *backend_aot_c_find_frame_slot_layout(
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

static void backend_aot_write_c_value_slot_layout(FILE *file,
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

static void backend_aot_write_c_value_copy(FILE *file,
                                           const SZrAotExecIrFrameLayout *frameLayout,
                                           const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);

    fprintf(file,
            "    /* zr_aot_value_copy dstSlot=%u sourceSlot=%u",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0);
    backend_aot_write_c_value_slot_layout(file, "dst", destinationLayout);
    backend_aot_write_c_value_slot_layout(file, "src", sourceLayout);
    if (destinationLayout != ZR_NULL && sourceLayout != ZR_NULL &&
        destinationLayout->byteSize == sourceLayout->byteSize) {
        fprintf(file,
                " zr_aot_value_expr_inline_copy expr=memmove((TZrByte *)frame.slotBase + %u,"
                " (const TZrByte *)frame.slotBase + %u, %u)",
                (unsigned)destinationLayout->byteOffset,
                (unsigned)sourceLayout->byteOffset,
                (unsigned)destinationLayout->byteSize);
    }
    fprintf(file, " */\n");
}

static TZrBool backend_aot_c_value_layouts_can_inline_copy(
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrFrameSlotLayout *sourceLayout) {
    return (TZrBool)(destinationLayout != ZR_NULL &&
                     sourceLayout != ZR_NULL &&
                     destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     destinationLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     destinationLayout->typeLayoutId == sourceLayout->typeLayoutId &&
                     destinationLayout->byteSize > 0u &&
                     destinationLayout->byteSize == sourceLayout->byteSize);
}

static TZrBool backend_aot_try_write_c_value_copy_exec(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout;
    const SZrAotExecIrFrameSlotLayout *sourceLayout;

    if (file == ZR_NULL || frameLayout == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    sourceLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    if (!backend_aot_c_value_layouts_can_inline_copy(destinationLayout, sourceLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_inline_copy dstSlot=%u sourceSlot=%u */\n"
            "    {\n"
            "        const SZrTypeLayout *zr_aot_copy_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        if (zr_aot_copy_layout == ZR_NULL || zr_aot_copy_layout->byteSize != %u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZrCore_TypeLayout_CanRawCopy(zr_aot_copy_layout)) {\n"
            "            memmove((TZrByte *)frame.slotBase + %u, (const TZrByte *)frame.slotBase + %u, %u);\n"
            "        } else {\n"
            "            /* zr_aot_value_exec_inline_field_copy dstSlot=%u sourceSlot=%u */\n"
            "            ZR_AOT_C_GUARD(ZrCore_TypeLayout_CopyInline(state,\n"
            "                                                        zr_aot_copy_layout,\n"
            "                                                        (TZrByte *)frame.slotBase + %u,\n"
            "                                                        (const TZrByte *)frame.slotBase + %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)destinationLayout->typeLayoutId,
            (unsigned)destinationLayout->byteSize,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)destinationLayout->byteSize,
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset);
    return ZR_TRUE;
}

void backend_aot_write_c_value_semir_for_function(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotExecIrModule *module,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  const SZrAotExecIrFrameLayout *frameLayout) {
    TZrUInt32 instructionIndex;

    if (file == ZR_NULL || module == ZR_NULL || functionIr == ZR_NULL || frameLayout == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    /* value SemIR lowering frameByteSize=%u frameByteAlign=%u slotLayouts=%u */\n",
            (unsigned)frameLayout->frameByteSize,
            (unsigned)frameLayout->frameByteAlign,
            (unsigned)frameLayout->slotLayoutCount);

    for (instructionIndex = 0; instructionIndex < functionIr->instructionCount; instructionIndex++) {
        TZrUInt32 moduleInstructionIndex = functionIr->firstInstructionOffset + instructionIndex;
        const SZrAotExecIrInstruction *instruction;

        if (moduleInstructionIndex >= module->instructionCount) {
            break;
        }

        instruction = &module->instructions[moduleInstructionIndex];

        switch ((EZrSemIrOpcode)instruction->semIrOpcode) {
            case ZR_SEMIR_OPCODE_FIELD_ADDR:
                backend_aot_write_c_value_semir_field_addr(file, state, functionIr, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_LOAD_VALUE:
                backend_aot_write_c_value_semir_load(file, state, functionIr, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_STORE_VALUE:
                backend_aot_write_c_value_semir_store(file, state, functionIr, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_COPY_VALUE:
                backend_aot_write_c_value_copy(file, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_CALL_TYPED:
                backend_aot_write_c_value_semir_call_typed(file, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_RETURN_TYPED:
                backend_aot_write_c_value_semir_return_typed(file, frameLayout, instruction);
                break;
            default:
                break;
        }
    }
}

TZrBool backend_aot_try_write_c_value_semir_for_exec_instruction(FILE *file,
                                                                 SZrState *state,
                                                                 const SZrAotExecIrModule *module,
                                                                 const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 execInstructionIndex,
                                                                 TZrUInt32 calleeFunctionIndex,
                                                                 TZrBool allowTypedReturn) {
    TZrUInt32 instructionIndex;

    if (file == ZR_NULL || module == ZR_NULL || functionIr == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0; instructionIndex < functionIr->instructionCount; instructionIndex++) {
        TZrUInt32 moduleInstructionIndex = functionIr->firstInstructionOffset + instructionIndex;
        const SZrAotExecIrInstruction *instruction;

        if (moduleInstructionIndex >= module->instructionCount) {
            break;
        }

        instruction = &module->instructions[moduleInstructionIndex];
        if (instruction->execInstructionIndex != execInstructionIndex) {
            continue;
        }

        switch ((EZrSemIrOpcode)instruction->semIrOpcode) {
            case ZR_SEMIR_OPCODE_LOAD_VALUE:
                if (backend_aot_try_write_c_value_semir_field_load_exec(
                            file, state, functionIr, &functionIr->frameLayout, instruction)) {
                    return ZR_TRUE;
                }
                break;
            case ZR_SEMIR_OPCODE_STORE_VALUE:
                if (backend_aot_try_write_c_value_semir_field_store_exec(
                            file, state, functionIr, &functionIr->frameLayout, instruction)) {
                    return ZR_TRUE;
                }
                break;
            case ZR_SEMIR_OPCODE_COPY_VALUE:
                if (backend_aot_try_write_c_value_copy_exec(file, &functionIr->frameLayout, instruction)) {
                    return ZR_TRUE;
                }
                break;
            case ZR_SEMIR_OPCODE_CALL_TYPED:
                if (backend_aot_try_write_c_value_semir_call_typed_exec(
                            file, &functionIr->frameLayout, instruction, calleeFunctionIndex)) {
                    return ZR_TRUE;
                }
                break;
            case ZR_SEMIR_OPCODE_RETURN_TYPED:
                if (backend_aot_try_write_c_value_semir_return_typed_exec(
                            file, &functionIr->frameLayout, instruction, allowTypedReturn)) {
                    return ZR_TRUE;
                }
                break;
            default:
                break;
        }
    }

    return ZR_FALSE;
}
