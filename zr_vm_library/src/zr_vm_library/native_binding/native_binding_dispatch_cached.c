#include "native_binding/native_binding_internal.h"

#include "native_binding/native_binding_dispatch_lanes.h"

/*
 * Cached known-native stack-root thunks stay on the same hot path profile
 * rules as the generic dispatcher: keep stack slot access/copy off helper
 * profiling branches inside benchmark runs.
 */
#define ZrCore_Stack_GetValue ZrCore_Stack_GetValueNoProfile
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile

static ZR_FORCE_INLINE void native_binding_cached_dispatch_finish_result(
        SZrState *state,
        const SZrFunctionStackAnchor *functionBaseAnchor,
        const SZrTypeValue *result) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || functionBaseAnchor == ZR_NULL || result == ZR_NULL) {
        return;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, functionBaseAnchor);
    closureValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    if (closureValue != ZR_NULL) {
        ZrCore_Value_Copy(state, closureValue, result);
    }
    if (functionBase != ZR_NULL) {
        state->stackTop.valuePointer = functionBase + 1;
    }
}

static ZR_FORCE_INLINE TZrBool native_binding_prepare_cached_stack_root_dispatch(
        SZrState *state,
        TZrSize expectedArgumentCount,
        SZrFunctionStackAnchor *outFunctionBaseAnchor,
        FZrLibBoundCallback *outCallback,
        ZrLibCallContext *outContext) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *closureValue;
    SZrClosureNative *closure;
    TZrSize rawArgumentCount;
    TZrBool usesReceiver;
    TZrSize expectedRawArgumentCount;

    if (outCallback != ZR_NULL) {
        *outCallback = ZR_NULL;
    }

    if (state == ZR_NULL || outFunctionBaseAnchor == ZR_NULL || outCallback == ZR_NULL ||
        outContext == ZR_NULL || state->global == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_FALSE;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL || state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < functionBase + 1) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, functionBase, outFunctionBaseAnchor);
    closureValue = ZrCore_Stack_GetValue(functionBase);
    if (closureValue == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureValue->value.object);
    if (!native_binding_closure_try_get_cached_callback(closure, outCallback) ||
        closure == ZR_NULL ||
        closure->nativeBindingModuleDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    rawArgumentCount = (TZrSize)(state->stackTop.valuePointer - (functionBase + 1));
    usesReceiver = closure != ZR_NULL ? (TZrBool)(closure->nativeBindingUsesReceiver != 0u) : ZR_FALSE;
    expectedRawArgumentCount = expectedArgumentCount + (usesReceiver ? 1u : 0u);
    if (rawArgumentCount != expectedRawArgumentCount) {
        return ZR_FALSE;
    }

    native_binding_init_cached_stack_root_context_from_closure(
            outContext, state, closure, functionBase, rawArgumentCount, usesReceiver);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrInt64 native_binding_dispatch_cached_stack_root_fixed_argument_count(
        SZrState *state,
        TZrSize expectedArgumentCount) {
    SZrFunctionStackAnchor functionBaseAnchor;
    ZrLibCallContext context;
    SZrTypeValue result;
    FZrLibBoundCallback callback;
    TZrBool success;

    if (!native_binding_prepare_cached_stack_root_dispatch(state,
                                                           expectedArgumentCount,
                                                           &functionBaseAnchor,
                                                           &callback,
                                                           &context)) {
        return native_binding_dispatcher(state);
    }

    ZrLib_Value_SetNull(&result);
    success = native_binding_dispatch_stack_root_callback_lane(state, callback, &context, &functionBaseAnchor, &result);
    if (!success) {
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return 0;
        }
        ZrLib_Value_SetNull(&result);
    }

    native_binding_cached_dispatch_finish_result(state, &functionBaseAnchor, &result);
    return 1;
}

TZrInt64 native_binding_dispatch_cached_stack_root_one_argument(SZrState *state) {
    return native_binding_dispatch_cached_stack_root_fixed_argument_count(state, 1u);
}

TZrInt64 native_binding_dispatch_cached_stack_root_two_arguments(SZrState *state) {
    return native_binding_dispatch_cached_stack_root_fixed_argument_count(state, 2u);
}
