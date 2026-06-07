#include "backend_aot_c_emitter.h"
#include "backend_aot_internal.h"

/* backend_aot_c_lowering_generic_logical.c */

static const SZrAotExecIrFrameSlotLayout *backend_aot_c_generic_logical_find_frame_slot_layout(
        const SZrAotExecIrFrameLayout *frameLayout,
        TZrUInt32 stackSlot) {
    TZrUInt32 layoutIndex;

    if (frameLayout == ZR_NULL || frameLayout->slotLayouts == ZR_NULL) {
        return ZR_NULL;
    }

    for (layoutIndex = 0; layoutIndex < frameLayout->slotLayoutCount; layoutIndex++) {
        const SZrAotExecIrFrameSlotLayout *layout = &frameLayout->slotLayouts[layoutIndex];

        if (layout->stackSlot == stackSlot) {
            return layout;
        }
    }

    return ZR_NULL;
}

static void backend_aot_c_write_string_logical_operand(FILE *file,
                                                       const char *name,
                                                       const SZrAotExecIrFrameLayout *frameLayout,
                                                       TZrUInt32 stackSlot) {
    const SZrAotExecIrFrameSlotLayout *layout =
            backend_aot_c_generic_logical_find_frame_slot_layout(frameLayout, stackSlot);

    if (file == ZR_NULL || name == ZR_NULL) {
        return;
    }

    if (layout != ZR_NULL && layout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
        fprintf(file,
                "        const SZrTypeValue *%s = (const SZrTypeValue *)((const TZrByte *)frame.slotBase + %u);\n",
                name,
                (unsigned)layout->byteOffset);
    } else {
        fprintf(file,
                "        const SZrTypeValue *%s = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
                name,
                (unsigned)stackSlot);
    }
}

static void backend_aot_c_write_generic_truthiness_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive truthiness\");\n"
            "        ZR_AOT_C_FAIL();\n");
}

static void backend_aot_c_write_generic_equality_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        ZrCore_Debug_RunError(state, \"unsupported AOT generic primitive equality\");\n"
            "        ZR_AOT_C_FAIL();\n");
}

static void backend_aot_c_write_bool_binary_logical(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (!ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_BOOL(zr_aot_right->type)) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT bool logical binary\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        TZrBool zr_aot_left_bool = (TZrBool)(zr_aot_left->value.nativeObject.nativeBool != 0u);\n"
            "        TZrBool zr_aot_right_bool = (TZrBool)(zr_aot_right->value.nativeObject.nativeBool != 0u);\n");
}

static void backend_aot_c_write_primitive_truthiness(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (ZR_VALUE_IS_TYPE_NULL(zr_aot_source->type)) {\n"
            "            zr_aot_truthy = ZR_FALSE;\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            zr_aot_truthy = (TZrBool)(zr_aot_source->value.nativeObject.nativeBool != 0u);\n"
            "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            zr_aot_truthy = (TZrBool)(zr_aot_source->value.nativeObject.nativeInt64 != 0);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            zr_aot_truthy = (TZrBool)(zr_aot_source->value.nativeObject.nativeUInt64 != 0);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            zr_aot_truthy = (TZrBool)(zr_aot_source->value.nativeObject.nativeDouble != 0.0);\n"
            "        } else {\n");
    backend_aot_c_write_generic_truthiness_unsupported(file);
    fprintf(file,
            "        }\n");
}

static void backend_aot_c_write_primitive_equality(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (zr_aot_left->type != zr_aot_right->type) {\n"
            "            zr_aot_equal = ZR_FALSE;\n"
            "        } else if (ZR_VALUE_IS_TYPE_NULL(zr_aot_left->type)) {\n"
            "            zr_aot_equal = ZR_TRUE;\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_left->type)) {\n"
            "            zr_aot_equal = (TZrBool)((zr_aot_left->value.nativeObject.nativeBool != 0u) ==\n"
            "                                      (zr_aot_right->value.nativeObject.nativeBool != 0u));\n"
            "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_equal = (TZrBool)(zr_aot_left->value.nativeObject.nativeInt64 == zr_aot_right->value.nativeObject.nativeInt64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_equal = (TZrBool)(zr_aot_left->value.nativeObject.nativeUInt64 == zr_aot_right->value.nativeObject.nativeUInt64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)) {\n"
            "            zr_aot_equal = (TZrBool)(zr_aot_left->value.nativeObject.nativeDouble == zr_aot_right->value.nativeObject.nativeDouble);\n"
            "        } else {\n");
    backend_aot_c_write_generic_equality_unsupported(file);
    fprintf(file,
            "        }\n");
}

