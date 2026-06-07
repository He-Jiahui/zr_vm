#include "backend_aot_c_emitter.h"

void backend_aot_write_c_direct_mod_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_float_mod */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrFloat64 zr_aot_left_scalar;\n"
            "        TZrFloat64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeDouble;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeDouble;\n"
            "        if (ZR_UNLIKELY(zr_aot_right_scalar == 0.0)) {\n"
            "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeDouble,\n"
            "                          fmod(zr_aot_left_scalar, zr_aot_right_scalar),\n"
            "                          ZR_VALUE_TYPE_DOUBLE);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}
