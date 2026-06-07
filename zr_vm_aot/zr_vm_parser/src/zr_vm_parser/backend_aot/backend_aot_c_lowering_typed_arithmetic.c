#include "backend_aot_c_emitter.h"

static TZrBool backend_aot_c_format_signed_integer_literal(char *buffer,
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

static TZrBool backend_aot_c_format_unsigned_integer_literal(char *buffer,
                                                             TZrSize bufferSize,
                                                             const SZrTypeValue *constantValue) {
    if (buffer == ZR_NULL || bufferSize == 0 || constantValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    snprintf(buffer,
             (size_t)bufferSize,
             "(TZrUInt64)%llu",
             (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
    return ZR_TRUE;
}

static void backend_aot_write_c_direct_typed_arithmetic_const_fail(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        ZrCore_Debug_RunError(state, \"unsupported typed arithmetic constant\");\n"
            "        ZR_AOT_C_FAIL();\n"
            "    }\n");
}

typedef enum EZrAotTypedArithmeticZeroGuard {
    ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE = 0,
    ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_DIVIDE,
    ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_MODULO
} EZrAotTypedArithmeticZeroGuard;

static void backend_aot_write_c_direct_integer_zero_guard(FILE *file,
                                                          const char *rightValueText,
                                                          EZrAotTypedArithmeticZeroGuard zeroGuard) {
    if (file == ZR_NULL || rightValueText == ZR_NULL ||
        zeroGuard == ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE) {
        return;
    }

    if (zeroGuard == ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_DIVIDE) {
        fprintf(file,
                "        if (ZR_UNLIKELY(%s == 0)) {\n"
                "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                rightValueText);
    } else if (zeroGuard == ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_MODULO) {
        fprintf(file,
                "        if (ZR_UNLIKELY(%s == 0)) {\n"
                "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n",
                rightValueText);
    }
}

static void backend_aot_write_c_direct_signed_binary(FILE *file,
                                                     const char *expressionText,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot,
                                                     EZrAotTypedArithmeticZeroGuard zeroGuard) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_signed */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_write_c_direct_integer_zero_guard(file, "zr_aot_right_scalar", zeroGuard);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            expressionText);
}

static void backend_aot_write_c_direct_signed_const_binary(FILE *file,
                                                           const SZrFunction *function,
                                                           const char *expressionText,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 constantIndex,
                                                           EZrAotTypedArithmeticZeroGuard zeroGuard) {
    const SZrTypeValue *constantValue;
    char rightLiteral[64];

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_signed_integer_literal(rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_typed_arithmetic_const_fail(file);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_signed_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_literal = %s;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            rightLiteral);
    backend_aot_write_c_direct_integer_zero_guard(file, "zr_aot_right_literal", zeroGuard);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            expressionText);
}

static void backend_aot_write_c_direct_unsigned_binary(FILE *file,
                                                       const char *expressionText,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       EZrAotTypedArithmeticZeroGuard zeroGuard) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_unsigned */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrUInt64 zr_aot_left_scalar;\n"
            "        TZrUInt64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeUInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    backend_aot_write_c_direct_integer_zero_guard(file, "zr_aot_right_scalar", zeroGuard);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeUInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_UINT64);\n"
            "    }\n",
            expressionText);
}

static void backend_aot_write_c_direct_unsigned_const_binary(FILE *file,
                                                             const SZrFunction *function,
                                                             const char *expressionText,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 constantIndex,
                                                             EZrAotTypedArithmeticZeroGuard zeroGuard) {
    const SZrTypeValue *constantValue;
    char rightLiteral[64];

    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_unsigned_integer_literal(rightLiteral, (TZrSize)sizeof(rightLiteral), constantValue)) {
        backend_aot_write_c_direct_typed_arithmetic_const_fail(file);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_unsigned_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrUInt64 zr_aot_left_scalar;\n"
            "        TZrUInt64 zr_aot_right_literal = %s;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeUInt64;\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            rightLiteral);
    backend_aot_write_c_direct_integer_zero_guard(file, "zr_aot_right_literal", zeroGuard);
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeUInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_UINT64);\n"
            "    }\n",
            expressionText);
}

