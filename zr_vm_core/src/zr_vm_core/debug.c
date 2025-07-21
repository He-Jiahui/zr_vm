//
// Created by HeJiahui on 2025/6/26.
//
#include "zr_vm_core/debug.h"

#include "zr_vm_core/state.h"

TBool ZrDebugInfoGet(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(type);
    ZR_TODO_PARAMETER(debugInfo);
    return ZR_FALSE;
}

void ZrDebugCallError(struct SZrState *state, struct SZrTypeValue *value) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(value);
}

void ZrDebugRunError(struct SZrState *state, TNativeString format, ...) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(format);
}


void ZrDebugErrorWhenHandlingError(struct SZrState *state) {
    // TODO:
}


void ZrDebugHook(struct SZrState *state, EZrDebugHookEvent event, TUInt32 line, TUInt32 transferStart,
                 TUInt32 transferCount) {
    FZrDebugHook hook = state->debugHook;
    if (hook && state->allowDebugHook) {
        EZrDebugHookMask mask = ZR_CALL_STATUS_DEBUG_HOOK;
        SZrCallInfo *callInfo = state->callInfoList;
        TZrMemoryOffset top = ZrStackSavePointerAsOffset(state, state->stackTop.valuePointer);
        TZrMemoryOffset callInfoTop = ZrStackSavePointerAsOffset(state, callInfo->functionTop.valuePointer);
        SZrDebugInfo debugInfo;
        debugInfo.event = event;
        debugInfo.currentLine = line;
        debugInfo.callInfo = callInfo;
        if (transferCount != 0) {
            mask |= ZR_CALL_STATUS_CALL_INFO_TRANSFER;
            callInfo->yieldContext.transferStart = transferStart;
            callInfo->yieldContext.transferCount = transferCount;
        }
        if (ZR_CALL_INFO_IS_VM(callInfo) && state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_MIN;
        }
        state->allowDebugHook = ZR_FALSE;
        callInfo->callStatus |= mask;
        ZR_THREAD_UNLOCK(state);
        hook(state, &debugInfo);
        ZR_THREAD_LOCK(state);

        ZR_ASSERT(!state->allowDebugHook);
        state->allowDebugHook = ZR_TRUE;
        callInfo->functionTop.valuePointer = ZrStackLoadOffsetToPointer(state, callInfoTop);
        state->stackTop.valuePointer = ZrStackLoadOffsetToPointer(state, top);
        callInfo->callStatus &= ~mask;
    }
}

void ZrDebugHookReturn(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount) {
    if (state->debugHookSignal & ZR_DEBUG_HOOK_MASK_RETURN) {
        TZrStackValuePointer stackPointer = callInfo->functionTop.valuePointer - resultCount;
        TUInt32 totalArgumentsCount = 0;
        TInt32 transferStart = 0;
        if (ZR_CALL_INFO_IS_VM(callInfo)) {
            SZrTypeValue *functionValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
            SZrFunction *function = (ZR_CAST_VM_CLOSURE(state, functionValue->value.object))->function;
            if (function->hasVariableArguments) {
                totalArgumentsCount = callInfo->context.context.variableArgumentCount + function->parameterCount + 1;
            }
        }
        callInfo->functionBase.valuePointer += totalArgumentsCount;
        transferStart = ZR_CAST_UINT(stackPointer - callInfo->functionBase.valuePointer);
        ZrDebugHook(state, ZR_DEBUG_HOOK_EVENT_RETURN, -1, transferStart, resultCount);
        callInfo->functionBase.valuePointer -= totalArgumentsCount;
    }
    callInfo = callInfo->previous;
    if (ZR_CALL_INFO_IS_VM(callInfo)) {
        SZrTypeValue *functionValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
        SZrFunction *function = (ZR_CAST_VM_CLOSURE(state, functionValue->value.object))->function;
        state->previousProgramCounter =
                ZR_CAST_INT64(callInfo->context.context.programCounter - function->instructionsList) - 1;
    }
}
