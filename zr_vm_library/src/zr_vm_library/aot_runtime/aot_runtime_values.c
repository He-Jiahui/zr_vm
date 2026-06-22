#include "zr_vm_library/aot_runtime.h"

#include "aot_runtime_internal.h"

#include <math.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

static SZrLibraryAotRuntimeState *aot_runtime_value_runtime_state(SZrState *state) {
    return state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
}

static TZrStackValuePointer aot_runtime_value_frame_slot(const ZrAotGeneratedFrame *frame, TZrUInt32 slotIndex) {
    if (frame == ZR_NULL || frame->slotBase == ZR_NULL || frame->function == ZR_NULL ||
        slotIndex >= frame->generatedFrameSlotCount) {
        return ZR_NULL;
    }

    return frame->slotBase + slotIndex;
}

TZrBool ZrLibrary_AotRuntime_CopyStack(SZrState *state,
                                       ZrAotGeneratedFrame *frame,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    const SZrFunctionFrameSlotLayout *destinationLayout;
    const SZrFunctionFrameSlotLayout *sourceLayout;
    SZrTypeValue *destinationValue;
    SZrTypeValue *sourceValue;

    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || destinationPointer == ZR_NULL ||
        sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "COPY_STACK: invalid stack slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "COPY_STACK: missing value");
        return ZR_FALSE;
    }

    destinationLayout = ZrCore_Function_FindFrameSlotLayout(frame->function, destinationSlot);
    sourceLayout = ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
    if (destinationLayout != ZR_NULL && sourceLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
        sourceLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
        destinationLayout->typeLayoutId == sourceLayout->typeLayoutId) {
        const SZrTypeLayout *inlineLayout =
                ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function, destinationLayout->typeLayoutId, state);
        if (inlineLayout == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "COPY_STACK: missing inline layout");
            return ZR_FALSE;
        }
        if (!ZrCore_Function_CopyFrameSlotInline(
                    state, inlineLayout, frame->function, frame->slotBase, destinationSlot, frame->function,
                    frame->slotBase, sourceSlot)) {
            aot_runtime_fail(state, runtimeState, "COPY_STACK: failed inline frame copy");
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (destinationLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
        if (!ZrCore_Function_CopyObjectValueToFrameSlotInline(
                    state, frame->function, frame->slotBase, destinationSlot, sourceValue)) {
            aot_runtime_fail(state, runtimeState, "COPY_STACK: failed inline object copy");
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (destinationLayout != ZR_NULL &&
        destinationLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
        destinationLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
        ZrCore_Value_Copy(state, destinationValue, sourceValue);
        return ZR_TRUE;
    }

    ZrCore_Value_AssignMaterializedStackValue(state, destinationValue, sourceValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ResetStackNull(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL: invalid destination slot");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL: missing destination value");
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(destinationValue);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ResetStackNull2(SZrState *state,
                                             ZrAotGeneratedFrame *frame,
                                             TZrUInt32 firstSlot,
                                             TZrUInt32 secondSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer firstPointer = aot_runtime_value_frame_slot(frame, firstSlot);
    TZrStackValuePointer secondPointer = aot_runtime_value_frame_slot(frame, secondSlot);
    SZrTypeValue *firstValue;
    SZrTypeValue *secondValue;

    if (state == ZR_NULL || firstPointer == ZR_NULL || secondPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL2: invalid stack slot");
        return ZR_FALSE;
    }

    firstValue = ZrCore_Stack_GetValue(firstPointer);
    secondValue = ZrCore_Stack_GetValue(secondPointer);
    if (firstValue == ZR_NULL || secondValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "RESET_STACK_NULL2: missing value");
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(firstValue);
    ZrCore_Value_ResetAsNull(secondValue);
    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_conversion_values(SZrState *state,
                                                     ZrAotGeneratedFrame *frame,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 sourceSlot,
                                                     SZrLibraryAotRuntimeState **runtimeStateOut,
                                                     SZrTypeValue **destinationValueOut,
                                                     const SZrTypeValue **sourceValueOut) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    *destinationValueOut = destinationValue;
    *sourceValueOut = sourceValue;
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ConvertGenericToBool(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          sourceValue->value.nativeObject.nativeInt64 != 0,
                          ZR_VALUE_TYPE_BOOL);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          sourceValue->value.nativeObject.nativeUInt64 != 0,
                          ZR_VALUE_TYPE_BOOL);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeBool,
                          sourceValue->value.nativeObject.nativeDouble != 0.0,
                          ZR_VALUE_TYPE_BOOL);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ConvertGenericToInt(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          (TZrInt64)sourceValue->value.nativeObject.nativeUInt64,
                          ZR_VALUE_TYPE_INT64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          (TZrInt64)sourceValue->value.nativeObject.nativeDouble,
                          ZR_VALUE_TYPE_INT64);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          sourceValue->value.nativeObject.nativeBool ? 1 : 0,
                          ZR_VALUE_TYPE_INT64);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ConvertGenericToUInt(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeUInt64,
                          (TZrUInt64)sourceValue->value.nativeObject.nativeInt64,
                          ZR_VALUE_TYPE_UINT64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeUInt64,
                          (TZrUInt64)sourceValue->value.nativeObject.nativeDouble,
                          ZR_VALUE_TYPE_UINT64);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeUInt64,
                          sourceValue->value.nativeObject.nativeBool ? (TZrUInt64)1u : (TZrUInt64)0u,
                          ZR_VALUE_TYPE_UINT64);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_ConvertGenericToFloat(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (!aot_runtime_generic_conversion_values(state,
                                               frame,
                                               destinationSlot,
                                               sourceSlot,
                                               &runtimeState,
                                               &destinationValue,
                                               &sourceValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        *destinationValue = *sourceValue;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          (TZrFloat64)sourceValue->value.nativeObject.nativeInt64,
                          ZR_VALUE_TYPE_DOUBLE);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          (TZrFloat64)sourceValue->value.nativeObject.nativeUInt64,
                          ZR_VALUE_TYPE_DOUBLE);
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          sourceValue->value.nativeObject.nativeBool ? (TZrFloat64)1.0 : (TZrFloat64)0.0,
                          ZR_VALUE_TYPE_DOUBLE);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive conversion");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

typedef enum EZrAotRuntimeGenericNumericBinaryOp {
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV,
    ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD
} EZrAotRuntimeGenericNumericBinaryOp;

static TZrBool aot_runtime_generic_numeric_values(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  SZrLibraryAotRuntimeState **runtimeStateOut,
                                                  SZrTypeValue **destinationValueOut,
                                                  const SZrTypeValue **leftValueOut,
                                                  const SZrTypeValue **rightValueOut) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_value_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_value_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (destinationValueOut == ZR_NULL || leftValueOut == ZR_NULL || rightValueOut == ZR_NULL ||
        state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    *destinationValueOut = destinationValue;
    *leftValueOut = leftValue;
    *rightValueOut = rightValue;
    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_numeric_extract_float64(SZrState *state,
                                                           SZrLibraryAotRuntimeState *runtimeState,
                                                           const SZrTypeValue *value,
                                                           TZrFloat64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = value->value.nativeObject.nativeDouble;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_numeric_extract_int64(SZrState *state,
                                                         SZrLibraryAotRuntimeState *runtimeState,
                                                         const SZrTypeValue *value,
                                                         TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_numeric_binary(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  EZrAotRuntimeGenericNumericBinaryOp operation) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;

    if (!aot_runtime_generic_numeric_values(state,
                                            frame,
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            &runtimeState,
                                            &destinationValue,
                                            &leftValue,
                                            &rightValue)) {
        return ZR_FALSE;
    }

    if (!ZR_VALUE_IS_TYPE_NUMBER(leftValue->type) || !ZR_VALUE_IS_TYPE_NUMBER(rightValue->type)) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(leftValue->type) || ZR_VALUE_IS_TYPE_FLOAT(rightValue->type)) {
        TZrFloat64 leftFloat;
        TZrFloat64 rightFloat;
        TZrFloat64 resultFloat;

        if (!aot_runtime_generic_numeric_extract_float64(state, runtimeState, leftValue, &leftFloat) ||
            !aot_runtime_generic_numeric_extract_float64(state, runtimeState, rightValue, &rightFloat)) {
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV && rightFloat == 0.0) {
            aot_runtime_fail(state, runtimeState, "divide by zero");
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD && rightFloat == 0.0) {
            aot_runtime_fail(state, runtimeState, "modulo by zero");
            return ZR_FALSE;
        }

        switch (operation) {
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD:
                resultFloat = leftFloat + rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB:
                resultFloat = leftFloat - rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL:
                resultFloat = leftFloat * rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV:
                resultFloat = leftFloat / rightFloat;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD:
                resultFloat = fmod(leftFloat, rightFloat);
                break;
            default:
                aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
                return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destinationValue, nativeDouble, resultFloat, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue->type)) {
        TZrUInt64 leftUInt = leftValue->value.nativeObject.nativeUInt64;
        TZrUInt64 rightUInt = rightValue->value.nativeObject.nativeUInt64;
        TZrUInt64 resultUInt;

        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV && rightUInt == 0u) {
            aot_runtime_fail(state, runtimeState, "divide by zero");
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD && rightUInt == 0u) {
            aot_runtime_fail(state, runtimeState, "modulo by zero");
            return ZR_FALSE;
        }

        switch (operation) {
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD:
                resultUInt = leftUInt + rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB:
                resultUInt = leftUInt - rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL:
                resultUInt = leftUInt * rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV:
                resultUInt = leftUInt / rightUInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD:
                resultUInt = leftUInt % rightUInt;
                break;
            default:
                aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
                return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destinationValue, nativeUInt64, resultUInt, ZR_VALUE_TYPE_UINT64);
        return ZR_TRUE;
    }

    {
        TZrInt64 leftInt;
        TZrInt64 rightInt;
        TZrInt64 resultInt;

        if (!aot_runtime_generic_numeric_extract_int64(state, runtimeState, leftValue, &leftInt) ||
            !aot_runtime_generic_numeric_extract_int64(state, runtimeState, rightValue, &rightInt)) {
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV && rightInt == 0) {
            aot_runtime_fail(state, runtimeState, "divide by zero");
            return ZR_FALSE;
        }
        if (operation == ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD && rightInt == 0) {
            aot_runtime_fail(state, runtimeState, "modulo by zero");
            return ZR_FALSE;
        }

        switch (operation) {
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD:
                resultInt = leftInt + rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB:
                resultInt = leftInt - rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL:
                resultInt = leftInt * rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV:
                resultInt = leftInt / rightInt;
                break;
            case ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD:
                resultInt = leftInt % rightInt;
                break;
            default:
                aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
                return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destinationValue, nativeInt64, resultInt, ZR_VALUE_TYPE_INT64);
        return ZR_TRUE;
    }
}

TZrBool ZrLibrary_AotRuntime_GenericNumericAdd(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_ADD);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericSub(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_SUB);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericMul(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_MUL);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericDiv(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_DIV);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericMod(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    return aot_runtime_generic_numeric_binary(state,
                                              frame,
                                              destinationSlot,
                                              leftSlot,
                                              rightSlot,
                                              ZR_AOT_RUNTIME_GENERIC_NUMERIC_MOD);
}

TZrBool ZrLibrary_AotRuntime_GenericNumericNeg(SZrState *state,
                                               ZrAotGeneratedFrame *frame,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (destinationValue == ZR_NULL || sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          -sourceValue->value.nativeObject.nativeInt64,
                          sourceValue->type);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeInt64,
                          -(TZrInt64)sourceValue->value.nativeObject.nativeUInt64,
                          ZR_VALUE_TYPE_INT64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        ZR_VALUE_FAST_SET(destinationValue,
                          nativeDouble,
                          -sourceValue->value.nativeObject.nativeDouble,
                          sourceValue->type);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic numeric arithmetic");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPower(SZrState *state,
                                          ZrAotGeneratedFrame *frame,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_value_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_value_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrMeta *metaValue;

    if (state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL ||
        rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic power meta dispatch");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic power meta dispatch");
        return ZR_FALSE;
    }

    metaValue = ZrCore_Value_GetMeta(state, leftValue, ZR_META_POW);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        ZrCore_Value_ResetAsNull(destinationValue);
        return ZR_TRUE;
    }

    aot_runtime_fail(state, runtimeState, "unsupported AOT generic power meta dispatch");
    return ZR_FALSE;
}

