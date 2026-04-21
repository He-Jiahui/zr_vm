//
// Split from execution.c: numeric helpers.
//

#include "execution/execution_internal.h"

static ZR_FORCE_INLINE SZrTypeValue *execution_stack_get_value_no_profile(TZrStackValuePointer stackPointer) {
    return stackPointer != ZR_NULL ? ZrCore_Stack_GetValueNoProfile(stackPointer) : ZR_NULL;
}

static TZrBool execution_value_points_into_stack(const SZrState *state, const SZrTypeValue *value) {
    TZrStackValuePointer stackPointer;

    if (state == ZR_NULL || value == ZR_NULL || state->stackBase.valuePointer == ZR_NULL ||
        state->stackTail.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    stackPointer = ZR_CAST_STACK_VALUE((TZrPtr)value);
    return stackPointer >= state->stackBase.valuePointer && stackPointer < state->stackTail.valuePointer;
}

static SZrTypeValue *execution_restore_stack_destination(SZrState *state,
                                                         const SZrFunctionStackAnchor *anchor,
                                                         TZrBool hasAnchor,
                                                         SZrTypeValue *destination) {
    TZrStackValuePointer destinationPointer;

    if (!hasAnchor) {
        return destination;
    }
    if (state == ZR_NULL || anchor == ZR_NULL) {
        return ZR_NULL;
    }

    destinationPointer = ZrCore_Function_StackAnchorRestore(state, anchor);
    return execution_stack_get_value_no_profile(destinationPointer);
}

static void execution_refresh_forwarded_value_copy(SZrTypeValue *value) {
    SZrRawObject *forwardedObject;

    if (value == ZR_NULL || !value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return;
    }

    forwardedObject = (SZrRawObject *)value->value.object->garbageCollectMark.forwardingAddress;
    if (forwardedObject == ZR_NULL) {
        return;
    }

    value->value.object = forwardedObject;
    value->type = (EZrValueType)forwardedObject->type;
    value->isNative = forwardedObject->isNative;
}

static TZrBool execution_try_format_fast_safe_concat_value(const SZrTypeValue *value,
                                                           TZrNativeString *outNativeString,
                                                           TZrSize *outNativeLength,
                                                           TZrChar *scratchBuffer,
                                                           TZrSize scratchBufferSize) {
    if (value == ZR_NULL || outNativeString == ZR_NULL || outNativeLength == ZR_NULL || scratchBuffer == ZR_NULL ||
        scratchBufferSize == 0) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            *outNativeString = ZR_STRING_NULL_STRING;
            *outNativeLength = sizeof(ZR_STRING_NULL_STRING) - 1u;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_BOOL:
            *outNativeString = value->value.nativeObject.nativeBool ? ZR_STRING_TRUE_STRING : ZR_STRING_FALSE_STRING;
            *outNativeLength =
                    value->value.nativeObject.nativeBool ? sizeof(ZR_STRING_TRUE_STRING) - 1u : sizeof(ZR_STRING_FALSE_STRING) - 1u;
            return ZR_TRUE;
            ZR_VALUE_CASES_SIGNED_INT {
                *outNativeLength = ZR_STRING_SIGNED_INTEGER_PRINT_FORMAT(scratchBuffer,
                                                                         scratchBufferSize,
                                                                         value->value.nativeObject.nativeInt64);
                *outNativeString = scratchBuffer;
                return ZR_TRUE;
            }
            ZR_VALUE_CASES_UNSIGNED_INT {
                *outNativeLength = ZR_STRING_UNSIGNED_INTEGER_PRINT_FORMAT(scratchBuffer,
                                                                           scratchBufferSize,
                                                                           value->value.nativeObject.nativeUInt64);
                *outNativeString = scratchBuffer;
                return ZR_TRUE;
            }
            ZR_VALUE_CASES_FLOAT {
                *outNativeLength = ZR_STRING_FLOAT_PRINT_FORMAT(scratchBuffer,
                                                                scratchBufferSize,
                                                                value->value.nativeObject.nativeDouble);
                if (scratchBuffer[ZrCore_NativeString_Span(scratchBuffer, ZR_STRING_DECIMAL_NUMBER_SET)] == '\0' &&
                    *outNativeLength + 2u < scratchBufferSize) {
                    scratchBuffer[(*outNativeLength)++] = ZR_STRING_LOCALE_DECIMAL_POINT;
                    scratchBuffer[(*outNativeLength)++] = '0';
                    scratchBuffer[*outNativeLength] = '\0';
                }
                *outNativeString = scratchBuffer;
                return ZR_TRUE;
            }
            ZR_VALUE_CASES_NATIVE {
                *outNativeLength = ZR_STRING_POINTER_PRINT_FORMAT(scratchBuffer,
                                                                  scratchBufferSize,
                                                                  value->value.nativeObject.nativePointer);
                *outNativeString = scratchBuffer;
                return ZR_TRUE;
            }
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_try_concat_fast_safe_string_pair(SZrState *state,
                                                          SZrTypeValue *outResult,
                                                          const SZrTypeValue *opA,
                                                          const SZrTypeValue *opB) {
    TZrBool stringOnLeft;
    SZrTypeValue stableStringValue;
    const SZrTypeValue *stringValue;
    const SZrTypeValue *otherValue;
    TZrChar conversionBuffer[ZR_NUMBER_TO_STRING_LENGTH_MAX + 3];
    TZrNativeString convertedNativeString;
    TZrSize convertedNativeLength;
    SZrString *result;

    if (state == ZR_NULL || outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL ||
        !execution_builtin_add_has_fast_safe_string_concat_pair(opA, opB)) {
        return ZR_FALSE;
    }

    stringOnLeft = ZR_VALUE_IS_TYPE_STRING(opA->type);
    stringValue = stringOnLeft ? opA : opB;
    otherValue = stringOnLeft ? opB : opA;
    stableStringValue = *stringValue;

    if (!execution_try_format_fast_safe_concat_value(otherValue,
                                                     &convertedNativeString,
                                                     &convertedNativeLength,
                                                     conversionBuffer,
                                                     sizeof(conversionBuffer))) {
        ZrCore_Value_ResetAsNull(outResult);
        return ZR_TRUE;
    }

    execution_refresh_forwarded_value_copy(&stableStringValue);
    if (!ZR_VALUE_IS_TYPE_STRING(stableStringValue.type) || stableStringValue.value.object == ZR_NULL) {
        ZrCore_Value_ResetAsNull(outResult);
        return ZR_TRUE;
    }

    result = ZrCore_String_ConcatStringAndNative(state,
                                                 ZR_CAST_STRING(state, stableStringValue.value.object),
                                                 convertedNativeString,
                                                 convertedNativeLength,
                                                 stringOnLeft);
    if (result == ZR_NULL) {
        ZrCore_Value_ResetAsNull(outResult);
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsRawObject(state, outResult, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    outResult->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

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
static ZR_FORCE_INLINE TZrBool execution_extract_integral_or_bool_int64(const SZrTypeValue *value, TZrInt64 *outValue);

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
    SZrFunctionStackAnchor functionTopAnchor;
    SZrFunctionStackAnchor activeFunctionTopAnchor;
    SZrFunctionStackAnchor tempBaseAnchor;
    SZrFunctionStackAnchor opAAnchor;
    SZrFunctionStackAnchor opBAnchor;
    TZrBool hasFunctionTopAnchor = ZR_FALSE;
    TZrBool hasActiveFunctionTopAnchor = ZR_FALSE;
    TZrBool hasTempBaseAnchor = ZR_FALSE;
    TZrBool hasOpAAnchor = ZR_FALSE;
    TZrBool hasOpBAnchor = ZR_FALSE;
    const SZrTypeValue *resolvedOpA = opA;
    const SZrTypeValue *resolvedOpB = opB;

    if (state == ZR_NULL || outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type)) {
        return execution_try_concat_exact_strings(state, outResult, opA, opB);
    }

    ZrCore_Value_ResetAsNull(outResult);

    savedStackTop = state->stackTop.valuePointer;
    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, savedStackTop);
    currentCallInfo = state->callInfoList;
    if (execution_value_points_into_stack(state, opA)) {
        ZrCore_Function_StackAnchorInit(state, ZR_CAST_STACK_VALUE((TZrPtr)opA), &opAAnchor);
        hasOpAAnchor = ZR_TRUE;
    }
    if (execution_value_points_into_stack(state, opB)) {
        ZrCore_Function_StackAnchorInit(state, ZR_CAST_STACK_VALUE((TZrPtr)opB), &opBAnchor);
        hasOpBAnchor = ZR_TRUE;
    }
    tempBase = currentCallInfo != ZR_NULL ? currentCallInfo->functionTop.valuePointer : savedStackTop;
    if (currentCallInfo != ZR_NULL && currentCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, currentCallInfo->functionTop.valuePointer, &functionTopAnchor);
        ZrCore_Function_StackAnchorInit(state, currentCallInfo->functionTop.valuePointer, &activeFunctionTopAnchor);
        hasFunctionTopAnchor = ZR_TRUE;
        hasActiveFunctionTopAnchor = ZR_TRUE;
    }
    tempBase = ZrCore_Function_ReserveScratchSlots(state, 2, tempBase);
    if (tempBase == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Function_StackAnchorInit(state, tempBase, &tempBaseAnchor);
    hasTempBaseAnchor = ZR_TRUE;
    if (hasFunctionTopAnchor) {
        currentCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &functionTopAnchor);
    }

    if (hasOpAAnchor) {
        TZrStackValuePointer restoredOpA = ZrCore_Function_StackAnchorRestore(state, &opAAnchor);
        resolvedOpA = execution_stack_get_value_no_profile(restoredOpA);
    }
    if (hasOpBAnchor) {
        TZrStackValuePointer restoredOpB = ZrCore_Function_StackAnchorRestore(state, &opBAnchor);
        resolvedOpB = execution_stack_get_value_no_profile(restoredOpB);
    }
    if (resolvedOpA == ZR_NULL || resolvedOpB == ZR_NULL) {
        return ZR_FALSE;
    }

    stableOpA = *resolvedOpA;
    stableOpB = *resolvedOpB;
    execution_refresh_forwarded_value_copy(&stableOpA);
    execution_refresh_forwarded_value_copy(&stableOpB);
    tempBaseOffset = ZrCore_Stack_SavePointerAsOffset(state, tempBase);

    state->stackTop.valuePointer = tempBase + 2;
    if (currentCallInfo != ZR_NULL && currentCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        currentCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, currentCallInfo->functionTop.valuePointer, &activeFunctionTopAnchor);
        hasActiveFunctionTopAnchor = ZR_TRUE;
    }
    ZrCore_Stack_CopyValue(state, tempBase, &stableOpA);
    if (hasTempBaseAnchor) {
        tempBase = ZrCore_Function_StackAnchorRestore(state, &tempBaseAnchor);
    }
    if (hasFunctionTopAnchor) {
        currentCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                      hasActiveFunctionTopAnchor
                                                                                              ? &activeFunctionTopAnchor
                                                                                              : &functionTopAnchor);
    }
    ZrCore_Stack_CopyValue(state, tempBase + 1, &stableOpB);
    if (hasTempBaseAnchor) {
        tempBase = ZrCore_Function_StackAnchorRestore(state, &tempBaseAnchor);
    }
    if (hasFunctionTopAnchor) {
        currentCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                      hasActiveFunctionTopAnchor
                                                                                              ? &activeFunctionTopAnchor
                                                                                              : &functionTopAnchor);
    }

    if (safeMode) {
        ZrCore_String_ConcatSafe(state, 2);
    } else {
        ZrCore_String_Concat(state, 2);
    }

    tempBase = ZrCore_Stack_LoadOffsetToPointer(state, tempBaseOffset);
    resultValue = execution_stack_get_value_no_profile(tempBase);
    if (resultValue != ZR_NULL) {
        *outResult = *resultValue;
    }
    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    if (hasFunctionTopAnchor) {
        currentCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &functionTopAnchor);
    }
    return ZR_TRUE;
}


