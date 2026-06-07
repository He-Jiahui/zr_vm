#include "backend_aot_c_emitter.h"

void backend_aot_write_c_direct_pow_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_signed_power */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n"
            "        TZrUInt64 zr_aot_power_result = 1u;\n"
            "        TZrUInt64 zr_aot_power_base;\n"
            "        TZrUInt64 zr_aot_power_exponent;\n"
            "        TZrBool zr_aot_power_overflow = ZR_FALSE;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        if (ZR_UNLIKELY((zr_aot_left_scalar == 0 && zr_aot_right_scalar <= 0) || zr_aot_left_scalar < 0)) {\n"
            "            ZrCore_Debug_RunError(state, \"power domain error\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_right_scalar < 0) {\n"
            "            zr_aot_power_result = 0u;\n"
            "        } else if (zr_aot_right_scalar == 0) {\n"
            "            zr_aot_power_result = 1u;\n"
            "        } else if (zr_aot_left_scalar == 0) {\n"
            "            zr_aot_power_result = 0u;\n"
            "        } else {\n"
            "            zr_aot_power_base = (TZrUInt64)zr_aot_left_scalar;\n"
            "            zr_aot_power_exponent = (TZrUInt64)zr_aot_right_scalar;\n"
            "            while (zr_aot_power_exponent > 0u) {\n"
            "                if ((zr_aot_power_exponent & 1u) != 0u) {\n"
            "                    if (ZR_UNLIKELY(zr_aot_power_base != 0u && zr_aot_power_result > ZR_UINT_MAX / zr_aot_power_base)) {\n"
            "                        zr_aot_power_overflow = ZR_TRUE;\n"
            "                        break;\n"
            "                    }\n"
            "                    zr_aot_power_result *= zr_aot_power_base;\n"
            "                }\n"
            "                zr_aot_power_exponent >>= 1;\n"
            "                if (zr_aot_power_exponent == 0u) {\n"
            "                    break;\n"
            "                }\n"
            "                if (ZR_UNLIKELY(zr_aot_power_base != 0u && zr_aot_power_base > ZR_UINT_MAX / zr_aot_power_base)) {\n"
            "                    zr_aot_power_overflow = ZR_TRUE;\n"
            "                    break;\n"
            "                }\n"
            "                zr_aot_power_base *= zr_aot_power_base;\n"
            "            }\n"
            "            if (ZR_UNLIKELY(zr_aot_power_overflow || zr_aot_power_result > (TZrUInt64)ZR_INT_MAX)) {\n"
            "                zr_aot_power_result = 0u;\n"
            "            }\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          (TZrInt64)zr_aot_power_result,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_pow_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_unsigned_power */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrUInt64 zr_aot_left_scalar;\n"
            "        TZrUInt64 zr_aot_right_scalar;\n"
            "        TZrUInt64 zr_aot_power_result = 1u;\n"
            "        TZrUInt64 zr_aot_power_base;\n"
            "        TZrUInt64 zr_aot_power_exponent;\n"
            "        TZrBool zr_aot_power_overflow = ZR_FALSE;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        if (ZR_UNLIKELY(zr_aot_left_scalar == 0u && zr_aot_right_scalar == 0u)) {\n"
            "            ZrCore_Debug_RunError(state, \"power domain error\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (zr_aot_right_scalar == 0u) {\n"
            "            zr_aot_power_result = 1u;\n"
            "        } else if (zr_aot_left_scalar == 0u) {\n"
            "            zr_aot_power_result = 0u;\n"
            "        } else {\n"
            "            zr_aot_power_base = zr_aot_left_scalar;\n"
            "            zr_aot_power_exponent = zr_aot_right_scalar;\n"
            "            while (zr_aot_power_exponent > 0u) {\n"
            "                if ((zr_aot_power_exponent & 1u) != 0u) {\n"
            "                    if (ZR_UNLIKELY(zr_aot_power_base != 0u && zr_aot_power_result > ZR_UINT_MAX / zr_aot_power_base)) {\n"
            "                        zr_aot_power_overflow = ZR_TRUE;\n"
            "                        break;\n"
            "                    }\n"
            "                    zr_aot_power_result *= zr_aot_power_base;\n"
            "                }\n"
            "                zr_aot_power_exponent >>= 1;\n"
            "                if (zr_aot_power_exponent == 0u) {\n"
            "                    break;\n"
            "                }\n"
            "                if (ZR_UNLIKELY(zr_aot_power_base != 0u && zr_aot_power_base > ZR_UINT_MAX / zr_aot_power_base)) {\n"
            "                    zr_aot_power_overflow = ZR_TRUE;\n"
            "                    break;\n"
            "                }\n"
            "                zr_aot_power_base *= zr_aot_power_base;\n"
            "            }\n"
            "            if (ZR_UNLIKELY(zr_aot_power_overflow)) {\n"
            "                zr_aot_power_result = 0u;\n"
            "            }\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeUInt64,\n"
            "                          zr_aot_power_result,\n"
            "                          ZR_VALUE_TYPE_UINT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_pow_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_float_power */\n"
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
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeDouble,\n"
            "                          pow(zr_aot_left_scalar, zr_aot_right_scalar),\n"
            "                          ZR_VALUE_TYPE_DOUBLE);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}
