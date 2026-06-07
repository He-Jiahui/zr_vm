#include "backend_aot_c_emitter.h"

void backend_aot_write_c_direct_mark_to_be_closed(FILE *file, TZrUInt32 slotIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_mark_to_be_closed */\n"
            "        TZrStackValuePointer zr_aot_slot_pointer = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.slotBase == ZR_NULL || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_slot_pointer = frame.slotBase + %u;\n"
            "        ZrCore_Closure_ToBeClosedValueClosureNew(state, zr_aot_slot_pointer);\n"
            "    }\n",
            (unsigned)slotIndex,
            (unsigned)slotIndex);
}

void backend_aot_write_c_direct_close_scope(FILE *file, TZrUInt32 cleanupCount) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_scope_close_scope */\n"
            "        const TZrUInt32 zr_aot_cleanup_count = %u;\n"
            "        TZrUInt32 zr_aot_closed_count = 0;\n"
            "        TZrMemoryOffset zr_aot_saved_stack_top_offset = 0;\n"
            "        SZrCallInfo *zr_aot_current_call_info = ZR_NULL;\n"
            "        if (state == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_cleanup_count != 0) {\n"
            "            zr_aot_saved_stack_top_offset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);\n"
            "            zr_aot_current_call_info = state->callInfoList;\n"
            "            if (zr_aot_current_call_info != ZR_NULL &&\n"
            "                state->stackTop.valuePointer < zr_aot_current_call_info->functionTop.valuePointer) {\n"
            "                state->stackTop.valuePointer = zr_aot_current_call_info->functionTop.valuePointer;\n"
            "            }\n"
            "            while (zr_aot_closed_count < zr_aot_cleanup_count &&\n"
            "                   state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {\n"
            "                TZrStackPointer zr_aot_to_be_closed = state->toBeClosedValueList;\n"
            "                ZrCore_Closure_CloseStackValue(state, zr_aot_to_be_closed.valuePointer);\n"
            "                ZrCore_Closure_CloseRegisteredValues(state, 1, ZR_THREAD_STATUS_INVALID, ZR_FALSE);\n"
            "                zr_aot_closed_count++;\n"
            "            }\n"
            "            state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, zr_aot_saved_stack_top_offset);\n"
            "        }\n"
            "    }\n",
            (unsigned)cleanupCount);
}
