//
// Split from execution.c: call/control helpers.
//

#include "execution/execution_internal.h"

#include <string.h>

#include "zr_vm_core/execution_control.h"
#include "zr_vm_common/zr_aot_abi.h"

#define ZR_EXCEPTION_HANDLER_STACK_INITIAL_CAPACITY 8U
#define ZR_EXCEPTION_HANDLER_STACK_GROWTH_FACTOR 2U
#define ZR_SCOPE_CLEANUP_CLOSED_COUNT_NONE ((TZrSize)0)

static SZrVmExceptionHandlerState *execution_find_top_handler_for_callinfo(SZrState *state, SZrCallInfo *callInfo);
static SZrFunction *execution_call_info_function(SZrState *state, SZrCallInfo *callInfo);
static TZrStackValuePointer execution_resolve_meta_scratch_base(TZrStackValuePointer savedStackTop,
                                                                TZrStackValuePointer requestedScratchBase,
                                                                const SZrCallInfo *savedCallInfo);

static void execution_close_exception_scope_registrations(
        SZrState *state,
        const SZrVmExceptionHandlerState *handlerState) {
    TZrStackValuePointer boundary;

    if (state == ZR_NULL || handlerState == ZR_NULL) {
        return;
    }
    boundary = ZrCore_Stack_LoadOffsetToPointer(
            state, handlerState->toBeClosedBoundaryOffset);
    while (state->toBeClosedValueList.valuePointer > boundary) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        ZrCore_Closure_CloseStackValue(state, toBeClosed.valuePointer);
        if (ZrCore_Closure_CloseRegisteredValues(
                    state,
                    1U,
                    state->currentExceptionStatus,
                    ZR_FALSE) == 0U) {
            break;
        }
    }
}

TZrSize close_scope_cleanup_registrations(SZrState *state, TZrSize cleanupCount) {
    TZrSize closedCount = ZR_SCOPE_CLEANUP_CLOSED_COUNT_NONE;
    TZrMemoryOffset savedStackTopOffset;
    SZrCallInfo *currentCallInfo;

    if (state == ZR_NULL || cleanupCount == 0) {
        return ZR_SCOPE_CLEANUP_CLOSED_COUNT_NONE;
    }

    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    currentCallInfo = state->callInfoList;
    if (currentCallInfo != ZR_NULL &&
        state->stackTop.valuePointer < currentCallInfo->functionTop.valuePointer) {
        state->stackTop.valuePointer = currentCallInfo->functionTop.valuePointer;
    }

    while (closedCount < cleanupCount &&
           state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        ZrCore_Closure_CloseStackValue(state, toBeClosed.valuePointer);
        ZrCore_Closure_CloseRegisteredValues(state, 1, ZR_THREAD_STATUS_INVALID, ZR_FALSE);
        closedCount++;
    }

    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    return closedCount;
}


