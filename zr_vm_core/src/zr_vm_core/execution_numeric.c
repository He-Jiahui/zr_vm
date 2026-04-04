//
// Split from execution.c: numeric helpers.
//

#include "execution_internal.h"

static TZrBool execution_extract_numeric_double(const SZrTypeValue *value, TZrFloat64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrFloat64) value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrFloat64) value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1.0 : 0.0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool execution_eval_binary_numeric_float(EZrExecutionNumericFallbackOp operation,
                                                   TZrFloat64 leftValue,
                                                   TZrFloat64 rightValue,
                                                   TZrFloat64 *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_EXEC_NUMERIC_FALLBACK_ADD:
            *outResult = leftValue + rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_SUB:
            *outResult = leftValue - rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_MUL:
            *outResult = leftValue * rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_DIV:
            *outResult = leftValue / rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_MOD:
            *outResult = fmod(leftValue, rightValue);
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_POW:
            *outResult = pow(leftValue, rightValue);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_try_binary_numeric_float_fallback(EZrExecutionNumericFallbackOp operation,
                                                           SZrTypeValue *destination,
                                                           const SZrTypeValue *opA,
                                                           const SZrTypeValue *opB);

static TZrBool execution_apply_binary_numeric_float(EZrExecutionNumericFallbackOp operation,
                                                    SZrTypeValue *destination,
                                                    const SZrTypeValue *opA,
                                                    const SZrTypeValue *opB) {
    TZrFloat64 resultValue;

    if (destination == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type)) {
        if (!execution_eval_binary_numeric_float(operation,
                                                 opA->value.nativeObject.nativeDouble,
                                                 opB->value.nativeObject.nativeDouble,
                                                 &resultValue)) {
            return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destination, nativeDouble, resultValue, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    return execution_try_binary_numeric_float_fallback(operation, destination, opA, opB);
}

void execution_apply_binary_numeric_float_or_raise(SZrState *state,
                                                   EZrExecutionNumericFallbackOp operation,
                                                   SZrTypeValue *destination,
                                                   const SZrTypeValue *opA,
                                                   const SZrTypeValue *opB,
                                                   const TZrChar *instructionName) {
    if (!execution_apply_binary_numeric_float(operation, destination, opA, opB)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
    }
}

static TZrBool execution_eval_binary_numeric_compare(EZrExecutionNumericCompareOp operation,
                                                     TZrFloat64 leftValue,
                                                     TZrFloat64 rightValue,
                                                     TZrBool *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_EXEC_NUMERIC_COMPARE_GREATER:
            *outResult = leftValue > rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_COMPARE_LESS:
            *outResult = leftValue < rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL:
            *outResult = leftValue >= rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL:
            *outResult = leftValue <= rightValue;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_apply_binary_numeric_compare(EZrExecutionNumericCompareOp operation,
                                                      SZrTypeValue *destination,
                                                      const SZrTypeValue *opA,
                                                      const SZrTypeValue *opB) {
    TZrFloat64 leftValue;
    TZrFloat64 rightValue;
    TZrBool resultValue;

    if (destination == ZR_NULL ||
        !execution_extract_numeric_double(opA, &leftValue) ||
        !execution_extract_numeric_double(opB, &rightValue) ||
        !execution_eval_binary_numeric_compare(operation, leftValue, rightValue, &resultValue)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destination, nativeBool, resultValue, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

void execution_apply_binary_numeric_compare_or_raise(SZrState *state,
                                                     EZrExecutionNumericCompareOp operation,
                                                     SZrTypeValue *destination,
                                                     const SZrTypeValue *opA,
                                                     const SZrTypeValue *opB,
                                                     const TZrChar *instructionName) {
    if (!execution_apply_binary_numeric_compare(operation, destination, opA, opB)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
    }
}

static TZrBool execution_try_binary_numeric_float_fallback(EZrExecutionNumericFallbackOp operation,
                                                           SZrTypeValue *destination,
                                                           const SZrTypeValue *opA,
                                                           const SZrTypeValue *opB) {
    TZrFloat64 leftValue;
    TZrFloat64 rightValue;
    TZrFloat64 resultValue;

    if (destination == ZR_NULL ||
        !execution_extract_numeric_double(opA, &leftValue) ||
        !execution_extract_numeric_double(opB, &rightValue)) {
        return ZR_FALSE;
    }

    if (!execution_eval_binary_numeric_float(operation, leftValue, rightValue, &resultValue)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destination, nativeDouble, resultValue, ZR_VALUE_TYPE_DOUBLE);
    return ZR_TRUE;
}

void execution_try_binary_numeric_float_fallback_or_raise(SZrState *state,
                                                          EZrExecutionNumericFallbackOp operation,
                                                          SZrTypeValue *destination,
                                                          const SZrTypeValue *opA,
                                                          const SZrTypeValue *opB,
                                                          const TZrChar *instructionName) {
    if (!execution_try_binary_numeric_float_fallback(operation, destination, opA, opB)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
    }
}

TZrInt64 value_to_int64(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? 1 : 0;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeDouble;
    }

    return 0;
}

TZrUInt64 value_to_uint64(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return 0;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return (TZrUInt64)value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? 1u : 0u;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return (TZrUInt64)value->value.nativeObject.nativeDouble;
    }

    return 0;
}

TZrDouble value_to_double(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return 0.0;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return value->value.nativeObject.nativeDouble;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return (TZrDouble)value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrDouble)value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? 1.0 : 0.0;
    }

    return 0.0;
}

TZrBool concat_values_to_destination(SZrState *state,
                                     SZrTypeValue *outResult,
                                     const SZrTypeValue *opA,
                                     const SZrTypeValue *opB,
                                     TZrBool safeMode) {
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset tempBaseOffset;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer tempBase;
    SZrCallInfo *currentCallInfo;
    SZrTypeValue *resultValue;
    SZrTypeValue stableOpA;
    SZrTypeValue stableOpB;

    if (state == ZR_NULL || outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    stableOpA = *opA;
    stableOpB = *opB;
    ZrCore_Value_ResetAsNull(outResult);

    savedStackTop = state->stackTop.valuePointer;
    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, savedStackTop);
    currentCallInfo = state->callInfoList;
    tempBase = currentCallInfo != ZR_NULL ? currentCallInfo->functionTop.valuePointer : savedStackTop;
    tempBase = ZrCore_Function_ReserveScratchSlots(state, 2, tempBase);
    if (currentCallInfo != ZR_NULL) {
        tempBase = currentCallInfo->functionTop.valuePointer;
    }
    tempBaseOffset = ZrCore_Stack_SavePointerAsOffset(state, tempBase);

    ZrCore_Stack_CopyValue(state, tempBase, &stableOpA);
    ZrCore_Stack_CopyValue(state, tempBase + 1, &stableOpB);
    state->stackTop.valuePointer = tempBase + 2;
    if (currentCallInfo != ZR_NULL && currentCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        currentCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    if (safeMode) {
        ZrCore_String_ConcatSafe(state, 2);
    } else {
        ZrCore_String_Concat(state, 2);
    }

    tempBase = ZrCore_Stack_LoadOffsetToPointer(state, tempBaseOffset);
    resultValue = ZrCore_Stack_GetValue(tempBase);
    if (resultValue != ZR_NULL) {
        *outResult = *resultValue;
    }
    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    return ZR_TRUE;
}


TZrBool try_builtin_add(SZrState *state,
                        SZrTypeValue *outResult,
                        const SZrTypeValue *opA,
                        const SZrTypeValue *opB) {
    if (state == ZR_NULL || outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_STRING(opA->type) || ZR_VALUE_IS_TYPE_STRING(opB->type)) {
        return concat_values_to_destination(state, outResult, opA, opB, ZR_TRUE);
    }

    if ((ZR_VALUE_IS_TYPE_NUMBER(opA->type) || ZR_VALUE_IS_TYPE_BOOL(opA->type)) &&
        (ZR_VALUE_IS_TYPE_NUMBER(opB->type) || ZR_VALUE_IS_TYPE_BOOL(opB->type))) {
        if (ZR_VALUE_IS_TYPE_FLOAT(opA->type) || ZR_VALUE_IS_TYPE_FLOAT(opB->type)) {
            ZrCore_Value_InitAsFloat(state, outResult, value_to_double(opA) + value_to_double(opB));
        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) || ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type) ||
                   ZR_VALUE_IS_TYPE_BOOL(opA->type) || ZR_VALUE_IS_TYPE_BOOL(opB->type)) {
            ZrCore_Value_InitAsInt(state, outResult, value_to_int64(opA) + value_to_int64(opB));
        } else {
            ZrCore_Value_InitAsUInt(state, outResult, value_to_uint64(opA) + value_to_uint64(opB));
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

// 辅助函数：查找类型原型（从当前模块或全局模块注册表）
// 返回找到的原型对象，如果未找到返回 ZR_NULL
