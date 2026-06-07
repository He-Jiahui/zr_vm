#include "backend_aot_c_emitter.h"

/* backend_aot_c_lowering_generic_conversion.c */

static void backend_aot_c_write_generic_conversion_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive conversion\");\n"
            "        ZR_AOT_C_FAIL();\n");
}

void backend_aot_write_c_direct_to_bool(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_bool */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            *zr_aot_destination = *zr_aot_source;\n"
            "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeBool,\n"
            "                              zr_aot_source->value.nativeObject.nativeInt64 != 0,\n"
            "                              ZR_VALUE_TYPE_BOOL);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeBool,\n"
            "                              zr_aot_source->value.nativeObject.nativeUInt64 != 0,\n"
            "                              ZR_VALUE_TYPE_BOOL);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeBool,\n"
            "                              zr_aot_source->value.nativeObject.nativeDouble != 0.0,\n"
            "                              ZR_VALUE_TYPE_BOOL);\n"
            "        } else {\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_generic_conversion_unsupported(file);
    fprintf(file,
            "        }\n"
            "    }\n");
}

void backend_aot_write_c_direct_to_int(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_int */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            *zr_aot_destination = *zr_aot_source;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              (TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              (TZrInt64)zr_aot_source->value.nativeObject.nativeDouble,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_source->value.nativeObject.nativeBool ? 1 : 0,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else {\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_generic_conversion_unsupported(file);
    fprintf(file,
            "        }\n"
            "    }\n");
}

void backend_aot_write_c_direct_to_uint(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_uint */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            *zr_aot_destination = *zr_aot_source;\n"
            "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeUInt64,\n"
            "                              (TZrUInt64)zr_aot_source->value.nativeObject.nativeInt64,\n"
            "                              ZR_VALUE_TYPE_UINT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeUInt64,\n"
            "                              (TZrUInt64)zr_aot_source->value.nativeObject.nativeDouble,\n"
            "                              ZR_VALUE_TYPE_UINT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeUInt64,\n"
            "                              zr_aot_source->value.nativeObject.nativeBool ? (TZrUInt64)1u : (TZrUInt64)0u,\n"
            "                              ZR_VALUE_TYPE_UINT64);\n"
            "        } else {\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_generic_conversion_unsupported(file);
    fprintf(file,
            "        }\n"
            "    }\n");
}

void backend_aot_write_c_direct_to_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_convert_generic_to_float */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            *zr_aot_destination = *zr_aot_source;\n"
            "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeDouble,\n"
            "                              (TZrFloat64)zr_aot_source->value.nativeObject.nativeInt64,\n"
            "                              ZR_VALUE_TYPE_DOUBLE);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeDouble,\n"
            "                              (TZrFloat64)zr_aot_source->value.nativeObject.nativeUInt64,\n"
            "                              ZR_VALUE_TYPE_DOUBLE);\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeDouble,\n"
            "                              zr_aot_source->value.nativeObject.nativeBool ? (TZrFloat64)1.0 : (TZrFloat64)0.0,\n"
            "                              ZR_VALUE_TYPE_DOUBLE);\n"
            "        } else {\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_generic_conversion_unsupported(file);
    fprintf(file,
            "        }\n"
            "    }\n");
}
