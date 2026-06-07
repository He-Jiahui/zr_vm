#include "backend_aot_c_value_semir_fields.h"

#include "backend_aot_internal.h"

static const SZrAotExecIrFrameSlotLayout *backend_aot_c_value_field_find_frame_slot_layout(
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

static TZrBool backend_aot_c_value_field_callsite_cache_kind_matches(
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

static struct SZrString *backend_aot_c_value_field_resolve_stable_member_name(
        const SZrFunction *function,
        TZrUInt32 memberEntryIndex) {
    if (function == ZR_NULL ||
        function->memberEntries == ZR_NULL ||
        memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return function->memberEntries[memberEntryIndex].symbol;
}

static struct SZrString *backend_aot_c_value_field_resolve_cache_member_name(
        const SZrFunction *function,
        TZrUInt32 cacheIndex,
        TZrUInt32 execInstructionIndex,
        EZrFunctionCallSiteCacheKind expectedKind) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;

    if (function == ZR_NULL ||
        function->callSiteCaches == ZR_NULL ||
        cacheIndex >= function->callSiteCacheLength) {
        return ZR_NULL;
    }

    cacheEntry = &function->callSiteCaches[cacheIndex];
    if (cacheEntry->instructionIndex != execInstructionIndex ||
        !backend_aot_c_value_field_callsite_cache_kind_matches(cacheEntry, expectedKind)) {
        return ZR_NULL;
    }

    return backend_aot_c_value_field_resolve_stable_member_name(function, cacheEntry->memberEntryIndex);
}

static struct SZrString *backend_aot_c_value_field_resolve_member_name(
        const SZrFunction *function,
        TZrUInt32 memberOrCacheIndex,
        TZrUInt32 execInstructionIndex,
        EZrFunctionCallSiteCacheKind expectedKind) {
    struct SZrString *memberName;

    /* SemIR field operands are stable member-entry indexes, not callsite-cache indexes. */
    memberName = backend_aot_c_value_field_resolve_stable_member_name(function, memberOrCacheIndex);
    if (memberName != ZR_NULL) {
        return memberName;
    }

    return backend_aot_c_value_field_resolve_cache_member_name(function,
                                                              memberOrCacheIndex,
                                                              execInstructionIndex,
                                                              expectedKind);
}

static TZrBool backend_aot_c_value_field_resolve_layout(
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *baseLayout,
        TZrUInt32 execInstructionIndex,
        TZrUInt32 memberOrCacheIndex,
        EZrFunctionCallSiteCacheKind expectedKind,
        SZrFunctionFrameFieldLayout *outFieldLayout) {
    const SZrFunction *sourceFunction;
    const SZrFunction *layoutFunction;
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
    layoutFunction = functionIr->metadataEntryFunction != ZR_NULL
            ? functionIr->metadataEntryFunction
            : sourceFunction;
    memberName = backend_aot_c_value_field_resolve_member_name(sourceFunction,
                                                               memberOrCacheIndex,
                                                               execInstructionIndex,
                                                               expectedKind);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
                                                          layoutFunction,
                                                          baseLayout->typeLayoutId,
                                                          memberName,
                                                          outFieldLayout)) {
        return ZR_FALSE;
    }

    return (TZrBool)(outFieldLayout->byteOffset <= baseLayout->byteSize &&
                     outFieldLayout->byteSize <= baseLayout->byteSize - outFieldLayout->byteOffset);
}