static void backend_aot_write_c_direct_float_binary(FILE *file,
                                                    const char *expressionText,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot,
                                                    TZrBool checkDivideByZero) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_float */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrFloat64 zr_aot_left_scalar;\n"
            "        TZrFloat64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeDouble;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeDouble;\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
    if (checkDivideByZero) {
        fprintf(file,
                "        if (ZR_UNLIKELY(zr_aot_right_scalar == 0.0)) {\n"
                "            ZrCore_Debug_RunError(state, \"divide by zero\");\n"
                "            ZR_AOT_C_FAIL();\n"
                "        }\n");
    }
    fprintf(file,
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeDouble,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_DOUBLE);\n"
            "    }\n",
            expressionText);
}

static void backend_aot_write_c_direct_signed_unary(FILE *file,
                                                    const char *expressionText,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_signed_unary */\n"
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
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          zr_aot_source->type);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            expressionText);
}

static void backend_aot_write_c_direct_float_unary(FILE *file,
                                                   const char *expressionText,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_float_unary */\n"
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
            "                          nativeDouble,\n"
            "                          %s,\n"
            "                          zr_aot_source->type);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot,
            expressionText);
}

static void backend_aot_write_c_direct_signed_comparison(FILE *file,
                                                         const char *expressionText,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_compare_exec_signed */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeBool,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_BOOL);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            expressionText);
}

static void backend_aot_write_c_direct_unsigned_comparison(FILE *file,
                                                           const char *expressionText,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 rightSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_compare_exec_unsigned */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrUInt64 zr_aot_left_scalar;\n"
            "        TZrUInt64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeBool,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_BOOL);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            expressionText);
}

