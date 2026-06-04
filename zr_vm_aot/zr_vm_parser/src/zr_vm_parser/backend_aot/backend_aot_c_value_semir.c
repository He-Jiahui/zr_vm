#include "backend_aot_c_value_semir.h"

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

static TZrBool backend_aot_c_callsite_cache_kind_matches(
        const SZrFunctionCallSiteCacheEntry *cacheEntry,
        EZrFunctionCallSiteCacheKind expectedKind) {
    if (cacheEntry == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expectedKind == ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE) {
        return (TZrBool)(cacheEntry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET ||
                         cacheEntry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET);
    }
    return (TZrBool)(cacheEntry->kind == (TZrUInt32)expectedKind);
}

static struct SZrString *backend_aot_c_resolve_member_name(
        const SZrFunction *function,
        TZrUInt32 memberOrCacheIndex,
        TZrUInt32 execInstructionIndex,
        EZrFunctionCallSiteCacheKind expectedKind) {
    TZrUInt32 memberEntryIndex = memberOrCacheIndex;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->callSiteCaches != ZR_NULL && memberOrCacheIndex < function->callSiteCacheLength) {
        const SZrFunctionCallSiteCacheEntry *cacheEntry = &function->callSiteCaches[memberOrCacheIndex];

        if (cacheEntry->instructionIndex == execInstructionIndex &&
            backend_aot_c_callsite_cache_kind_matches(cacheEntry, expectedKind)) {
            memberEntryIndex = cacheEntry->memberEntryIndex;
        }
    }

    if (function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return function->memberEntries[memberEntryIndex].symbol;
}

static TZrBool backend_aot_c_resolve_field_layout(
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *baseLayout,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 memberOrCacheIndex,
        EZrFunctionCallSiteCacheKind expectedKind,
        SZrFunctionFrameFieldLayout *outFieldLayout) {
    const SZrFunction *sourceFunction;
    struct SZrString *memberName;

    if (state == ZR_NULL ||
        functionIr == ZR_NULL ||
        baseLayout == ZR_NULL ||
        outFieldLayout == ZR_NULL ||
        baseLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        baseLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    sourceFunction = functionIr->function;
    memberName = backend_aot_c_resolve_member_name(sourceFunction,
                                                   memberOrCacheIndex,
                                                   execInstructionIndex,
                                                   expectedKind);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
                                                          sourceFunction,
                                                          baseLayout->typeLayoutId,
                                                          memberName,
                                                          outFieldLayout)) {
        return ZR_FALSE;
    }

    return (TZrBool)(outFieldLayout->byteOffset <= baseLayout->byteSize &&
                     outFieldLayout->byteSize <= baseLayout->byteSize - outFieldLayout->byteOffset);
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

static void backend_aot_write_c_value_field_addr(FILE *file,
                                                 SZrState *state,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrAotExecIrFrameLayout *frameLayout,
                                                 const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *baseLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrBool hasFieldLayout = backend_aot_c_resolve_field_layout(state,
                                                                functionIr,
                                                                baseLayout,
                                                                instruction->execInstructionIndex,
                                                                instruction->operand1,
                                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE,
                                                                &fieldLayout);

    fprintf(file,
            "    /* zr_aot_value_field_addr dstSlot=%u baseSlot=%u member=%u",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)instruction->operand1);
    backend_aot_write_c_value_slot_layout(file, "base", baseLayout);
    backend_aot_write_c_value_slot_layout(file, "dst", destinationLayout);
    if (hasFieldLayout) {
        fprintf(file,
                " zr_aot_value_expr_field_addr expr=((TZrByte *)frame.slotBase + %u + %u)"
                " field.offset=%u field.size=%u field.typeLayoutId=%u",
                (unsigned)baseLayout->byteOffset,
                (unsigned)fieldLayout.byteOffset,
                (unsigned)fieldLayout.byteOffset,
                (unsigned)fieldLayout.byteSize,
                (unsigned)fieldLayout.typeLayoutId);
    }
    fprintf(file, " */\n");
}

