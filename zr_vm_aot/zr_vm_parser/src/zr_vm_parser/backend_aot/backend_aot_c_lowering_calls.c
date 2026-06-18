#include "backend_aot_c_emitter.h"
#include "backend_aot_internal.h"

static void backend_aot_write_c_core_function_call(FILE *file,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 functionSlot,
                                                   TZrUInt32 argumentCount,
                                                   const char *marker,
                                                   const char *errorLabel) {
    if (file == ZR_NULL || marker == ZR_NULL || errorLabel == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        TZrStackValuePointer zr_aot_call_base;\n"
            "        TZrStackValuePointer zr_aot_destination_pointer;\n"
            "        TZrStackValuePointer zr_aot_result_base;\n"
            "        SZrFunctionStackAnchor zr_aot_call_anchor;\n"
            "        SZrFunctionStackAnchor zr_aot_destination_anchor;\n"
            "        SZrTypeValue *zr_aot_callable_value;\n"
            "        SZrTypeValue *zr_aot_destination_value;\n"
            "        SZrTypeValue *zr_aot_result_value;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_call_base = frame.slotBase + %u;\n"
            "        zr_aot_destination_pointer = frame.slotBase + %u;\n"
            "        if (state->callInfoList == ZR_NULL || state->stackTop.valuePointer == ZR_NULL ||\n"
            "            state->stackTop.valuePointer < zr_aot_call_base + 1 + %u) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT %s has invalid stack range\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_callable_value = ZrCore_Stack_GetValue(zr_aot_call_base);\n"
            "        if (zr_aot_callable_value == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT %s is missing callable value\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Function_StackAnchorInit(state, zr_aot_call_base, &zr_aot_call_anchor);\n"
            "        ZrCore_Function_StackAnchorInit(state, zr_aot_destination_pointer, &zr_aot_destination_anchor);\n"
            "        state->stackTop.valuePointer = zr_aot_call_base + 1 + %u;\n"
            "        if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {\n"
            "            state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;\n"
            "        }\n"
            "        zr_aot_result_base = ZrCore_Function_CallAndRestoreAnchor(state, &zr_aot_call_anchor, 1);\n"
            "        if (zr_aot_result_base == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||\n"
            "            state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT %s failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination_pointer = ZrCore_Function_StackAnchorRestore(state, &zr_aot_destination_anchor);\n"
            "        if (zr_aot_destination_pointer == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT %s lost destination slot\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination_value = ZrCore_Stack_GetValue(zr_aot_destination_pointer);\n"
            "        zr_aot_result_value = ZrCore_Stack_GetValue(zr_aot_result_base);\n"
            "        if (zr_aot_destination_value == ZR_NULL || zr_aot_result_value == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT %s has invalid result slot\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        *zr_aot_destination_value = *zr_aot_result_value;\n"
            "        frame.callInfo = state->callInfoList;\n"
            "        frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;\n"
            "        state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;\n"
            "    }\n",
            marker,
            (unsigned)functionSlot,
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)destinationSlot,
            (unsigned)argumentCount,
            errorLabel,
            errorLabel,
            (unsigned)argumentCount,
            errorLabel,
            errorLabel,
            errorLabel);
}

void backend_aot_write_c_unsupported_meta_call(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 receiverSlot,
                                               TZrUInt32 argumentCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* zr_aot_unsupported_meta_call */\n"
            "        const TZrUInt32 zr_aot_destination_slot = %u;\n"
            "        const TZrUInt32 zr_aot_receiver_slot = %u;\n"
            "        const TZrUInt32 zr_aot_argument_count = %u;\n"
            "        if (state == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            zr_aot_destination_slot >= frame.generatedFrameSlotCount ||\n"
            "            zr_aot_receiver_slot >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        SZrTypeValue *zr_aot_receiver = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_receiver == ZR_NULL || zr_aot_destination == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        (void)zr_aot_argument_count;\n"
            "        (void)zr_aot_receiver;\n"
            "        (void)zr_aot_destination;\n"
            "        ZrCore_Debug_RunError(state, \"unsupported AOT meta call\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    } while (0);\n",
            (unsigned)destinationSlot,
            (unsigned)receiverSlot,
            (unsigned)argumentCount,
            (unsigned)receiverSlot,
            (unsigned)destinationSlot);
}

