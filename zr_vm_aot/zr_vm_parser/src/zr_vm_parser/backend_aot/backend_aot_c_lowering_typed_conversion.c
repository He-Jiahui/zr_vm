#include "backend_aot_c_emitter.h"

/* 下列入口由 ExecIR 转换 opcode 分派调用，生成代码须在写回值槽前检查源值类型；
 * 保留显式有符号性路径，以匹配解释器与现役 AOT runtime 的类型标签契约。 */
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

/* TODO: 浮点源值为 NaN、无穷或超出 int64 可表示范围时直接 C 转型的契约未定义；
 * 解释器 execution_dispatch.c 的 TO_INT_FLOAT 和现役 aot_runtime_values.c 也使用同类转型，
 * 需在语言层确认范围语义并补边界一致性测试。 */
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

/* uint64 到 int64 采用显式模 2^64 映射，避免直接越界有符号转型依赖宿主实现。 */
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

/* TODO: 与 TO_INT_FLOAT 相同，负数、NaN、无穷及越界浮点值到 uint64 的语义待确认；
 * 核对 execution_dispatch.c 的 TO_UINT_FLOAT 与 aot_runtime_values.c，再补跨执行路径测试。 */
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