TZrBool execution_invoke_meta_call_to_destination(SZrState *state,
                                                  SZrCallInfo *savedCallInfo,
                                                  TZrStackValuePointer savedStackTop,
                                                  TZrStackValuePointer requestedScratchBase,
                                                  SZrMeta *meta,
                                                  const SZrTypeValue *arg0,
                                                  const SZrTypeValue *arg1,
                                                  TZrSize argumentCount,
                                                  TZrStackValuePointer returnDestination,
                                                  TZrStackValuePointer *outMetaBase,
                                                  TZrStackValuePointer *outSavedStackTop) {
    SZrTypeValue stableArguments[ZR_META_CALL_MAX_ARGUMENTS];
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor metaBaseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor originalCallInfoTopAnchor;
    SZrFunctionStackAnchor activeCallInfoTopAnchor;
    SZrFunctionStackAnchor returnDestinationAnchor;
    TZrStackValuePointer scratchBase;
    TZrStackValuePointer metaBase;
    TZrBool hasCallInfoAnchors = ZR_FALSE;
    TZrBool hasActiveCallInfoTopAnchor = ZR_FALSE;
    TZrBool hasReturnDestinationAnchor = ZR_FALSE;

    scratchBase = execution_resolve_meta_scratch_base(savedStackTop, requestedScratchBase, savedCallInfo);
    ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_META_FALLBACK);

    if (outMetaBase != ZR_NULL) {
        *outMetaBase = scratchBase;
    }
    if (outSavedStackTop != ZR_NULL) {
        *outSavedStackTop = savedStackTop;
    }

    if (state == ZR_NULL || meta == ZR_NULL || meta->function == ZR_NULL || arg0 == ZR_NULL || argumentCount == 0 ||
        argumentCount > ZR_META_CALL_MAX_ARGUMENTS) {
        return ZR_FALSE;
    }

    stableArguments[0] = *arg0;
    if (argumentCount > ZR_META_CALL_UNARY_ARGUMENT_COUNT) {
        if (arg1 == ZR_NULL) {
            return ZR_FALSE;
        }
        stableArguments[1] = *arg1;
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, scratchBase, &metaBaseAnchor);
    if (returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, returnDestination, &returnDestinationAnchor);
        hasReturnDestinationAnchor = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &originalCallInfoTopAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &activeCallInfoTopAnchor);
        hasCallInfoAnchors = ZR_TRUE;
        hasActiveCallInfoTopAnchor = ZR_TRUE;
    }
    metaBase = ZrCore_Function_ReserveScratchSlots(state, ZR_META_CALL_SLOT_COUNT(argumentCount), scratchBase);
    if (metaBase == ZR_NULL) {
        return ZR_FALSE;
    }
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    metaBase = ZrCore_Function_StackAnchorRestore(state, &metaBaseAnchor);
    if (hasReturnDestinationAnchor) {
        returnDestination = ZrCore_Function_StackAnchorRestore(state, &returnDestinationAnchor);
    }
    if (hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &originalCallInfoTopAnchor);
    }

    state->stackTop.valuePointer = ZR_META_CALL_STACK_TOP(metaBase, argumentCount);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &activeCallInfoTopAnchor);
        hasActiveCallInfoTopAnchor = ZR_TRUE;
    }

    ZrCore_Stack_SetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    ZrCore_Stack_CopyValue(state, ZR_META_CALL_SELF_SLOT(metaBase), &stableArguments[0]);
    metaBase = ZrCore_Function_StackAnchorRestore(state, &metaBaseAnchor);
    if (hasReturnDestinationAnchor) {
        returnDestination = ZrCore_Function_StackAnchorRestore(state, &returnDestinationAnchor);
    }
    if (hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                    hasActiveCallInfoTopAnchor
                                                                                            ? &activeCallInfoTopAnchor
                                                                                            : &originalCallInfoTopAnchor);
    }
    if (argumentCount > ZR_META_CALL_UNARY_ARGUMENT_COUNT) {
        ZrCore_Stack_CopyValue(state, ZR_META_CALL_SECOND_ARGUMENT_SLOT(metaBase), &stableArguments[1]);
        metaBase = ZrCore_Function_StackAnchorRestore(state, &metaBaseAnchor);
        if (hasReturnDestinationAnchor) {
            returnDestination = ZrCore_Function_StackAnchorRestore(state, &returnDestinationAnchor);
        }
        if (hasCallInfoAnchors) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                        hasActiveCallInfoTopAnchor
                                                                                                ? &activeCallInfoTopAnchor
                                                                                                : &originalCallInfoTopAnchor);
        }
    }

    if (returnDestination != ZR_NULL) {
        metaBase = ZrCore_Function_CallWithoutYieldAndRestoreWithReturnDestination(
                state, metaBase, 1, returnDestination);
    } else {
        metaBase = ZrCore_Function_CallWithoutYieldAndRestore(state, metaBase, 1);
    }
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &originalCallInfoTopAnchor);
    }
    if (outMetaBase != ZR_NULL) {
        *outMetaBase = metaBase;
    }
    if (outSavedStackTop != ZR_NULL) {
        *outSavedStackTop = savedStackTop;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

TZrBool execution_invoke_meta_call(SZrState *state,
                                   SZrCallInfo *savedCallInfo,
                                   TZrStackValuePointer savedStackTop,
                                   TZrStackValuePointer requestedScratchBase,
                                   SZrMeta *meta,
                                   const SZrTypeValue *arg0,
                                   const SZrTypeValue *arg1,
                                   TZrSize argumentCount,
                                   TZrStackValuePointer *outMetaBase,
                                   TZrStackValuePointer *outSavedStackTop) {
    return execution_invoke_meta_call_to_destination(state,
                                                     savedCallInfo,
                                                     savedStackTop,
                                                     requestedScratchBase,
                                                     meta,
                                                     arg0,
                                                     arg1,
                                                     argumentCount,
                                                     ZR_NULL,
                                                     outMetaBase,
                                                     outSavedStackTop);
}

static TZrStackValuePointer execution_resolve_meta_scratch_base(TZrStackValuePointer savedStackTop,
                                                                TZrStackValuePointer requestedScratchBase,
                                                                const SZrCallInfo *savedCallInfo) {
    TZrStackValuePointer scratchBase = requestedScratchBase;

    if (savedStackTop != ZR_NULL && (scratchBase == ZR_NULL || scratchBase < savedStackTop)) {
        scratchBase = savedStackTop;
    }

    if (savedCallInfo != ZR_NULL &&
        (scratchBase == ZR_NULL || scratchBase < savedCallInfo->functionTop.valuePointer)) {
        scratchBase = savedCallInfo->functionTop.valuePointer;
    }

    return scratchBase;
}

static SZrFunction *execution_call_info_function(SZrState *state, SZrCallInfo *callInfo) {
    return ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
}

static TZrBool execution_exception_handler_stack_ensure_capacity(SZrState *state, TZrUInt32 minCapacity) {
    SZrVmExceptionHandlerState *newHandlers;
    TZrUInt32 newCapacity;
    TZrSize bytes;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state->exceptionHandlerStackCapacity >= minCapacity) {
        return ZR_TRUE;
    }

    newCapacity = state->exceptionHandlerStackCapacity > 0
                      ? state->exceptionHandlerStackCapacity
                      : ZR_EXCEPTION_HANDLER_STACK_INITIAL_CAPACITY;
    while (newCapacity < minCapacity) {
        newCapacity *= ZR_EXCEPTION_HANDLER_STACK_GROWTH_FACTOR;
    }

    bytes = newCapacity * sizeof(SZrVmExceptionHandlerState);
    newHandlers = (SZrVmExceptionHandlerState *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                                bytes,
                                                                                ZR_MEMORY_NATIVE_TYPE_STATE);
    if (newHandlers == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newHandlers, 0, bytes);
    if (state->exceptionHandlerStack != ZR_NULL && state->exceptionHandlerStackLength > 0) {
        memcpy(newHandlers,
               state->exceptionHandlerStack,
               state->exceptionHandlerStackLength * sizeof(SZrVmExceptionHandlerState));
        ZrCore_Memory_RawFreeWithType(state->global,
                                      state->exceptionHandlerStack,
                                      state->exceptionHandlerStackCapacity * sizeof(SZrVmExceptionHandlerState),
                                      ZR_MEMORY_NATIVE_TYPE_STATE);
    }

    state->exceptionHandlerStack = newHandlers;
    state->exceptionHandlerStackCapacity = newCapacity;
    return ZR_TRUE;
}