static void backend_aot_write_c_direct_float_comparison(FILE *file,
                                                        const char *expressionText,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot) {
    if (file == ZR_NULL || expressionText == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_compare_exec_float */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrFloat64 zr_aot_left_scalar;\n"
            "        TZrFloat64 zr_aot_right_scalar;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_FLOAT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeDouble;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeDouble;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeBool,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_BOOL);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            expressionText);
}

void backend_aot_write_c_direct_add_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_binary(file,
                                             "zr_aot_left_scalar + zr_aot_right_scalar",
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot,
                                             ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_add_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_binary(file,
                                               "zr_aot_left_scalar + zr_aot_right_scalar",
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_add_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_const_binary(file,
                                                   function,
                                                   "zr_aot_left_scalar + zr_aot_right_literal",
                                                   destinationSlot,
                                                   leftSlot,
                                                   constantIndex,
                                                   ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_add_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_unsigned_const_binary(file,
                                                     function,
                                                     "zr_aot_left_scalar + zr_aot_right_literal",
                                                     destinationSlot,
                                                     leftSlot,
                                                     constantIndex,
                                                     ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_sub_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_binary(file,
                                             "zr_aot_left_scalar - zr_aot_right_scalar",
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot,
                                             ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_sub_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_binary(file,
                                               "zr_aot_left_scalar - zr_aot_right_scalar",
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_sub_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_const_binary(file,
                                                   function,
                                                   "zr_aot_left_scalar - zr_aot_right_literal",
                                                   destinationSlot,
                                                   leftSlot,
                                                   constantIndex,
                                                   ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_sub_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_unsigned_const_binary(file,
                                                     function,
                                                     "zr_aot_left_scalar - zr_aot_right_literal",
                                                     destinationSlot,
                                                     leftSlot,
                                                     constantIndex,
                                                     ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_mul_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_binary(file,
                                             "zr_aot_left_scalar * zr_aot_right_scalar",
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot,
                                             ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_mul_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_binary(file,
                                               "zr_aot_left_scalar * zr_aot_right_scalar",
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_mul_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_const_binary(file,
                                                   function,
                                                   "zr_aot_left_scalar * zr_aot_right_literal",
                                                   destinationSlot,
                                                   leftSlot,
                                                   constantIndex,
                                                   ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_mul_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_unsigned_const_binary(file,
                                                     function,
                                                     "zr_aot_left_scalar * zr_aot_right_literal",
                                                     destinationSlot,
                                                     leftSlot,
                                                     constantIndex,
                                                     ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_NONE);
}

void backend_aot_write_c_direct_div_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_binary(file,
                                             "zr_aot_left_scalar / zr_aot_right_scalar",
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot,
                                             ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_DIVIDE);
}

void backend_aot_write_c_direct_div_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_binary(file,
                                               "zr_aot_left_scalar / zr_aot_right_scalar",
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_DIVIDE);
}

void backend_aot_write_c_direct_div_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_signed_const_binary(file,
                                                   function,
                                                   "zr_aot_left_scalar / zr_aot_right_literal",
                                                   destinationSlot,
                                                   leftSlot,
                                                   constantIndex,
                                                   ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_DIVIDE);
}

void backend_aot_write_c_direct_div_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex) {
    backend_aot_write_c_direct_unsigned_const_binary(file,
                                                     function,
                                                     "zr_aot_left_scalar / zr_aot_right_literal",
                                                     destinationSlot,
                                                     leftSlot,
                                                     constantIndex,
                                                     ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_DIVIDE);
}

void backend_aot_write_c_direct_mod_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot) {
    if (file != ZR_NULL) {
        fprintf(file, "    /* zr_aot_arith_exec_signed_mod */\n");
    }
    backend_aot_write_c_direct_signed_binary(file,
                                             "zr_aot_left_scalar % zr_aot_right_scalar",
                                             destinationSlot,
                                             leftSlot,
                                             rightSlot,
                                             ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_MODULO);
}

void backend_aot_write_c_direct_mod_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot) {
    if (file != ZR_NULL) {
        fprintf(file, "    /* zr_aot_arith_exec_unsigned_mod */\n");
    }
    backend_aot_write_c_direct_unsigned_binary(file,
                                               "zr_aot_left_scalar % zr_aot_right_scalar",
                                               destinationSlot,
                                               leftSlot,
                                               rightSlot,
                                               ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_MODULO);
}

void backend_aot_write_c_direct_mod_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex) {
    if (file != ZR_NULL) {
        fprintf(file, "    /* zr_aot_arith_exec_signed_mod */\n");
    }
    backend_aot_write_c_direct_signed_const_binary(file,
                                                   function,
                                                   "zr_aot_left_scalar % zr_aot_right_literal",
                                                   destinationSlot,
                                                   leftSlot,
                                                   constantIndex,
                                                   ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_MODULO);
}

void backend_aot_write_c_direct_add_signed_mod_const(FILE *file,
                                                     const SZrFunction *function,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot,
                                                     TZrUInt32 constantIndex) {
    const char *expressionText = "(zr_aot_left_scalar + zr_aot_right_scalar) % zr_aot_mod_literal";
    const SZrTypeValue *constantValue;
    char modLiteral[64];

    if (file == ZR_NULL) {
        return;
    }

    constantValue = backend_aot_c_get_constant_value(function, (TZrInt32)constantIndex);
    if (!backend_aot_c_format_signed_integer_literal(modLiteral, (TZrSize)sizeof(modLiteral), constantValue)) {
        backend_aot_write_c_direct_typed_arithmetic_const_fail(file);
        return;
    }

    fprintf(file,
            "    {\n"
            "        /* zr_aot_arith_exec_signed_add_mod_const */\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_scalar;\n"
            "        TZrInt64 zr_aot_right_scalar;\n"
            "        TZrInt64 zr_aot_mod_literal = %s;\n"
            "        if (zr_aot_destination == ZR_NULL || zr_aot_left == ZR_NULL || zr_aot_right == ZR_NULL) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) || !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_UNLIKELY(zr_aot_mod_literal == 0)) {\n"
            "            ZrCore_Debug_RunError(state, \"modulo by zero\");\n"
            "            ZR_AOT_C_FAIL();\n"
            "        }\n"
            "        if (ZR_UNLIKELY(zr_aot_mod_literal < 0)) {\n"
            "            zr_aot_mod_literal = -zr_aot_mod_literal;\n"
            "        }\n"
            "        zr_aot_left_scalar = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        zr_aot_right_scalar = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          %s,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot,
            modLiteral,
            expressionText);
}

void backend_aot_write_c_direct_mod_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex) {
    if (file != ZR_NULL) {
        fprintf(file, "    /* zr_aot_arith_exec_unsigned_mod */\n");
    }
    backend_aot_write_c_direct_unsigned_const_binary(file,
                                                     function,
                                                     "zr_aot_left_scalar % zr_aot_right_literal",
                                                     destinationSlot,
                                                     leftSlot,
                                                     constantIndex,
                                                     ZR_AOT_TYPED_ARITHMETIC_ZERO_GUARD_MODULO);
}

void backend_aot_write_c_direct_add_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_binary(file,
                                            "zr_aot_left_scalar + zr_aot_right_scalar",
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            ZR_FALSE);
}

void backend_aot_write_c_direct_sub_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_binary(file,
                                            "zr_aot_left_scalar - zr_aot_right_scalar",
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            ZR_FALSE);
}

void backend_aot_write_c_direct_mul_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_binary(file,
                                            "zr_aot_left_scalar * zr_aot_right_scalar",
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            ZR_FALSE);
}

