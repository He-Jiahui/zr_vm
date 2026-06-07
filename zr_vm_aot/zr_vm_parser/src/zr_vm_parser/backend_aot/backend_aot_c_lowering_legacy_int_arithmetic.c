#include "backend_aot_c_emitter.h"

/* backend_aot_c_lowering_legacy_int_arithmetic.c */

static TZrBool backend_aot_c_format_integer_like_literal(char *buffer,
                                                         TZrSize bufferSize,
                                                         const SZrTypeValue *constantValue) {
    if (buffer == ZR_NULL || bufferSize == 0 || constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        snprintf(buffer,
                 (size_t)bufferSize,
                 "(TZrInt64)%lld",
                 (long long)constantValue->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        snprintf(buffer,
                 (size_t)bufferSize,
                 "(TZrInt64)%llu",
                 (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
        snprintf(buffer,
                 (size_t)bufferSize,
                 "(TZrInt64)%s",
                 constantValue->value.nativeObject.nativeBool ? "1" : "0");
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void backend_aot_write_c_direct_int_const_fail(FILE *file, const char *message) {
    if (file == ZR_NULL || message == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        ZrCore_Debug_RunError(state, \"%s\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n",
            message);
}

void backend_aot_write_c_direct_add_int(FILE *file,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_int */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int;\n"
            "        TZrInt64 zr_aot_right_int;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_left_int + zr_aot_right_int,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_add_int_const(FILE *file,
                                              const SZrFunction *function,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;
    char rightLiteral[64];

    if (file == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_integer_like_literal(rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_int_const_fail(file, "unsupported AOT ADD_INT_CONST constant");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_int_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int = 0;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_left_int + zr_aot_right_literal,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            rightLiteral);
}

void backend_aot_write_c_direct_sub_int(FILE *file,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_int */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int;\n"
            "        TZrInt64 zr_aot_right_int;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_left_int - zr_aot_right_int,\n"
            "                          zr_aot_left->type);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

void backend_aot_write_c_direct_sub_int_const(FILE *file,
                                              const SZrFunction *function,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;
    char rightLiteral[64];

    if (file == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_integer_like_literal(rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_int_const_fail(file, "unsupported AOT SUB_INT_CONST constant");
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_int_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int = 0;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_left_int - zr_aot_right_literal,\n"
            "                          zr_aot_left->type);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            rightLiteral);
}
