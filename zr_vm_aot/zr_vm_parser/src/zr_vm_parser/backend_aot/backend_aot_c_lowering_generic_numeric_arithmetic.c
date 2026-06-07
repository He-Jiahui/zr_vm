#include "backend_aot_c_emitter.h"

/* backend_aot_c_lowering_generic_numeric_arithmetic.c */

static void backend_aot_c_write_generic_numeric_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        ZrCore_Debug_RunError(state, \"unsupported AOT generic numeric arithmetic\");\n"
            "        ZR_AOT_C_FAIL();\n");
}

static void backend_aot_c_write_generic_numeric_zero_guard(FILE *file,
                                                           const char *rightValueText,
                                                           const char *messageText) {
    if (file == ZR_NULL || rightValueText == ZR_NULL || messageText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "                if (ZR_UNLIKELY(%s == 0)) {\n"
            "                    ZrCore_Debug_RunError(state, \"%s\");\n"
            "                    ZR_AOT_C_FAIL();\n"
            "                }\n",
            rightValueText,
            messageText);
}

static void backend_aot_c_write_generic_numeric_extract_float64(FILE *file,
                                                                const char *targetName,
                                                                const char *valueName) {
    if (file == ZR_NULL || targetName == ZR_NULL || valueName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "                if (ZR_VALUE_IS_TYPE_FLOAT(%s->type)) {\n"
            "                    %s = %s->value.nativeObject.nativeDouble;\n"
            "                } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(%s->type)) {\n"
            "                    %s = (TZrFloat64)%s->value.nativeObject.nativeInt64;\n"
            "                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(%s->type)) {\n"
            "                    %s = (TZrFloat64)%s->value.nativeObject.nativeUInt64;\n"
            "                } else {\n",
            valueName,
            targetName,
            valueName,
            valueName,
            targetName,
            valueName,
            valueName,
            targetName,
            valueName);
    backend_aot_c_write_generic_numeric_unsupported(file);
    fprintf(file, "                }\n");
}

static void backend_aot_c_write_generic_numeric_extract_int64(FILE *file,
                                                              const char *targetName,
                                                              const char *valueName) {
    if (file == ZR_NULL || targetName == ZR_NULL || valueName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "                if (ZR_VALUE_IS_TYPE_SIGNED_INT(%s->type)) {\n"
            "                    %s = %s->value.nativeObject.nativeInt64;\n"
            "                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(%s->type)) {\n"
            "                    %s = (TZrInt64)%s->value.nativeObject.nativeUInt64;\n"
            "                } else {\n",
            valueName,
            targetName,
            valueName,
            valueName,
            targetName,
            valueName);
    backend_aot_c_write_generic_numeric_unsupported(file);
    fprintf(file, "                }\n");
}

static void backend_aot_c_write_generic_numeric_float_binary(FILE *file,
                                                             const char *expressionText,
                                                             const char *zeroGuardMessage) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "            {\n"
            "                TZrFloat64 zr_aot_left_float;\n"
            "                TZrFloat64 zr_aot_right_float;\n");
    backend_aot_c_write_generic_numeric_extract_float64(file, "zr_aot_left_float", "zr_aot_left");
    backend_aot_c_write_generic_numeric_extract_float64(file, "zr_aot_right_float", "zr_aot_right");
    backend_aot_c_write_generic_numeric_zero_guard(file, "zr_aot_right_float", zeroGuardMessage);
    fprintf(file,
            "                ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                                  nativeDouble,\n"
            "                                  %s,\n"
            "                                  ZR_VALUE_TYPE_DOUBLE);\n"
            "            }\n",
            expressionText);
}

static void backend_aot_c_write_generic_numeric_int_binary(FILE *file,
                                                           const char *expressionText,
                                                           const char *zeroGuardMessage) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "            {\n"
            "                TZrInt64 zr_aot_left_int;\n"
            "                TZrInt64 zr_aot_right_int;\n");
    backend_aot_c_write_generic_numeric_extract_int64(file, "zr_aot_left_int", "zr_aot_left");
    backend_aot_c_write_generic_numeric_extract_int64(file, "zr_aot_right_int", "zr_aot_right");
    backend_aot_c_write_generic_numeric_zero_guard(file, "zr_aot_right_int", zeroGuardMessage);
    fprintf(file,
            "                ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                                  nativeInt64,\n"
            "                                  %s,\n"
            "                                  ZR_VALUE_TYPE_INT64);\n"
            "            }\n",
            expressionText);
}