static void backend_aot_write_c_value_field_slot_layout(FILE *file,
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

void backend_aot_write_c_value_semir_field_addr(FILE *file,
                                                SZrState *state,
                                                const SZrAotExecIrFunction *functionIr,
                                                const SZrAotExecIrFrameLayout *frameLayout,
                                                const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *baseLayout =
            backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->operand0);
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrBool hasFieldLayout = backend_aot_c_value_field_resolve_layout(state,
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
    backend_aot_write_c_value_field_slot_layout(file, "base", baseLayout);
    backend_aot_write_c_value_field_slot_layout(file, "dst", destinationLayout);
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

void backend_aot_write_c_value_semir_load(FILE *file,
                                          SZrState *state,
                                          const SZrAotExecIrFunction *functionIr,
                                          const SZrAotExecIrFrameLayout *frameLayout,
                                          const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->operand0);
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrBool hasFieldLayout = backend_aot_c_value_field_resolve_layout(state,
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
    backend_aot_write_c_value_field_slot_layout(file, "src", sourceLayout);
    backend_aot_write_c_value_field_slot_layout(file, "dst", destinationLayout);
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

void backend_aot_write_c_value_semir_store(FILE *file,
                                           SZrState *state,
                                           const SZrAotExecIrFunction *functionIr,
                                           const SZrAotExecIrFrameLayout *frameLayout,
                                           const SZrAotExecIrInstruction *instruction) {
    const SZrAotExecIrFrameSlotLayout *destinationLayout =
            backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    const SZrAotExecIrFrameSlotLayout *sourceLayout =
            backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->operand0);
    SZrFunctionFrameFieldLayout fieldLayout;
    TZrBool hasFieldLayout = backend_aot_c_value_field_resolve_layout(state,
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
    backend_aot_write_c_value_field_slot_layout(file, "dst", destinationLayout);
    backend_aot_write_c_value_field_slot_layout(file, "src", sourceLayout);
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

static TZrUInt32 backend_aot_c_value_field_primitive_size(EZrValueType valueType) {
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

static const char *backend_aot_c_value_field_primitive_c_type(EZrValueType valueType) {
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

    primitiveSize = backend_aot_c_value_field_primitive_size(fieldLayout->valueType);
    return (TZrBool)(primitiveSize > 0u && fieldLayout->byteSize == primitiveSize);
}

static TZrBool backend_aot_c_value_field_layout_can_value_slot_exec(
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    return (TZrBool)(fieldLayout != ZR_NULL &&
                     fieldLayout->isValueSlot &&
                     fieldLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue));
}

static TZrBool backend_aot_c_value_field_layout_can_value_slot_destination_exec(
        const SZrFunctionFrameFieldLayout *fieldLayout,
        const SZrAotExecIrFrameSlotLayout *destinationLayout) {
    return (TZrBool)(backend_aot_c_value_field_layout_can_value_slot_exec(fieldLayout) &&
                     destinationLayout != ZR_NULL &&
                     destinationLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue));
}

static TZrBool backend_aot_c_value_field_layout_can_value_slot_source_exec(
        const SZrFunctionFrameFieldLayout *fieldLayout,
        const SZrAotExecIrFrameSlotLayout *sourceLayout) {
    return (TZrBool)(backend_aot_c_value_field_layout_can_value_slot_exec(fieldLayout) &&
                     sourceLayout != ZR_NULL &&
                     sourceLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue));
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
            "    {\n"
            "        const SZrTypeLayout *zr_aot_field_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        if (zr_aot_field_layout == ZR_NULL || zr_aot_field_layout->byteSize != %u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)) {\n"
            "            memmove((TZrByte *)frame.slotBase + %u, (const TZrByte *)frame.slotBase + %u + %u, %u);\n"
            "        } else {\n"
            "            /* zr_aot_value_exec_field_inline_struct_copy dstSlot=%u sourceSlot=%u */\n"
            "            ZR_AOT_C_GUARD(ZrCore_TypeLayout_CopyInline(state,\n"
            "                                                        zr_aot_field_layout,\n"
            "                                                        (TZrByte *)frame.slotBase + %u,\n"
            "                                                        (const TZrByte *)frame.slotBase + %u + %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->typeLayoutId,
            (unsigned)fieldLayout->typeLayoutId,
            (unsigned)fieldLayout->byteSize,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset);
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
            "    {\n"
            "        const SZrTypeLayout *zr_aot_field_layout =\n"
            "                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
            "        if (zr_aot_field_layout == ZR_NULL || zr_aot_field_layout->byteSize != %u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)) {\n"
            "            memmove((TZrByte *)frame.slotBase + %u + %u, (const TZrByte *)frame.slotBase + %u, %u);\n"
            "        } else {\n"
            "            /* zr_aot_value_exec_field_inline_struct_copy dstSlot=%u sourceSlot=%u */\n"
            "            ZR_AOT_C_GUARD(ZrCore_TypeLayout_CopyInline(state,\n"
            "                                                        zr_aot_field_layout,\n"
            "                                                        (TZrByte *)frame.slotBase + %u + %u,\n"
            "                                                        (const TZrByte *)frame.slotBase + %u));\n"
            "        }\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)fieldLayout->typeLayoutId,
            (unsigned)fieldLayout->typeLayoutId,
            (unsigned)fieldLayout->byteSize,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_value_slot_load_exec(
        FILE *file,
        const SZrAotExecIrFrameSlotLayout *sourceLayout,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || sourceLayout == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL ||
        !backend_aot_c_value_field_layout_can_value_slot_destination_exec(fieldLayout, destinationLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_value_slot_load dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u */\n"
            "    {\n"
            "        const TZrByte *zr_aot_field = (const TZrByte *)frame.slotBase + %u + %u;\n"
            "        SZrTypeValue *zr_aot_destination = (SZrTypeValue *)((TZrByte *)frame.slotBase + %u);\n"
            "        ZrCore_Value_Copy(state, zr_aot_destination, (const SZrTypeValue *)zr_aot_field);\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)destinationLayout->byteOffset);
    return ZR_TRUE;
}

static TZrBool backend_aot_try_write_c_value_field_value_slot_store_exec(
        FILE *file,
        const SZrAotExecIrFrameSlotLayout *destinationLayout,
        const SZrAotExecIrFrameSlotLayout *sourceLayout,
        const SZrAotExecIrInstruction *instruction,
        const SZrFunctionFrameFieldLayout *fieldLayout) {
    if (file == ZR_NULL || destinationLayout == ZR_NULL || instruction == ZR_NULL || fieldLayout == ZR_NULL ||
        !backend_aot_c_value_field_layout_can_value_slot_source_exec(fieldLayout, sourceLayout)) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_value_slot_store dstSlot=%u sourceSlot=%u"
            " fieldOffset=%u fieldSize=%u */\n"
            "    {\n"
            "        TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;\n"
            "        const SZrTypeValue *zr_aot_source = (const SZrTypeValue *)((const TZrByte *)frame.slotBase + %u);\n"
            "        ZrCore_Value_Copy(state, (SZrTypeValue *)zr_aot_field, zr_aot_source);\n"
            "    }\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)fieldLayout->byteSize,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout->byteOffset,
            (unsigned)sourceLayout->byteOffset);
    return ZR_TRUE;
}

TZrBool backend_aot_try_write_c_value_semir_field_load_exec(
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

    sourceLayout = backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->operand0);
    destinationLayout = backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    if (!backend_aot_c_value_field_resolve_layout(state,
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

    if (backend_aot_try_write_c_value_field_value_slot_load_exec(
                file, sourceLayout, destinationLayout, instruction, &fieldLayout)) {
        return ZR_TRUE;
    }

    fieldTypeName = backend_aot_c_value_field_primitive_c_type(fieldLayout.valueType);
    if (fieldTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_load dstSlot=%u sourceSlot=%u fieldOffset=%u fieldSize=%u valueType=%u */\n"
            "    {\n"
            "        const TZrByte *zr_aot_field = (const TZrByte *)frame.slotBase + %u + %u;\n"
            "        SZrTypeValue *zr_aot_destination = (SZrTypeValue *)((TZrByte *)frame.slotBase + %u);\n"
            "        %s zr_aot_field_value;\n"
            "        memcpy(&zr_aot_field_value, zr_aot_field, sizeof(zr_aot_field_value));\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)fieldLayout.byteSize,
            (unsigned)fieldLayout.valueType,
            (unsigned)sourceLayout->byteOffset,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)destinationLayout->byteOffset,
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

TZrBool backend_aot_try_write_c_value_semir_field_store_exec(
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

    destinationLayout = backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->destinationSlot);
    sourceLayout = backend_aot_c_value_field_find_frame_slot_layout(frameLayout, instruction->operand0);
    if (!backend_aot_c_value_field_resolve_layout(state,
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

    if (backend_aot_try_write_c_value_field_value_slot_store_exec(
                file, destinationLayout, sourceLayout, instruction, &fieldLayout)) {
        return ZR_TRUE;
    }

    fieldTypeName = backend_aot_c_value_field_primitive_c_type(fieldLayout.valueType);
    if (fieldTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    fprintf(file,
            "    /* zr_aot_value_exec_field_store dstSlot=%u sourceSlot=%u fieldOffset=%u fieldSize=%u valueType=%u */\n"
            "    {\n"
            "        TZrByte *zr_aot_field = (TZrByte *)frame.slotBase + %u + %u;\n"
            "        const SZrTypeValue *zr_aot_source = (const SZrTypeValue *)((const TZrByte *)frame.slotBase + %u);\n"
            "        %s zr_aot_stored_value;\n",
            (unsigned)instruction->destinationSlot,
            (unsigned)instruction->operand0,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)fieldLayout.byteSize,
            (unsigned)fieldLayout.valueType,
            (unsigned)destinationLayout->byteOffset,
            (unsigned)fieldLayout.byteOffset,
            (unsigned)sourceLayout->byteOffset,
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
