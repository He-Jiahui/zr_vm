#include "backend_aot_c_frame_setup.h"

static TZrUInt32 backend_aot_c_frame_setup_register_frame_bytes(
        const SZrAotExecIrFrameLayout *frameLayout) {
    TZrUInt32 layoutIndex;
    TZrUInt32 registerFrameBytes = 0u;

    if (frameLayout == ZR_NULL || frameLayout->slotLayouts == ZR_NULL) {
        return 0u;
    }

    for (layoutIndex = 0u; layoutIndex < frameLayout->slotLayoutCount; layoutIndex++) {
        const SZrAotExecIrFrameSlotLayout *layout = &frameLayout->slotLayouts[layoutIndex];
        TZrUInt32 slotEnd;

        if (layout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            layout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
            layout->byteSize == 0u) {
            continue;
        }

        slotEnd = layout->byteOffset + layout->byteSize;
        if (slotEnd > registerFrameBytes) {
            registerFrameBytes = slotEnd;
        }
    }

    return registerFrameBytes;
}

void backend_aot_write_c_frame_setup(FILE *file,
                                     const SZrAotExecIrFrameLayout *frameLayout,
                                     TZrUInt32 functionIndex,
                                     TZrBool includeExportContext,
                                     TZrBool includeFrameDescriptor) {
    TZrUInt32 frameByteSize = backend_aot_c_frame_setup_register_frame_bytes(frameLayout);
    TZrBool includeStackFrameSetup = (TZrBool)(includeFrameDescriptor || frameByteSize > 0u);

    if (file == ZR_NULL) {
        return;
    }

    if (!includeStackFrameSetup) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_generated_frame_setup */\n"
            "    SZrCallInfo *zr_aot_call_info = state->callInfoList;\n");

    fprintf(file,
            "    ZrAotGeneratedModuleContext zr_aot_context;\n"
            "    TZrStackValuePointer zr_aot_function_base;\n"
            "    TZrStackValuePointer zr_aot_slot_base;\n"
            "    TZrStackValuePointer zr_aot_frame_top;\n"
            "    TZrSize zr_aot_argument_count;\n"
            "    TZrSize zr_aot_frame_slot_count;\n"
            "    SZrFunctionStackAnchor zr_aot_base_anchor;\n"
            "    SZrFunctionStackAnchor zr_aot_return_anchor;\n"
            "    TZrBool zr_aot_has_return_anchor = ZR_FALSE;\n"
            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, %u, &zr_aot_context));\n"
            "    if (zr_aot_call_info == ZR_NULL || zr_aot_call_info->functionBase.valuePointer == ZR_NULL) {\n"
            "        ZrCore_Debug_RunError(state, \"generated AOT function has no call frame\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n"
            "    zr_aot_function_base = zr_aot_call_info->functionBase.valuePointer;\n"
            "    ZrCore_Function_StackAnchorInit(state, zr_aot_function_base, &zr_aot_base_anchor);\n"
            "    if (zr_aot_call_info->hasReturnDestination && zr_aot_call_info->returnDestination != ZR_NULL) {\n"
            "        ZrCore_Function_StackAnchorInit(state, zr_aot_call_info->returnDestination, &zr_aot_return_anchor);\n"
            "        zr_aot_has_return_anchor = ZR_TRUE;\n"
            "    }\n"
            "    zr_aot_frame_slot_count = (TZrSize)zr_aot_context.generatedFrameSlotCount;\n",
            (unsigned)functionIndex);

    if (frameByteSize > 0u) {
        fprintf(file,
                "    TZrSize zr_aot_frame_byte_size = (TZrSize)%uu;\n"
                "    TZrSize zr_aot_frame_byte_slot_count =\n"
                "            (zr_aot_frame_byte_size + sizeof(SZrTypeValue) - 1u) / sizeof(SZrTypeValue);\n"
                "    if (zr_aot_frame_slot_count < zr_aot_frame_byte_slot_count) {\n"
                "        zr_aot_frame_slot_count = zr_aot_frame_byte_slot_count;\n"
                "    }\n",
                (unsigned)frameByteSize);
    }

    fprintf(file,
            "    ZrCore_Function_CheckStackAndGc(state, zr_aot_frame_slot_count, zr_aot_function_base + 1);\n"
            "    zr_aot_function_base = ZrCore_Function_StackAnchorRestore(state, &zr_aot_base_anchor);\n"
            "    zr_aot_call_info->functionBase.valuePointer = zr_aot_function_base;\n"
            "    if (zr_aot_has_return_anchor) {\n"
            "        zr_aot_call_info->returnDestination = ZrCore_Function_StackAnchorRestore(state, &zr_aot_return_anchor);\n"
            "    }\n"
            "    zr_aot_slot_base = zr_aot_function_base + 1;\n"
            "    zr_aot_argument_count =\n"
            "            (state->stackTop.valuePointer != ZR_NULL && state->stackTop.valuePointer > zr_aot_slot_base)\n"
            "                    ? (TZrSize)(state->stackTop.valuePointer - zr_aot_slot_base)\n"
            "                    : 0;\n"
            "    if (zr_aot_frame_slot_count < zr_aot_argument_count) {\n"
            "        zr_aot_frame_slot_count = zr_aot_argument_count;\n"
            "    }\n"
            "    zr_aot_frame_top = zr_aot_slot_base + zr_aot_frame_slot_count;\n"
            "    if (ZrCore_CallInfo_IsNative(zr_aot_call_info)) {\n"
            "        for (TZrSize zr_aot_slot = zr_aot_argument_count; zr_aot_slot < zr_aot_frame_slot_count; zr_aot_slot++) {\n"
            "            ZrCore_Value_ResetAsNull(&zr_aot_slot_base[zr_aot_slot].value);\n"
            "        }\n"
            "    }\n"
            "    if (zr_aot_call_info->functionTop.valuePointer < zr_aot_frame_top) {\n"
            "        zr_aot_call_info->functionTop.valuePointer = zr_aot_frame_top;\n"
            "    }\n"
            "    if (state->stackTop.valuePointer < zr_aot_frame_top) {\n"
            "        state->stackTop.valuePointer = zr_aot_frame_top;\n"
            "    }\n");

    if (includeFrameDescriptor) {
        fprintf(file,
                "    frame.function = zr_aot_context.metadataFunction;\n"
                "    frame.callInfo = zr_aot_call_info;\n"
                "    frame.slotBase = zr_aot_slot_base;\n"
                "    frame.generatedFrameSlotCount = zr_aot_context.generatedFrameSlotCount;\n");
        if (includeExportContext) {
            fprintf(file,
                    "    frame.module = zr_aot_context.module;\n"
                    "    frame.moduleExecuted = zr_aot_context.moduleExecuted;\n"
                    "    frame.functionTable = zr_aot_context.functionTable;\n"
                    "    frame.functionCount = zr_aot_context.functionCount;\n"
                    "    frame.functionThunks = zr_aot_context.functionThunks;\n"
                    "    frame.functionThunkCount = zr_aot_context.functionThunkCount;\n");
        }
    }
}