static TZrBool aot_runtime_generic_logical_source_value(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 sourceSlot,
                                                        SZrLibraryAotRuntimeState **runtimeStateOut,
                                                        const SZrTypeValue **sourceValueOut,
                                                        const char *failureMessage) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer sourcePointer = aot_runtime_value_frame_slot(frame, sourceSlot);
    const SZrTypeValue *sourceValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (sourceValueOut == ZR_NULL || state == ZR_NULL || sourcePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    sourceValue = ZrCore_Stack_GetValue(sourcePointer);
    if (sourceValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    *sourceValueOut = sourceValue;
    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_logical_values(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  SZrLibraryAotRuntimeState **runtimeStateOut,
                                                  SZrTypeValue **destinationValueOut,
                                                  const SZrTypeValue **leftValueOut,
                                                  const SZrTypeValue **rightValueOut,
                                                  const char *failureMessage) {
    SZrLibraryAotRuntimeState *runtimeState = aot_runtime_value_runtime_state(state);
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    TZrStackValuePointer leftPointer = aot_runtime_value_frame_slot(frame, leftSlot);
    TZrStackValuePointer rightPointer = aot_runtime_value_frame_slot(frame, rightSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;

    if (runtimeStateOut != ZR_NULL) {
        *runtimeStateOut = runtimeState;
    }
    if (destinationValueOut == ZR_NULL || leftValueOut == ZR_NULL || rightValueOut == ZR_NULL ||
        state == ZR_NULL || destinationPointer == ZR_NULL || leftPointer == ZR_NULL || rightPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    leftValue = ZrCore_Stack_GetValue(leftPointer);
    rightValue = ZrCore_Stack_GetValue(rightPointer);
    if (destinationValue == ZR_NULL || leftValue == ZR_NULL || rightValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, failureMessage);
        return ZR_FALSE;
    }

    *destinationValueOut = destinationValue;
    *leftValueOut = leftValue;
    *rightValueOut = rightValue;
    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_primitive_truthy(SZrState *state,
                                                   SZrLibraryAotRuntimeState *runtimeState,
                                                   const SZrTypeValue *sourceValue,
                                                   TZrBool *outTruthy) {
    if (sourceValue == ZR_NULL || outTruthy == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_NULL(sourceValue->type)) {
        *outTruthy = ZR_FALSE;
    } else if (ZR_VALUE_IS_TYPE_BOOL(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeBool != 0u);
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeUInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue->type)) {
        *outTruthy = (TZrBool)(sourceValue->value.nativeObject.nativeDouble != 0.0);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_generic_primitive_equal(SZrState *state,
                                                  SZrLibraryAotRuntimeState *runtimeState,
                                                  const SZrTypeValue *leftValue,
                                                  const SZrTypeValue *rightValue,
                                                  TZrBool *outEqual) {
    if (leftValue == ZR_NULL || rightValue == ZR_NULL || outEqual == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive equality");
        return ZR_FALSE;
    }

    if (leftValue->type != rightValue->type) {
        *outEqual = ZR_FALSE;
    } else if (ZR_VALUE_IS_TYPE_NULL(leftValue->type)) {
        *outEqual = ZR_TRUE;
    } else if (ZR_VALUE_IS_TYPE_BOOL(leftValue->type)) {
        *outEqual = (TZrBool)((leftValue->value.nativeObject.nativeBool != 0u) ==
                              (rightValue->value.nativeObject.nativeBool != 0u));
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue->type)) {
        *outEqual = (TZrBool)(leftValue->value.nativeObject.nativeInt64 ==
                              rightValue->value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue->type)) {
        *outEqual = (TZrBool)(leftValue->value.nativeObject.nativeUInt64 ==
                              rightValue->value.nativeObject.nativeUInt64);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue->type)) {
        *outEqual = (TZrBool)(leftValue->value.nativeObject.nativeDouble ==
                              rightValue->value.nativeObject.nativeDouble);
    } else {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive equality");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(SZrState *state,
                                                      ZrAotGeneratedFrame *frame,
                                                      TZrUInt32 sourceSlot,
                                                      TZrBool *outTruthy) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrTypeValue *sourceValue;

    if (outTruthy != ZR_NULL) {
        *outTruthy = ZR_FALSE;
    }
    if (!aot_runtime_generic_logical_source_value(state,
                                                  frame,
                                                  sourceSlot,
                                                  &runtimeState,
                                                  &sourceValue,
                                                  "unsupported AOT generic primitive truthiness")) {
        return ZR_FALSE;
    }

    return aot_runtime_generic_primitive_truthy(state, runtimeState, sourceValue, outTruthy);
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(SZrState *state,
                                                       ZrAotGeneratedFrame *frame,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 sourceSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer destinationPointer = aot_runtime_value_frame_slot(frame, destinationSlot);
    SZrTypeValue *destinationValue;
    const SZrTypeValue *sourceValue;
    TZrBool truthy = ZR_FALSE;

    if (!aot_runtime_generic_logical_source_value(state,
                                                  frame,
                                                  sourceSlot,
                                                  &runtimeState,
                                                  &sourceValue,
                                                  "unsupported AOT generic primitive truthiness")) {
        return ZR_FALSE;
    }
    if (state == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    if (destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT generic primitive truthiness");
        return ZR_FALSE;
    }
    if (!aot_runtime_generic_primitive_truthy(state, runtimeState, sourceValue, &truthy)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, !truthy, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(SZrState *state,
                                                         ZrAotGeneratedFrame *frame,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;
    TZrBool equal = ZR_FALSE;

    if (!aot_runtime_generic_logical_values(state,
                                            frame,
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            &runtimeState,
                                            &destinationValue,
                                            &leftValue,
                                            &rightValue,
                                            "unsupported AOT generic primitive equality") ||
        !aot_runtime_generic_primitive_equal(state, runtimeState, leftValue, rightValue, &equal)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, equal, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

TZrBool ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(SZrState *state,
                                                            ZrAotGeneratedFrame *frame,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrTypeValue *destinationValue;
    const SZrTypeValue *leftValue;
    const SZrTypeValue *rightValue;
    TZrBool equal = ZR_FALSE;

    if (!aot_runtime_generic_logical_values(state,
                                            frame,
                                            destinationSlot,
                                            leftSlot,
                                            rightSlot,
                                            &runtimeState,
                                            &destinationValue,
                                            &leftValue,
                                            &rightValue,
                                            "unsupported AOT generic primitive equality") ||
        !aot_runtime_generic_primitive_equal(state, runtimeState, leftValue, rightValue, &equal)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destinationValue, nativeBool, !equal, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}