static void backend_aot_write_c_value_load(FILE *file,
                                           SZrState *state,
                                           const SZrAotExecIrFunction *functionIr,
                                           const SZrAotExecIrFrameLayout *frameLayout,
                                           const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrBool hasFieldLayout = backend_aot_c_resolve_field_layout(state,
                                                                functionIr,
                                                                sourceLayout,
                                                                instruction->execInstructionIndex,
                                                                instruction->operand1,
                                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET,
                                                                &fieldLayout);

    fprintf(file,
            "    /* zr_aot_value_load dstSlot=%u sourceSlot=%u",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0);
    backend_aot_write_c_value_slot_layout(file, "src", sourceLayout);
    backend_aot_write_c_value_slot_layout(file, "dst", destinationLayout);
    if (hasFieldLayout && destinationLayout != ZR_NULL) {
        fprintf(file,
                " zr_aot_value_expr_field_load dst=((TZrByte *)frame.slotBase + %u)"
                " src=((const TZrByte *)frame.slotBase + %u + %u)"
                " field.offset=%u field.size=%u field.valueType=%u",
                (unsigned)destinationLayout->byteOffset,
                (unsigned)sourceLayout->byteOffset,
                (unsigned)fieldLayout.byteOffset,
                (unsigned)fieldLayout.byteOffset,
                (unsigned)fieldLayout.byteSize,
                (unsigned)fieldLayout.valueType);
    }
    fprintf(file, " */\n");
}

static void backend_aot_write_c_value_store(FILE *file,
                                            SZrState *state,
                                            const SZrAotExecIrFunction *functionIr,
                                            const SZrAotExecIrFrameLayout *frameLayout,
                                            const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrBool hasFieldLayout = backend_aot_c_resolve_field_layout(state,
                                                                functionIr,
                                                                destinationLayout,
                                                                instruction->execInstructionIndex,
                                                                instruction->operand1,
                                                                ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET,
                                                                &fieldLayout);

    fprintf(file,
            "    /* zr_aot_value_store dstSlot=%u sourceSlot=%u",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0);
    backend_aot_write_c_value_slot_layout(file, "dst", destinationLayout);
    backend_aot_write_c_value_slot_layout(file, "src", sourceLayout);
    if (hasFieldLayout && sourceLayout != ZR_NULL) {
        fprintf(file,
                " zr_aot_value_expr_field_store dst=((TZrByte *)frame.slotBase + %u + %u)"
                " src=((const TZrByte *)frame.slotBase + %u)"
                " field.offset=%u field.size=%u field.valueType=%u",
                (unsigned)destinationLayout->byteOffset,
                (unsigned)fieldLayout.byteOffset,
                (unsigned)sourceLayout->byteOffset,
                (unsigned)fieldLayout.byteOffset,
                (unsigned)fieldLayout.byteSize,
                (unsigned)fieldLayout.valueType);
    }
    fprintf(file, " */\n");
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

static TZrBool backend_aot_c_value_layout_can_inline_struct(
        const SZrAotExecIrFrameSlotLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     layout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     layout->byteSize > 0u);
}

static TZrUInt32 backend_aot_c_value_primitive_size(EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_INT8:
            return (TZrUInt32)sizeof(TZrInt8);
        case ZR_VALUE_TYPE_INT16:
            return (TZrUInt32)sizeof(TZrInt16);
        case ZR_VALUE_TYPE_INT32:
            return (TZrUInt32)sizeof(TZrInt32);
        case ZR_VALUE_TYPE_INT64:
            return (TZrUInt32)sizeof(TZrInt64);
        case ZR_VALUE_TYPE_UINT8:
            return (TZrUInt32)sizeof(TZrUInt8);
        case ZR_VALUE_TYPE_UINT16:
            return (TZrUInt32)sizeof(TZrUInt16);
        case ZR_VALUE_TYPE_UINT32:
            return (TZrUInt32)sizeof(TZrUInt32);
        case ZR_VALUE_TYPE_UINT64:
            return (TZrUInt32)sizeof(TZrUInt64);
        case ZR_VALUE_TYPE_BOOL:
            return (TZrUInt32)sizeof(TZrBool);
        case ZR_VALUE_TYPE_FLOAT:
            return (TZrUInt32)sizeof(TZrFloat32);
        case ZR_VALUE_TYPE_DOUBLE:
            return (TZrUInt32)sizeof(TZrDouble);
        default:
            return 0u;
    }
}

