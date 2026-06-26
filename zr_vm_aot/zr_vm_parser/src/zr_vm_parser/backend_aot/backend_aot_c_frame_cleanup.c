#include "backend_aot_c_frame_cleanup.h"

#include "zr_vm_core/function.h"

static TZrBool backend_aot_c_frame_cleanup_layout_needs_drop(
        const SZrAotExecIrFrameSlotLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     layout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     layout->byteSize > 0u);
}

TZrBool backend_aot_c_frame_cleanup_would_emit(const SZrAotExecIrFrameLayout *frameLayout) {
    TZrUInt32 layoutIndex;

    if (frameLayout == ZR_NULL || frameLayout->slotLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (layoutIndex = 0u; layoutIndex < frameLayout->slotLayoutCount; layoutIndex++) {
        if (backend_aot_c_frame_cleanup_layout_needs_drop(&frameLayout->slotLayouts[layoutIndex])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void backend_aot_write_c_frame_root_cleanup(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    if (zr_aot_has_gc_root_frame) {\n"
            "        ZrCore_Gc_AotRootFramePop(state, &zr_aot_gc_root_frame);\n"
            "        zr_aot_has_gc_root_frame = ZR_FALSE;\n"
            "    }\n");
}

void backend_aot_write_c_frame_cleanup(FILE *file, const SZrAotExecIrFrameLayout *frameLayout) {
    TZrUInt32 reverseIndex;

    if (file == ZR_NULL || frameLayout == ZR_NULL || frameLayout->slotLayouts == ZR_NULL) {
        return;
    }

    for (reverseIndex = frameLayout->slotLayoutCount; reverseIndex > 0u; reverseIndex--) {
        const SZrAotExecIrFrameSlotLayout *layout = &frameLayout->slotLayouts[reverseIndex - 1u];

        if (!backend_aot_c_frame_cleanup_layout_needs_drop(layout)) {
            continue;
        }

        fprintf(file,
                "        /* zr_aot_value_frame_drop slot=%u offset=%u size=%u typeLayoutId=%u */\n"
                "        if (zr_aot_skip_drop_slot != %u) {\n"
                "            const SZrTypeLayout *zr_aot_drop_layout =\n"
                "                    ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, %u, state);\n"
                "            if (zr_aot_drop_layout != ZR_NULL &&\n"
                "                zr_aot_drop_layout->byteSize == %u &&\n"
                "                zr_aot_drop_layout->dropKind != ZR_TYPE_LAYOUT_DROP_KIND_NONE) {\n"
                "                ZrCore_TypeLayout_DropInline(state,\n"
                "                                             zr_aot_drop_layout,\n"
                "                                             (TZrByte *)frame.slotBase + %u);\n"
                "            }\n"
                "        }\n",
                (unsigned)layout->stackSlot,
                (unsigned)layout->byteOffset,
                (unsigned)layout->byteSize,
                (unsigned)layout->typeLayoutId,
                (unsigned)layout->stackSlot,
                (unsigned)layout->typeLayoutId,
                (unsigned)layout->byteSize,
                (unsigned)layout->byteOffset);
    }
}