TZrBool try_builtin_add(SZrState *state,
                        SZrTypeValue *outResult,
                        const SZrTypeValue *opA,
                        const SZrTypeValue *opB) {
    EZrValueType typeA;
    EZrValueType typeB;

    if (state == ZR_NULL || outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_try_builtin_add_exact_numeric_fast(outResult, opA, opB)) {
        return ZR_TRUE;
    }

    typeA = opA->type;
    typeB = opB->type;

    if (typeA == ZR_VALUE_TYPE_STRING && typeB == ZR_VALUE_TYPE_STRING) {
        return execution_try_concat_exact_strings(state, outResult, opA, opB);
    }

    if (execution_builtin_add_has_fast_safe_string_concat_pair(opA, opB)) {
        return execution_try_concat_fast_safe_string_pair(state, outResult, opA, opB);
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

static ZR_FORCE_INLINE TZrBool execution_extract_integral_or_bool_int64(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1 : 0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool execution_try_builtin_sub(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB) {
    ZR_UNUSED_PARAMETER(state);

    if (outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type)) {
        TZrBool result = opA->value.nativeObject.nativeBool && opB->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(outResult, nativeBool, result, ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type)) {
        TZrInt64 result = opA->value.nativeObject.nativeInt64 - opB->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, outResult, result);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(opB->type)) {
        TZrUInt64 result = opA->value.nativeObject.nativeUInt64 - opB->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, outResult, result);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type)) {
        TZrFloat64 result = opA->value.nativeObject.nativeDouble - opB->value.nativeObject.nativeDouble;
        ZrCore_Value_InitAsFloat(state, outResult, result);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool execution_try_builtin_div(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB) {
    ZR_UNUSED_PARAMETER(state);

    if (outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type)) {
        TZrInt64 divisor = opB->value.nativeObject.nativeInt64;
        if (divisor == 0) {
            ZrCore_Value_ResetAsNull(outResult);
        } else {
            TZrInt64 result = opA->value.nativeObject.nativeInt64 / divisor;
            ZrCore_Value_InitAsInt(state, outResult, result);
        }
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(opB->type)) {
        TZrUInt64 divisor = opB->value.nativeObject.nativeUInt64;
        if (divisor == 0) {
            ZrCore_Value_ResetAsNull(outResult);
        } else {
            TZrUInt64 result = opA->value.nativeObject.nativeUInt64 / divisor;
            ZrCore_Value_InitAsUInt(state, outResult, result);
        }
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type)) {
        TZrFloat64 divisor = opB->value.nativeObject.nativeDouble;
        if (divisor == 0.0) {
            ZrCore_Value_ResetAsNull(outResult);
        } else {
            TZrFloat64 result = opA->value.nativeObject.nativeDouble / divisor;
            ZrCore_Value_InitAsFloat(state, outResult, result);
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool execution_try_builtin_mul_mixed_numeric_fast(SZrTypeValue *outResult,
                                                     const SZrTypeValue *opA,
                                                     const SZrTypeValue *opB) {
    EZrValueType typeA;
    EZrValueType typeB;
    TZrInt64 leftInt64;
    TZrInt64 rightInt64;

    if (outResult == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    typeA = opA->type;
    typeB = opB->type;
    if (!(ZR_VALUE_IS_TYPE_NUMBER(typeA) || ZR_VALUE_IS_TYPE_BOOL(typeA)) ||
        !(ZR_VALUE_IS_TYPE_NUMBER(typeB) || ZR_VALUE_IS_TYPE_BOOL(typeB))) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(typeA) && ZR_VALUE_IS_TYPE_BOOL(typeB)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(typeA) || ZR_VALUE_IS_TYPE_FLOAT(typeB)) {
        ZR_VALUE_FAST_SET(outResult, nativeDouble, value_to_double(opA) * value_to_double(opB), ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    if (!(ZR_VALUE_IS_TYPE_SIGNED_INT(typeA) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeA) || ZR_VALUE_IS_TYPE_BOOL(typeA)) ||
        !(ZR_VALUE_IS_TYPE_SIGNED_INT(typeB) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(typeB) || ZR_VALUE_IS_TYPE_BOOL(typeB))) {
        return ZR_FALSE;
    }

    if (!execution_extract_integral_or_bool_int64(opA, &leftInt64) ||
        !execution_extract_integral_or_bool_int64(opB, &rightInt64)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(outResult, nativeInt64, leftInt64 * rightInt64, ZR_VALUE_TYPE_INT64);
    return ZR_TRUE;
}

TZrBool ZrCore_Execution_Add(SZrState *state,
                             SZrCallInfo *callInfo,
                             SZrTypeValue *destination,
                             const SZrTypeValue *opA,
                             const SZrTypeValue *opB) {
    TZrBool builtinNeedsTemporaryResult;
    SZrTypeValue builtinResult;
    SZrTypeValue stableLeft;
    SZrFunctionStackAnchor destinationAnchor;
    SZrMeta *meta;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer metaBase = ZR_NULL;
    TZrStackValuePointer restoredStackTop = ZR_NULL;
    SZrCallInfo *savedCallInfo;
    TZrBool hasDestinationAnchor = ZR_FALSE;
    TZrBool metaCallSucceeded;

    if (state == ZR_NULL || destination == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_value_points_into_stack(state, destination)) {
        ZrCore_Function_StackAnchorInit(state, ZR_CAST_STACK_VALUE((TZrPtr)destination), &destinationAnchor);
        hasDestinationAnchor = ZR_TRUE;
    }

    if (execution_try_builtin_add_exact_numeric_fast(destination, opA, opB)) {
        return ZR_TRUE;
    }

    builtinNeedsTemporaryResult = execution_builtin_add_requires_temporary_result(opA, opB);
    if (!builtinNeedsTemporaryResult) {
        destination = execution_restore_stack_destination(state, &destinationAnchor, hasDestinationAnchor, destination);
        if (destination == ZR_NULL) {
            return ZR_FALSE;
        }
        if (try_builtin_add(state, destination, opA, opB)) {
            return ZR_TRUE;
        }
    } else {
        ZrCore_Value_ResetAsNull(&builtinResult);
        if (try_builtin_add(state, &builtinResult, opA, opB)) {
            destination =
                    execution_restore_stack_destination(state, &destinationAnchor, hasDestinationAnchor, destination);
            if (destination == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_Copy(state, destination, &builtinResult);
            return ZR_TRUE;
        }
    }

    destination = execution_restore_stack_destination(state, &destinationAnchor, hasDestinationAnchor, destination);
    if (destination == ZR_NULL) {
        return ZR_FALSE;
    }

    stableLeft = *opA;
    meta = ZrCore_Value_GetMeta(state, &stableLeft, ZR_META_ADD);
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destination);
        return ZR_TRUE;
    }

    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = callInfo != ZR_NULL ? callInfo : state->callInfoList;
    restoredStackTop = savedStackTop;
    metaCallSucceeded = execution_invoke_meta_call(state,
                                                   savedCallInfo,
                                                   savedStackTop,
                                                   savedStackTop,
                                                   meta,
                                                   opA,
                                                   opB,
                                                   ZR_META_CALL_MAX_ARGUMENTS,
                                                   &metaBase,
                                                   &restoredStackTop);

    state->stackTop.valuePointer = restoredStackTop;
    state->callInfoList = savedCallInfo;
    destination = execution_restore_stack_destination(state, &destinationAnchor, hasDestinationAnchor, destination);
    if (destination == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!metaCallSucceeded) {
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }
        ZrCore_Value_ResetAsNull(destination);
        return ZR_TRUE;
    }

    {
        SZrTypeValue *metaResult = execution_stack_get_value_no_profile(metaBase);
        if (metaResult != ZR_NULL) {
            ZrCore_Value_Copy(state, destination, metaResult);
        } else {
            ZrCore_Value_ResetAsNull(destination);
        }
    }
    return ZR_TRUE;
}

// 辅助函数：查找类型原型（从当前模块或全局模块注册表）
// 返回找到的原型对象，如果未找到返回 ZR_NULL