static const char *backend_aot_c_value_primitive_c_type(EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_INT8:
            return "TZrInt8";
        case ZR_VALUE_TYPE_INT16:
            return "TZrInt16";
        case ZR_VALUE_TYPE_INT32:
            return "TZrInt32";
        case ZR_VALUE_TYPE_INT64:
            return "TZrInt64";
        case ZR_VALUE_TYPE_UINT8:
            return "TZrUInt8";
        case ZR_VALUE_TYPE_UINT16:
            return "TZrUInt16";
        case ZR_VALUE_TYPE_UINT32:
            return "TZrUInt32";
        case ZR_VALUE_TYPE_UINT64:
            return "TZrUInt64";
        case ZR_VALUE_TYPE_BOOL:
            return "TZrBool";
        case ZR_VALUE_TYPE_FLOAT:
            return "TZrFloat32";
        case ZR_VALUE_TYPE_DOUBLE:
            return "TZrDouble";
        default:
            return ZR_NULL;
    }
}

static TZrBool backend_aot_c_value_field_layout_can_primitive_exec(
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    TZrUInt32 primitiveSize;

    if (fieldLayout == ZR_NULL || !fieldLayout->isPrimitivePod) {
        return ZR_FALSE;
    }

    primitiveSize = backend_aot_c_value_primitive_size(fieldLayout->valueType);
    return (TZrBool)(primitiveSize > 0u && fieldLayout->byteSize == primitiveSize);
}

static TZrBool backend_aot_c_value_field_layout_can_value_slot_exec(
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    return (TZrBool)(fieldLayout != ZR_NULL &&
                     fieldLayout->isValueSlot &&
                     fieldLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue));
}

static TZrBool backend_aot_c_value_field_layout_can_inline_struct_exec(
        const SZrFunctionFrameFieldLayout *fieldLayout,
        const SZrAotExecIrFrameSlotLayout *inlineLayout) {
    return (TZrBool)(fieldLayout != ZR_NULL &&
                     inlineLayout != ZR_NULL &&
                     inlineLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     fieldLayout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     inlineLayout->typeLayoutId == fieldLayout->typeLayoutId &&
                     fieldLayout->byteSize > 0u &&
                     inlineLayout->byteSize == fieldLayout->byteSize);
}

static TZrBool backend_aot_write_c_value_unsupported_field_load_exec(
        FILE *file,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_unsupported_field_load dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u fieldLayout.isPrimitivePod=%u"
            " fieldLayout.isValueSlot=%u fieldLayout.typeLayoutId=%u */\n"
            "    {\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT value SemIR field load\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->isPrimitivePod,
            (unsigned)fieldLayout->isValueSlot,
            (unsigned)fieldLayout->typeLayoutId);
    return ZR_TRUE;
}

