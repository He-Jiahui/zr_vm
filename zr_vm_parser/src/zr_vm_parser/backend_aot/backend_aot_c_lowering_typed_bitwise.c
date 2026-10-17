#include "backend_aot/backend_aot_c_emitter.h"

/* 位运算的有符号读取路径兼容 signed/unsigned 整数标签；结果统一写为 int64。 */
static void backend_aot_write_c_integer_like_extract(FILE *file, const char *valueName, const char *outName) {
    if (file == ZR_NULL || valueName == ZR_NULL || outName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(%s->type)) {\n"
            "            %s = %s->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(%s->type)) {\n"
            "            %s = (TZrInt64)%s->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            valueName,
            outName,
            valueName,
            valueName,
            outName,
            valueName);
}

/* 左移和逻辑右移改用 uint64 计算，避免左移负数的 C 未定义行为。 */
static void backend_aot_write_c_unsigned_integer_like_extract(FILE *file,
                                                              const char *valueName,
                                                              const char *outName) {
    if (file == ZR_NULL || valueName == ZR_NULL || outName == ZR_NULL) {
        return;
    }

    fprintf(file,
            "        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(%s->type)) {\n"
            "            %s = %s->value.nativeObject.nativeUInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(%s->type)) {\n"
            "            %s = (TZrUInt64)%s->value.nativeObject.nativeInt64;\n"
            "        } else {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            valueName,
            outName,
            valueName,
            valueName,
            outName,
            valueName);
}

/* The copied shift count is masked to six bits, matching the VM contract. */
static void backend_aot_write_c_shift_count_guard(FILE *file, const char *shiftCountName) {
    if (file == ZR_NULL || shiftCountName == ZR_NULL) {
        return;
    }

    fprintf(file, "        %s = (TZrInt64)((TZrUInt64)%s & 63u);\n",
            shiftCountName, shiftCountName);
}

static void backend_aot_write_c_direct_bitwise_unary(FILE *file,
                                                     const char *expressionText,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bitwise_exec_unary */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_source_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
    backend_aot_write_c_integer_like_extract(file, "zr_aot_source", "zr_aot_source_scalar");
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            expressionText);
}

static void backend_aot_write_c_direct_bitwise_binary(FILE *file,
                                                      const char *expressionText,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bitwise_exec_binary */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_write_c_integer_like_extract(file, "zr_aot_left", "zr_aot_left_scalar");
    backend_aot_write_c_integer_like_extract(file, "zr_aot_right", "zr_aot_right_scalar");
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            expressionText);
}

/* Signed right shifts use the shared, explicit sign-extension rule. */
static void backend_aot_write_c_direct_signed_shift(FILE *file,
                                                    const char *expressionText,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_bitwise_exec_binary */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_shift_count;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_write_c_integer_like_extract(file, "zr_aot_left", "zr_aot_left_scalar");
    backend_aot_write_c_integer_like_extract(file, "zr_aot_right", "zr_aot_shift_count");
    backend_aot_write_c_shift_count_guard(file, "zr_aot_shift_count");
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            expressionText);
}

/* 通用 SHIFT opcode 先验证双侧都是整数，再复用类型提取与位数保护；
 * 类型不匹配属于运行时失败，不应按 quickening 的静态类型直接读字段。 */
static void backend_aot_write_c_direct_generic_shift(FILE *file,
                                                     const char *marker,
                                                     const char *expressionText,
                                                     TZrBool useUnsignedLeft,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot) {
    if (file == ZR_NULL || marker == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* %s */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n",
            marker,
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    if (useUnsignedLeft) {
        fprintf(file, "        TZrUInt64 zr_aot_left_unsigned;\n");
    } else {
        fprintf(file, "        TZrInt64 zr_aot_left_scalar;\n");
    }
    fprintf(file,
            "        TZrInt64 zr_aot_shift_count;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_INT(zr_aot_right->type)) {\n"
            "            ZrCore_Debug_RunError(state, \"unsupported AOT generic integer shift\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n");
    if (useUnsignedLeft) {
        backend_aot_write_c_unsigned_integer_like_extract(file, "zr_aot_left", "zr_aot_left_unsigned");
    } else {
        backend_aot_write_c_integer_like_extract(file, "zr_aot_left", "zr_aot_left_scalar");
    }
    backend_aot_write_c_integer_like_extract(file, "zr_aot_right", "zr_aot_shift_count");
    backend_aot_write_c_shift_count_guard(file, "zr_aot_shift_count");
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            expressionText);
}

void backend_aot_write_c_direct_bitwise_not(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_bitwise_unary(file, "~zr_aot_source_scalar", destinationSlot, sourceSlot);
}

void backend_aot_write_c_direct_bitwise_and(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_bitwise_binary(file,
                                              "zr_aot_left_scalar & zr_aot_right_scalar",
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot);
}

void backend_aot_write_c_direct_bitwise_or(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_bitwise_binary(file,
                                              "zr_aot_left_scalar | zr_aot_right_scalar",
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot);
}

void backend_aot_write_c_direct_bitwise_xor(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_bitwise_binary(file,
                                              "zr_aot_left_scalar ^ zr_aot_right_scalar",
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot);
}

void backend_aot_write_c_direct_shift_left_int(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_shift(file,
            "ZrCore_Integer_ShiftLeft(zr_aot_left_scalar, (TZrUInt64)zr_aot_shift_count)",
            destinationSlot, leftSlot, rightSlot);
}

void backend_aot_write_c_direct_shift_right_int(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_shift(file,
                                            "ZrCore_Integer_ShiftRight(zr_aot_left_scalar, (TZrUInt64)zr_aot_shift_count)",
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot);
}

void backend_aot_write_c_direct_shift_left(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_generic_shift(file,
                                             "zr_aot_shift_exec_generic_left",
                                             "ZrCore_Integer_FromBits(zr_aot_left_unsigned << zr_aot_shift_count)",
                                             ZR_TRUE,
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot);
}

void backend_aot_write_c_direct_shift_right(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_generic_shift(file,
                                             "zr_aot_shift_exec_generic_right",
                                             "ZrCore_Integer_ShiftRight(zr_aot_left_scalar, (TZrUInt64)zr_aot_shift_count)",
                                             ZR_FALSE,
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot);
}

void backend_aot_write_c_direct_bitwise_shift_left(FILE *file,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    fprintf(file, "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BitwiseShiftLeft(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot, (unsigned)leftSlot, (unsigned)rightSlot);
}

void backend_aot_write_c_direct_bitwise_shift_right(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    fprintf(file, "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BitwiseShiftRight(state, &frame, %u, %u, %u));\n",
            (unsigned)destinationSlot, (unsigned)leftSlot, (unsigned)rightSlot);
}