static void backend_aot_c_write_generic_numeric_uint_binary(FILE *file,
                                                            const char *expressionText,
                                                            const char *zeroGuardMessage) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "            {\n"
            "                TZrUInt64 zr_aot_left_uint = zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "                TZrUInt64 zr_aot_right_uint = zr_aot_right->value.nativeObject.nativeUInt64;\n");
    backend_aot_c_write_generic_numeric_zero_guard(file, "zr_aot_right_uint", zeroGuardMessage);
    fprintf(file,
            "                ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                                  nativeUInt64,\n"
            "                                  %s,\n"
            "                                  ZR_VALUE_TYPE_UINT64);\n"
            "            }\n",
            expressionText);
}

static void backend_aot_c_write_generic_numeric_binary(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       const char *floatExpressionText,
                                                       const char *intExpressionText,
                                                       const char *uintExpressionText,
                                                       TZrBool supportsFloat,
                                                       const char *zeroGuardMessage) {
    if (file == ZR_NULL || intExpressionText == ZR_NULL || uintExpressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_binary */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_NUMBER(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_NUMBER(zr_aot_right->type)) {\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_c_write_generic_numeric_unsupported(file);
    fprintf(file,
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type) || ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)) {\n");
    if (supportsFloat) {
        backend_aot_c_write_generic_numeric_float_binary(file, floatExpressionText, zeroGuardMessage);
    } else {
        backend_aot_c_write_generic_numeric_unsupported(file);
    }
    fprintf(file,
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type) &&\n"
            "                   ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n");
    backend_aot_c_write_generic_numeric_uint_binary(file, uintExpressionText, zeroGuardMessage);
    fprintf(file, "        } else {\n");
    backend_aot_c_write_generic_numeric_int_binary(file, intExpressionText, zeroGuardMessage);
    fprintf(file,
            "        }\n"
            "    }\n");
}

void backend_aot_write_c_direct_add(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_c_write_generic_numeric_binary(file,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               "zr_aot_left_float + zr_aot_right_float",
                                               "zr_aot_left_int + zr_aot_right_int",
                                               "zr_aot_left_uint + zr_aot_right_uint",
                                               ZR_TRUE,
                                               ZR_NULL);
}

void backend_aot_write_c_direct_sub(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_c_write_generic_numeric_binary(file,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               "zr_aot_left_float - zr_aot_right_float",
                                               "zr_aot_left_int - zr_aot_right_int",
                                               "zr_aot_left_uint - zr_aot_right_uint",
                                               ZR_TRUE,
                                               ZR_NULL);
}

void backend_aot_write_c_direct_mul(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_c_write_generic_numeric_binary(file,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               "zr_aot_left_float * zr_aot_right_float",
                                               "zr_aot_left_int * zr_aot_right_int",
                                               "zr_aot_left_uint * zr_aot_right_uint",
                                               ZR_TRUE,
                                               ZR_NULL);
}

void backend_aot_write_c_direct_div(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_c_write_generic_numeric_binary(file,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               "zr_aot_left_float / zr_aot_right_float",
                                               "zr_aot_left_int / zr_aot_right_int",
                                               "zr_aot_left_uint / zr_aot_right_uint",
                                               ZR_TRUE,
                                               "divide by zero");
}

void backend_aot_write_c_direct_mod(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot) {
    backend_aot_c_write_generic_numeric_binary(file,
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               "fmod(zr_aot_left_float, zr_aot_right_float)",
                                               "zr_aot_left_int % zr_aot_right_int",
                                               "zr_aot_left_uint % zr_aot_right_uint",
                                               ZR_TRUE,
                                               "modulo by zero");
}

void backend_aot_write_c_direct_neg(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_generic_numeric_unary */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              -zr_aot_source->value.nativeObject.nativeInt64,\n"
            "                              zr_aot_source->type);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              -(TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeDouble,\n"
            "                              -zr_aot_source->value.nativeObject.nativeDouble,\n"
            "                              zr_aot_source->type);\n"
            "        } else {\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_c_write_generic_numeric_unsupported(file);
    fprintf(file,
            "        }\n"
            "    }\n");
}
