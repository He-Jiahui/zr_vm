#include "backend_aot_c_emitter.h"

/* backend_aot_c_lowering_generic_power.c */

void backend_aot_write_c_direct_pow(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_power */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrMeta *zr_aot_meta;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_meta = ZrCore_Value_GetMeta(state, zr_aot_left, ZR_META_POW);\n"
            "        if (zr_aot_meta == ZR_NULL || zr_aot_meta->function == ZR_NULL) {\n"
            "            ZrCore_Value_ResetAsNull(zr_aot_destination);\n"
            "        } else {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT generic power meta dispatch\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}
