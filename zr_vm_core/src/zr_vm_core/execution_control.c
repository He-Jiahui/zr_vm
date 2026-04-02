//
// Split from execution.c: call/control helpers.
//

#include "execution_internal.h"

TZrSize close_scope_cleanup_registrations(SZrState *state, TZrSize cleanupCount) {
    TZrSize closedCount = 0;
    TZrMemoryOffset savedStackTopOffset;
    SZrCallInfo *currentCallInfo;

    if (state == ZR_NULL || cleanupCount == 0) {
        return 0;
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


TZrBool execution_invoke_meta_call(SZrState *state,
                                   SZrCallInfo *savedCallInfo,
                                   TZrStackValuePointer savedStackTop,
                                   TZrStackValuePointer scratchBase,
                                   TZrBool reloadScratchFromFunctionTop,
                                   SZrMeta *meta,
                                   const SZrTypeValue *arg0,
                                   const SZrTypeValue *arg1,
                                   TZrSize argumentCount,
                                   TZrStackValuePointer *outMetaBase,
                                   TZrStackValuePointer *outSavedStackTop) {
    SZrTypeValue stableArguments[2];
    SZrFunctionStackAnchor savedStackTopAnchor;
    TZrStackValuePointer metaBase;

    if (outMetaBase != ZR_NULL) {
        *outMetaBase = scratchBase;
    }
    if (outSavedStackTop != ZR_NULL) {
        *outSavedStackTop = savedStackTop;
    }

    if (state == ZR_NULL || meta == ZR_NULL || meta->function == ZR_NULL || arg0 == ZR_NULL || argumentCount == 0 ||
        argumentCount > 2) {
        return ZR_FALSE;
    }

    stableArguments[0] = *arg0;
    if (argumentCount > 1) {
        if (arg1 == ZR_NULL) {
            return ZR_FALSE;
        }
        stableArguments[1] = *arg1;
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    metaBase = ZrCore_Function_CheckStackAndGc(state, 1 + argumentCount, scratchBase);
    if (reloadScratchFromFunctionTop && savedCallInfo != ZR_NULL) {
        metaBase = savedCallInfo->functionTop.valuePointer;
    }

    ZrCore_Stack_SetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    ZrCore_Stack_CopyValue(state, metaBase + 1, &stableArguments[0]);
    if (argumentCount > 1) {
        ZrCore_Stack_CopyValue(state, metaBase + 2, &stableArguments[1]);
    }

    state->stackTop.valuePointer = metaBase + 1 + argumentCount;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    metaBase = ZrCore_Function_CallWithoutYieldAndRestore(state, metaBase, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (outMetaBase != ZR_NULL) {
        *outMetaBase = metaBase;
    }
    if (outSavedStackTop != ZR_NULL) {
        *outSavedStackTop = savedStackTop;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static SZrFunction *execution_call_info_function(SZrState *state, SZrCallInfo *callInfo) {
    SZrTypeValue *baseValue;

    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    baseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (baseValue == ZR_NULL || baseValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (baseValue->type == ZR_VALUE_TYPE_CLOSURE) {
        if (baseValue->isNative) {
            return ZR_NULL;
        }
        SZrClosure *frameClosure = ZR_CAST_VM_CLOSURE(state, baseValue->value.object);
        return frameClosure != ZR_NULL ? frameClosure->function : ZR_NULL;
    }
    if (baseValue->type == ZR_VALUE_TYPE_FUNCTION) {
        if (baseValue->isNative) {
            return ZR_NULL;
        }
        return ZR_CAST_FUNCTION(state, baseValue->value.object);
    }

    return ZR_NULL;
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

    newCapacity = state->exceptionHandlerStackCapacity > 0 ? state->exceptionHandlerStackCapacity : 8;
    while (newCapacity < minCapacity) {
        newCapacity *= 2;
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

static void execution_pop_handlers_for_callinfo(SZrState *state, SZrCallInfo *callInfo) {
    while (state != ZR_NULL && state->exceptionHandlerStackLength > 0 &&
           state->exceptionHandlerStack[state->exceptionHandlerStackLength - 1].callInfo == callInfo) {
        state->exceptionHandlerStackLength--;
    }
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
    if (state == ZR_NULL) {
        return;
    }

    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_EXCEPTION;
    state->pendingControl.callInfo = callInfo;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

void execution_clear_pending_control(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

void execution_set_pending_control(SZrState *state,
                                   EZrVmPendingControlKind kind,
                                   SZrCallInfo *callInfo,
                                   TZrMemoryOffset targetInstructionOffset,
                                   TZrUInt32 valueSlot,
                                   const SZrTypeValue *value) {
    if (state == ZR_NULL) {
        return;
    }

    state->pendingControl.kind = kind;
    state->pendingControl.callInfo = callInfo;
    state->pendingControl.targetInstructionOffset = targetInstructionOffset;
    state->pendingControl.valueSlot = valueSlot;
    if (value != ZR_NULL) {
        ZrCore_Value_Copy(state, &state->pendingControl.value, (SZrTypeValue *)value);
        state->pendingControl.hasValue = ZR_TRUE;
    } else {
        ZrCore_Value_ResetAsNull(&state->pendingControl.value);
        state->pendingControl.hasValue = ZR_FALSE;
    }
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
                execution_pop_exception_handler(state, handlerState);
                continue;
            }

            if (handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_TRY) {
                for (TZrUInt32 catchIndex = 0; catchIndex < handlerInfo->catchClauseCount; catchIndex++) {
                    SZrFunctionCatchClauseInfo *catchInfo =
                            &function->catchClauseList[handlerInfo->catchClauseStartIndex + catchIndex];
                    if (ZrCore_Exception_CatchMatchesTypeName(state, &state->currentException, catchInfo->typeName)) {
                        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_CATCH;
                        return execution_jump_to_instruction_offset(state,
                                                                    ioCallInfo,
                                                                    callInfo,
                                                                    catchInfo->targetInstructionOffset);
                    }
                }
            }

            if (handlerInfo->hasFinally) {
                handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
                execution_set_pending_exception(state, callInfo);
                return execution_jump_to_instruction_offset(state,
                                                            ioCallInfo,
                                                            callInfo,
                                                            handlerInfo->finallyTargetInstructionOffset);
            }

            execution_pop_exception_handler(state, handlerState);
        }

        execution_pop_handlers_for_callinfo(state, callInfo);
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
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

