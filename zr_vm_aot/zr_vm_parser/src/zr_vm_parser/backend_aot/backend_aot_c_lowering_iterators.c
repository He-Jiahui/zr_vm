#include "backend_aot_c_emitter.h"

static void backend_aot_c_write_iterator_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "            ZrCore_Debug_RunError(state, \"unsupported AOT iterator core path\");\n"
            "            ZR_AOT_C_FAIL();\n");
}

static void backend_aot_c_write_iterator_slot_header(FILE *file,
                                                     const char *marker,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     const char *sourceName) {
    if (file == ZR_NULL || marker == ZR_NULL || sourceName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    do {\n"
            "        /* %s */\n"
            "        SZrTypeValue *zr_aot_destination = ZR_NULL;\n"
            "        SZrTypeValue *%s = ZR_NULL;\n"
            "        if (state == ZR_NULL || frame.function == ZR_NULL || frame.slotBase == ZR_NULL ||\n"
            "            %u >= frame.generatedFrameSlotCount || %u >= frame.generatedFrameSlotCount) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        %s = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || %s == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            marker,
            sourceName,
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)destinationSlot,
            sourceName,
            (unsigned)sourceSlot,
            sourceName);
}

void backend_aot_write_c_direct_iter_init(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iterableSlot) {
    if (file == ZR_NULL) {
        return;
    }

    backend_aot_c_write_iterator_slot_header(file,
                                             "zr_aot_value_exec_iter_init",
                                             destinationSlot,
                                             iterableSlot,
                                             "zr_aot_iterable");
    fprintf(file,
            "        SZrTypeValue zr_aot_stable_iterable = *zr_aot_iterable;\n"
            "        if (!ZrCore_Object_IterInit(state, &zr_aot_stable_iterable, zr_aot_destination)) {\n");
    backend_aot_c_write_iterator_unsupported(file);
    fprintf(file,
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_iter_move_next(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iteratorSlot) {
    if (file == ZR_NULL) {
        return;
    }

    backend_aot_c_write_iterator_slot_header(file,
                                             "zr_aot_value_exec_iter_move_next",
                                             destinationSlot,
                                             iteratorSlot,
                                             "zr_aot_iterator");
    fprintf(file,
            "        if (!ZrCore_Object_TryIterMoveNextCachedArrayFast(state, zr_aot_iterator, zr_aot_destination)) {\n"
            "            SZrTypeValue zr_aot_stable_iterator = *zr_aot_iterator;\n"
            "            if (!ZrCore_Object_IterMoveNext(state, &zr_aot_stable_iterator, zr_aot_destination)) {\n");
    backend_aot_c_write_iterator_unsupported(file);
    fprintf(file,
            "            }\n"
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_iter_current(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iteratorSlot) {
    if (file == ZR_NULL) {
        return;
    }

    backend_aot_c_write_iterator_slot_header(file,
                                             "zr_aot_value_exec_iter_current",
                                             destinationSlot,
                                             iteratorSlot,
                                             "zr_aot_iterator");
    fprintf(file,
            "        if (!ZrCore_Object_TryIterCurrentCachedMemberFastStackResult(state, zr_aot_iterator, zr_aot_destination)) {\n"
            "            SZrTypeValue zr_aot_stable_iterator = *zr_aot_iterator;\n"
            "            if (!ZrCore_Object_IterCurrent(state, &zr_aot_stable_iterator, zr_aot_destination)) {\n");
    backend_aot_c_write_iterator_unsupported(file);
    fprintf(file,
            "            }\n"
            "        }\n"
            "    } while (0);\n");
}

void backend_aot_write_c_direct_iter_move_next_jump_if_false(FILE *file,
                                                             TZrUInt32 functionIndex,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 iteratorSlot,
                                                             TZrUInt32 targetInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }

    backend_aot_c_write_iterator_slot_header(file,
                                             "zr_aot_value_exec_iter_move_next_jump_if_false",
                                             destinationSlot,
                                             iteratorSlot,
                                             "zr_aot_iterator");
    fprintf(file,
            "        if (!ZrCore_Object_TryIterMoveNextCachedArrayFast(state, zr_aot_iterator, zr_aot_destination)) {\n"
            "            SZrTypeValue zr_aot_stable_iterator = *zr_aot_iterator;\n"
            "            if (!ZrCore_Object_IterMoveNext(state, &zr_aot_stable_iterator, zr_aot_destination)) {\n");
    backend_aot_c_write_iterator_unsupported(file);
    fprintf(file,
            "            }\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_destination->type)) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT iterator branch result\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!zr_aot_destination->value.nativeObject.nativeBool) {\n"
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    } while (0);\n",
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}
