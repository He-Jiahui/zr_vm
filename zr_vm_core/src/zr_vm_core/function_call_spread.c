#include "function_call_spread_internal.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

typedef struct SZrFunctionCallSpreadCopyContext {
    SZrObject *array;
    TZrStackValuePointer callWindow;
    TZrSize prefixArgumentCount;
    TZrSize spreadArgumentCount;
} SZrFunctionCallSpreadCopyContext;

static SZrObject *function_call_spread_array(const SZrTypeValue *spreadValue) {
    SZrObject *array;

    if (spreadValue == ZR_NULL ||
        (spreadValue->type != ZR_VALUE_TYPE_ARRAY &&
         spreadValue->type != ZR_VALUE_TYPE_OBJECT) ||
        spreadValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZR_CAST_OBJECT(ZR_NULL, spreadValue->value.object);
    return array != ZR_NULL &&
                   array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY
            ? array
            : ZR_NULL;
}

static const SZrTypeValue *function_call_spread_get_array_value(
        struct SZrState *state,
        SZrObject *array,
        TZrSize index) {
    SZrTypeValue key;

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

static TZrBool function_call_spread_validate_dense(
        struct SZrState *state,
        SZrObject *array,
        TZrSize spreadArgumentCount) {
    for (TZrSize index = 0u; index < spreadArgumentCount; index++) {
        if (function_call_spread_get_array_value(state, array, index) == ZR_NULL) {
            ZrCore_Debug_RunError(
                    state,
                    "FUNCTION_CALL_SPREAD: spread array must be dense");
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool function_call_spread_validate_arity(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize spreadArgumentCount) {
    const SZrTypeValue *callableValue =
            ZrCore_Stack_GetValueNoProfile(callWindow);
    SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromValue(
            state, callableValue);
    TZrSize argumentCount = prefixArgumentCount + spreadArgumentCount;

    if (function == ZR_NULL || function->hasVariableArguments ||
        function->parameterCount == argumentCount) {
        return ZR_TRUE;
    }

    ZrCore_Debug_RunError(
            state,
            "FUNCTION_CALL_SPREAD: argument count does not match callable signature");
    return ZR_FALSE;
}

static void function_call_spread_copy_arguments(
        struct SZrState *state,
        TZrPtr rawContext) {
    SZrFunctionCallSpreadCopyContext *context =
            (SZrFunctionCallSpreadCopyContext *)rawContext;

    for (TZrSize remaining = context->spreadArgumentCount;
         remaining > 0u;
         remaining--) {
        const TZrSize index = remaining - 1u;
        const SZrTypeValue *source = function_call_spread_get_array_value(
                state, context->array, index);
        SZrTypeValue *destination = ZrCore_Stack_GetValueNoProfile(
                context->callWindow +
                context->prefixArgumentCount +
                index +
                1u);

        ZR_ASSERT(source != ZR_NULL);
        ZrCore_Value_Copy(state, destination, source);
    }
}

static void function_call_spread_clear_destinations(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize spreadArgumentCount) {
    const TZrSize destinationCount =
            spreadArgumentCount > 0u ? spreadArgumentCount : 1u;

    for (TZrSize index = 0u; index < destinationCount; index++) {
        SZrTypeValue *destination = ZrCore_Stack_GetValueNoProfile(
                callWindow + prefixArgumentCount + index + 1u);

        ZrCore_Value_PrepareDestinationForOverwriteNoProfile(
                state, destination);
        ZrCore_Value_ResetAsNullNoProfile(destination);
    }
}

ZR_CORE_API TZrBool ZrCore_Function_CallSpread_TryGetArgumentCount(
        const SZrTypeValue *spreadValue,
        TZrSize *outArgumentCount) {
    SZrObject *array;

    if (outArgumentCount == ZR_NULL) {
        return ZR_FALSE;
    }
    *outArgumentCount = 0u;
    array = function_call_spread_array(spreadValue);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    *outArgumentCount = ZrCore_Object_SuperArrayLength(array);
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_Function_CallSpread_ExpandPrepared(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize *outArgumentCount) {
    SZrTypeValue *spreadValue;
    SZrObject *array;
    TZrSize spreadArgumentCount;
    TZrStackValuePointer originalStackTop;
    TZrStackValuePointer expandedStackTop;
    SZrGcNativeCallPin arrayPin;
    SZrFunctionCallSpreadCopyContext copyContext;
    EZrThreadStatus copyStatus;

    if (outArgumentCount != ZR_NULL) {
        *outArgumentCount = 0u;
    }
    if (state == ZR_NULL || callWindow == ZR_NULL) {
        return ZR_FALSE;
    }

    spreadValue = ZrCore_Stack_GetValueNoProfile(
            callWindow + prefixArgumentCount + 1u);
    array = function_call_spread_array(spreadValue);
    if (array == ZR_NULL) {
        ZrCore_Debug_RunError(
                state,
                "FUNCTION_CALL_SPREAD: spread operand must be an array");
        return ZR_FALSE;
    }
    if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return ZR_FALSE;
    }
    spreadArgumentCount = array->nodeMap.elementCount;
    if (!function_call_spread_validate_arity(
                state,
                callWindow,
                prefixArgumentCount,
                spreadArgumentCount)) {
        return ZR_FALSE;
    }
    if (!function_call_spread_validate_dense(
                state, array, spreadArgumentCount)) {
        return ZR_FALSE;
    }

    if (!ZrCore_Gc_NativeCallPinObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(array), &arrayPin)) {
        ZrCore_Debug_RunError(
                state,
                "FUNCTION_CALL_SPREAD: failed to pin spread array");
        return ZR_FALSE;
    }

    originalStackTop = state->stackTop.valuePointer;
    expandedStackTop =
            callWindow + prefixArgumentCount + spreadArgumentCount + 1u;
    if (state->stackTop.valuePointer < expandedStackTop) {
        state->stackTop.valuePointer = expandedStackTop;
    }

    copyContext.array = array;
    copyContext.callWindow = callWindow;
    copyContext.prefixArgumentCount = prefixArgumentCount;
    copyContext.spreadArgumentCount = spreadArgumentCount;
    copyStatus = ZrCore_Exception_TryRun(
            state, function_call_spread_copy_arguments, &copyContext);
    if (copyStatus != ZR_THREAD_STATUS_FINE) {
        function_call_spread_clear_destinations(
                state,
                callWindow,
                prefixArgumentCount,
                spreadArgumentCount);
        state->stackTop.valuePointer = originalStackTop;
        ZrCore_Gc_NativeCallUnpin(state->global, &arrayPin);
        if (state->exceptionRecoverPoint != ZR_NULL) {
            ZrCore_Exception_Throw(state, copyStatus);
        }
        state->threadStatus = copyStatus;
        return ZR_FALSE;
    }

    if (spreadArgumentCount == 0u) {
        function_call_spread_clear_destinations(
                state, callWindow, prefixArgumentCount, 0u);
    }

    ZrCore_Gc_NativeCallUnpin(state->global, &arrayPin);
    state->stackTop.valuePointer = expandedStackTop;
    if (outArgumentCount != ZR_NULL) {
        *outArgumentCount = prefixArgumentCount + spreadArgumentCount;
    }
    return ZR_TRUE;
}

struct SZrCallInfo *ZrCore_Function_CallSpread_PreCallPrepared(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        TZrBool *outInvocationStarted) {
    TZrSize argumentCount;

    if (outInvocationStarted != ZR_NULL) {
        *outInvocationStarted = ZR_FALSE;
    }
    if (state == ZR_NULL || callWindow == ZR_NULL) {
        return ZR_NULL;
    }

    if (!ZrCore_Function_CallSpread_ExpandPrepared(
                state, callWindow, prefixArgumentCount, &argumentCount)) {
        return ZR_NULL;
    }
    if (outInvocationStarted != ZR_NULL) {
        *outInvocationStarted = ZR_TRUE;
    }
    ZR_UNUSED_PARAMETER(argumentCount);
    return ZrCore_Function_PreCall(
            state, callWindow, resultCount, returnDestination);
}
