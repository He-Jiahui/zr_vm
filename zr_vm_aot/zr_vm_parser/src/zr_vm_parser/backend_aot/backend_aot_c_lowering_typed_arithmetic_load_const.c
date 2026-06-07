#include "backend_aot_c_emitter.h"

typedef enum EZrAotSignedLoadConstResultKind {
    ZR_AOT_SIGNED_LOAD_CONST_RESULT_LEFT_TYPE = 0,
    ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64
} EZrAotSignedLoadConstResultKind;

typedef enum EZrAotSignedLoadConstZeroGuard {
    ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE = 0,
    ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_DIVIDE,
    ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_MODULO
} EZrAotSignedLoadConstZeroGuard;

static TZrBool backend_aot_c_format_signed_load_const_integer_literal(char *buffer,
                                                                      TZrSize bufferSize,
                                                                      const SZrTypeValue *constantValue) {
    if (buffer == ZR_NULL || bufferSize == 0 || constantValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    snprintf(buffer,
             (size_t)bufferSize,
             "(TZrInt64)%lld",
             (long long)constantValue->value.nativeObject.nativeInt64);
    return ZR_TRUE;
}

static const TZrChar *backend_aot_c_signed_load_const_value_type_literal(EZrValueType type) {
    switch (type) {
        case ZR_VALUE_TYPE_INT8:
            return "ZR_VALUE_TYPE_INT8";
        case ZR_VALUE_TYPE_INT16:
            return "ZR_VALUE_TYPE_INT16";
        case ZR_VALUE_TYPE_INT32:
            return "ZR_VALUE_TYPE_INT32";
        case ZR_VALUE_TYPE_INT64:
            return "ZR_VALUE_TYPE_INT64";
        default:
            return "ZR_VALUE_TYPE_UNKNOWN";
    }
}

static void backend_aot_write_c_direct_signed_load_const_unsupported(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        ZrCore_Debug_RunError(state, \"unsupported typed signed load-const arithmetic constant\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n");
}

static void backend_aot_write_c_direct_signed_load_const_zero_guard(FILE *file,
                                                                    EZrAotSignedLoadConstZeroGuard zeroGuard) {
    if (file == ZR_NULL || zeroGuard == ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE) {
        return;
    }

    if (zeroGuard == ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_DIVIDE) {
        fprintf(file,
                "        if (ZR_UNLIKELY(zr_aot_right_literal == 0)) {\n"
                "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    } else if (zeroGuard == ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_MODULO) {
        fprintf(file,
                "        if (ZR_UNLIKELY(zr_aot_right_literal == 0)) {\n"
                "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n"
                "        if (ZR_UNLIKELY(zr_aot_right_literal < 0)) {\n"
                "            zr_aot_right_literal = -zr_aot_right_literal;\n"
                "        }\n");
    }
}

static void backend_aot_write_c_direct_signed_load_const_binary(FILE *file,
                                                                const SZrFunction *function,
                                                                const char *expressionText,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 leftSlot,
                                                                TZrUInt32 materializedConstantSlot,
                                                                TZrUInt32 constantIndex,
                                                                EZrAotSignedLoadConstResultKind resultKind,
                                                                EZrAotSignedLoadConstZeroGuard zeroGuard) {
    const SZrTypeValue *constantValue;
    const TZrChar *constantTypeText;
    const TZrChar *resultTypeText;
    char rightLiteral[64];

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_signed_load_const_integer_literal(
                rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_signed_load_const_unsupported(file);
        return;
    }

    constantTypeText = backend_aot_c_signed_load_const_value_type_literal(constantValue->type);
    resultTypeText = resultKind == ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64
                             ? "ZR_VALUE_TYPE_INT64"
                             : "zr_aot_left->type";

    fprintf(file,
            "    {\n"
            "        /* backend_aot_c_lowering_typed_arithmetic_load_const.c */\n"
            "        /* zr_aot_arith_exec_signed_load_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_materialized_constant = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        EZrValueType zr_aot_load_const_result_type;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_materialized_constant == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_materialized_constant,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          %s);\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)materializedConstantSlot,
            rightLiteral,
            rightLiteral,
            constantTypeText);

    backend_aot_write_c_direct_signed_load_const_zero_guard(file, zeroGuard);

    fprintf(file,
            "        zr_aot_load_const_result_type = %s;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          zr_aot_load_const_result_type);\n"
            "    }\n",
            resultTypeText,
            expressionText);
}

static void backend_aot_write_c_direct_signed_load_stack_const_binary(FILE *file,
                                                                      const SZrFunction *function,
                                                                      const char *expressionText,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 sourceSlot,
                                                                      TZrUInt32 materializedStackSlot,
                                                                      TZrUInt32 constantIndex,
                                                                      EZrAotSignedLoadConstResultKind resultKind,
                                                                      EZrAotSignedLoadConstZeroGuard zeroGuard) {
    const SZrTypeValue *constantValue;
    const TZrChar *resultTypeText;
    char rightLiteral[64];

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_signed_load_const_integer_literal(
                rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_signed_load_const_unsupported(file);
        return;
    }

    resultTypeText = resultKind == ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64
                             ? "ZR_VALUE_TYPE_INT64"
                             : "zr_aot_materialized_left->type";

    fprintf(file,
            "    {\n"
            "        /* backend_aot_c_lowering_typed_arithmetic_load_const.c */\n"
            "        /* zr_aot_arith_exec_signed_load_stack_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_materialized_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        EZrValueType zr_aot_load_stack_const_result_type;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL || zr_aot_materialized_left == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        *zr_aot_materialized_left = *zr_aot_source;\n"
            "        zr_aot_left_scalar = zr_aot_materialized_left->value.nativeObject.nativeInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)materializedStackSlot,
            rightLiteral);

    backend_aot_write_c_direct_signed_load_const_zero_guard(file, zeroGuard);

    fprintf(file,
            "        zr_aot_load_stack_const_result_type = %s;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          zr_aot_load_stack_const_result_type);\n"
            "    }\n",
            resultTypeText,
            expressionText);
}

static void backend_aot_write_c_direct_signed_load_stack_load_const_binary(FILE *file,
                                                                           const SZrFunction *function,
                                                                           const char *expressionText,
                                                                           TZrUInt32 destinationSlot,
                                                                           TZrUInt32 sourceSlot,
                                                                           TZrUInt32 materializedStackSlot,
                                                                           TZrUInt32 materializedConstantSlot,
                                                                           TZrUInt32 constantIndex,
                                                                           EZrAotSignedLoadConstResultKind resultKind,
                                                                           EZrAotSignedLoadConstZeroGuard zeroGuard) {
    const SZrTypeValue *constantValue;
    const TZrChar *constantTypeText;
    const TZrChar *resultTypeText;
    char rightLiteral[64];

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_signed_load_const_integer_literal(
                rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_signed_load_const_unsupported(file);
        return;
    }

    constantTypeText = backend_aot_c_signed_load_const_value_type_literal(constantValue->type);
    resultTypeText = resultKind == ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64
                             ? "ZR_VALUE_TYPE_INT64"
                             : "zr_aot_materialized_left->type";

    fprintf(file,
            "    {\n"
            "        /* backend_aot_c_lowering_typed_arithmetic_load_const.c */\n"
            "        /* zr_aot_arith_exec_signed_load_stack_load_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_materialized_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        SZrTypeValue *zr_aot_materialized_constant = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        EZrValueType zr_aot_load_stack_load_const_result_type;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_source == ZR_NULL ||\n"
            "            zr_aot_materialized_left == ZR_NULL || zr_aot_materialized_constant == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        *zr_aot_materialized_left = *zr_aot_source;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_materialized_constant,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          %s);\n"
            "        zr_aot_left_scalar = zr_aot_materialized_left->value.nativeObject.nativeInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            (unsigned)materializedStackSlot,
            (unsigned)materializedConstantSlot,
            rightLiteral,
            rightLiteral,
            constantTypeText);

    backend_aot_write_c_direct_signed_load_const_zero_guard(file, zeroGuard);

    fprintf(file,
            "        zr_aot_load_stack_load_const_result_type = %s;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          zr_aot_load_stack_load_const_result_type);\n"
            "    }\n",
            resultTypeText,
            expressionText);
}

void backend_aot_write_c_direct_add_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_const_binary(file,
                                                        function,
                                                        "zr_aot_left_scalar + zr_aot_right_literal",
                                                        destinationSlot,
                                                        leftSlot,
                                                        materializedConstantSlot,
                                                        constantIndex,
                                                        ZR_AOT_SIGNED_LOAD_CONST_RESULT_LEFT_TYPE,
                                                        ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_sub_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_const_binary(file,
                                                        function,
                                                        "zr_aot_left_scalar - zr_aot_right_literal",
                                                        destinationSlot,
                                                        leftSlot,
                                                        materializedConstantSlot,
                                                        constantIndex,
                                                        ZR_AOT_SIGNED_LOAD_CONST_RESULT_LEFT_TYPE,
                                                        ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_mul_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_const_binary(file,
                                                        function,
                                                        "zr_aot_left_scalar * zr_aot_right_literal",
                                                        destinationSlot,
                                                        leftSlot,
                                                        materializedConstantSlot,
                                                        constantIndex,
                                                        ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64,
                                                        ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_div_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_const_binary(file,
                                                        function,
                                                        "zr_aot_left_scalar / zr_aot_right_literal",
                                                        destinationSlot,
                                                        leftSlot,
                                                        materializedConstantSlot,
                                                        constantIndex,
                                                        ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64,
                                                        ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_DIVIDE);
}

void backend_aot_write_c_direct_mod_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_const_binary(file,
                                                        function,
                                                        "zr_aot_left_scalar % zr_aot_right_literal",
                                                        destinationSlot,
                                                        leftSlot,
                                                        materializedConstantSlot,
                                                        constantIndex,
                                                        ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64,
                                                        ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_MODULO);
}

void backend_aot_write_c_direct_add_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_stack_const_binary(file,
                                                              function,
                                                              "zr_aot_left_scalar + zr_aot_right_literal",
                                                              destinationSlot,
                                                              sourceSlot,
                                                              materializedStackSlot,
                                                              constantIndex,
                                                              ZR_AOT_SIGNED_LOAD_CONST_RESULT_LEFT_TYPE,
                                                              ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_add_signed_load_stack_load_const(FILE *file,
                                                                 const SZrFunction *function,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 sourceSlot,
                                                                 TZrUInt32 materializedStackSlot,
                                                                 TZrUInt32 materializedConstantSlot,
                                                                 TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_stack_load_const_binary(file,
                                                                   function,
                                                                   "zr_aot_left_scalar + zr_aot_right_literal",
                                                                   destinationSlot,
                                                                   sourceSlot,
                                                                   materializedStackSlot,
                                                                   materializedConstantSlot,
                                                                   constantIndex,
                                                                   ZR_AOT_SIGNED_LOAD_CONST_RESULT_LEFT_TYPE,
                                                                   ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_sub_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_stack_const_binary(file,
                                                              function,
                                                              "zr_aot_left_scalar - zr_aot_right_literal",
                                                              destinationSlot,
                                                              sourceSlot,
                                                              materializedStackSlot,
                                                              constantIndex,
                                                              ZR_AOT_SIGNED_LOAD_CONST_RESULT_LEFT_TYPE,
                                                              ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_mul_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_stack_const_binary(file,
                                                              function,
                                                              "zr_aot_left_scalar * zr_aot_right_literal",
                                                              destinationSlot,
                                                              sourceSlot,
                                                              materializedStackSlot,
                                                              constantIndex,
                                                              ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64,
                                                              ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_div_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_stack_const_binary(file,
                                                              function,
                                                              "zr_aot_left_scalar / zr_aot_right_literal",
                                                              destinationSlot,
                                                              sourceSlot,
                                                              materializedStackSlot,
                                                              constantIndex,
                                                              ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64,
                                                              ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_DIVIDE);
}

void backend_aot_write_c_direct_mod_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_load_stack_const_binary(file,
                                                              function,
                                                              "zr_aot_left_scalar % zr_aot_right_literal",
                                                              destinationSlot,
                                                              sourceSlot,
                                                              materializedStackSlot,
                                                              constantIndex,
                                                              ZR_AOT_SIGNED_LOAD_CONST_RESULT_INT64,
                                                              ZR_AOT_SIGNED_LOAD_CONST_ZERO_GUARD_MODULO);
}