TZrBool execution_push_exception_handler(SZrState *state, SZrCallInfo *callInfo, TZrUInt32 handlerIndex) {
    SZrVmExceptionHandlerState *handlerState;

    if (state == ZR_NULL || callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!execution_exception_handler_stack_ensure_capacity(state, state->exceptionHandlerStackLength + 1)) {
        return ZR_FALSE;
    }

    handlerState = &state->exceptionHandlerStack[state->exceptionHandlerStackLength++];
    handlerState->callInfo = callInfo;
    handlerState->handlerIndex = handlerIndex;
    handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_TRY;
    handlerState->toBeClosedBoundaryOffset =
            ZrCore_Stack_SavePointerAsOffset(
                    state, state->toBeClosedValueList.valuePointer);
    return ZR_TRUE;
}

SZrVmExceptionHandlerState *execution_find_handler_state(SZrState *state,
                                                         SZrCallInfo *callInfo,
                                                         TZrUInt32 handlerIndex) {
    if (state == ZR_NULL || callInfo == ZR_NULL || state->exceptionHandlerStackLength == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = state->exceptionHandlerStackLength; index > 0; index--) {
        SZrVmExceptionHandlerState *handlerState = &state->exceptionHandlerStack[index - 1];
        if (handlerState->callInfo == callInfo && handlerState->handlerIndex == handlerIndex) {
            return handlerState;
        }
    }

    return ZR_NULL;
}

