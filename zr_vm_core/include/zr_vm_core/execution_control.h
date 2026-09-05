#ifndef ZR_VM_CORE_EXECUTION_CONTROL_H
#define ZR_VM_CORE_EXECUTION_CONTROL_H

#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_common/zr_aot_abi.h"

ZR_CORE_API TZrBool execution_push_exception_handler(SZrState *state, SZrCallInfo *callInfo, TZrUInt32 handlerIndex);

ZR_CORE_API SZrVmExceptionHandlerState *execution_find_handler_state(SZrState *state,
                                                                     SZrCallInfo *callInfo,
                                                                     TZrUInt32 handlerIndex);

ZR_CORE_API void execution_pop_exception_handler(SZrState *state, SZrVmExceptionHandlerState *handlerState);

static inline void execution_enter_finally(SZrState *state, SZrVmExceptionHandlerState *handlerState) {
    if (state == ZR_NULL || handlerState == ZR_NULL ||
        handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY) {
        return;
    }
    /* Handler fields are GC roots while nested code uses the visible pending slot. */
    handlerState->suspendedControl = state->pendingControl;
    handlerState->hasSuspendedException = (TZrBool)(
            state->pendingControl.kind == ZR_VM_PENDING_CONTROL_EXCEPTION && state->hasCurrentException);
    handlerState->suspendedExceptionStatus = state->currentExceptionStatus;
    ZrCore_Value_ResetAsNull(&handlerState->suspendedException);
    if (handlerState->hasSuspendedException) {
        handlerState->suspendedException = state->currentException;
        ZrCore_Exception_ClearCurrent(state);
    }
    handlerState->restoreSuspendedControl = ZR_FALSE;
    handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0u;
    state->pendingControl.valueSlot = 0u;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

static inline void execution_finish_finally(SZrState *state, SZrVmExceptionHandlerState *handlerState) {
    if (handlerState != ZR_NULL) {
        handlerState->restoreSuspendedControl = ZR_TRUE;
        execution_pop_exception_handler(state, handlerState);
    }
}

ZR_CORE_API void execution_discard_exception_handlers_for_callinfo(SZrState *state, SZrCallInfo *callInfo);

static inline void execution_discard_top_handler(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(argument);
    execution_pop_exception_handler(state, &state->exceptionHandlerStack[state->exceptionHandlerStackLength - 1u]);
}

static inline EZrThreadStatus execution_discard_exception_handlers_to_depth(SZrState *state, TZrUInt32 depth) {
    static const SZrAotGcRootSlot rootSlot = {
        0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u
    };
    static const SZrAotGcRootMap rootMap = {1u, &rootSlot};
    SZrTypeValue firstException;
    SZrRawObject *root = ZR_NULL;
    SZrAotGcRootFrame rootFrame;
    EZrThreadStatus firstStatus = ZR_THREAD_STATUS_FINE;
    EZrThreadStatus firstExceptionStatus = ZR_THREAD_STATUS_FINE;
    TZrBool hasFirstException = ZR_FALSE;
    EZrThreadStatus originalStatus;
    TZrBool preserveOriginalFailure;

    if (state == ZR_NULL || state->exceptionHandlerStackLength <= depth) {
        return ZR_THREAD_STATUS_FINE;
    }
    originalStatus = state->threadStatus;
    preserveOriginalFailure = (TZrBool)(state->hasCurrentException ||
                                       originalStatus != ZR_THREAD_STATUS_FINE);
    hasFirstException = state->hasCurrentException;
    ZrCore_Value_ResetAsNull(&firstException);
    if (hasFirstException) {
        firstException = state->currentException;
        firstExceptionStatus = state->currentExceptionStatus;
    }
    if (hasFirstException && ZrCore_Value_IsGarbageCollectable(&firstException)) {
        root = ZrCore_Value_GetRawObject(&firstException);
    }
    if (!ZrCore_Gc_AotRootFramePush(state, &rootFrame, (TZrStackValuePointer)&root, &rootMap)) {
        return ZR_THREAD_STATUS_MEMORY_ERROR;
    }
    while (state->exceptionHandlerStackLength > depth) {
        EZrThreadStatus status = ZrCore_Exception_TryRun(state, execution_discard_top_handler, ZR_NULL);
        if (firstStatus == ZR_THREAD_STATUS_FINE && status != ZR_THREAD_STATUS_FINE) {
            firstStatus = status;
            if (!preserveOriginalFailure) {
                firstException = state->currentException;
                firstExceptionStatus = state->currentExceptionStatus;
                hasFirstException = state->hasCurrentException;
                root = hasFirstException && ZrCore_Value_IsGarbageCollectable(&firstException)
                        ? ZrCore_Value_GetRawObject(&firstException) : ZR_NULL;
            }
        }
    }
    if (preserveOriginalFailure || firstStatus != ZR_THREAD_STATUS_FINE) {
        state->currentException = firstException;
        if (root != ZR_NULL) {
            state->currentException.value.object = root;
        }
        state->currentExceptionStatus = firstExceptionStatus;
        state->hasCurrentException = hasFirstException;
        state->threadStatus = preserveOriginalFailure ? originalStatus : firstStatus;
    }
    (void)ZrCore_Gc_AotRootFramePop(state, &rootFrame);
    return firstStatus;
}

ZR_CORE_API TZrBool execution_jump_to_instruction_offset(SZrState *state,
                                                         SZrCallInfo **ioCallInfo,
                                                         SZrCallInfo *targetCallInfo,
                                                         TZrMemoryOffset instructionOffset);

ZR_CORE_API void execution_clear_pending_control(SZrState *state);

ZR_CORE_API void execution_set_pending_control(SZrState *state,
                                               EZrVmPendingControlKind kind,
                                               SZrCallInfo *callInfo,
                                               TZrMemoryOffset targetInstructionOffset,
                                               TZrUInt32 valueSlot,
                                               const SZrTypeValue *value);

ZR_CORE_API TZrBool execution_resume_pending_via_outer_finally(SZrState *state, SZrCallInfo **ioCallInfo);

ZR_CORE_API TZrBool execution_unwind_exception_to_handler(SZrState *state, SZrCallInfo **ioCallInfo);

#endif // ZR_VM_CORE_EXECUTION_CONTROL_H