static TZrBool backend_aot_write_c_value_unsupported_field_store_exec(
        FILE *file,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_unsupported_field_store dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u fieldLayout.isPrimitivePod=%u"
            " fieldLayout.isValueSlot=%u fieldLayout.typeLayoutId=%u */\n"
            "    {\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT value SemIR field store\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->isPrimitivePod,
            (unsigned)fieldLayout->isValueSlot,
            (unsigned)fieldLayout->typeLayoutId);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_inline_struct_load_exec(
        FILE *file,
        const SZrAotExecIrFrameSlotLayout *sourceLayout,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || sourceLayout == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL ||
        !backend_aot_c_value_field_layout_can_inline_struct_exec(fieldLayout, destinationLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_inline_struct_load dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u typeLayoutId=%u */\n"
            "    memmove((TZrByte *)frame.slotBase + %u, (const TZrByte *)frame.slotBase + %u + %u, %u);\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->typeLayoutId,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_inline_struct_store_exec(
        FILE *file,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrFrameSlotLayout *sourceLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || destinationLayout == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL ||
        !backend_aot_c_value_field_layout_can_inline_struct_exec(fieldLayout, sourceLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_inline_struct_store dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u typeLayoutId=%u */\n"
            "    memmove((TZrByte *)frame.slotBase + %u + %u, (const TZrByte *)frame.slotBase + %u, %u);\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->typeLayoutId,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteSize);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_value_slot_load_exec(
        FILE *file,
        const SZrAotExecIrFrameSlotLayout *sourceLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || sourceLayout == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL ||
        !backend_aot_c_value_field_layout_can_value_slot_exec(fieldLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_value_slot_load dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u */\n"
            "    {\n"
            "        const TZrByte *zr_aot_field = (const TZrByte *)frame.slotBase + %u + %u;\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        ZrCore_Value_Copy(state, zr_aot_destination, (const SZrTypeValue *)zr_aot_field);\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)instruction->destinationSlot);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_value_slot_store_exec(
        FILE *file,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || destinationLayout == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL ||
        !backend_aot_c_value_field_layout_can_value_slot_exec(fieldLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_value_slot_store dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u */\n"
            "    {\n"
            "        TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        ZrCore_Value_Copy(state, (SZrTypeValue *)zr_aot_field, zr_aot_source);\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)instruction->operand0);
    return ZR_TRUE;
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

static TZrBool backend_aot_try_write_c_value_field_load_exec(
        FILE *file,
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout;
    const SZrAotExecIrFrameSlotLayout *destinationLayout;
    SZrFunctionFrameFieldLayout fieldLayout;
    const char *fieldTypeName;

    if (file == ZR_NULL || state == ZR_NULL || functionIr == ZR_NULL || frameLayout == ZR_NULL ||
        instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    destinationLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    if (!backend_aot_c_resolve_field_layout(state,
                                            functionIr,
                                            sourceLayout,
                                            instruction->execInstructionIndex,
                                            instruction->operand1,
                                            ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET,
                                            &fieldLayout)) {
        return ZR_FALSE;
    }
    if (backend_aot_try_write_c_value_field_inline_struct_load_exec(
                file, sourceLayout, destinationLayout, instruction, &fieldLayout)) {
        return ZR_TRUE;
    }
    if ((destinationLayout != ZR_NULL &&
         destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) ||
        (!backend_aot_c_value_field_layout_can_primitive_exec(&fieldLayout) &&
         !backend_aot_c_value_field_layout_can_value_slot_exec(&fieldLayout))) {
        return backend_aot_write_c_value_unsupported_field_load_exec(file, instruction, &fieldLayout);
    }

    if (sourceLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_try_write_c_value_field_value_slot_load_exec(file, sourceLayout, instruction, &fieldLayout)) {
        return ZR_TRUE;
    }

    fieldTypeName = backend_aot_c_value_primitive_c_type(fieldLayout.valueType);
    if (fieldTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_load dstSlot=%u sourceSlot=%u fieldOffset=%u fieldSize=%u valueType=%u */\n"
            "    {\n"
            "        const TZrByte *zr_aot_field = (const TZrByte *)frame.slotBase + %u + %u;\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        %s zr_aot_field_value;\n"
            "        memcpy(&zr_aot_field_value, zr_aot_field, sizeof(zr_aot_field_value));\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)fieldLayout.byteSize,
            (unsigned)fieldLayout.valueType,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)instruction->destinationSlot,
            fieldTypeName);

    switch (fieldLayout.valueType) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            fprintf(file,
                    "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
                    "                          nativeInt64,\n"
                    "                          (TZrInt64)zr_aot_field_value,\n"
                    "                          ZR_VALUE_TYPE_INT64);\n");
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            fprintf(file,
                    "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
                    "                          nativeUInt64,\n"
                    "                          (TZrUInt64)zr_aot_field_value,\n"
                    "                          ZR_VALUE_TYPE_UINT64);\n");
            break;
        case ZR_VALUE_TYPE_BOOL:
            fprintf(file,
                    "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
                    "                          nativeBool,\n"
                    "                          (TZrBool)(zr_aot_field_value != 0u),\n"
                    "                          ZR_VALUE_TYPE_BOOL);\n");
            break;
        case ZR_VALUE_TYPE_FLOAT:
            fprintf(file,
                    "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
                    "                          nativeDouble,\n"
                    "                          (TZrDouble)zr_aot_field_value,\n"
                    "                          ZR_VALUE_TYPE_DOUBLE);\n");
            break;
        case ZR_VALUE_TYPE_DOUBLE:
            fprintf(file,
                    "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
                    "                          nativeDouble,\n"
                    "                          zr_aot_field_value,\n"
                    "                          ZR_VALUE_TYPE_DOUBLE);\n");
            break;
        default:
            return ZR_FALSE;
    }

    fprintf(file, "    }\n");
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_store_exec(
        FILE *file,
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout;
    const SZrAotExecIrFrameSlotLayout *sourceLayout;
    SZrFunctionFrameFieldLayout fieldLayout;
    const char *fieldTypeName;

    if (file == ZR_NULL || state == ZR_NULL || functionIr == ZR_NULL || frameLayout == ZR_NULL ||
        instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    destinationLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    sourceLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    if (!backend_aot_c_resolve_field_layout(state,
                                            functionIr,
                                            destinationLayout,
                                            instruction->execInstructionIndex,
                                            instruction->operand1,
                                            ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET,
                                            &fieldLayout)) {
        return ZR_FALSE;
    }
    if (backend_aot_try_write_c_value_field_inline_struct_store_exec(
                file, destinationLayout, sourceLayout, instruction, &fieldLayout)) {
        return ZR_TRUE;
    }
    if ((sourceLayout != ZR_NULL &&
         sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) ||
        (!backend_aot_c_value_field_layout_can_primitive_exec(&fieldLayout) &&
         !backend_aot_c_value_field_layout_can_value_slot_exec(&fieldLayout))) {
        return backend_aot_write_c_value_unsupported_field_store_exec(file, instruction, &fieldLayout);
    }

    if (destinationLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    if (backend_aot_try_write_c_value_field_value_slot_store_exec(file, destinationLayout, instruction, &fieldLayout)) {
        return ZR_TRUE;
    }

    fieldTypeName = backend_aot_c_value_primitive_c_type(fieldLayout.valueType);
    if (fieldTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_store dstSlot=%u sourceSlot=%u fieldOffset=%u fieldSize=%u valueType=%u */\n"
            "    {\n"
            "        TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        %s zr_aot_stored_value;\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)fieldLayout.byteSize,
            (unsigned)fieldLayout.valueType,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)instruction->operand0,
            fieldTypeName);

    switch (fieldLayout.valueType) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            fprintf(file,
                    "        if (!ZR_VALUE_IS_TYPE_INT(zr_aot_source->type)) {\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n"
                    "        zr_aot_stored_value = (%s)(ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)\n"
                    "                ? (TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64\n"
                    "                : zr_aot_source->value.nativeObject.nativeInt64);\n",
                    fieldTypeName);
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            fprintf(file,
                    "        if (!ZR_VALUE_IS_TYPE_INT(zr_aot_source->type)) {\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n"
                    "        zr_aot_stored_value = (%s)(ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)\n"
                    "                ? (TZrUInt64)zr_aot_source->value.nativeObject.nativeInt64\n"
                    "                : zr_aot_source->value.nativeObject.nativeUInt64);\n",
                    fieldTypeName);
            break;
        case ZR_VALUE_TYPE_BOOL:
            fprintf(file,
                    "        if (zr_aot_source->type != ZR_VALUE_TYPE_BOOL) {\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n"
                    "        zr_aot_stored_value = (TZrBool)(zr_aot_source->value.nativeObject.nativeBool != 0u);\n");
            break;
        case ZR_VALUE_TYPE_FLOAT:
            fprintf(file,
                    "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n"
                    "        zr_aot_stored_value = (TZrFloat32)zr_aot_source->value.nativeObject.nativeDouble;\n");
            break;
        case ZR_VALUE_TYPE_DOUBLE:
            fprintf(file,
                    "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
                    "            ZR_AOT_C_FAIL();\n"
                    "        }\n"
                    "        zr_aot_stored_value = zr_aot_source->value.nativeObject.nativeDouble;\n");
            break;
        default:
            return ZR_FALSE;
    }

    fprintf(file,
            "        memcpy(zr_aot_field, &zr_aot_stored_value, sizeof(zr_aot_stored_value));\n"
            "    }\n");
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_call_typed_exec(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction,
        TZrUInt32 calleeFunctionIndex) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout;

    if (file == ZR_NULL || frameLayout == ZR_NULL || instruction == ZR_NULL ||
        calleeFunctionIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return ZR_FALSE;
    }

    destinationLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    if (!backend_aot_c_value_layout_can_inline_struct(destinationLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_call_typed dstSlot=%u calleeSlot=%u argCount=%u callee=%u */\n"
            "    {\n"
            "        const SZrTypeLayout *zr_aot_return_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        ZrAotGeneratedDirectCall zr_aot_direct_call;\n"
            "        if (zr_aot_return_layout == ZR_NULL ||\n"
            "            zr_aot_return_layout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||\n"
            "            zr_aot_return_layout->byteSize != %u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,\n"
            "                                                                  &frame,\n"
            "                                                                  %u,\n"
            "                                                                  %u,\n"
            "                                                                  %u,\n"
            "                                                                  %u,\n"
            "                                                                  &zr_aot_direct_call));\n"
            "        ZR_AOT_C_GUARD(zr_aot_fn_%u(state));\n"
            "        /* FinishDirectCall routes the callee inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n"
            "        ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1));\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)instruction->operand1,
            (unsigned)calleeFunctionIndex,
            (unsigned)destinationLayout->typeLayoutId,
            (unsigned)destinationLayout->byteSize,
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)instruction->operand1,
            (unsigned)calleeFunctionIndex,
            (unsigned)calleeFunctionIndex);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_return_typed_exec(
        FILE *file,
        const SZrAotExecIrFrameLayout *frameLayout,
        const SZrAotExecIrInstruction *instruction,
        TZrBool allowTypedReturn) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout;

    if (file == ZR_NULL || frameLayout == ZR_NULL || instruction == ZR_NULL || !allowTypedReturn) {
        return ZR_FALSE;
    }

    sourceLayout = backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);
    if (!backend_aot_c_value_layout_can_inline_struct(sourceLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_return_typed sourceSlot=%u source.offset=%u source.size=%u source.typeLayoutId=%u */\n"
            "    {\n"
            "        const SZrTypeLayout *zr_aot_return_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        SZrCallInfo *zr_aot_call_info = frame.callInfo;\n"
            "        TZrStackValuePointer zr_aot_return_source = frame.slotBase + %u;\n"
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
            "        /* FinishDirectCall routes this inline source through ZrCore_Function_TryCopyInlineFrameReturnValue(state, ...). */\n"
            "        state->stackTop.valuePointer = zr_aot_return_source + 1;\n"
            "        zr_aot_skip_drop_slot = %u;\n"
            "        ZR_AOT_C_RETURN(1);\n"
            "    }\n",
            (unsigned)instruction->operand0,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)sourceLayout->byteSize,
            (unsigned)sourceLayout->typeLayoutId,
            (unsigned)sourceLayout->typeLayoutId,
            (unsigned)sourceLayout->byteSize,
            (unsigned)instruction->operand0,
            (unsigned)instruction->operand0);
    return ZR_TRUE;
}

static void backend_aot_write_c_value_call_typed(FILE *file,
                                                 const SZrAotExecIrFrameLayout *frameLayout,
                                                 const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->destinationSlot);

    fprintf(file,
            "    /* zr_aot_value_call_typed dstSlot=%u calleeSlot=%u argCount=%u type=%u",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)instruction->operand1,
            (unsigned)instruction->typeTableIndex);
    backend_aot_write_c_value_slot_layout(file, "dst", destinationLayout);
    fprintf(file, " */\n");
}

static void backend_aot_write_c_value_return_typed(FILE *file,
                                                   const SZrAotExecIrFrameLayout *frameLayout,
                                                   const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_find_frame_slot_layout(frameLayout, instruction->operand0);

    fprintf(file,
            "    /* zr_aot_value_return_typed sourceSlot=%u type=%u",
            (unsigned)instruction->operand0,
            (unsigned)instruction->typeTableIndex);
    backend_aot_write_c_value_slot_layout(file, "src", sourceLayout);
    fprintf(file, " */\n");
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
                backend_aot_write_c_value_field_addr(file, state, functionIr, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_LOAD_VALUE:
                backend_aot_write_c_value_load(file, state, functionIr, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_STORE_VALUE:
                backend_aot_write_c_value_store(file, state, functionIr, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_COPY_VALUE:
                backend_aot_write_c_value_copy(file, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_CALL_TYPED:
                backend_aot_write_c_value_call_typed(file, frameLayout, instruction);
                break;
            case ZR_SEMIR_OPCODE_RETURN_TYPED:
                backend_aot_write_c_value_return_typed(file, frameLayout, instruction);
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
                if (backend_aot_try_write_c_value_field_load_exec(
                            file, state, functionIr, &functionIr->frameLayout, instruction)) {
                    return ZR_TRUE;
                }
                break;
            case ZR_SEMIR_OPCODE_STORE_VALUE:
                if (backend_aot_try_write_c_value_field_store_exec(
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
                if (backend_aot_try_write_c_value_call_typed_exec(
                            file, &functionIr->frameLayout, instruction, calleeFunctionIndex)) {
                    return ZR_TRUE;
                }
                break;
            case ZR_SEMIR_OPCODE_RETURN_TYPED:
                if (backend_aot_try_write_c_value_return_typed_exec(
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
