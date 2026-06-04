#include "backend_aot_c_emitter.h"

void backend_aot_write_c_direct_to_float_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_signed_to_float */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_source_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_scalar = zr_aot_source->value.nativeObject.nativeInt64;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeDouble,\n"
            "                          (TZrFloat64)zr_aot_source_scalar,\n"
            "                          ZR_VALUE_TYPE_DOUBLE);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_float_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_unsigned_to_float */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrUInt64 zr_aot_source_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_scalar = zr_aot_source->value.nativeObject.nativeUInt64;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeDouble,\n"
            "                          (TZrFloat64)zr_aot_source_scalar,\n"
            "                          ZR_VALUE_TYPE_DOUBLE);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_int_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_float_to_signed */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrFloat64 zr_aot_source_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_scalar = zr_aot_source->value.nativeObject.nativeDouble;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          (TZrInt64)zr_aot_source_scalar,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_int_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_unsigned_to_signed */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrUInt64 zr_aot_source_scalar;\n"
            "        TZrUInt64 zr_aot_unsigned_to_signed_limit;\n"
            "        TZrInt64 zr_aot_result_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_scalar = zr_aot_source->value.nativeObject.nativeUInt64;\n"
            "        zr_aot_unsigned_to_signed_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;\n"
            "        if (zr_aot_source_scalar >= zr_aot_unsigned_to_signed_limit) {\n"
            "            zr_aot_result_scalar = ZR_TYPE_RANGE_INT64_MIN +\n"
            "                                   (TZrInt64)(zr_aot_source_scalar - zr_aot_unsigned_to_signed_limit);\n"
            "        } else {\n"
            "            zr_aot_result_scalar = (TZrInt64)zr_aot_source_scalar;\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_result_scalar,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_uint_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_float_to_unsigned */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrFloat64 zr_aot_source_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_scalar = zr_aot_source->value.nativeObject.nativeDouble;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeUInt64,\n"
            "                          (TZrUInt64)zr_aot_source_scalar,\n"
            "                          ZR_VALUE_TYPE_UINT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

void backend_aot_write_c_direct_to_uint_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_signed_to_unsigned */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_source_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_source_scalar = zr_aot_source->value.nativeObject.nativeInt64;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeUInt64,\n"
            "                          (TZrUInt64)zr_aot_source_scalar,\n"
            "                          ZR_VALUE_TYPE_UINT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}