TZrBool execution_has_exception_handler_for_callinfo(SZrState *state, SZrCallInfo *callInfo) {
    return execution_find_top_handler_for_callinfo(state, callInfo) != ZR_NULL;
}

static SZrVmExceptionHandlerState *execution_find_top_handler_for_callinfo(SZrState *state, SZrCallInfo *callInfo) {
    if (state == ZR_NULL || callInfo == ZR_NULL || state->exceptionHandlerStackLength == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = state->exceptionHandlerStackLength; index > 0; index--) {
        SZrVmExceptionHandlerState *handlerState = &state->exceptionHandlerStack[index - 1];
        if (handlerState->callInfo == callInfo) {
            return handlerState;
        }
    }

    return ZR_NULL;
}

void execution_pop_exception_handler(SZrState *state, SZrVmExceptionHandlerState *handlerState) {
    TZrUInt32 index;

    if (state == ZR_NULL || handlerState == ZR_NULL || state->exceptionHandlerStackLength == 0) {
        return;
    }

    index = (TZrUInt32)(handlerState - state->exceptionHandlerStack);
    if (index >= state->exceptionHandlerStackLength) {
        return;
    }

    memmove(&state->exceptionHandlerStack[index],
            &state->exceptionHandlerStack[index + 1],
            (state->exceptionHandlerStackLength - index - 1) * sizeof(SZrVmExceptionHandlerState));
    state->exceptionHandlerStackLength--;
}

void execution_discard_exception_handlers_for_callinfo(SZrState *state, SZrCallInfo *callInfo) {
    if (state == ZR_NULL || callInfo == ZR_NULL || state->exceptionHandlerStackLength == 0u) {
        return;
    }

    execution_discard_exception_handlers_for_callinfo_fast(state, callInfo);
}

const SZrFunctionExceptionHandlerInfo *execution_lookup_exception_handler_info(
        SZrState *state,
        const SZrVmExceptionHandlerState *handlerState,
        SZrFunction **outFunction) {
    SZrFunction *function;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }

    if (state == ZR_NULL || handlerState == ZR_NULL) {
        return ZR_NULL;
    }

    function = execution_call_info_function(state, handlerState->callInfo);
    if (outFunction != ZR_NULL) {
        *outFunction = function;
    }
    if (function == ZR_NULL || function->exceptionHandlerList == ZR_NULL ||
        handlerState->handlerIndex >= function->exceptionHandlerCount) {
        return ZR_NULL;
    }

    return &function->exceptionHandlerList[handlerState->handlerIndex];
}

TZrBool execution_jump_to_instruction_offset(SZrState *state,
                                             SZrCallInfo **ioCallInfo,
                                             SZrCallInfo *targetCallInfo,
                                             TZrMemoryOffset instructionOffset) {
    SZrFunction *function;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || targetCallInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    function = execution_call_info_function(state, targetCallInfo);
    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionOffset > function->instructionsLength) {
        return ZR_FALSE;
    }

    targetCallInfo->context.context.programCounter = function->instructionsList + instructionOffset;
    state->callInfoList = targetCallInfo;
    state->stackTop.valuePointer = targetCallInfo->functionTop.valuePointer;
    *ioCallInfo = targetCallInfo;
    return ZR_TRUE;
}