static void backend_aot_c_write_string_equality(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        TZrBool zr_aot_equal = ZR_FALSE;\n"
            "        if (!ZR_VALUE_IS_TYPE_STRING(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_STRING(zr_aot_right->type) ||\n"
            "            zr_aot_left->value.object == ZR_NULL || zr_aot_right->value.object == ZR_NULL) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT string equality\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        const SZrString *zr_aot_left_string = ZR_CAST_STRING(state, zr_aot_left->value.object);\n"
            "        const SZrString *zr_aot_right_string = ZR_CAST_STRING(state, zr_aot_right->value.object);\n"
            "        if (zr_aot_left_string == zr_aot_right_string) {\n"
            "            zr_aot_equal = ZR_TRUE;\n"
            "        } else {\n"
            "            TZrSize zr_aot_left_length = ZrCore_String_GetByteLength(zr_aot_left_string);\n"
            "            TZrSize zr_aot_right_length = ZrCore_String_GetByteLength(zr_aot_right_string);\n"
            "            const TZrChar *zr_aot_left_bytes = ZrCore_String_GetNativeString(zr_aot_left_string);\n"
            "            const TZrChar *zr_aot_right_bytes = ZrCore_String_GetNativeString(zr_aot_right_string);\n"
            "            if (zr_aot_left_bytes == ZR_NULL || zr_aot_right_bytes == ZR_NULL) {\n"
            "                ZrCore_Debug_RunError(state, \"unsupported AOT string equality\");\n"
            "                ZR_AOT_C_FAIL();\n"
            "            }\n"
            "            zr_aot_equal = (TZrBool)(zr_aot_left_length == zr_aot_right_length &&\n"
            "                                       (zr_aot_left_length == 0u ||\n"
            "                                        memcmp(zr_aot_left_bytes, zr_aot_right_bytes, zr_aot_left_length) == 0));\n"
            "        }\n");
}

void backend_aot_write_c_direct_logical_equal(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_logical_equal */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_equal = ZR_FALSE;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_primitive_equality(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_equal, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_logical_not_equal(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_logical_not_equal */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_equal = ZR_FALSE;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_primitive_equality(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_equal, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_logical_equal_string(FILE *file,
                                                     const SZrAotExecIrFrameLayout *frameLayout,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_string_logical_equal */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            (unsigned)destinationSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_left", frameLayout, leftSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_right", frameLayout, rightSlot);
    fprintf(file,
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    backend_aot_c_write_string_equality(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_equal, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_logical_not_equal_string(FILE *file,
                                                         const SZrAotExecIrFrameLayout *frameLayout,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_string_logical_not_equal */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            (unsigned)destinationSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_left", frameLayout, leftSlot);
    backend_aot_c_write_string_logical_operand(file, "zr_aot_right", frameLayout, rightSlot);
    fprintf(file,
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    backend_aot_c_write_string_equality(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_equal, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_logical_and(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_logical_and */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_bool_binary_logical(file);
    fprintf(file,
            "        TZrBool zr_aot_result = (TZrBool)(zr_aot_left_bool && zr_aot_right_bool);\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_logical_or(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bool_logical_or */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_bool_binary_logical(file);
    fprintf(file,
            "        TZrBool zr_aot_result = (TZrBool)(zr_aot_left_bool || zr_aot_right_bool);\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_logical_not(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_logical_not */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_truthy = ZR_FALSE;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_primitive_truthiness(file);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, !zr_aot_truthy, ZR_VALUE_TYPE_BOOL);\n"
            "    }\n");
}

void backend_aot_write_c_direct_jump_if(FILE *file,
                                        TZrUInt32 functionIndex,
                                        TZrUInt32 conditionSlot,
                                        TZrUInt32 targetInstructionIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_generic_jump_if */\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrBool zr_aot_truthy = ZR_FALSE;\n"
            "        if (zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)conditionSlot);
    backend_aot_c_write_primitive_truthiness(file);
    fprintf(file,
            "        if (!zr_aot_truthy) {\n"
            "            goto zr_aot_fn_%u_ins_%u;\n"
            "        }\n"
            "    }\n",
            (unsigned)functionIndex,
            (unsigned)targetInstructionIndex);
}
