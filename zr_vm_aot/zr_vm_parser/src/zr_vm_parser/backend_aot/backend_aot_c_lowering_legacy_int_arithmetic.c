#include "backend_aot_c_emitter.h"

/* 旧 INT opcode 仍由主 AOT 分派使用；值槽类型与解释器的快路径契约需逐项一致。 */

/* 常量版本在生成期展开字面量，失败时由生成代码报告不支持的常量类型。 */
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

/* 无法展开常量时仍输出合法 C；运行到该 opcode 才显式失败。 */
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

/* ADD_INT 来自 quickening，解释器按左操作数 signed/unsigned 类别保存结果。 */
/* BUG: 两个 unsigned 操作数可进入此 opcode；当前生成代码把两者转 signed
 * 且把结果标为 INT64，与解释器的 UINT64 路径不一致。见 execution_dispatch.c
 * 的 EXECUTE_ADD_INT_BODY；需以 UINT64_MAX 等输入做 AOT/解释器对照。 */
/* BUG: signed 输入相加越界时生成 C 直接执行 signed 加法，触发未定义行为。 */
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

/* 常量版本由同一 ADD_INT quickening 家族分派，常量类型仍须遵守运行时值标签。 */
/* BUG: unsigned 左值加 unsigned 常量在解释器返回 UINT64，而此处统一写 INT64；
 * 常量展开又先转 signed，UINT64_MAX 等输入会产生实现相关结果。 */
/* BUG: signed 左值与常量相加越界时生成 C 直接执行 signed 加法。 */
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

/* SUB_INT 延续解释器以左操作数类型标记结果的契约；生成代码仍读取 boxed 槽。 */
/* TODO: unsigned 高位输入被强转为 signed 后做减法；需核对解释器对混合符号值
 * 和溢出的确切语义，并用边界输入验证 C 生成结果。 */
/* BUG: signed 左右值相减越界时生成 C 未检查溢出。 */
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

/* 常量 SUB_INT 保留左值类型标签；常量的值表示应与解释器 CONST 路径一致。 */
/* TODO: unsigned 常量超过 INT64_MAX 时此处生成 signed 强转；需与解释器
 * EXECUTE_SUB_INT_CONST_BODY 做高位值及溢出对照。 */
/* BUG: signed 左值减常量越界时生成 C 未检查溢出。 */
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
