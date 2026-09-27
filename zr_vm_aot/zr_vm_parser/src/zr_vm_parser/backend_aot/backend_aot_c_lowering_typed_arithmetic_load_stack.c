#include "backend_aot_c_emitter.h"

/* LOAD_STACK 的结果标签由 quickening 变体约定：加法沿用左值类型，乘法为 int64。 */
typedef enum EZrAotSignedLoadStackResultKind {
    ZR_AOT_SIGNED_LOAD_STACK_RESULT_LEFT_TYPE = 0,
    ZR_AOT_SIGNED_LOAD_STACK_RESULT_INT64
} EZrAotSignedLoadStackResultKind;

/* 函数体分派器只传已具备两个物化栈槽的指令；生成代码仍验证槽与整数标签，
 * 以免优化指令和实际运行时值偏离时读取错误的原生字段。 */
/* TODO: 此变体直接用 C int64 加法或乘法；若结果越界，需核对解释器对应 quickening
 * opcode 的语义并补跨执行路径边界值测试。 */
static void backend_aot_write_c_direct_signed_load_stack_binary(FILE *file,
                                                                const char *expressionText,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 rightSlot,
                                                                EZrAotSignedLoadStackResultKind resultKind) {
    const TZrChar *resultTypeText;

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    resultTypeText =
            resultKind == ZR_AOT_SIGNED_LOAD_STACK_RESULT_INT64 ? "ZR_VALUE_TYPE_INT64" : "zr_aot_left->type";

    fprintf(file,
            "    {\n"
            "        /* backend_aot_c_lowering_typed_arithmetic_load_stack.c */\n"
            "        /* zr_aot_arith_exec_signed_load_stack */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n"
            "        EZrValueType zr_aot_load_stack_result_type;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        zr_aot_load_stack_result_type = %s;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          zr_aot_load_stack_result_type);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            resultTypeText,
            expressionText);
}

void backend_aot_write_c_direct_add_signed_load_stack(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_load_stack_binary(file,
                                                        "zr_aot_left_scalar + zr_aot_right_scalar",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot,
                                                        ZR_AOT_SIGNED_LOAD_STACK_RESULT_LEFT_TYPE);
}

void backend_aot_write_c_direct_mul_signed_load_stack(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_load_stack_binary(file,
                                                        "zr_aot_left_scalar * zr_aot_right_scalar",
                                                        destinationSlot,
                                                        leftSlot,
                                                        rightSlot,
                                                        ZR_AOT_SIGNED_LOAD_STACK_RESULT_INT64);
}