static void execution_set_pending_exception(SZrState *state, SZrCallInfo *callInfo) {
    execution_set_pending_control(state, ZR_VM_PENDING_CONTROL_EXCEPTION,
                                  callInfo, 0u, 0u, ZR_NULL);
}

static void execution_release_pending_value(SZrState *state, TZrPtr value) {
    ZrCore_Ownership_ReleaseValue(state, (SZrTypeValue *)value);
}

static void execution_clear_pending_body(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(argument);
    execution_clear_pending_control(state);
}

typedef struct SZrPendingValueCopy {
    SZrTypeValue source;
    SZrTypeValue destination;
} SZrPendingValueCopy;

static void execution_copy_pending_value(SZrState *state, TZrPtr argument) {
    SZrPendingValueCopy *copy = (SZrPendingValueCopy *)argument;
    ZrCore_Value_Copy(state, &copy->destination, &copy->source);
}

static const SZrAotGcRootSlot execution_pending_roots[] = {
    {0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
    {0u, (TZrUInt32)sizeof(SZrRawObject *), 0u, 0u,
     ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u}
};
static const SZrAotGcRootMap execution_pending_root_map = {
    2u, execution_pending_roots
};

static SZrRawObject *execution_pending_root(const SZrTypeValue *value) {
    return ZrCore_Value_IsGarbageCollectable(value)
                   ? ZrCore_Value_GetRawObject(value) : ZR_NULL;
}

void execution_clear_pending_control(SZrState *state) {
    SZrTypeValue previous;
    SZrTypeValue savedException;
    SZrRawObject *roots[2] = {ZR_NULL, ZR_NULL};
    SZrAotGcRootFrame rootFrame;
    EZrThreadStatus savedExceptionStatus;
    EZrThreadStatus savedThreadStatus;
    EZrThreadStatus releaseStatus;
    TZrBool hadException;

    if (state == ZR_NULL) {
        return;
    }

    /* Detach before release: a final owner can reenter through resource Drop. */
    previous = state->pendingControl.value;
    savedException = state->currentException;
    savedExceptionStatus = state->currentExceptionStatus;
    savedThreadStatus = state->threadStatus;
    hadException = state->hasCurrentException;
    roots[0] = hadException ? execution_pending_root(&savedException) : ZR_NULL;
    if (!ZrCore_Gc_AotRootFramePush(state, &rootFrame,
                (TZrStackValuePointer)roots, &execution_pending_root_map)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    }
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    releaseStatus = ZrCore_Exception_TryRun(state, execution_release_pending_value, &previous);
    if (releaseStatus == ZR_THREAD_STATUS_FINE) {
        state->currentException = savedException;
        if (roots[0] != ZR_NULL) {
            state->currentException.value.object = roots[0];
        }
        state->currentExceptionStatus = savedExceptionStatus;
        state->hasCurrentException = hadException;
        state->threadStatus = savedThreadStatus;
    }
    (void)ZrCore_Gc_AotRootFramePop(state, &rootFrame);
    if (releaseStatus != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, releaseStatus);
    }
}