void backend_aot_write_c_static_direct_function_call(FILE *file,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 calleeFlatIndex) {
    if (file == ZR_NULL || calleeFlatIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_direct_static_function_call */\n"
            "        TZrStackValuePointer zr_aot_call_base;\n"
            "        TZrStackValuePointer zr_aot_destination_pointer;\n"
            "        SZrCallInfo *zr_aot_call_info;\n"
            "        SZrFunction *zr_aot_metadata_function;\n"
            "        SZrTypeValue *zr_aot_callable_value;\n"
            "        TZrUInt32 zr_aot_call_window_index;\n"
            "        TZrUInt32 zr_aot_argument_index;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            frame.callInfo == ZR_NULL || frame.callInfo != state->callInfoList ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount ||\n"
            "            %u > frame.generatedFrameSlotCount - %u - 1u) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);\n"
            "        if (zr_aot_call_base == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call has invalid stack range\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_call_base = ZrCore_Function_CheckStackAndGc(state, 1u + %u, zr_aot_call_base);\n"
            "        if (zr_aot_call_base == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||\n"
            "            state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call has invalid stack range\");\n"
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
            "        zr_aot_destination_pointer = frame.slotBase + %u;\n"
            "        if (frame.callInfo->functionTop.valuePointer < frame.slotBase + %u + 1u + %u) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call has invalid stack range\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_metadata_function = ZrCore_Closure_GetMetadataFunctionFromValue(state, zr_aot_callable_value);\n"
            "        if (zr_aot_callable_value == ZR_NULL || zr_aot_metadata_function == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(zr_aot_call_base), zr_aot_callable_value);\n"
            "        for (zr_aot_argument_index = 0u; zr_aot_argument_index < %u; zr_aot_argument_index++) {\n"
            "            ZrCore_Value_Copy(state,\n"
            "                              ZrCore_Stack_GetValue(zr_aot_call_base + 1u + zr_aot_argument_index),\n"
            "                              ZrCore_Stack_GetValue(frame.slotBase + %u + 1u + zr_aot_argument_index));\n"
            "        }\n"
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
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_AOT_C_GUARD(zr_aot_fn_%u(state));\n"
            "        ZrCore_Function_PostCall(state, zr_aot_call_info, 1);\n"
            "        if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL ||\n"
            "            state->callInfoList->functionBase.valuePointer == ZR_NULL ||\n"
            "            state->callInfoList->functionTop.valuePointer == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo = state->callInfoList;\n"
            "        frame.slotBase = state->callInfoList->functionBase.valuePointer + 1;\n"
            "        zr_aot_call_base = ZrCore_Function_GetCallInfoFrameStorageTop(state, frame.callInfo);\n"
            "        if (zr_aot_call_base == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"generated AOT static direct call failed\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        frame.callInfo->functionTop.valuePointer = zr_aot_call_base;\n"
            "        state->stackTop.valuePointer = zr_aot_call_base;\n"
            "    }\n",
            (unsigned)functionSlot,
            (unsigned)destinationSlot,
            (unsigned)argumentCount,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)argumentCount,
            (unsigned)argumentCount,
            (unsigned)functionSlot,
            (unsigned)destinationSlot,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)argumentCount,
            (unsigned)functionSlot,
            (unsigned)argumentCount,
            (unsigned)(functionSlot + 1u),
            (unsigned)calleeFlatIndex);
}

void backend_aot_write_c_direct_function_call(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount) {
    backend_aot_write_c_core_function_call(file,
                                           destinationSlot,
                                           functionSlot,
                                           argumentCount,
                                           "zr_aot_direct_function_call",
                                           "function call");
}

void backend_aot_write_c_dynamic_function_call(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount) {
    backend_aot_write_c_core_function_call(file,
                                           destinationSlot,
                                           functionSlot,
                                           argumentCount,
                                           "zr_aot_direct_dynamic_function_call",
                                           "dynamic call");
}