void backend_aot_write_c_direct_div_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_binary(file,
                                            "zr_aot_left_scalar / zr_aot_right_scalar",
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            ZR_TRUE);
}

void backend_aot_write_c_direct_neg_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_signed_unary(file, "-zr_aot_source_scalar", destinationSlot, sourceSlot);
}

void backend_aot_write_c_direct_neg_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot) {
    backend_aot_write_c_direct_float_unary(file, "-zr_aot_source_scalar", destinationSlot, sourceSlot);
}

void backend_aot_write_c_direct_logical_equal_signed(FILE *file,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_comparison(file,
                                                 "zr_aot_left_scalar == zr_aot_right_scalar",
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
}

void backend_aot_write_c_direct_logical_not_equal_signed(FILE *file,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_comparison(file,
                                                 "zr_aot_left_scalar != zr_aot_right_scalar",
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
}

void backend_aot_write_c_direct_logical_equal_unsigned(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_comparison(file,
                                                   "zr_aot_left_scalar == zr_aot_right_scalar",
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
}

void backend_aot_write_c_direct_logical_not_equal_unsigned(FILE *file,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_comparison(file,
                                                   "zr_aot_left_scalar != zr_aot_right_scalar",
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
}

void backend_aot_write_c_direct_logical_equal_float(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_comparison(file,
                                                "zr_aot_left_scalar == zr_aot_right_scalar",
                                                destinationSlot,
                                                leftSlot,
                                                rightSlot);
}

void backend_aot_write_c_direct_logical_not_equal_float(FILE *file,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_comparison(file,
                                                "zr_aot_left_scalar != zr_aot_right_scalar",
                                                destinationSlot,
                                                leftSlot,
                                                rightSlot);
}

void backend_aot_write_c_direct_logical_less_signed(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_comparison(file,
                                                 "zr_aot_left_scalar < zr_aot_right_scalar",
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
}

void backend_aot_write_c_direct_logical_less_unsigned(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_comparison(file,
                                                   "zr_aot_left_scalar < zr_aot_right_scalar",
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
}

void backend_aot_write_c_direct_logical_less_float(FILE *file,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_comparison(file,
                                                "zr_aot_left_scalar < zr_aot_right_scalar",
                                                destinationSlot,
                                                leftSlot,
                                                rightSlot);
}

void backend_aot_write_c_direct_logical_greater_signed(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_comparison(file,
                                                 "zr_aot_left_scalar > zr_aot_right_scalar",
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
}

void backend_aot_write_c_direct_logical_greater_unsigned(FILE *file,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_comparison(file,
                                                   "zr_aot_left_scalar > zr_aot_right_scalar",
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
}

void backend_aot_write_c_direct_logical_greater_float(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_comparison(file,
                                                "zr_aot_left_scalar > zr_aot_right_scalar",
                                                destinationSlot,
                                                leftSlot,
                                                rightSlot);
}

void backend_aot_write_c_direct_logical_less_equal_signed(FILE *file,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_comparison(file,
                                                 "zr_aot_left_scalar <= zr_aot_right_scalar",
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
}

void backend_aot_write_c_direct_logical_less_equal_unsigned(FILE *file,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_comparison(file,
                                                   "zr_aot_left_scalar <= zr_aot_right_scalar",
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
}

void backend_aot_write_c_direct_logical_less_equal_float(FILE *file,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_comparison(file,
                                                "zr_aot_left_scalar <= zr_aot_right_scalar",
                                                destinationSlot,
                                                leftSlot,
                                                rightSlot);
}

void backend_aot_write_c_direct_logical_greater_equal_signed(FILE *file,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_signed_comparison(file,
                                                 "zr_aot_left_scalar >= zr_aot_right_scalar",
                                                 destinationSlot,
                                                 leftSlot,
                                                 rightSlot);
}

void backend_aot_write_c_direct_logical_greater_equal_unsigned(FILE *file,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_unsigned_comparison(file,
                                                   "zr_aot_left_scalar >= zr_aot_right_scalar",
                                                   destinationSlot,
                                                   leftSlot,
                                                   rightSlot);
}

void backend_aot_write_c_direct_logical_greater_equal_float(FILE *file,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot) {
    backend_aot_write_c_direct_float_comparison(file,
                                                "zr_aot_left_scalar >= zr_aot_right_scalar",
                                                destinationSlot,
                                                leftSlot,
                                                rightSlot);
}