void execution_set_pending_control(SZrState *state,
                                   EZrVmPendingControlKind kind,
                                   SZrCallInfo *callInfo,
                                   TZrMemoryOffset targetInstructionOffset,
                                   TZrUInt32 valueSlot,
                                   const SZrTypeValue *value) {
    SZrTypeValue replacement;
    SZrPendingValueCopy copy;
    SZrRawObject *roots[2] = {ZR_NULL, ZR_NULL};
    SZrAotGcRootFrame rootFrame;
    EZrThreadStatus releaseStatus;

    if (state == ZR_NULL) {
        return;
    }

    /* Stack-local roots avoid allocating while protecting a release callback. */
    if (!ZrCore_Gc_AotRootFramePush(state, &rootFrame,
                (TZrStackValuePointer)roots, &execution_pending_root_map)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    }
    /* Retain before clearing, including when value aliases pending storage. */
    ZrCore_Value_ResetAsNull(&replacement);
    if (value != ZR_NULL) {
        TZrBool addedPin;
        copy.source = *value;
        ZrCore_Value_ResetAsNull(&copy.destination);
        roots[0] = execution_pending_root(&copy.source);
        addedPin = roots[0] != ZR_NULL &&
                   (roots[0]->garbageCollectMark.pinFlags &
                    ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0u;
        if (addedPin) {
            ZrCore_GarbageCollector_PinObject(
                    state, roots[0], ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
        }
        releaseStatus = ZrCore_Exception_TryRun(state, execution_copy_pending_value, &copy);
        if (addedPin) {
            roots[0]->garbageCollectMark.pinFlags &=
                    (TZrUInt32)~((TZrUInt32)ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
        }
        if (releaseStatus != ZR_THREAD_STATUS_FINE) {
            (void)ZrCore_Gc_AotRootFramePop(state, &rootFrame);
            ZrCore_Exception_Throw(state, releaseStatus);
        }
        replacement = copy.destination;
    }
    roots[0] = execution_pending_root(&replacement);
    releaseStatus = ZrCore_Exception_TryRun(state, execution_clear_pending_body, ZR_NULL);
    if (roots[0] != ZR_NULL) {
        replacement.value.object = roots[0];
    }
    if (releaseStatus != ZR_THREAD_STATUS_FINE) {
        SZrTypeValue failure = state->currentException;
        TZrBool hadFailure = state->hasCurrentException;
        EZrThreadStatus failureStatus = state->currentExceptionStatus;
        roots[1] = hadFailure ? execution_pending_root(&failure) : ZR_NULL;
        (void)ZrCore_Exception_TryRun(state, execution_release_pending_value, &replacement);
        state->currentException = failure;
        if (roots[1] != ZR_NULL) {
            state->currentException.value.object = roots[1];
        }
        state->currentExceptionStatus = failureStatus;
        state->hasCurrentException = hadFailure;
        state->threadStatus = releaseStatus;
        (void)ZrCore_Gc_AotRootFramePop(state, &rootFrame);
        ZrCore_Exception_Throw(state, releaseStatus);
    }
    state->pendingControl.kind = kind;
    state->pendingControl.callInfo = callInfo;
    state->pendingControl.targetInstructionOffset = targetInstructionOffset;
    state->pendingControl.valueSlot = valueSlot;
    state->pendingControl.value = replacement;
    state->pendingControl.hasValue = (TZrBool)(value != ZR_NULL);
    (void)ZrCore_Gc_AotRootFramePop(state, &rootFrame);
}

TZrBool execution_resume_pending_via_outer_finally(SZrState *state, SZrCallInfo **ioCallInfo) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || *ioCallInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = *ioCallInfo;
    for (TZrUInt32 index = state->exceptionHandlerStackLength; index > 0; index--) {
        SZrVmExceptionHandlerState *handlerState = &state->exceptionHandlerStack[index - 1];
        SZrFunction *function = ZR_NULL;
        const SZrFunctionExceptionHandlerInfo *handlerInfo;

        if (handlerState->callInfo != callInfo) {
            break;
        }

        handlerInfo = execution_lookup_exception_handler_info(state, handlerState, &function);
        if (handlerInfo != ZR_NULL &&
            (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_BREAK ||
             state->pendingControl.kind == ZR_VM_PENDING_CONTROL_CONTINUE) &&
            state->pendingControl.targetInstructionOffset >= handlerInfo->protectedStartInstructionOffset &&
            state->pendingControl.targetInstructionOffset < handlerInfo->afterFinallyInstructionOffset) {
            return ZR_FALSE;
        }
        if (handlerInfo == ZR_NULL || !handlerInfo->hasFinally ||
            handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY) {
            continue;
        }

        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
        return execution_jump_to_instruction_offset(state,
                                                    ioCallInfo,
                                                    callInfo,
                                                    handlerInfo->finallyTargetInstructionOffset);
    }

    return ZR_FALSE;
}

TZrBool execution_unwind_exception_to_handler(SZrState *state, SZrCallInfo **ioCallInfo) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || *ioCallInfo == ZR_NULL || !state->hasCurrentException) {
        return ZR_FALSE;
    }

    callInfo = *ioCallInfo;
    while (callInfo != ZR_NULL) {
        if (!ZR_CALL_INFO_IS_VM(callInfo)) {
            state->callInfoList = callInfo;
            if (callInfo->functionTop.valuePointer != ZR_NULL) {
                state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
            }
            break;
        }

        for (;;) {
            SZrVmExceptionHandlerState *handlerState = execution_find_top_handler_for_callinfo(state, callInfo);
            SZrFunction *function = ZR_NULL;
            const SZrFunctionExceptionHandlerInfo *handlerInfo;

            if (handlerState == ZR_NULL) {
                break;
            }

            handlerInfo = execution_lookup_exception_handler_info(state, handlerState, &function);
            if (handlerInfo == ZR_NULL) {
                execution_pop_exception_handler(state, handlerState);
                continue;
            }

            if (handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY) {
                execution_close_exception_scope_registrations(
                        state, handlerState);
                execution_pop_exception_handler(state, handlerState);
                continue;
            }

            if (handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_TRY) {
                for (TZrUInt32 catchIndex = 0; catchIndex < handlerInfo->catchClauseCount; catchIndex++) {
                    SZrFunctionCatchClauseInfo *catchInfo =
                            &function->catchClauseList[handlerInfo->catchClauseStartIndex + catchIndex];
                    if (ZrCore_Exception_CatchMatchesTypeName(state, &state->currentException, catchInfo->typeName)) {
                        execution_close_exception_scope_registrations(
                                state, handlerState);
                        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_CATCH;
                        state->threadStatus = ZR_THREAD_STATUS_FINE;
                        return execution_jump_to_instruction_offset(state,
                                                                    ioCallInfo,
                                                                    callInfo,
                                                                    catchInfo->targetInstructionOffset);
                    }
                }
            }

            if (handlerInfo->hasFinally) {
                execution_close_exception_scope_registrations(
                        state, handlerState);
                handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
                execution_set_pending_exception(state, callInfo);
                state->threadStatus = ZR_THREAD_STATUS_FINE;
                return execution_jump_to_instruction_offset(state,
                                                            ioCallInfo,
                                                            callInfo,
                                                            handlerInfo->finallyTargetInstructionOffset);
            }

            execution_close_exception_scope_registrations(
                    state, handlerState);
            execution_pop_exception_handler(state, handlerState);
        }

        execution_discard_exception_handlers_for_callinfo_fast(state, callInfo);
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        {
            SZrFunction *unwindFunction =
                    ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
            TZrStackValuePointer unwindFrameBase =
                    callInfo->functionBase.valuePointer != ZR_NULL
                            ? callInfo->functionBase.valuePointer + 1
                            : ZR_NULL;
            if (unwindFunction != ZR_NULL && unwindFrameBase != ZR_NULL) {
                (void)ZrCore_Function_DropInlineFrameValuesOnUnwind(
                        state,
                        unwindFunction,
                        unwindFrameBase,
                        ZrCore_Function_ResolvePrototypeFrameTypeLayout,
                        state);
            }
        }
        ZrCore_Closure_CloseClosure(state,
                                    callInfo->functionBase.valuePointer + 1,
                                    state->currentExceptionStatus,
                                    ZR_FALSE);
        state->callInfoList = callInfo->previous;
        callInfo = callInfo->previous;
        if (callInfo != ZR_NULL) {
            state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        }
    }

    return ZR_FALSE;
}

