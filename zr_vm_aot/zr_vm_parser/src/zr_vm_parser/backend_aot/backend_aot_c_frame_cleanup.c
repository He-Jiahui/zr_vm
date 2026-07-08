#include "backend_aot_c_frame_cleanup.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/type_layout.h"

static TZrBool backend_aot_c_frame_cleanup_layout_needs_drop(
        const SZrAotExecIrFrameSlotLayout *layout) {
    return (TZrBool)(layout != ZR_NULL &&
                     layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                     layout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
                     layout->byteSize > 0u);
}

static const SZrTypeLayout *backend_aot_c_frame_cleanup_resolve_layout(
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *layout) {
    if (state == ZR_NULL ||
        functionIr == ZR_NULL ||
        functionIr->function == ZR_NULL ||
        layout == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Function_ResolvePrototypeFrameTypeLayout(functionIr->function,
                                                           layout->typeLayoutId,
                                                           state);
}

static TZrBool backend_aot_c_frame_cleanup_layout_needs_drop_for_function(
        SZrState *state,
        const SZrAotExecIrFunction *functionIr,
        const SZrAotExecIrFrameSlotLayout *layout) {
    const SZrTypeLayout *typeLayout;

    if (!backend_aot_c_frame_cleanup_layout_needs_drop(layout)) {
        return ZR_FALSE;
    }

    typeLayout = backend_aot_c_frame_cleanup_resolve_layout(state, functionIr, layout);
    if (typeLayout == ZR_NULL) {
        return ZR_TRUE;
    }

    return (TZrBool)(typeLayout->dropKind != (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE);
}

TZrBool backend_aot_c_frame_cleanup_would_emit_for_function(SZrState *state,
                                                            const SZrAotExecIrFunction *functionIr) {
    TZrUInt32 layoutIndex;
    const SZrAotExecIrFrameLayout *frameLayout;

    if (functionIr == ZR_NULL) {
        return ZR_FALSE;
    }

    frameLayout = &functionIr->frameLayout;
    if (frameLayout->slotLayouts == ZR_NULL) {
        return ZR_FALSE;
    }

    for (layoutIndex = 0u; layoutIndex < frameLayout->slotLayoutCount; layoutIndex++) {
        if (backend_aot_c_frame_cleanup_layout_needs_drop_for_function(state,
                                                                       functionIr,
                                                                       &frameLayout->slotLayouts[layoutIndex])) {
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

void backend_aot_write_c_frame_cleanup(FILE *file,
                                       SZrState *state,
                                       const SZrAotExecIrFunction *functionIr) {
    TZrUInt32 reverseIndex;
    const SZrAotExecIrFrameLayout *frameLayout;

    if (file == ZR_NULL || functionIr == ZR_NULL) {
        return;
    }

    frameLayout = &functionIr->frameLayout;
    if (frameLayout->slotLayouts == ZR_NULL) {
        return;
    }

    for (reverseIndex = frameLayout->slotLayoutCount; reverseIndex > 0u; reverseIndex--) {
        const SZrAotExecIrFrameSlotLayout *layout = &frameLayout->slotLayouts[reverseIndex - 1u];

        if (!backend_aot_c_frame_cleanup_layout_needs_drop_for_function(state, functionIr, layout)) {
            continue;
        }

        fprintf(file,
                "        /* zr_aot_value_frame_drop slot=%u offset=%u size=%u typeLayoutId=%u */\n"
                "        if (zr_aot_skip_drop_slot != %u) {\n"
                "            const SZrTypeLayout *zr_aot_drop_layout =\n"
                "                    ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, %u);\n"
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
